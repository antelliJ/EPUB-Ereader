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
};

bool EpubReader::load()
{
  ESP_LOGI(TAG, "Before epub load: %d", esp_get_free_heap_size());
  // do we need to load the epub?
  if (!epub || epub->get_path() != state.path)
  {
    // renderer->show_busy();
    parser.reset();
    epub = std::make_unique<Epub>(state.path);
    if (epub->load())
    {
      ESP_LOGI(TAG, "After epub load: %d", esp_get_free_heap_size());
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
    Serial.printf("Currently Parsing section %d\n", state.current_section);
    // renderer->show_busy();
    ESP_LOGI(TAG, "Parse and render section %d", state.current_section);
    ESP_LOGD(TAG, "Before read html: %d", esp_get_free_heap_size());
    Serial.printf("Debug: Free heap size before read html: %d", esp_get_free_heap_size());

    // if spine item is not found here then it will return get_spine_item(0)
    // so it does not crashes when you want to go after last page (out of vector range)
    std::string item = epub->get_spine_item(state.current_section);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    ESP_LOGD(TAG, "After read html: %d", esp_get_free_heap_size());
    Serial.printf("Debug: loaded html of section, now parsing:\n");
    parser = std::make_unique<HtmlParser>(html, strlen(html), base_path);
    free(html);
    ESP_LOGD(TAG, "After parse: %d", esp_get_free_heap_size());
    // parser->layout(renderer, epub);
    renderer->loadFromHtml(*parser);
    Serial.printf("Debug: loaded html into renderer, now calculating pages:\n");
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
    parser.reset();
  }
}

void EpubReader::prev()
{
  if (state.current_page == 0)
  {
    if (state.current_section > 0)
    {
      parser.reset();
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
    Serial.printf("No parser found for current section %d, parsing now...\n", state.current_section);
    parse_and_layout_current_section();
  }
  ESP_LOGD(TAG, "rendering page %d of %d", state.current_page, get_total_pages());
  // parser->render_page(state.current_page, renderer, epub);
  // renderer->drawPage(state.current_page); 
  Serial.printf("Rendering page %d of section, current page: %d\n", state.current_page, current_page_to_section_page(state.current_page));
  renderer-> drawPage(current_page_to_section_page(state.current_page));

  ESP_LOGD(TAG, "rendered page %d of %d", state.current_page, get_total_pages());
  ESP_LOGD(TAG, "after render: %d", esp_get_free_heap_size());
}

int EpubReader::current_page_to_section_page(int current_page) {
  int accumulated_page = 0;
  auto temp_parser = std::make_unique<HtmlParser>();
  auto temp_renderer = std::make_unique<TextRenderer<DISPLAY_TYPE>>(renderer->getDisplay());
  for (int i = 0; i < state.current_section; i++) {
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
  auto temp_parser = std::make_unique<HtmlParser>();
  auto temp_renderer = std::make_unique<TextRenderer<DISPLAY_TYPE>>(renderer->getDisplay());
  int pages = 0;

  for (int i = 0; i < epub->get_spine_items_count();
       i++) {
    std::string item = epub->get_spine_item(i);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    Serial.printf("Debug: loaded html, now parsing:\n");
    temp_parser.parseHtml(html, strlen(html));
    Serial.printf("Debug: loaded html, now loading into temp renderer:\n");
    temp_renderer->loadFromHtml(temp_parser);
    Serial.printf("Debug: loaded html, now calculating pages:\n");
    temp_renderer->calculateAllPages();
    total_pages += temp_renderer->getTotalPages();
    free(html);
  }
  
  delete temp_renderer;
  return total_pages;
}