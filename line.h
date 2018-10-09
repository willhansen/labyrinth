#ifndef LINE_H
#define LINE_H

#include <vector>
#include "geometry.h"
#include <ncursesw/ncurses.h>

struct SquareMap
{
  // in the board's frame of reference (absolute)
  vect2Di board_pos;

  // relative to the line's starting position
  vect2Di line_pos;

  // Number of counter-clockwise rotations from the default world rotation
  int ccw_rotations = 0;

  // A color tint from going through portals
  int color = COLOR_WHITE;
};

class Line
{
public:
  // In order from the line's start to its end
  std::vector<SquareMap> mappings;
};

#endif
