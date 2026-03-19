#pragma once

#include <GxEPD2_3C.h>

typedef enum
{
    NORMAL = 0,
    BOLD_SPAN = 1,
    ITALIC_SPAN = 2,
} SPAN_STYLE;

typedef enum
{
  JUSTIFIED = 0,
  LEFT_ALIGN = 1,
  CENTER_ALIGN = 2,
  RIGHT_ALIGN = 3,
} BLOCK_STYLE;

class TextBlock { //AI METHODS BELOW, NOT FULLY IMPLEMENTED
private:
    std::string text;
    BLOCK_STYLE blockStyle;
    std::vector<SPAN_STYLE> spans;
    
public:
    TextBlock(const std::string &text, BLOCK_STYLE blockStyle, std::vector<SPAN_STYLE> spans);
    std::string getText();
    BLOCK_STYLE getBlockStyle();
    std::vector<SPAN_STYLE> getSpans();
    void render(GxEPD2_583c_GDEQ0583Z31 *display, int line_break_index, int x_pos, int y_pos); // This will render the text block to the display, applying styles as needed
};

// CLAUDE VERSION
struct TextElement {
    std::string text;
    SPAN_STYLE style;
    bool isBlockEnd;  // true if this ends a paragraph/block
    
    TextElement(const std::string& t, SPAN_STYLE s = NORMAL, bool blockEnd = false)
        : text(t), style(s), isBlockEnd(blockEnd) {}
};