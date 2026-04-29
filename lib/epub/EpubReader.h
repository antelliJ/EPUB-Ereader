#pragma once

#include <string.h>
#include <memory>
#include "epub.h"
#include "TextRenderer.h"
#include "HtmlParser.h"

#include "State.h"

#include "Arduino.h"

// static const char *TAG = "EpubReader";

class EpubReader
{
private:
  EpubListItem &state;
  std::unique_ptr<Epub> epub;
  std::unique_ptr<HtmlParser> parser;
  
  TextRenderer<DISPLAY_TYPE> *renderer = nullptr;

  void parse_and_layout_current_section();

  int total_pages = 0;
  int global_current_page = 0; // this is the current page in the whole book, not just the current section

  // number of pages in each section, used to calculate global page number and for "go to page" feature
  std::map<int, int> section_page_lengths; // key = section index, value = page count

public:
  EpubReader(EpubListItem &state, TextRenderer<DISPLAY_TYPE> *renderer) : state(state), renderer(renderer){};
  ~EpubReader() = default;
  bool load();
  void next();
  void prev();
  void render();
  void set_state_section(uint16_t current_section);
  int get_total_pages();
  int current_page_to_section_page(int current_page);
  int section_page_to_global_page(int section, int page);
  int get_current_page();
  int get_current_page_global();
  void go_to_page(int global_page); // not used or tested yet
  int get_current_section();
  int set_state_page(uint16_t current_page) {
    state.current_page = current_page;
    return state.current_page;
  }
  void reset_parser() { parser.reset(); }
};

bool EpubReader::load()
{
  ESP_LOGI(TAG, "Before epub load: %d", esp_get_free_heap_size());
  // do we need to load the epub?
  if (!epub || epub->get_path() != state.path)
  {
    // renderer->show_busy();
    parser.reset();
    epub = std::unique_ptr<Epub>(new Epub(state.path));
    if (epub->load())
    {
      ESP_LOGI(TAG, "After epub load: %d", esp_get_free_heap_size());
      return false;
    }
  }
  renderer->show_busy();
  total_pages = 0; // reset total pages so it will be recalculated when needed
  return true;
}


void EpubReader::parse_and_layout_current_section()
{
  if (!parser)
  {
    Serial.printf("Currently Parsing section %d\n", state.current_section);
    // renderer->show_busy();
    ESP_LOGI(TAG, "Parse and render section %d", state.current_section);
    ESP_LOGD(TAG, "Before read html: %d", ESP.getFreePsram());
    // Serial.printf("Debug: Free heap size before read html: %d\n", esp_get_free_heap_size());

    
    // if spine item is not found here then it will return get_spine_item(0)
    // so it does not crashes when you want to go after last page (out of vector range)
    std::string item = epub->get_spine_item(state.current_section);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    ESP_LOGD(TAG, "After read html: %d -- current_section: %d, current_page: %d", ESP.getFreePsram(), state.current_section, state.current_page);
    
    if (!html)
    {
      Serial.printf("Failed to get html for section %d\n", state.current_section);
      return;
    }

    /* SOMETHING FROM THIS POINT ON IS NOT WORKING BECAUSE state.current_section changes to something weird */
    
    Serial.printf("Debug: loaded html of section, now parsing:\n");
    parser = std::unique_ptr<HtmlParser>(new HtmlParser(html, strlen(html), base_path));
    ESP_LOGD(TAG, "After parse: %d -- current_section: %d, current_page: %d", esp_get_free_heap_size(), state.current_section, state.current_page);
    free(html);
    // parser->layout(renderer, epub);
    /* LOAD FROM HTML IS ALSO CAUSING ISSUES WITH CURRENT SECTION AND CURRENT PAGE*/
    renderer->loadFromHtml(*parser);
    Serial.printf("Debug: loaded html into renderer, now calculating pages: -- current_section: %d, current_page: %d\n", state.current_section, state.current_page);
    
    
    renderer->calculateAllPages();
    ESP_LOGD(TAG, "After layout: %d", esp_get_free_heap_size());
    ESP_LOGD(TAG, "Section %d has %d pages, current page: %d", state.current_section, renderer->getTotalPages(), state.current_page);
    state.pages_in_current_section = renderer->getTotalPages();
  }
}

void EpubReader::next()
{
  state.current_page++;
  
  // if (get_current_page_global() >= get_total_pages() - 1) {
  //   // debug message
  //   ESP_LOGD(TAG, "Reached end of book, wrapping to beginning.");
  //   // wrap to the beginning of the first section
  //   set_state_section(0);
  //   state.current_page = 0;
  //   parser.reset();
  //   parse_and_layout_current_section();
  //   return;
  // }

  // set_state_section automatically wraps to the first
  
  if (state.current_page >= state.pages_in_current_section)
  {
    set_state_section(state.current_section + 1);
    state.current_page = 0;
    parser.reset();
    parse_and_layout_current_section();
  }
}

void EpubReader::prev()
{
  if (get_current_page_global() == 0) {
    // wrap to the end of the last section
    set_state_section(epub->get_spine_items_count() - 1);
    parser.reset();
    parse_and_layout_current_section();
    state.current_page = state.pages_in_current_section - 1;
    return;
  }

  if (state.current_page == 0)
  {
    if (state.current_section > 0)
    {
      parser.reset();
      set_state_section(state.current_section - 1);
      ESP_LOGD(TAG, "Going to previous section %d", state.current_section);
      parse_and_layout_current_section();
      state.current_page = state.pages_in_current_section - 1;
      return;
    }
  }
  state.current_page--;
}

void EpubReader::render()
{
  if (!parser)
  {
    Serial.printf("No parser found for current section %d, parsing now...\n", state.current_section);
    parse_and_layout_current_section();
  }
  ESP_LOGD(TAG, "rendering page %d of %d", state.current_page, get_total_pages());
  // parser->render_page(state.current_page, renderer, epub);
  // renderer->drawPage(state.current_page); 
  // Serial.printf("Rendering page %d of section, current page: %d\n", state.current_page, current_page_to_section_page(state.current_page));
  get_current_page_global(); // properly set global_current_page before rendering so that it can be displayed in page info
  renderer->set_global_pages(&total_pages, &global_current_page);
  renderer-> drawPage(state.current_page);

  ESP_LOGD(TAG, "rendered page %d of %d", get_current_page_global(), get_total_pages());
  ESP_LOGD(TAG, "after render psram: %d", ESP.getFreePsram());
}

// This function is basically unused right now, but it can be useful if we want to implement a "go to page" feature in the future, where we need to convert a global page number to a page number within the current section
int EpubReader::current_page_to_section_page(int current_page) {
  int accumulated_page = 0;
  std::unique_ptr<HtmlParser> temp_parser(new HtmlParser());
  std::unique_ptr<TextRenderer<DISPLAY_TYPE>> temp_renderer(new TextRenderer<DISPLAY_TYPE>(renderer->getDisplay()));
  for (int i = 0; i < min((int)state.current_section, epub->get_spine_items_count()); i++) {
    // check if this section is already calculated and stored in section_page_lengths vector
    if (i < section_page_lengths.size()) {
      accumulated_page += section_page_lengths[i];
      if (accumulated_page > current_page) {
        return current_page - (accumulated_page - section_page_lengths[i]);
      }
      continue;
    }

    // page += state.epub_list[i].pages_in_current_section;
    std::string item = epub->get_spine_item(i);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    if (!html) {
      return 0;
    }
    temp_parser->parseHtml(html, strlen(html));
    temp_renderer->loadFromHtml(*temp_parser);
    temp_renderer->calculateAllPages();
    if ((accumulated_page + temp_renderer->getTotalPages()) >= current_page) {
      free(html);
      return current_page - accumulated_page;
    }
    accumulated_page += temp_renderer->getTotalPages();
    // section_page_lengths.push_back(temp_renderer->getTotalPages());
    section_page_lengths[i] = temp_renderer->getTotalPages();
    free(html);
  }
  return 0; // default to first page if something goes wrong
}

// I SHOULD PROBABLY UPDATE THIS TO A HASHMAP
// performance not *too* bad right now, but it would be better if we don't have to iterate through all the sections every time we want to calculate the global page number
int EpubReader::section_page_to_global_page(int section, int page) {
  int global_page = 0;
  std::unique_ptr<HtmlParser> temp_parser(new HtmlParser());
  std::unique_ptr<TextRenderer<DISPLAY_TYPE>> temp_renderer(new TextRenderer<DISPLAY_TYPE>(renderer->getDisplay()));
  for (int i = 0; i < section; i++) {
    // check if this section is already calculated and stored in section_page_lengths map
    // btw end is a theoretical thing that just signifies the end of the map, it is not actually a valid section index
    if (section_page_lengths.find(i) != section_page_lengths.end()) {
      global_page += section_page_lengths[i];
      continue;
    }
    std::string item = epub->get_spine_item(i);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));

    int pages_in_section = 0;
    if (html) {
      temp_parser->parseHtml(html, strlen(html));
      temp_renderer->loadFromHtml(*temp_parser);
      temp_renderer->calculateAllPages();
      pages_in_section += temp_renderer->getTotalPages();
      free(html);
    }

    // section_page_lengths.push_back(temp_renderer->getTotalPages());
    section_page_lengths[i] = pages_in_section; // store even if 0
    global_page += pages_in_section;
  }
  global_page += page;

  return global_page;
}


void EpubReader::set_state_section(uint16_t current_section) {
  if (current_section >= epub->get_spine_items_count()) {
    current_section = 0; // wrap around to first section if out of range
  }
  if (current_section < 0) {
    current_section = epub->get_spine_items_count() - 1; // wrap around to last section if out of range
  }
  ESP_LOGI(TAG, "go to section:%d", current_section);
  state.current_section = current_section;
}

// This should probably be fixed
// thiss section gose thru every section and sums the pages
int EpubReader::get_total_pages() {
  if (total_pages > 0) {
    return total_pages;
  }

  // iterate through all sections and sum up total pages
  std::unique_ptr<HtmlParser> temp_parser(new HtmlParser());
  std::unique_ptr<TextRenderer<DISPLAY_TYPE>> temp_renderer(new TextRenderer<DISPLAY_TYPE>(renderer->getDisplay()));
  int pages = 0;

  for (int i = 0; i < epub->get_spine_items_count(); i++) {
    // check if this section is already calculated and stored in section_page_lengths vector
    if (i < section_page_lengths.size()) {
      pages += section_page_lengths[i];
      continue;
    }
    std::string item = epub->get_spine_item(i);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    if (!html) {
      continue;
    }
    Serial.printf("Debug: loaded html, now parsing:\n");
    temp_parser->parseHtml(html, strlen(html));
    Serial.printf("Debug: loaded html, now loading into temp renderer:\n");
    temp_renderer->loadFromHtml(*temp_parser);
    Serial.printf("Debug: loaded html, now calculating pages:\n");
    temp_renderer->calculateAllPages();
    pages += temp_renderer->getTotalPages();
    //DEBUG PRINT THE FIRST AND LAST WORD OF THE LAST PAGE TO MAKE SURE IT IS CALCULATING THE GLOBAL PAGE NUMBER CORRECTLY 
  
    free(html);
    // section_page_lengths.push_back(temp_renderer->getTotalPages());
    section_page_lengths[i] = temp_renderer->getTotalPages();


  }
  total_pages = pages;
  return total_pages;
}

// // This method calcs the last section and page and calculates from section_page_to_global_page
// int EpubReader::get_total_pages() {
//   if (total_pages > 0) {
//     return total_pages;
//   }
//     std::unique_ptr<HtmlParser> temp_parser(new HtmlParser());
//   int last_section = epub->get_spine_items_count() - 1;
//   std::string item = epub->get_spine_item(last_section);
//   std::string base_path = item.substr(0, item.find_last_of('/') + 1);
//   char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
//   if (!html) {
//     return 0;
//   }
//   std::unique_ptr<TextRenderer<DISPLAY_TYPE>> temp_renderer(new TextRenderer<DISPLAY_TYPE>(renderer->getDisplay()));
//   temp_renderer->loadFromHtml(*temp_parser);
//   temp_renderer->calculateAllPages();
//   int last_page = temp_renderer->getTotalPages();
//   // can cache this if it isn't already
//   if (section_page_lengths[last_section] == 0) {
//     section_page_lengths[last_section] = last_page;
//   }
//   total_pages = section_page_to_global_page(last_section, last_page);
//   return total_pages;
// }


// This only returns the current page in the current section
int EpubReader::get_current_page() {
  return state.current_page;
}

// I think this is a bit broken since it causes a failed reader initialization - or not?
int EpubReader::get_current_page_global() {
  global_current_page = section_page_to_global_page(state.current_section, state.current_page);
  return global_current_page;
}

void EpubReader::go_to_page(int global_page) {
  if (global_page < 0 || global_page >= get_total_pages()) {
    ESP_LOGW(TAG, "Page %d out of range, total pages: %d", global_page, get_total_pages());
    return;
  }

  int accumulated_page = 0;
  for (int i = 0; i < epub->get_spine_items_count(); i++) {
    int section_pages = 0;
    if (section_page_lengths.find(i) != section_page_lengths.end()) {
      section_pages = section_page_lengths[i];
    } else {
      std::unique_ptr<HtmlParser> temp_parser(new HtmlParser());
      std::unique_ptr<TextRenderer<DISPLAY_TYPE>> temp_renderer(new TextRenderer<DISPLAY_TYPE>(renderer->getDisplay()));
      
      std::string item = epub->get_spine_item(i);
      std::string base_path = item.substr(0, item.find_last_of('/') + 1);
      char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
      if (html) {
        temp_parser->parseHtml(html, strlen(html));
        temp_renderer->loadFromHtml(*temp_parser);
        temp_renderer->calculateAllPages();
        section_pages = temp_renderer->getTotalPages();
        free(html);
      }
      // section_page_lengths.push_back(section_pages);
      section_page_lengths[i] = section_pages;
    }


    if (accumulated_page + section_pages > global_page) {
      set_state_section(i);
      state.current_page = global_page - accumulated_page;
      parser.reset();
      parse_and_layout_current_section();

      // check if calc page actually matches
      if (state.current_page >= state.pages_in_current_section) {
        ESP_LOGW(TAG, "Page calculation mismatch: wanted page %d but section only has %d pages",
                 state.current_page, state.pages_in_current_section);
                 state.current_page = state.pages_in_current_section - 1; // set to last page of section if mismatch
      }

      return;
    }
    accumulated_page += section_pages;
  }


  // If we get here, go to last page of last section
  ESP_LOGW(TAG, "Reached end of loop, going to last section");
  set_state_section(epub->get_spine_items_count() - 1);
  parser.reset();
  parse_and_layout_current_section();
  state.current_page = state.pages_in_current_section - 1;
}

int EpubReader::get_current_section() {
  return state.current_section;
}