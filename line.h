#ifndef LINE_H
#define LINE_H

#include <vector>

struct SquareMap
{
  // in the board's frame of reference (absolute)
  int board_x;
  int board_y;

  // relative to the line's starting position
  int line_x;
  int line_y;
};

class Line
{
public:
  // In order from the line's start to its end
  std::vector<SquareMap> mappings;
};

#endif
