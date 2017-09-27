#ifndef MOTE_H
#define MOTE_H

#include "geometry.h"

struct Mote
{
  // These values are relative to the current position.  The mote will walk in a relatively straight line.  PUN!
  vect2Di rel_player_pos;
};

#endif
