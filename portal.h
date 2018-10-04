#ifndef PORTAL_H
#define PORTAL_H

#include "geometry.h"
#include <ncurses.h>

struct Portal
{
  vect2Di new_pos;
  mat2Di transform;
  int color = COLOR_GREEN; // white is unchanged, otherwise tints by color (maybe black does something else)
};

#endif
