#ifndef SQUARE_H
#define SQUARE_H

#include "portal.h"
#include <memory>

// The board is a 2d array of squares.  Each square can have a portal and/or a block.  Entities are tracked separately for the time being.
struct Square
{
  bool wall = false;
  std::shared_ptr<Portal> portal = nullptr;
};

#endif
