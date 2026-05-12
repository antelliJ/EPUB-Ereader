#include <Arduino.h>
#include "LittleFS.h"
// TODO: 
// x fix discrepencies in TextRenderer of calculateNextPage and regular rendering, 
// x since a little off on where starting and ending -- something with cutting off newline to put on next page?
//
// x fix get_total_pages
// x fix page numbering in text render and its pointer
// x section_page_to_global_page and the others should use a hashmap somewhere
// x add a "go to page" feature that uses the global page number to jump to the correct section and page within that section (for toc)
// x make menu stuff
// x add table of contents support in epub reader
// x (just text) add loading img when loading book / section
// x add bookmark support in epub reader
// x open last saved page from bookmark when opening book
// add bookmark export with webserver (current page and the text of the page)
//    x? Goto feature (test it more)
//      x - add ui
//    x upload books through webserver
//    x delete books through webserver
// 
//    x view bookmarks by creating reader (no rendering tho - headless), then display on webserver
//       add textRenderer get page content (already exists??)
//       add feature to prettify the output (not really necessary ig)
// only restart device if webserver has made file system writes (some flag) 

//    maybe clean up the discrepencies between how webserver saves filepath, and change to state version
//        ("/littlefs/" + server->arg("file"))

// GxEPD2_HelloWorld.ino by Jean-Marc Zingg
//
// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>

#define USE_HSPI_FOR_EPD

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
// Ima just comment these out
// #include <GxEPD2_4C.h>
// #include <GxEPD2_7C.h>


#include <Fonts/FreeMonoBold9pt7b.h> // First Font test
#include <Fonts/FreeSans9pt7b.h> // A cleaner font for long text

#include "sampletext.h"


#include <SPI.h>

#include "sampleReads.h"
#include "TextRenderer.h"
#include "HtmlParser.h"

#include "display_config.h"

// #include "epub.h" // This is just for testing, not used in main code (EpubReader loads it)
#include "EpubReader.h"
#include "EpubList.h"
#include "EpubToc.h"

#include "State.h"

#include "bookmark.h"
#include "ClaudeWebServer.h"

// #include "SD.h"
// #define SD_CS 5

#include "Actions.h"
#include "RemoteTable.h"

#define EPD_MOSI 11
#define EPD_SCK  12
#define EPD_CS   10
#define EPD_DC   17
#define EPD_RST  16
#define EPD_BUSY 4

#define NEXT_BUTTON_PIN 7
#define PREV_BUTTON_PIN 6

// Add this near your #defines
// #define MAX_DISPLAY_BUFFER_SIZE 65536ul // 64k limit for buffer
// #define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

// #define GxEPD2_DRIVER_CLASS GxEPD2_583c_Z83
// #define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.
// #define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))

// GxEPD2_3C<GxEPD2_583c_Z83, 480> display(GxEPD2_DRIVER_CLASS(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));

// GxEPD2_3C<GxEPD2_583c_Z83, 480> display(GxEPD2_583c_Z83(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // THIS IS WORKING :D

  // GxEPD2_3C<GxEPD2_583c_GDEQ0583Z31, 480>display(GxEPD2_583c_GDEQ0583Z31(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // This ALSO works, maybe a TAD faster ??
// GxEPD2_BW<GxEPD2_420_GDEY042T81, 400> display(GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // This is just for testing the black and white display, not used in main code

typedef enum
{
  SELECTING_EPUB,
  SELECTING_TABLE_CONTENTS,
  READING_EPUB,
  // OPENING_WEB_SERVER
} UIState;

RTC_NOINIT_ATTR UIState ui_state = SELECTING_EPUB;


DISPLAY_TYPE display(DISPLAY_DRIVER(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

RTC_DATA_ATTR EpubListState epub_list_state;

// the state data for the epub index list
RTC_DATA_ATTR EpubTocState epub_index_state = {0, 0, 0}; 

SPIClass hspi(HSPI);

// Pagination variables
int pageStarts[10]; // Stores the char index where each page begins
int currentPage = 0;
int totalPages = 0; // NOT USED IN EPUB

unsigned long btnNextPressTime = 0;
unsigned long btnPrevPressTime = 0;
bool btnNextPressed = false;
bool btnPrevPressed = false;

const unsigned long HOLD_DURATION = 1000; // 1 second hold duration for page turn rendering

TextRenderer<DISPLAY_TYPE>* renderer = nullptr;
static EpubList *epub_list = nullptr;
static EpubReader *reader = nullptr;
static EpubToc *contents = nullptr;
static BookmarkManager *bookmark_manager = nullptr;

static webServer *web_server = nullptr;

void drawTextPage(int pageNum);
void epub_read_test(void *parameter);
void clearWindow();

void setup()
{
  
  // Initialize SPI with your custom MOSI/SCK
  // hspi.begin(CLK, MISO, MOSI, SS)
  hspi.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  
  // pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
  // pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);

  
  Serial.begin(115200);
  esp_log_level_set("TextRenderer", ESP_LOG_INFO); // Set log level to INFO for all tags
  esp_log_level_set("EPUB", ESP_LOG_INFO);
  
  setupRemote();

  display.setTextWrap(false);

  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  Serial.println("Listing files on LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while(file){
      Serial.printf(" - File: %s, Size: %u\n", file.name(), file.size());
      file.close();
      file = root.openNextFile();
  }

  // Serial.printf("PSRAM Total heap %d, PSRAM Free Heap %d\n",ESP.getPsramSize(),ESP.getFreePsram());

  //NEED THIS FOR 3C
  // display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  
  display.init(115200,true,50,false);

  Serial.printf("Debug: free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Debug: available PSRAM heap: %d\n", ESP.getFreePsram());
  renderer = new TextRenderer<DISPLAY_TYPE>(display);

  // helloWorld();
  // display.hibernate();

  // drawTextPage(0); // Draw the first page of sample text to test

  // clearWindow();

  // printChapterToSerial("/littlefs/starwars.epub", "META-INF/container.xml");
  // The last one is to use core 1
  // xTaskCreatePinnedToCore(epub_read_test, "EPUB Read Test", 16384, NULL, 1, NULL, 1);
  
}

void epub_read_test(void *parameter) {
  // printChapterToSerial("/littlefs/poop.zip", "poop.txt");
  // printChapterToSerial("/littlefs/starwars.epub", "META-INF/container.xml");

  // printChapterToSerial("/littlefs/starwars.epub", "index_split_000.html");

  // Epub epub("/littlefs/starwars.epub");
  // if (!epub.load()) {
  //   Serial.println("Failed to load EPUB file.");
  //   vTaskDelete(NULL); // Delete the task if loading fails
  //   return;
  // }
  // Serial.printf("EPUB Title: %s\n", epub.get_title().c_str());

  // // prine elements in spine
  // for (int i = 0; i < epub.get_spine_items_count(); i++) {
  //   Serial.printf("Spine item %d: %s\n", i, epub.get_spine_item(i).c_str());
  // }

  // create test Epub List Item from starwars epub
  // use default values for EpubListItem
  // EpubListItem item = {}; // zero initialize fields
  memset(&epub_list_state.epub_list[0], 0, sizeof(EpubListItem)); // zero initialize fields 
  strncpy(epub_list_state.epub_list[0].path, "/littlefs/sherlock.epub", MAX_PATH_SIZE);
  epub_list_state.epub_list[0].current_page = 0;
  epub_list_state.epub_list[0].current_section = 0; // set to 1 to start from the first section (after cover)
  epub_list_state.epub_list[0].pages_in_current_section = 0;

  epub_list_state.num_epubs = 1;
  epub_list_state.selected_item = 0;
  epub_list_state.is_loaded = true;

  Serial.printf("Debug: free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Debug: available PSRAM heap: %d\n", ESP.getFreePsram());
  renderer = new TextRenderer<DISPLAY_TYPE>(display);
  reader = new EpubReader(epub_list_state.epub_list[0], renderer);
  reader->load();

  Serial.printf("Debug: Epub Loaded\n");
  totalPages = reader->get_total_pages();
  Serial.printf("Debug: Total Pages: %d -- Begin rendering\n", totalPages);
  reader->render();



  // ZipFile zip("/littlefs/bold-italics-test.epub");
  
  // if (renderer->loadText(zip, "index.html")) {
    //   Serial.println("Text loaded successfully!");
    //   renderer->drawPage(0);
    // } else {
      //   Serial.println("Failed to load text from EPUB.");
      // }
      
  // ZipFile zip("/littlefs/starwars.epub");
  // HtmlParser parser;
  // // for bold-italics test its just index.html, for starwars it is index_split_000.html
  // if (!parser.parseFromZip(zip, "index_split_000.html")) {
  //   Serial.println("Failed to parse HTML from EPUB.");
  //   vTaskDelete(NULL); // Delete the task if parsing fails
  //   return;
  // }

  // renderer = new TextRenderer<DISPLAY_TYPE>(display);
  // if (renderer->loadFromHtml(parser)) {
  //   Serial.println("Text loaded successfully from HTML parser!");
  //   renderer->drawPage(0);
  //   totalPages = renderer->getTotalPages();
  // } else {
  //   Serial.println("Failed to load text from HTML parser.");
  // }

  vTaskDelete(NULL); // Delete the task when done

}

// void handleEpub(TextRenderer<DISPLAY_TYPE> *renderer) {
//   if (!reader) {
//     reader = new EpubReader(epub_list_state, renderer);
//   }
// }

const char HelloWorld[] = "Hello World (again)!";

void helloWorld()
{
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloWorld);
  }
  while (display.nextPage());
}

int wrap(int start, int end, int wrapNum) {
    if (wrapNum < start) {
        return end;
    } else if (wrapNum > end) {
      return start;
    }
    return wrapNum;
}

// helper function before HandleEpubList
void epub_list_load_task(void *parameter) {
  EpubList *list = (EpubList *)parameter;

  if (web_server) {
    delete web_server;
    web_server = nullptr;
  }

  Serial.println("Starting EPUB list load task");
  if (list->load("/")) {
    Serial.println("Epub files loaded");
    list->set_needs_redraw();
    list->render();
  } else {
    Serial.println("Failed to load Epub files");
  }

  vTaskDelete(NULL); // Delete the task when done
}

void epub_reader_task(void *parameter) {
  EpubReader *reader = (EpubReader *)parameter;
  reader->load();
  reader->retrieve_bookmarks(bookmark_manager);

  if (reader->get_if_want_to_open_last_saved_page()) {
    reader->open_last_saved_page(bookmark_manager);
  }
  if (reader->get_want_to_go_to_page() != -1) {
    reader->go_to_page(reader->get_want_to_go_to_page());
  }

  // check if load requested by web server, unblock it now that its loaded
  if (web_server->hasPendingReader()) {
    Serial.printf("Give Web server Semaphore");
    // web_server->s_epub_reader = reader; // the epub reader set in web server init
    web_server->setEpubReader(reader);
    web_server->s_need_reader = false;
    xSemaphoreGive(web_server->getReaderReadySem());
    vTaskDelete(NULL);
    return;
  }
  
  reader->render();
  vTaskDelete(NULL);
}

void epub_contents_load_task(void *parameter) {
  EpubToc *contents = (EpubToc *)parameter;
  if (contents->load()) {
    Serial.println("Contents loaded successfully");
    contents->set_needs_redraw();
    contents->render();
  } else {
    Serial.println("Failed to load contents");
  }
  vTaskDelete(NULL);
}

void handleEpub(TextRenderer<DISPLAY_TYPE> *renderer, UIAction ui_action);
void handleEpubTableContents(TextRenderer<DISPLAY_TYPE> *renderer, UIAction ui_action, bool needs_redraw=true);

void handleEpubList(TextRenderer<DISPLAY_TYPE> *renderer, UIAction ui_action, bool needs_redraw=true) {
  if (!epub_list) {
    ESP_LOGI("main", "Creating epub list");
    epub_list = new EpubList(renderer, epub_list_state);

    // if (epub_list->load("/")) {
    //   ESP_LOGI("main", "Epub files loaded");
    //   epub_list->render();
    // }

    // Load the EPUB list in a separate task to avoid blocking the main loop
    xTaskCreatePinnedToCore(epub_list_load_task, 
      "Epub List Load Task", 
      16384,              // stack size
      (void *)epub_list,  // pass the epub_list pointer as parameter
      1,                  // priority
      NULL,
      1);                 // core 1
    
    return;
  }

  switch(ui_action) {
    case UP:
      epub_list->prev();
      epub_list->render();
      break;

    case DOWN:
      epub_list->next();
      epub_list->render();
      break;

    case SELECT:
      ui_state = READING_EPUB;
      renderer->clear_screen();

      delete epub_list;
      epub_list = nullptr;

      if (!reader)
      {
        reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);

        // say we want to attempt to open last page
        reader->mark_open_last_saved_page();
        // reader->load();
        xTaskCreatePinnedToCore(epub_reader_task, 
          "Epub Reader Task", 
          16384,              // stack size
          (void *)reader,  // pass the epub_list pointer as parameter
          1,                  // priority
          NULL,
          1);                 // core 1
      }
      handleEpub(renderer, NONE);
      return;

    case BOOKMARK:
      // TODO: implement bookmark webserver export
      if (web_server) {
        web_server->stopWebServer();
        delete web_server;
        web_server = nullptr;
      }
      web_server = new webServer(epub_list_state, bookmark_manager, reader, renderer);
      web_server->startWebServer();
      renderer->show_msg("Bookmark webserver started");
      break;

    case NONE:
    default:  
    if (needs_redraw) {
      epub_list->render();
    }
      break;
  }
}

void handleEpub(TextRenderer<DISPLAY_TYPE> *renderer, UIAction ui_action) {
  if (!reader) {
    reader = new EpubReader(epub_list_state.epub_list[0], renderer);
    xTaskCreatePinnedToCore(epub_reader_task, 
          "Epub Reader Task", 
          16384,              // stack size
          (void *)reader,  // pass the epub_list pointer as parameter
          1,                  // priority
          NULL,
          1);                 // core 1
    // reader->load();
  }

  switch (ui_action)
  {
  case UP:
    reader->prev();
    reader->render();
    break;
  
  case DOWN:
    reader->next();
    reader->render();
    break;

  case REWIND:
    reader->skip_back();
    reader->render();
    break;
  
  case FAST_FORWARD:
    reader->skip_fwd();
    reader->render();
    break;

  case BOOKMARK:
    reader->toggle_bookmark(bookmark_manager);
    break;

  case SAVE:
    // TODO: implement save
    reader->save_progress(bookmark_manager);
    break;

  case MENU:
    ui_state = SELECTING_EPUB;
    renderer->clear_screen();

    delete reader;
    reader = nullptr;
    delete bookmark_manager;
    bookmark_manager = nullptr;
    delete contents;
    contents = nullptr;

    if (!epub_list)
    {
      epub_list = new EpubList(renderer, epub_list_state);
      xTaskCreatePinnedToCore(epub_list_load_task, 
      "Epub List Load Task", 
      16384,              // stack size
      (void *)epub_list,  // pass the epub_list pointer as parameter
      1,                  // priority
      NULL,
      1);                 // core 1
    }
    handleEpubList(renderer, NONE, true);
    return;

  case OPTIONS:
    ui_state = SELECTING_TABLE_CONTENTS;
    renderer->clear_screen();

    delete reader;
    reader = nullptr;

    if (!contents) {
      contents = new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item],epub_index_state, renderer);
      xTaskCreatePinnedToCore(epub_contents_load_task, 
        "Epub Contents Load Task", 
        65536,  // 64KB stack - XML parsing needs space
        (void *)contents,  // pass the contents pointer as parameter
        1,                  // priority
        NULL,
        1);                 // core 1
    }
    handleEpubTableContents(renderer, NONE, true);
    return;

  case NONE:
  default:
    break;
  }
}

void handleEpubTableContents(TextRenderer<DISPLAY_TYPE> *renderer, UIAction ui_action, bool needs_redraw) {
  if (!contents) {
    ESP_LOGI("main", "Creating contents");
    contents = new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item],epub_index_state, renderer);
    // contents->set_needs_redraw();
    // contents->load();
    xTaskCreatePinnedToCore(epub_contents_load_task, 
        "Epub Contents Load Task", 
        65536,  // 64KB stack - XML parsing needs space
        (void *)contents,  // pass the contents pointer as parameter
        1,                  // priority
        NULL,
        1);                 // core 1
  }
  switch (ui_action) {
    case UP:
      contents->prev();
      contents->render();
      break;

    case DOWN:
      contents->next();
      contents->render();
      break;

    case SELECT:
      ESP_LOGI("main", "Selected TOC item");
      ui_state = READING_EPUB;
      renderer->clear_screen();

      // added scope since crossing initialization thingy
      {uint16_t selected_section = contents->get_selected_toc_spine();

      delete contents;
      contents = nullptr;

      if (!reader){
        reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);
        reader->set_state_section(selected_section);
        reader->set_state_page(0); // reset to first page of new section
        
        xTaskCreatePinnedToCore(epub_reader_task, 
            "Epub Reader Task", 
            16384,              // stack size
            (void *)reader,  // pass the epub_list pointer as parameter
            1,                  // priority
            NULL,
            1);                 // core 1
      }}
      handleEpub(renderer, NONE);
      return;
    case NONE:
    default:
    if (needs_redraw) {
      contents->render();
    }
      break;

  }
}

void handleUserInteraction(TextRenderer<DISPLAY_TYPE> *renderer, UIAction ui_action, bool needs_redraw=true) {
  // This function can be called in the main loop to handle user interactions
  // such as button presses for next/previous page, and it can call renderer->drawPage() accordingly.
  if (!renderer) return; // safety check

  //stop web server if its running, restart device
  if (ui_action == MENU && web_server) {
    web_server->stopWebServer();
    if (web_server->ifFilesChanged()) {
      renderer->show_msg("Web server stopped, restarting device...");
      delay(500);
      ESP.restart();
    } else {
      ESP.restart(); // probably faster than setting everything

      // ui_state = SELECTING_EPUB;
      // delete reader;
      // reader = nullptr;
      // delete bookmark_manager;
      // bookmark_manager = nullptr;
      // delete contents;
      // contents = nullptr;

      // if (!epub_list)
      // {
      //   epub_list = new EpubList(renderer, epub_list_state);
      //   xTaskCreatePinnedToCore(epub_list_load_task, 
      //   "Epub List Load Task", 
      //   16384,              // stack size
      //   (void *)epub_list,  // pass the epub_list pointer as parameter
      //   1,                  // priority
      //   NULL,
      //   1);                 // core 1
      // }
      // handleEpubList(renderer, NONE, false);
    }
  }

  //check if action is none
  // if (ui_action == NONE) return; 

  switch (ui_state)
  {
  case READING_EPUB:
    handleEpub(renderer, ui_action);
    break;
  case SELECTING_TABLE_CONTENTS:
    handleEpubTableContents(renderer, ui_action, false);
    break;
  case SELECTING_EPUB:
  default:
    handleEpubList(renderer, ui_action, false);
    break;
  }
}

void checkSerialCmds(){
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    // skip section
    if (command.equalsIgnoreCase("skip")) {
      Serial.println("Serial command: Skip Section");
      if (renderer) {
        // reader->next();
        reader->set_state_section(reader->get_current_section() + 1);
        reader->set_state_page(0); // reset to first page of new section
        Serial.printf("Current page: %d\n", reader->get_current_page_global());
      }
    } else if (command.equalsIgnoreCase("rewind")) {
      Serial.println("Serial command: Previous Section");
      if (renderer) {
        reader->set_state_section(reader->get_current_section() - 1);
        reader->set_state_page(0); // reset to first page of new section
        Serial.printf("Current page: %d\n", reader->get_current_page_global());
      }
    } else if (command.equalsIgnoreCase("render")) {
      Serial.println("Serial command: Render current page");
      display.init(115200, true, 2, false);
      if (renderer) {
        if (ui_state == READING_EPUB) {
          reader->reset_parser();
          reader->render();
        } else if (ui_state == SELECTING_EPUB) {
          epub_list->set_needs_redraw();
          epub_list->render();
        } else if (ui_state == SELECTING_TABLE_CONTENTS) {
          contents->render();
        }
      }
    } else if (command.equalsIgnoreCase("next")) {
      Serial.println("Serial command: Next Page");
      if (renderer) {
        reader->next();
        Serial.printf("Current page: %d\n", reader->get_current_page_global());
      }
    } else if (command.equalsIgnoreCase("prev")) {
      Serial.println("Serial command: Previous Page");
      if (renderer) {
        reader->prev();
        Serial.printf("Current page: %d\n", reader->get_current_page_global());
      }
    } else if (command.equalsIgnoreCase("current")) {
      Serial.println("Serial command: Print current page info");
      if (renderer) {
        Serial.printf("Current section: %d, Current page in section: %d, Current global page: %d\n", reader->get_current_section(), reader->get_current_page(), reader->get_current_page_global());
      }
    } else if (command.equalsIgnoreCase("total")) {
      Serial.println("Serial command: Print total pages");
      if (renderer) {
        Serial.printf("Total pages: %d\n", reader->get_total_pages());
      }
    } else if (command.startsWith("goto ")) {
      int pageNum = command.substring(5).toInt();
      Serial.printf("Serial command: Go to global page %d\n", pageNum);
      if (renderer) {
        reader->go_to_page(pageNum);
        Serial.printf("Current page after goto: %d\n", reader->get_current_page_global());
      }
    } else if (command.equalsIgnoreCase("pageContent")) {
      Serial.println("Serial command: Print current page content");
      // This is just for testing, not an actual feature
      if (renderer) {
        Serial.printf("Page content: %s\n", renderer->getPageContent(reader->get_current_page()).c_str());
      }
    } else if (command.equalsIgnoreCase("pageContentHex")) {
      Serial.println("Serial command: Print current page content in hex");
      // This is just for testing, not an actual feature
      if (renderer) {
        String content = renderer->getPageContent(reader->get_current_page());
        Serial.print("Page content in hex: ");
        for (size_t i = 0; i < content.length(); i++) {
          Serial.printf("%02X ", content[i]);
        }
        Serial.println();
      }
    } else {
      Serial.println("Available commands:");
      Serial.println(" - skip: Skip to the next section");
      Serial.println(" - rewind: Go back to the previous section");
      Serial.println(" - render: Re-render the current page");
      Serial.println(" - next: Go to the next page");
      Serial.println(" - prev: Go to the previous page");
      Serial.println(" - current: Print current section and page info");
      Serial.println(" - total: Print total number of pages in the book");
      Serial.println(" - goto [pageNum]: Go to a specific global page number (e.g. 'goto 42')");
      Serial.println(" - pageContent: Print the content of the current page");
    }
  }
}


void loop() {
  uint32_t Signature = scanSignature();
  // Serial.printf("Scanned remote signal: 0x%08X\n", Signature);
  UIAction action = getActionForSignal(Signature);
  handleUserInteraction(renderer, action);
  checkSerialCmds();
  // debounce delay

  if (web_server) {
    // handleClient no longer neccessary since web server is running on a separate task
    // web_server->handleClient();

    if (web_server->hasPendingGoto()){
      String book_path = "/littlefs/" + web_server->getPendingFile();
      int page_num = web_server->getPendingPage();
      Serial.printf("Web server goto request: book %s, page %d\n", book_path.c_str(), page_num);
      if (renderer) {
        ui_state = READING_EPUB;
        renderer->clear_screen();

        delete epub_list;
        epub_list = nullptr;

        // go through epub_list to find the index of the book that matches the requested path, set selected_item to that index
        for (int i = 0; i < epub_list_state.num_epubs; i++) {
          if (strcmp(epub_list_state.epub_list[i].path, book_path.c_str()) == 0) {
            epub_list_state.selected_item = i;
            break;
          }
        }

        delete reader;
        reader = nullptr;

        if (!reader){
          reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);
          // reader->set_state_section(selected_section);
          // reader->set_state_page(0); // reset to first page of new section
          reader->go_to_page(page_num-1); // people are one-indexed, but we are zero-indexed
          reader->set_headless(false);

          xTaskCreatePinnedToCore(epub_reader_task, 
              "Epub Reader Task", 
              16384,              // stack size
              (void *)reader,  // pass the epub_list pointer as parameter
              1,                  // priority
              NULL,
              1);                 // core 1
        }}
        web_server->clearPendingGoto();
        web_server->stopWebServer();
        ui_state = READING_EPUB;
        handleEpub(renderer, NONE);
        
      }
    }
    if (web_server->hasPendingReader()){
      Serial.println("Web server reader request");
      web_server->setEpubReader(nullptr);
      if (reader){
        delete reader;
        reader = nullptr;
      }
      String book_path = "/littlefs/" + web_server->getPendingFile();
      // go through epub_list to find the index of the book that matches the requested path, set selected_item to that index
        for (int i = 0; i < epub_list_state.num_epubs; i++) {
          if (strcmp(epub_list_state.epub_list[i].path, book_path.c_str()) == 0) {
            epub_list_state.selected_item = i;
            break;
          }
        }
      reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);
      reader->set_headless(true);
      xTaskCreatePinnedToCore(epub_reader_task, 
          "Epub Reader Task", 
          16384,              // stack size
          (void *)reader,  // pass the epub_list pointer as parameter
          1,                  // priority
          NULL,
          1);                 // core 1

      
    }
  

  delay(100);
};

void oldGPIOcmd(){
  if (digitalRead(NEXT_BUTTON_PIN)==LOW){
    if (!btnNextPressed) {
      btnNextPressed = true;
      btnNextPressTime = millis();

      if (renderer) {
        currentPage = wrap(0, totalPages - 1, currentPage + 1);
        reader->next();
        Serial.printf("Next button pressed! Current page (of section): %d\n", reader->get_current_page());
        // Serial.printf("Next button pressed! Current page (global): %d\n", reader->get_current_page_global());
      }
    } else {
      if(millis() - btnNextPressTime >= HOLD_DURATION) {
        Serial.println("Next button held for page turn rendering!");
        btnNextPressed = false; // reset the button state
        // Reinit if hibernated
        display.init(115200, true, 2, false);

        if (renderer) {
          // renderer->drawPage(currentPage);
          reader->render();
        }

        btnNextPressed = false;

        // wait for button release
        while(digitalRead(NEXT_BUTTON_PIN)==LOW) {
          delay(10); // debounce delay
        }
      }
    }
  } else {
    if (btnNextPressed) {
      btnNextPressed = false; // reset the button state on release
    }
  }

  if (digitalRead(PREV_BUTTON_PIN)==LOW){
    
    if (!btnPrevPressed) {
      btnPrevPressed = true;
      btnPrevPressTime = millis();

      if (renderer) {
        // currentPage = max(0, currentPage - 1); // Ensure we don't go below page 0
        currentPage = wrap(0, totalPages - 1, currentPage - 1); // Wrap around using the helper function
        reader->prev();
        Serial.printf("Previous button pressed! Current page (of section): %d\n", reader->get_current_page());
      }
    } else {
      if (millis() - btnPrevPressTime >= HOLD_DURATION) {
        Serial.printf("Rendering previous page!\n");
        
        
        display.init(115200, true, 2, false);

        if (renderer) {
          // renderer->drawPage(currentPage);
          reader->render();
        }

        btnNextPressed = false;

        // wait for button release
        while(digitalRead(PREV_BUTTON_PIN)==LOW) {
          delay(10); // debounce delay
        }
      }
    }
  } else {
    if (btnPrevPressed) {
      btnPrevPressed = false; // reset the button state on release
    }
  }

  

  delay(100); // Small delay to avoid busy looping
}


void clearWindow() { // UNUSED
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());
}


