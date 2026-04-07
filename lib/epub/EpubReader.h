#pragma once

#include <string.h>
#include "epub.h"
#include "TextRenderer.h"
#include "HtmlParser.h"

#include "State.h"

class EpubReader
{
private:
  EpubListItem &state;
  Epub *epub = nullptr;
  HtmlParser *parser = nullptr;
  
  TextRenderer<DISPLAY_TYPE> *renderer = nullptr;

  void parse_and_layout_current_section();

  int total_pages = 0;

public:
  EpubReader(EpubListItem &state, TextRenderer<DISPLAY_TYPE> *renderer) : state(state), renderer(renderer){};
  ~EpubReader() {}
  bool load();
  void next();
  void prev();
  void render();
  void set_state_section(uint16_t current_section);
  int get_total_pages();
  int current_page_to_section_page(int current_page);
};

bool EpubReader::load()
{
  ESP_LOGD(TAG, "Before epub load: %d", esp_get_free_heap_size());
  // do we need to load the epub?
  if (!epub || epub->get_path() != state.path)
  {
    // renderer->show_busy();
    delete epub;
    delete parser;
    parser = nullptr;
    epub = new Epub(state.path);
    if (epub->load())
    {
      ESP_LOGD(TAG, "After epub load: %d", esp_get_free_heap_size());
      return false;
    }
  }
  total_pages = 0; // reset total pages so it will be recalculated when needed
  return true;
}


void EpubReader::parse_and_layout_current_section()
{
  if (!parser)
  {
    // renderer->show_busy();
    ESP_LOGI(TAG, "Parse and render section %d", state.current_section);
    ESP_LOGD(TAG, "Before read html: %d", esp_get_free_heap_size());

    // if spine item is not found here then it will return get_spine_item(0)
    // so it does not crashes when you want to go after last page (out of vector range)
    std::string item = epub->get_spine_item(state.current_section);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    ESP_LOGD(TAG, "After read html: %d", esp_get_free_heap_size());
    parser = new HtmlParser(html, strlen(html), base_path);
    free(html);
    ESP_LOGD(TAG, "After parse: %d", esp_get_free_heap_size());
    // parser->layout(renderer, epub);
    renderer->loadFromHtml(*parser);
    renderer->calculateAllPages();
    ESP_LOGD(TAG, "After layout: %d", esp_get_free_heap_size());
    state.pages_in_current_section = renderer->getTotalPages();
  }
}

void EpubReader::next()
{
  state.current_page++;
  if (state.current_page >= state.pages_in_current_section)
  {
    state.current_section++;
    state.current_page = 0;
    delete parser;
    parser = nullptr;
  }
}

void EpubReader::prev()
{
  if (state.current_page == 0)
  {
    if (state.current_section > 0)
    {
      delete parser;
      parser = nullptr;
      state.current_section--;
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
    parse_and_layout_current_section();
  }
  ESP_LOGD(TAG, "rendering page %d of %d", state.current_page, get_total_pages());
  // parser->render_page(state.current_page, renderer, epub);
  // renderer->drawPage(state.current_page); 
  renderer-> drawPage(current_page_to_section_page(state.current_page));

  ESP_LOGD(TAG, "rendered page %d of %d", state.current_page, get_total_pages());
  ESP_LOGD(TAG, "after render: %d", esp_get_free_heap_size());
}

int EpubReader::current_page_to_section_page(int current_page) {
  int accumulated_page = 0;
  HtmlParser temp_parser;
  for (int i = 0; i < state.current_section; i++) {
    // page += state.epub_list[i].pages_in_current_section;
    std::string item = epub->get_spine_item(i);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    temp_parser.parseHtml(html, strlen(html));
    renderer->loadFromHtml(temp_parser);
    renderer->calculateAllPages();
    if ((accumulated_page+renderer->getTotalPages()) >= current_page) {
      free(html);
      return current_page - accumulated_page;
    }
    accumulated_page += renderer->getTotalPages();
    free(html);
  }
  return 0; // default to first page if something goes wrong
}


void EpubReader::set_state_section(uint16_t current_section) {
  ESP_LOGI(TAG, "go to section:%d", current_section);
  state.current_section = current_section;
}

int EpubReader::get_total_pages() {
  if (total_pages > 0) {
    return total_pages;
  }

  // iterate through all sections and sum up total pages
  int total_pages = 0;
  for (int i = 0; i < epub->get_spine_items_count();
       i++) {
    std::string item = epub->get_spine_item(i);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    HtmlParser temp_parser(html, strlen(html), base_path);
    renderer->loadFromHtml(temp_parser);
    renderer->calculateAllPages();
    total_pages += renderer->getTotalPages();
    free(html);
  }
  return total_pages;
}