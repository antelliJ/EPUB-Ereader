#pragma once

typedef enum
{
  NONE,
  UP, // right arrow
  DOWN, // left arrow
  SELECT, // ok
  MENU, // disk menu
  OPTIONS, // options
  REWIND, // <<
  FAST_FORWARD, // >>
  BOOKMARK, // subtitle?
  SAVE, // Stop?
  LAST_INTERACTION
} UIAction;