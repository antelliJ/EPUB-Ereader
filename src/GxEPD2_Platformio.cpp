#include <Arduino.h>
#include "LittleFS.h"
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
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>


#include <Fonts/FreeMonoBold9pt7b.h> // First Font test
#include <Fonts/FreeSans9pt7b.h> // A cleaner font for long text

#include "sampletext.h"


#include <SPI.h>

#include "sampleReads.h"
#include "TextRenderer.h"

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
  GxEPD2_3C<GxEPD2_583c_GDEQ0583Z31, 480>display(GxEPD2_583c_GDEQ0583Z31(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // This ALSO works, maybe a TAD faster ??

SPIClass hspi(HSPI);

// Pagination variables
int pageStarts[10]; // Stores the char index where each page begins
int currentPage = 0;
int totalPages = 0;

TextRenderer* renderer = nullptr;

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

  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  
  // helloWorld();
  // display.hibernate();

  // drawTextPage(0);

  // clearWindow();

  // printChapterToSerial("/littlefs/starwars.epub", "META-INF/container.xml");
  // The last one is to use core 1
  xTaskCreatePinnedToCore(epub_read_test, "EPUB Read Test", 16384, NULL, 1, NULL, 1);
  
}

void epub_read_test(void *parameter) {
  // printChapterToSerial("/littlefs/poop.zip", "poop.txt");
  // printChapterToSerial("/littlefs/starwars.epub", "META-INF/container.xml");

  // printChapterToSerial("/littlefs/starwars.epub", "index_split_000.html");



  renderer = new TextRenderer(display);
  ZipFile zip("/littlefs/bold-italics-test.epub");

  if (renderer->loadText(zip, "index.html")) {
    Serial.println("Text loaded successfully!");
    renderer->drawPage(0);
  } else {
    Serial.println("Failed to load text from EPUB.");
  }

  vTaskDelete(NULL); // Delete the task when done

}

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

void loop() {
  if (digitalRead(NEXT_BUTTON_PIN)==LOW){
    // Reinit if hibernated
    display.init(115200, true, 2, false);
    Serial.println("Next button pressed!");

    delay(50);

    currentPage++;

    // if (currentPage > totalPages) currentPage = 0; // Loop back to start
    // drawTextPage(currentPage);

      if (renderer) {
          renderer->nextPage();
      }

    // wait for button release
    while(digitalRead(NEXT_BUTTON_PIN)==LOW);
  };

  if (digitalRead(PREV_BUTTON_PIN)==LOW){
    // Reinit if hibernated
    display.init(115200, true, 2, false);
    Serial.println("Previous button pressed!");

    delay(50);

    currentPage--;

    if (renderer){
      renderer->previousPage();
    }

    // wait for button release
    while(digitalRead(PREV_BUTTON_PIN)==LOW);
  };
};

void clearWindow() { // UNUSED
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());
}


void drawTextPage(int pageNum) {
  display.setRotation(1);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();

  int charIndex = pageStarts[pageNum];

  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(10, 30); // Start with a small margin

    // Logic to print character by character and check for overflow
    for (int i = charIndex; longStory[i] != '\0'; i++) {
      display.print(longStory[i]);

      // If we hit the bottom of the screen
      if (display.getCursorY() > display.height() - 20) {
        // Record where the NEXT page should start if we haven't yet
        if (pageNum + 1 < 10) {
          pageStarts[pageNum + 1] = i + 1;
          if (pageNum + 1 > totalPages) totalPages = pageNum + 1;
        }
        break; // Stop drawing this page
      }
    }
  } while (display.nextPage());

  display.hibernate();
}