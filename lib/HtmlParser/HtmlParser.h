#ifndef HTMLPARSER_H
#define HTMLPARSER_H

#include "tinyxml2.h"
#include "ZipFile.h"

#ifndef HALLUCINATION
#define HALLUCINATION
#include <esp_heap_caps.h>

void* operator new(size_t size) {
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) ptr = malloc(size);
  return ptr;
}

void operator delete(void* ptr) {
  if (ptr) heap_caps_free(ptr);
}

void* operator new[](size_t size) {
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) ptr = malloc(size);
  return ptr;
}

void operator delete[](void* ptr) {
  if (ptr) heap_caps_free(ptr);
}
#endif

static const char *DEBUGTAG = "HTMLPARSE";

const char *HEADER_TAGS[] = {"h1", "h2", "h3", "h4", "h5", "h6"};
const int NUM_HEADER_TAGS = sizeof(HEADER_TAGS) / sizeof(HEADER_TAGS[0]);

const char *BLOCK_TAGS[] = {"p", "li", "div", "br"};
const int NUM_BLOCK_TAGS = sizeof(BLOCK_TAGS) / sizeof(BLOCK_TAGS[0]);

const char *BOLD_TAGS[] = {"b", "strong"};
const int NUM_BOLD_TAGS = sizeof(BOLD_TAGS) / sizeof(BOLD_TAGS[0]);

const char *ITALIC_TAGS[] = {"i", "em"};
const int NUM_ITALIC_TAGS = sizeof(ITALIC_TAGS) / sizeof(ITALIC_TAGS[0]);

const char *IMAGE_TAGS[] = {"img"};
const int NUM_IMAGE_TAGS = sizeof(IMAGE_TAGS) / sizeof(IMAGE_TAGS[0]);

const char *SKIP_TAGS[] = {"head", "table"};
const int NUM_SKIP_TAGS = sizeof(SKIP_TAGS) / sizeof(SKIP_TAGS[0]);

bool matches(const char *tag_name, const char *possible_tags[], int possible_tag_count)
{
  for (int i = 0; i < possible_tag_count; i++)
  {
    if (strcmp(tag_name, possible_tags[i]) == 0)
    {
      return true;
    }
  }
  return false;
}

class HtmlParser {
private:
std::vector<TextElement> blocks;
std::string convertSmartQuotes(const std::string& text) {
  std::string result;

  for (size_t i = 0; i < text.length(); i++) {
    unsigned char c = text[i];

    // identify that UTF-8 smart quotes are multi-byte characters
    if (c == 0xE2 && i + 2 < text.length()) {
      unsigned char b2 = text[i+1];
      unsigned char b3 = text[i+2];

      // “ (U+201C) and ” (U+201D) -> E2 80 9C and E2 80 9D
      if (b2 == 0x80 && (b3 == 0x9C || b3 == 0x9D)) {
        result += '"';
        i += 2; // skip the next two bytes
        continue;
      }

      // ‘ (U+2018) and ’ (U+2019) -> E2 80 98 and E2 80 99
      if (b2 == 0x80 && (b3 == 0x98 || b3 == 0x99)) {
        result += '\'';
        i += 2; // skip the next two bytes
        continue;
      }

      // — (em dash U+2014) -> E2 80 94
      if (b2 == 0x80 && b3 == 0x94) {
        result += '-';
        i += 2; // skip the next two bytes
        continue;
      }

      // – (en dash U+2013) -> E2 80 93
      if (b2 == 0x80 && b3 == 0x93) {
        result += '-';
        i += 2; // skip the next two bytes
        continue;
      }

      // … (ellipsis U+2026) -> E2 80 A6
      if (b2 == 0x80 && b3 == 0xA6) {
        result += "...";
        i += 2; // skip the next two bytes
        continue;
      }

      // • (bullet U+2022) -> E2 80 A2
      if (b2 == 0x80 && b3 == 0xA2) {
        result += "*";
        i += 2; // skip the next two bytes
        continue;
      }
    }

    result += c;
  }

  return result;
}
bool is_whitespace(char c)
{
  return (c == ' ' || c == '\r' || c == '\n');
}

  SPAN_STYLE getCurrentStyle(bool isBold, bool isItalic) {
    if (isBold && isItalic) {
      // For simplicity, we can prioritize bold if both are true
      return BOLD_SPAN;
    } else if (isBold) {
      return BOLD_SPAN;
    } else if (isItalic) {
      return ITALIC_SPAN;
    } else {
      return NORMAL;
    }
  };

  void parseNode(tinyxml2::XMLNode *node, bool isBold, bool isItalic) {
    if (node == nullptr) return;

    if (node->ToText()) {
      // clean up whitespace
      const char* text = node->Value();
      if (text && strlen(text)>0) {
        std::string cleaned;
        bool lastWasWhitespace = false;

        for (const char* p = text; *p; p++) {
          if (is_whitespace(*p)) {
            if (!lastWasWhitespace && !cleaned.empty()) { // Avoid leading whitespace
              cleaned += ' ';
              lastWasWhitespace = true;
            }
          } else {
            cleaned += *p;
            lastWasWhitespace = false;
          }
        }

        if (!cleaned.empty()) {
          SPAN_STYLE style = getCurrentStyle(isBold, isItalic);
          std::string converted = convertSmartQuotes(cleaned);
          blocks.push_back(TextElement(converted, style));
        }
      }

      return;
    }
    // Other case where it is an element
    tinyxml2::XMLElement* element = node->ToElement();
    if (element) {
      const char *tag_name = element->Name();
      if (matches(tag_name, SKIP_TAGS, NUM_SKIP_TAGS)) {
        return; // Skip this tag and its children entirely 
      }

      bool newIsBold = isBold || matches(tag_name, BOLD_TAGS, NUM_BOLD_TAGS);
      bool newIsItalic = isItalic || matches(tag_name, ITALIC_TAGS, NUM_ITALIC_TAGS);

      if (strcmp(tag_name, "img")==0) {
        blocks.push_back(TextElement("[IMAGE]", ITALIC_SPAN)); // Placeholder for images, could be improved to store src or even render the image
      }

      // Recursively parse child nodes with updated styles
      for (tinyxml2::XMLNode* child = element->FirstChild(); child; child = child->NextSibling()) {
        parseNode(child, newIsBold, newIsItalic);
      }

      // If this is a block-level element, add a block end marker
      if (matches(tag_name, BLOCK_TAGS, NUM_BLOCK_TAGS) || 
      matches(tag_name, HEADER_TAGS, NUM_HEADER_TAGS)) {
        if (!blocks.empty()) {
          blocks.back().isBlockEnd = true; // Mark the last element as the end of a block
        }
      }

      // add extra spacing after headers
      if (matches(tag_name, HEADER_TAGS, NUM_HEADER_TAGS)) {
        blocks.push_back(TextElement("", NORMAL, true)); // Add an empty block to create spacing after headers
      }
    }
  }
public:
  HtmlParser(){}
  HtmlParser(const char *html, int length, const std::string &base_path);

  ~HtmlParser() {
    // Clean up any dynamically allocated memory if needed
  }

  bool parseHtml(const char *html, int length) {
    blocks.clear();
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError result = doc.Parse(html, length);
    if (result != tinyxml2::XML_SUCCESS) {
      ESP_LOGE(DEBUGTAG, "Failed to parse HTML: %s", doc.ErrorStr());
      return false;
    }
    parseNode(doc.RootElement(), false, false);
    ESP_LOGI(DEBUGTAG, "Parsed %d text elements", blocks.size());
    return true;
  }

  bool parseFromZip(ZipFile& zip, const char* filename) {
    size_t size;
    uint8_t* data = zip.read_file_to_memory(filename, &size);

    if (!data) {
      ESP_LOGE(DEBUGTAG, "Failed to read HTML from ZIP: %s", filename);
      return false;
    }

    bool success = parseHtml((const char*)data, size);
    ZipFile::free_file_memory(data);
    return success;
  }

  std::vector<TextElement> getBlocks() const {
    return blocks;
  }
};

HtmlParser::HtmlParser(const char *html, int length, const std::string &base_path)
{
  //actually having a base path would probably be needed for images and other resources, but for now we can ignore it and just assume all resources are in the same directory as the html file
  // m_base_path = base_path;
  parseHtml(html, length);
}

#endif