#pragma once

#include <vector>
#include "ZipFile.h"
#include <GxEPD2_GFX.h>
#include <GxEPD2_3C.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansOblique9pt7b.h>
#include <Fonts/TomThumb.h>
#include <Fonts/Font5x7Fixed.h>

#include "mark.h"

#include "Arduino.h"

#include "State.h"
#include "epub.h"

#define MAX_FAST_REFRESHES 4
#define FAST_REFRESH_TIMEOUT 30000  // 30s

#ifndef HALLUCINATION // courtesy of Grok
#define HALLUCINATION
#include <esp_heap_caps.h>

void* operator new(size_t size) {
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) ptr = malloc(size);
  return ptr;
}

// void operator delete(void* ptr) {
//   if (ptr) heap_caps_free(ptr);
// }

void* operator new[](size_t size) {
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) ptr = malloc(size);
  return ptr;
}

// void operator delete[](void* ptr) {
//   if (ptr) heap_caps_free(ptr);
// }

void operator delete(void* ptr) {
    if (ptr) free(ptr);  // free() works for both PSRAM and internal heap
}

void operator delete[](void* ptr) {
    if (ptr) free(ptr);
}
#endif

#include <JPEGDEC.h>
static GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> *s_jpeg_display = nullptr; // maybe use the proper types?
static int s_jpeg_x_offset = 0;
static int s_jpeg_y_offset = 0;


static int jpegDrawCallback(JPEGDRAW *pDraw) {
    // allocate line buffer on heap, not stack
    uint8_t *lineBuf = (uint8_t *)malloc((pDraw->iWidth + 7) / 8);
    if (!lineBuf) return 0;

    for (int y = 0; y < pDraw->iHeight; y++) {
        memset(lineBuf, 0xFF, (pDraw->iWidth + 7) / 8);
        for (int x = 0; x < pDraw->iWidth; x++) {
            uint16_t px = pDraw->pPixels[y * pDraw->iWidth + x];
            uint8_t r = ((px >> 11) & 0x1F) << 3;
            uint8_t g = ((px >> 5)  & 0x3F) << 2;
            uint8_t b = (px & 0x1F) << 3;
            uint8_t gray = (r * 299 + g * 587 + b * 114) / 1000;
            if (gray < 128) {
                lineBuf[x / 8] &= ~(0x80 >> (x % 8)); // set bit black
            }
        }
        s_jpeg_display->drawBitmap(
            s_jpeg_x_offset + pDraw->x,
            s_jpeg_y_offset + pDraw->y + y,
            lineBuf, pDraw->iWidth, 1,
            GxEPD_WHITE,
            GxEPD_BLACK
        );
    }
    free(lineBuf);
    return 1;
}

// for dithered images
static int jpegDrawCallback2(JPEGDRAW *pDraw) {
    if (pDraw->iBpp == 1) {
        // ONE_BIT_DITHERED: pixels are already packed 1bpp, copy row by row
        int pitch = (pDraw->iWidth + 7) / 8;
        for (int y = 0; y < pDraw->iHeight; y++) {
            uint8_t *src = (uint8_t *)pDraw->pPixels + y * pitch;
            s_jpeg_display->drawBitmap(
                s_jpeg_x_offset + pDraw->x,
                s_jpeg_y_offset + pDraw->y + y,
                src, pDraw->iWidth, 1,
                GxEPD_WHITE,
                GxEPD_BLACK
            );
        }
    }
    return 1;
}


#include "TextBlock.h"
#include "HtmlParser.h"

#include "display_config.h"

template<typename DisplayType>
class TextRenderer
{
private:
    // GxEPD2_3C<GxEPD2_583c_GDEQ0583Z31, 480>& display;
    DisplayType& display;

    // uint8_t* textData;
    // size_t textSize;
    std::vector<size_t> pageStarts; // Stores the starting index of each page in textData

    std::vector<TextElement> textElements;

    FastRefreshState *fastRefreshState;

    Epub *curEpub = nullptr;

    int currentPage;

    const int MARGIN_LEFT = 10;
    const int MARGIN_TOP = 30;
    const int MARGIN_RIGHT = 10;
    const int MARGIN_BOTTOM = 10;

    const int LINESPACE = 22;

    int* global_total_pages = 0;
    int* global_current_page = 0;

    bool do_dither = true;


    const GFXfont* getFontForStyle(SPAN_STYLE style) {
        #ifdef USE_3C_DISPLAY
            switch (style) {
                case BOLD_SPAN: return &FreeSansBold9pt7b;
                case ITALIC_SPAN: return &FreeSansOblique9pt7b;
                default: return &FreeSans9pt7b;
            }
        #else
            switch(style) {
                case BOLD_SPAN: return &FreeSansBold9pt7b;
                case ITALIC_SPAN: return &FreeSansOblique9pt7b;
                default: return &Font5x7Fixed; // FreeSans9pt7b
            }
        #endif
    }

    bool is_whitespace(char c)
    {
    return (c == ' ' || c == '\n');
    }

public:
    TextRenderer(DisplayType& disp) :
        display(disp), currentPage(0) {}

    ~TextRenderer() {}

    void set_fast_refresh_state(FastRefreshState *state) {
        fastRefreshState = state;
    }

    void set_current_epub(Epub *epub) {
        curEpub = epub;    
    }
    

    void set_global_pages(int *total_pages_ptr, int *current_page_ptr) {
    global_total_pages = total_pages_ptr;
    global_current_page = current_page_ptr;
  }

    void prepareRefresh() {
        uint32_t now = millis();
        bool timedOut = (now - fastRefreshState->last_fast_refresh_time) > FAST_REFRESH_TIMEOUT;
        bool tooManyFast = fastRefreshState->amt_fast_refreshes >= MAX_FAST_REFRESHES;
        bool doFull = tooManyFast || (timedOut && fastRefreshState->was_last_fast_refresh);

        // comment this out when everything works
        // doFull = true;

        if (doFull) {
            display.setFullWindow();
            fastRefreshState->amt_fast_refreshes = 0;
            fastRefreshState->was_last_fast_refresh = false;
            Serial.printf("[Renderer] Full refresh (timedOut=%d tooMany=%d)\n", timedOut, tooManyFast);
        } else {
            display.setPartialWindow(0, 0, display.width(), display.height());
            fastRefreshState->amt_fast_refreshes++;
            fastRefreshState->was_last_fast_refresh = true;
            fastRefreshState->last_fast_refresh_time = now;
            Serial.printf("[Renderer] Fast refresh %d/%d\n", fastRefreshState->amt_fast_refreshes, MAX_FAST_REFRESHES);
        }
    }

    void forceFullRefresh(){
        fastRefreshState->amt_fast_refreshes = MAX_FAST_REFRESHES;
        fastRefreshState->was_last_fast_refresh = true;
        fastRefreshState->last_fast_refresh_time = 0;
    }

  

    // Load text data from a file in the EPUB
    // bool loadText(ZipFile& zip, const char* filename) {
    //     if (textData) {ZipFile::free_file_memory(textData);}

    //     textData = zip.read_file_to_memory(filename, &textSize);

    //     if (!textData) {
    //         return false;
    //     }

    //     Serial.printf("Text Loaded! Data from h file: %s\n", textData);


    //     // Parse text here

    //     // Init paginations
    //     pageStarts.clear();
    //     pageStarts.push_back(0); // First page starts at index 0
    //     currentPage = 0;

    //     return true;
    // }

    bool loadFromHtml(HtmlParser& parser) {
        textElements = parser.getBlocks();

        Serial.printf("Loaded %d text elements from HTML parser\n", textElements.size());
        
        pageStarts.clear();
        pageStarts.push_back(0); // First page starts at index 0
        currentPage = 0;

        if (textElements.empty()) {
            return false;
        }

        return true;
    }

    // returns height consumed so text layout advances accordingly
    int renderImage(Epub *epub, const std::string &imageSrc, int x, int y, int maxWidth, int maxHeight, bool dryRun = false) 
    {
        if (dryRun) {
            return maxHeight;
        }
        if (!epub) {
            ESP_LOGE(TAG, "Epub is null");
            return 0;
        }
        // std::string fullPath = normalise_path(basePath + imageSrc);
        std::string fullPath = epub->get_base_path() + imageSrc;

        Serial.printf("Loading image data: %s\n", fullPath.c_str());
        size_t imageSize = 0;
        uint8_t *imageData = epub->get_item_contents(fullPath, &imageSize);

        if (!imageData) {
            ESP_LOGE(TAG, "Failed to read image %s", fullPath.c_str());
            return 0;
        }

        Serial.printf("opening jpeg ram %s\n", fullPath.c_str());

        
        JPEGDEC jpeg;
        if (getDither()){
            if (!jpeg.openRAM(imageData, imageSize, jpegDrawCallback2)) {
                ESP_LOGE(TAG, "JPEGDEC failed to open image");
                // heap_caps_free(imageData);
                free(imageData);
                return 0;
            }
        } else {
            if (!jpeg.openRAM(imageData, imageSize, jpegDrawCallback)) {
                ESP_LOGE(TAG, "JPEGDEC failed to open image");
                // heap_caps_free(imageData);
                free(imageData);
                return 0;
            }
        }

        int imgW = jpeg.getWidth();
        int imgH = jpeg.getHeight();

        float scale = 1.0f;
        if (imgW > maxWidth) scale = (float)maxWidth / imgW;
        if (imgH * scale > maxHeight) scale = (float)maxHeight / imgH;

        int scaledH = (int) (imgH * scale);

        // JPEGDEC scale factors of 2
        int jpegScale = 1;
        if (scale <= 0.125f) jpegScale = JPEG_SCALE_EIGHTH;
        else if (scale <= 0.25f) jpegScale = JPEG_SCALE_QUARTER;
        else if (scale <= 0.5f) jpegScale = JPEG_SCALE_HALF;

        s_jpeg_display = &getDisplay();
        s_jpeg_x_offset = x + (maxWidth - (int)(imgW * scale)) / 2; // center horizontally
        s_jpeg_y_offset = y;

        if (getDither()){
            // uint8_t *pDither = (uint8_t *)malloc((imgW+16)*16);
            int mcuWidth = (imgW + 15) & ~15;  // round up to nearest 16
            uint8_t *pDither = (uint8_t *)malloc(mcuWidth * 16);
            if (!pDither) {
                ESP_LOGE(TAG, "Failed to allocate dither buffer");
                jpeg.close();
                free(imageData);
                return 0;
            }
    
            // jpeg.decode(0, 0, jpegScale);
            jpeg.setPixelType(ONE_BIT_DITHERED);
            jpeg.decodeDither(0, 0, pDither, jpegScale);
    
            free(pDither);
        } else {
            jpeg.decode(0, 0, jpegScale);
        }

        jpeg.close();
        // heap_caps_free(imageData);
        free(imageData);

        return maxHeight;
        // return scaledH + 4; // height used + margin
                        
    }

    void drawPage(int pageNum, bool isBookmarked=false) {
        Serial.printf("TextRenderer: page start size: %d, pageNum: %d, textElements size: %d\n", pageStarts.size(), pageNum, textElements.size());
        if (pageNum < 0) return;



        // Ensure that we have calculated up to this page
        // while (pageStarts.size() <= pageNum + 1) {
        //     if (!calculateNextPage()) {
        //         ESP_LOGI("TextRenderer", "No more pages to calculate");
        //         break; // We're on the last page, draw what we have
        //     }
        //     ESP_LOGI("TextRenderer", "Calculated page %d, starts at char %d", 
        //             pageStarts.size() - 1, pageStarts.back());
        // }
        calculateAllPages();

        display.setRotation(1);
        // display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        prepareRefresh();
        display.firstPage();

        

        if (textElements.empty()) {
            display.fillScreen(GxEPD_WHITE);
            display.setCursor(MARGIN_LEFT, MARGIN_TOP);
            display.print("No text to display");

            char pageInfo[32];
            // snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d", pageNum + 1, pageStarts.size());
            snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d", *global_current_page + 1, *global_total_pages);
            int16_t px = (display.width() -strlen(pageInfo)*6)/2;
            display.setCursor(px, display.height() - MARGIN_BOTTOM);
            display.print(pageInfo);

            display.nextPage();
            display.hibernate();
            return;
        }

        size_t startIndex = pageStarts[pageNum];
        size_t endIndex = (pageNum + 1 < pageStarts.size()) 
                            ? pageStarts[pageNum + 1] 
                            : textElements.size();

        do {
            display.fillScreen(GxEPD_WHITE);
            // display.setCursor(MARGIN_LEFT, MARGIN_TOP); // Start with a small margin
            int16_t x = MARGIN_LEFT;
            int16_t y = MARGIN_TOP;

            for (size_t i = startIndex; i < endIndex; i++) {
                // I skipped claude's newline and carriage checks since the library should implement it
                // char c = (char)textData[i];
                const TextElement& element = textElements[i];

                display.setFont(getFontForStyle(element.style));

                

                std::string word;
                for (char c : element.text) {
                    if (is_whitespace(c)) {
                        // Print the current word
                        if (!word.empty()) {
                            if (!renderWord(word.c_str(), x, y, LINESPACE)) {
                                Serial.printf("Word '%s' does not fit on page %d, moving to next page\n", word.c_str(), pageNum);
                                goto page_full;
                            }
                            word.clear();
                        }
                        x += 5;
                        // Print the space or handle newline
                        // if (c == ' ') {
                        //     display.print(' ');
                        // } else if (c == '\n') {
                        //     display.println();
                        // }
                    } else {
                        word += c;
                    }
                }

                // Print the last word in the element
                if (!word.empty()) {
                    if (!renderWord(word.c_str(), x, y, LINESPACE)) {
                        Serial.printf("Word '%s' does not fit on page %d, moving to next page\n", word.c_str(), pageNum);
                        goto page_full;
                    }
                }

                if (element.isBlockEnd) {
                    x = MARGIN_LEFT;
                    y += LINESPACE; // Move to next line after a block
                }

                // magic number for padding
                if (element.type == ElementType::IMAGE) {
                    if (curEpub == nullptr) {
                        Serial.println("curEpub is null");
                        continue;
                    }
                    int imageHeight = get_page_height()/2;
                    // if image won't fit on remaining page, start a new page
                    if (y + imageHeight > get_page_height()) {
                        break;
                    }
                    int used = renderImage(curEpub, element.imageSrc, 0, y, 
                    get_page_width(), imageHeight);

                    y += used;
                    continue;
                }
                
                //This may be unoptimized since it checks for every character, 
                // int16_t x1, y1;
                // uint16_t w, h;
                // display.getTextBounds(&c, 0, 0, &x1, &y1, &w, &h);
                
                // //temp
                // if (c == 'w') {
                //     Serial.printf("char: %c, x1: %d, y1: %d, w: %d, h: %d\n", c, x1, y1, w, h);
                //     Serial.printf("cursorX: %d, cursorY: %d\n", display.getCursorX(), display.getCursorY());
                //     Serial.printf("display width: %d, display height: %d\n", display.width(), display.height());
                // }

                // check if char moves past right margin
                // if (display.getCursorX() + w > display.width() - MARGIN_RIGHT) {
                //     display.setCursor(MARGIN_LEFT, display.getCursorY() + 20);
                // }
                
                // display.print(c);


            }

            page_full:

            //draw page num
            // display.setFont(&FreeSans9pt7b);
            // technically pageNum should be calculated, and the total pages is calculated by EpubReader
            char pageInfo[32];
            // snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d", pageNum + 1, pageStarts.size());
            snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d", *global_current_page + 1, *global_total_pages);
            int16_t px = (display.width() -strlen(pageInfo)*6)/2;
            
            display.setFont(getFontForStyle(NORMAL));
            // check if page is bookmarked and draw bookmark icon if it is
            if (isBookmarked) {
                drawBookmarkIcon();
            }
            display.setCursor(px, display.height() - MARGIN_BOTTOM);
            display.print(pageInfo);


        } while (display.nextPage());

        display.hibernate();
        currentPage = pageNum;
    }

    String getPageContent(int pageNum) {
        if (pageNum < 0 || pageNum >= pageStarts.size()) return "";

        size_t startIndex = pageStarts[pageNum];
        size_t endIndex = (pageNum + 1 < pageStarts.size()) 
                            ? pageStarts[pageNum + 1] 
                            : textElements.size();

        String content = "";
        for (size_t i = startIndex; i < endIndex; i++) {
            content += textElements[i].text.c_str();
            if (textElements[i].isBlockEnd) {
                content += "\n";
            }
        }
        return content;
    }
private:
    bool renderWord(const char* word, int16_t& x, int16_t& y, int lineSpace) {
        //technically there is a bug where if a single word is longer than the entire line it will go offscreen (after drawing a newline), but I think this is an edge case that can be ignored for now
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(word, x, y, &x1, &y1, &w, &h);

        // word wrap
        if ((x+w) > (display.width() - MARGIN_RIGHT)) {
            x = MARGIN_LEFT;
            y += lineSpace;

            // recalculate bounds - I have no clue why this fixes the double wrapping bug but it does
            display.getTextBounds(word, x, y, &x1, &y1, &w, &h);

            Serial.printf("Word '%s' wrapped to next line on page %d\n", word, currentPage);

            // check if stil on page
            if (y> display.height() - MARGIN_BOTTOM - lineSpace) {
                return false; // word doesn't fit on this page
            }
        }

        display.setCursor(x, y);
        display.print(word);
        x += w;
        return true;
    }
    // Calculate where the next page should start
    bool calculateNextPage() {
        if (pageStarts.empty()) return false;

        size_t lastStart  = pageStarts.back();
        if (lastStart  >= textElements.size()) return false; // have already reached the end

        // Simulate rendering to find where the next page ends
        int16_t x = MARGIN_LEFT;
        int16_t y = MARGIN_TOP;
        
        for (size_t i = lastStart; i < textElements.size(); i++) {
            const TextElement& element = textElements[i];

            const GFXfont* font = getFontForStyle(element.style);
            display.setFont(font);

            // bool wentNewline = false;

            std::string word;
            for (char c : element.text) {
                if (is_whitespace(c)) {
                    if (!word.empty()) {
                        int16_t x1, y1;
                        uint16_t w, h;
                        display.getTextBounds(word.c_str(), x, y, &x1, &y1, &w, &h);

                        if (x + w > display.width() - MARGIN_RIGHT) {
                            x = MARGIN_LEFT;
                            y += LINESPACE;
                            // wentNewline = true;

                            // recalculate bounds after newline
                            display.getTextBounds(word.c_str(), x, y, &x1, &y1, &w, &h);
                        }

                        // check if word fits on page
                        if (y > display.height() - MARGIN_BOTTOM - LINESPACE) {
                            // check if we're stuck on the same element
                            // without checking large textElements that exceed one page would cause an infinite loop since it would keep trying to start the next page at the same element
                            // alternative: pageStarts.size() >= 2 && pageStarts[pageStarts.size() - 2] == i
                            //      This check uses 2 since the first element is always 0, and must have the second
                            //      I do get different results from the two checks, but I'm not sure which one is correct, I'm pretty sure its the one below tho 
                            if (!pageStarts.empty() && pageStarts.back() == i) {
                                // already tried this element, skip to next
                                ESP_LOGW("TextRenderer", "Element %d is too large for a single page, skipping to %d", i, i+1);
                                pageStarts.pop_back(); // remove the last page start since it doesn't work
                                pageStarts.push_back(i+1);
                            } else {
                                pageStarts.push_back(i);
                            }
                            return true; // next page starts at this element
                        }
                        
                        x += w; // add space width of the word
                        word.clear();
                    } 
                    x += 5; // space between each word - this is a simplification and could be improved by measuring actual space width
                } else {
                    word += c;
                }
            }

            // handle last word
            if (!word.empty()) {
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(word.c_str(), x, y, &x1, &y1, &w, &h);

                if ((x + w) > (display.width() - MARGIN_RIGHT)) {
                    x = MARGIN_LEFT;
                    y += LINESPACE;
                    
                    // recalculate bounds after newline
                    display.getTextBounds(word.c_str(), x, y, &x1, &y1, &w, &h);
                }

                if (y > display.height() - MARGIN_BOTTOM - LINESPACE) {
                    // check if we're stuck on the same element
                    if (!pageStarts.empty() && pageStarts.back() == i) {
                        ESP_LOGW("TextRenderer", "Element %d is too large for a single page, skipping to %d", i, i+1);
                        pageStarts.pop_back(); // remove the last page start since it doesn't work
                        pageStarts.push_back(i+1);
                    } else {
                        pageStarts.push_back(i); // next page starts at end of text
                    }
                    return true;
                }

                x += w;
            }

            if (element.isBlockEnd) {
                x = MARGIN_LEFT;
                y += LINESPACE; // Move to next line after a block

                // check if still on page
                if (y > display.height() - MARGIN_BOTTOM - LINESPACE) {
                    pageStarts.push_back(i+1); // next page starts at end of text
                    return true;
                }
            }

            if (element.type == ElementType::IMAGE) {
                if (curEpub == nullptr) {
                    Serial.println("curEpub is null");
                    continue;
                }
                int imageHeight = get_page_height() / 2;

                if (y + imageHeight > get_page_height()) {
                    pageStarts.push_back(i);
                    // y = 0;
                    return true;
                }
                Serial.printf("Rendering image %s\n", element.imageSrc.c_str());
                int used = renderImage(curEpub, element.imageSrc, 0, y, 
                    get_page_width(), get_page_height(), true);

                y += used;
                continue;
            }
        }
        return false;
    }

public:
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

    // Remember this only calculates pages of the html given, not of the entire epub, so it should be called after loading each section
    // Also it may be a bit slow since it simulates rendering each page, but it works
    // Loop max to add safety to prevent infinite loops in case of bugs
    bool calculateAllPages(int loop_max = 1000) {
        while (calculateNextPage() && loop_max-- > 0){
            // feed the watchdog to prevent reset during long calculations
            vTaskDelay(1);
        };
        return pageStarts.size() > 1; // returns true if there is more than one page
    }

    //get pagestarts (temp)
    std::vector<size_t> getPageStarts() { return pageStarts; }


    DisplayType& getDisplay() { return display; }

    
    void clear_screen() {
        // display.setFullWindow(); // full refresh when clearing (thats up to théuser)
        prepareRefresh();
        display.firstPage();
        display.fillScreen(GxEPD_WHITE);
        display.nextPage();
    }
    
    int get_page_height() { return display.height(); }
    int get_page_width() { return display.width(); }

    void drawBookmarkIcon() {
        // Draw a simple bookmark icon in the top right corner to indicate a bookmark
        // int iconSize = 8;
        int x = get_page_width() - MARGIN_RIGHT - mark::xres;
        int y = MARGIN_TOP/3; // slightly below the top margin

        // Simple bookmark shape using mark.h
        // display.fillRect(x, y, iconSize, iconSize, GxEPD_BLACK); // bottom square
        // display.fillTriangle(x, y+iconSize, x, y+(iconSize*2), x + iconSize/2, y + iconSize, GxEPD_BLACK); // left triangle
        // display.fillTriangle(x+iconSize, y + iconSize, x + iconSize, y + (iconSize*2), x + iconSize/2, y + iconSize, GxEPD_BLACK); // right triangle
        display.drawBitmap(x, y, mark::pixels, mark::xres, mark::yres, GxEPD_BLACK);
    }
    
    // AI basically made this
    void show_msg(const char* msg) {
        display.setRotation(1);
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        int16_t tbx, tby; uint16_t tbw, tbh;
        display.getTextBounds(msg, 0, 0, &tbx, &tby, &tbw, &tbh);
        int16_t x = (display.width() - tbw) / 2 - tbx;
        int16_t y = (display.height() - tbh) / 2 - tby;
        // display.setPartialWindow(0, 0, display.width(), display.height()); // this should make it do a fast draw
        prepareRefresh();
        display.firstPage();
        // use fast method to draw a filled rectangle as background for the message
        display.fillScreen(GxEPD_WHITE);
        // display.fillRect(x, y, tbw, tbh, GxEPD_BLACK);
        display.setCursor(x, y);
        display.print(msg);
        display.nextPage();
    }

    void show_busy(){
        // write "Loading..." in a box in the middle of the screen
        show_msg("Loading...");
    }

    void setDither(bool dither){
        do_dither = dither;
    }

    bool getDither(){
        return do_dither;
    }
};