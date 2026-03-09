#include <vector>
#include "ZipFile.h"
#include <GxEPD2_GFX.h>
#include <GxEPD2_3C.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansOblique9pt7b.h>

#include "Arduino.h"

class TextRenderer
{
    private:
    GxEPD2_3C<GxEPD2_583c_GDEQ0583Z31, 480>& display;
    uint8_t* textData;
    size_t textSize;

    std::vector<size_t> pageStarts; // Stores the starting index of each page in textData
    int currentPage;

    const int MARGIN_LEFT = 10;
    const int MARGIN_TOP = 30;
    const int MARGIN_RIGHT = 10;
    const int MARGIN_BOTTOM = 10;

    public:
    TextRenderer(GxEPD2_3C<GxEPD2_583c_GDEQ0583Z31, 480>& disp) :
        display(disp), textData(nullptr), textSize(0), currentPage(0) {}

    ~TextRenderer() {
        if (textData) {
            ZipFile::free_file_memory(textData);
        }
    }

    // Load text data from a file in the EPUB
    bool loadText(ZipFile& zip, const char* filename) {
        if (textData) {ZipFile::free_file_memory(textData);}

        textData = zip.read_file_to_memory(filename, &textSize);

        if (!textData) {
            return false;
        }

        Serial.printf("Text Loaded! Data from h file: %s\n", textData);


        // Parse text here

        // Init paginations
        pageStarts.clear();
        pageStarts.push_back(0); // First page starts at index 0
        currentPage = 0;

        return true;
    }

    void drawPage(int pageNum) {
        ESP_LOGI("TextRenderer", "page start size: %d, pageNum: %d", pageStarts.size(), pageNum);
        if (pageNum < 0 || !textData) return;



        // Ensure that we have calculated up to this page
        while (pageStarts.size() <= pageNum + 1) {
            if (!calculateNextPage()) {
                ESP_LOGI("TextRenderer", "No more pages to calculate");
                break; // We're on the last page, draw what we have
            }
            ESP_LOGI("TextRenderer", "Calculated page %d, starts at char %d", 
                    pageStarts.size() - 1, pageStarts.back());
        }

        display.setRotation(1);
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setFullWindow();
        display.firstPage();

        size_t startIndex = pageStarts[pageNum];
        size_t endIndex = (pageNum + 1 < pageStarts.size()) 
                            ? pageStarts[pageNum + 1] 
                            : textSize;

        do {
            display.fillScreen(GxEPD_WHITE);
            display.setCursor(MARGIN_LEFT, MARGIN_TOP); // Start with a small margin

            for (size_t i = startIndex; i < endIndex && i < textSize; i++) {
                // I skipped claude's newline and carriage checks since the library should implement it
                char c = (char)textData[i];
                if (c == '\n') {
                    display.setCursor(MARGIN_LEFT, display.getCursorY() + 20);
                    continue;
                } else if (c == '\r') {
                    continue; // ignore carriage returns
                }
                
                //This may be unoptimized since it checks for every character, 
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(&c, 0, 0, &x1, &y1, &w, &h);
                
                // //temp
                // if (c == 'w') {
                //     Serial.printf("char: %c, x1: %d, y1: %d, w: %d, h: %d\n", c, x1, y1, w, h);
                //     Serial.printf("cursorX: %d, cursorY: %d\n", display.getCursorX(), display.getCursorY());
                //     Serial.printf("display width: %d, display height: %d\n", display.width(), display.height());
                // }

                // check if char moves past right margin
                if (display.getCursorX() + w > display.width() - MARGIN_RIGHT) {
                    display.setCursor(MARGIN_LEFT, display.getCursorY() + 20);
                }
                
                display.print(c);


            }

            //draw page num
            char pageInfo[32];
            snprintf(pageInfo, sizeof(pageInfo), "page %d/%d", pageNum + 1, pageStarts.size());
            int16_t x = (display.width() -strlen(pageInfo)*6)/2;
            display.setCursor(x, display.height() - MARGIN_BOTTOM);
            display.print(pageInfo);


        } while (display.nextPage());

        display.hibernate();
        currentPage = pageNum;
    }

    // Calculate where the next page should start
    bool calculateNextPage() {
        if (pageStarts.empty()) return false;

        size_t lastPageStart = pageStarts.back();
        if (lastPageStart >= textSize) return false; // have already reached the end

        // Simulate rendering to find where the next page ends
        int16_t x = MARGIN_LEFT;
        int16_t y = MARGIN_TOP;
        size_t i = lastPageStart;
        size_t lastBreak = lastPageStart; // last possible break (space or newline)

        display.setFont(&FreeSans9pt7b);

        for (; i < textSize; i++) {
            char c = (char)textData[i];

            if (c == '\n') {
                lastBreak = i + 1; // break after newline
                x = MARGIN_LEFT;
                y += 20;
            } else if (c == ' ') {
                lastBreak = i + 1; // break after space
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(&c, x, y, &x1, &y1, &w, &h);
                x += w;
            } else if (c != '\r') {
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(&c, x, y, &x1, &y1, &w, &h);
                x += w;
            }

            // If we exceed the right margin, wrap to next line
            if (x > display.width() - MARGIN_RIGHT) {
                x = MARGIN_LEFT;
                y += 20;
            }

            // If we exceed the bottom margin, start a new page
            if (y > display.height() - MARGIN_BOTTOM - 20) {
                size_t pageBreak = (lastBreak > lastPageStart) ? lastBreak : i;
                pageStarts.push_back(pageBreak);
                return true;
            }
        }

        // If we reach the end of the text, no more pages
        return false;
    }

    void nextPage(){
        if (currentPage < pageStarts.size() - 1) {
            drawPage(currentPage + 1);
        }
    }

    void previousPage(){
        if (currentPage > 0) {
            drawPage(currentPage - 1);
        }
    }

    int getCurrentPage() { return currentPage; }
    int getTotalPages() { return pageStarts.size(); }
};