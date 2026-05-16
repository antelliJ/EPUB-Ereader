#include "TextRenderer.h"
#include "State.h"

#define PADDING 20
#define EPUBS_PER_PAGE 5

#ifndef HALLUCINATION // courtesy of Grok
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

class EpubList
{
private:
  TextRenderer<DISPLAY_TYPE> *renderer;
  EpubListState &state;
  bool m_needs_redraw = false;

public:
  EpubList(TextRenderer<DISPLAY_TYPE> *renderer, EpubListState &state) : renderer(renderer), state(state){};
  ~EpubList() {}
  bool load(const char *path)
  {
    

    if (state.is_loaded)
    {
        ESP_LOGI(TAG, "Already loaded books");
        return true;
    }
    state.num_epubs = 0;
    File root = LittleFS.open(path);
    File file = root.openNextFile();
    while(file){
        // check if state already has max number of epubs
        if (state.num_epubs >= MAX_EPUB_LIST_SIZE){
          break;
        }
        Serial.printf(" - File: %s, Size: %u\n", file.name(), file.size());
        //   if file is an epub, add it to the list
        if (strstr(file.name(), ".epub") != nullptr){
            // state.epub_list.push_back(EpubListItem(file.name()));
            // save to end as a c array
            Epub *epub = new Epub(std::string("/littlefs/") + file.name());
            if (epub->load())
            {
                Serial.printf(" - Title: %s\n", epub->get_title().c_str());
                strncpy(state.epub_list[state.num_epubs].path, epub->get_path().c_str(), MAX_PATH_SIZE);
                strncpy(state.epub_list[state.num_epubs].title, (epub->get_title()).c_str(), MAX_TITLE_SIZE);
                state.num_epubs++;
                if (state.num_epubs == MAX_EPUB_LIST_SIZE)
                {
                ESP_LOGE(TAG, "Too many epubs, max is %d", MAX_EPUB_LIST_SIZE);
                break;
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to load epub %s", file.name());
            }
            delete epub;
        }
    
      file = root.openNextFile();
    }
    root.close();
    // sanity check our state
    if (state.selected_item >= state.num_epubs)
    {
        state.selected_item = 0;
        state.previous_rendered_page = -1;
        state.previous_selected_item = -1;
    }
    state.is_loaded = true;
    return true;
  };
  void set_needs_redraw() { m_needs_redraw = true; }
  void next() {state.selected_item = (state.selected_item + 1) % state.num_epubs;};
  void prev() {state.selected_item = (state.selected_item - 1 + state.num_epubs) % state.num_epubs;};
  void render() {

    renderer->getDisplay().setRotation(1);
    renderer->prepareRefresh();
    renderer->getDisplay().firstPage();
    
    int current_page = state.selected_item / EPUBS_PER_PAGE;
    int cell_height = renderer->get_page_height() / EPUBS_PER_PAGE;
    int start_index = current_page * EPUBS_PER_PAGE;

    do {

    int ypos = 0;

    if (current_page != state.previous_rendered_page || m_needs_redraw)
    {
      m_needs_redraw = false;
      // renderer->show_busy();
      // renderer->clear_screen();
      renderer->getDisplay().fillScreen(GxEPD_WHITE);
      state.previous_selected_item = -1;
      // trigger a redraw of the items
      state.previous_rendered_page = -1;
    }

    ESP_LOGI(TAG, "start_index: %d num_epubs: %d", start_index, state.num_epubs);
    for (int i = start_index; i < start_index + EPUBS_PER_PAGE && i < state.num_epubs; i++) {

      // draw the title
      int text_xpos = PADDING;
      int text_ypos = ypos + PADDING + 10;

      renderer->getDisplay().setCursor(text_xpos, text_ypos);
      renderer->getDisplay().setTextColor(GxEPD_BLACK);
      renderer->getDisplay().setFont(&Font5x7Fixed);
      renderer->getDisplay().print(state.epub_list[i].title);

      // draw file path below title, indented

      renderer->getDisplay().setCursor(text_xpos + 10, text_ypos + 12);
      renderer->getDisplay().setFont(&TomThumb);
      renderer->getDisplay().print(state.epub_list[i].path);


      // draw selection box if selected
      if (state.selected_item == i)
      {
        renderer->getDisplay().drawRect(0, ypos, renderer->getDisplay().width(), cell_height, GxEPD_BLACK);
      }


    // state.previous_rendered_page = current_page;
      
      ypos += cell_height;
    }

  } while (renderer->getDisplay().nextPage());
  renderer->getDisplay().hibernate();
  
  state.previous_selected_item = state.selected_item;
  state.previous_rendered_page = current_page;
  };
};