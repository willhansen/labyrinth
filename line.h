#ifndef LINE_H
#define LINE_H

#include <vector>
#include "geometry.h"

struct SquareMap
{
  // in the board's frame of reference (absolute)
  vect2Di board_pos;

  // relative to the line's starting position
  vect2Di line_pos;
};

class Line
{
public:
  // In order from the line's start to its end
  std::vector<SquareMap> mappings;
};

#endif
