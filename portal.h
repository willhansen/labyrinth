#ifndef PORTAL_H
#define PORTAL_H

#include "geometry.h"
#include <ncursesw/ncurses.h>
#include <memory>

struct Board;

struct Portal
{
  vect2Di new_pos;
  std::weak_ptr<Board> new_board;
  mat2Di transform;
  int color = COLOR_WHITE; // white is unchanged, otherwise tints by color (maybe black does something else)
};

#endif
