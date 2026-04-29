#include "TextRenderer.h"
#include "State.h"

class EpubToc
{
private:
  TextRenderer<DISPLAY_TYPE> *renderer;
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
  uint16_t get_selected_toc();
};