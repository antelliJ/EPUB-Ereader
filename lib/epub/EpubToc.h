#include "TextRenderer.h"
#include "State.h"

// static const char *TAG = "PUBINDEX";
#define PADDING 14
#define ITEMS_PER_PAGE 6

struct TocTarget {
  uint16_t spine_index;
  std::string anchor;
};

class EpubToc
{
private:
  TextRenderer<DISPLAY_TYPE> *renderer;
  //std::unique_ptr<Epub> epub;
  Epub *epub = nullptr;
  EpubListItem &selected_epub;
  EpubTocState &state;
  bool m_needs_redraw = false;

public:
  EpubToc(EpubListItem &selected_epub, EpubTocState &state, TextRenderer<DISPLAY_TYPE> *renderer) : renderer(renderer), selected_epub(selected_epub), state(state){};
  ~EpubToc() {}
  bool load();
  void next();
  void prev();
  void render();
  void set_needs_redraw() { m_needs_redraw = true; }
  uint16_t get_selected_toc_spine();
  TocTarget get_selected_toc_target();
};

bool EpubToc::load() {
    if (!epub || epub->get_path() != selected_epub.path)
    {
        // Remove renderer->show_busy() - doesn't exist
        delete epub;
        epub = nullptr;

        Epub *new_epub = new Epub(selected_epub.path);
        
        // Feed watchdog during load
        vTaskDelay(1);
        
        if (!new_epub->load()) {
            ESP_LOGE(TAG, "Failed to load epub for TOC");
            delete new_epub;
            return false;
        }

        epub = new_epub;
    }
    
    // Validate TOC exists
    if (epub->get_toc_items_count() == 0) {
        ESP_LOGW(TAG, "No TOC entries found");
        delete epub;
        epub = nullptr;
        return false;
    }
    
    return true;
}

void EpubToc::next(){
  if (!epub){
    load();
  }
  state.selected_item = (state.selected_item + 1) % epub->get_toc_items_count();
}

void EpubToc::prev(){
  if (!epub){
    load();
  }
  state.selected_item = (state.selected_item - 1 + epub->get_toc_items_count()) % epub->get_toc_items_count();
}

void EpubToc::render() {
  if (!epub){
    if (!load()) {
      ESP_LOGE(TAG, "Failed to load epub for TOC render");
      return;
    }
  }

  int current_page = state.selected_item / ITEMS_PER_PAGE;
  int cell_height = renderer->get_page_height() / ITEMS_PER_PAGE;
  int start_index = current_page * ITEMS_PER_PAGE;

  bool full_redraw = current_page != state.previous_rendered_page || m_needs_redraw;

  do {
  int ypos = 0;


  if (full_redraw)
  {
    renderer->getDisplay().fillScreen(GxEPD_WHITE);
  }

  ESP_LOGI(TAG, "start_index: %d num_toc_items: %d", start_index, epub->get_toc_items_count());
  for (int i = start_index; 
        i < start_index + ITEMS_PER_PAGE && i < epub->get_toc_items_count(); 
        i++)
  {
    int text_xpos = PADDING;
    int text_ypos = ypos + PADDING + 10;

    if (full_redraw) {
      renderer->getDisplay().setCursor(text_xpos, text_ypos);
      renderer->getDisplay().setTextColor(GxEPD_BLACK);
      renderer->getDisplay().setFont(&Font5x7Fixed);
      renderer->getDisplay().print(epub->get_toc_item(i).title.c_str());
    }

    if (i == state.selected_item) {
      // Draw a rectangle around the selected item
      renderer->getDisplay().drawRect(0, ypos, renderer->get_page_width(), cell_height, GxEPD_BLACK);
    } else if (state.previous_selected_item == i) {
      // Clear the rectangle around the previously selected item
      renderer->getDisplay().drawRect(0, ypos, renderer->get_page_width(), cell_height, GxEPD_WHITE);
    }

    ypos += cell_height;
  }
} while (renderer->getDisplay().nextPage());

  // update state
  m_needs_redraw = false;
  state.previous_rendered_page = current_page;
  state.previous_selected_item = state.selected_item;

  renderer->getDisplay().hibernate();
}

uint16_t EpubToc::get_selected_toc_spine()
{
  return epub->get_spine_index_for_toc_index(state.selected_item);
}

TocTarget EpubToc::get_selected_toc_target()
{
  TocTarget target;
  target.spine_index = get_selected_toc_spine();
  target.anchor = epub->get_toc_item(state.selected_item).anchor;
  return target;
}