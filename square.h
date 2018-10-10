#ifndef SQUARE_H
#define SQUARE_H

#include "portal.h"
#include "line.h"
#include "mote.h"
#include <utility>
#include <memory>
#include <list>

// The board is a 2d array of squares.  Each square can have a portal and/or a block.  Entities are tracked separately for the time being.
struct Square
{
  bool wall = false;
  bool fire = false;
  int water = 0; // Water depth
  int plant = 0; // plant health
  int steam = 0; // steam pressure
  const wchar_t* grass_glyph;
  int grass_color;
  // These are portals you go through if you are leaving this square.
  std::shared_ptr<Portal> right_portal;
  std::shared_ptr<Portal> up_portal;
  std::shared_ptr<Portal> left_portal;
  std::shared_ptr<Portal> down_portal;
  std::weak_ptr<Mote> mote;
  std::weak_ptr<Arrow> arrow;
};

#endif
