#include <Arduino.h>
#include "LittleFS.h"
// TODO: 
// fix discrepencies in TextRenderer of calculateNextPage and regular rendering, 
// since a little off on where starting and ending -- something with cutting off newline to put on next page?
//
// fix get_total_pages
// fix page numbering in text render and its pointer
// section_page_to_global_page and the others should use a hashmap somewhere
// add a "go to page" feature that uses the global page number to jump to the correct section and page within that section (for toc)
// make menu stuff
// add table of contents support in epub reader

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

#include "State.h"

#include "SD.h"
#define SD_CS 5

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

DISPLAY_TYPE display(DISPLAY_DRIVER(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

RTC_DATA_ATTR EpubListState epub_list_state;

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
static EpubReader *reader = nullptr;

void drawTextPage(int pageNum);
void epub_read_test(void *parameter);
void clearWindow();

void setup()
{
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PREV_BUTTON_PIN, INPUT_PULLUP);
  // Initialize SPI with your custom MOSI/SCK
  // hspi.begin(CLK, MISO, MOSI, SS)
  hspi.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  Serial.begin(115200);
  esp_log_level_set("TextRenderer", ESP_LOG_INFO); // Set log level to INFO for all tags
  esp_log_level_set("EPUB", ESP_LOG_INFO);

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
      file = root.openNextFile();
  }

  // Serial.printf("PSRAM Total heap %d, PSRAM Free Heap %d\n",ESP.getPsramSize(),ESP.getFreePsram());

  //TODO
  //NEED THIS FOR 3C
  // display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  
  display.init(115200,true,50,false);

  // helloWorld();
  // display.hibernate();

  // drawTextPage(0); // Draw the first page of sample text to test

  // clearWindow();

  // printChapterToSerial("/littlefs/starwars.epub", "META-INF/container.xml");
  // The last one is to use core 1
  xTaskCreatePinnedToCore(epub_read_test, "EPUB Read Test", 16384, NULL, 1, NULL, 1);
  
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


void loop() {
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
        reader->reset_parser();
        reader->render();
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
    } else {
      Serial.println("Unknown command. Use 'skip', 'rewind', or 'render'.");
    }
  }

  delay(100); // Small delay to avoid busy looping
};




void clearWindow() { // UNUSED
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());
}


