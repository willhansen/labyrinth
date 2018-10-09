#ifndef MOTE_H
#define MOTE_H

#include "geometry.h"

struct Mote
{
  vect2Di pos;
  // These values are relative to the current position.  The mote will walk in a relatively straight line.  PUN!
  // if this vector is zero, the mote cannot see the player
  vect2Di rel_player_pos;
  vect2Di faced_direction = LEFT; // This is so there is consistency when stepping through portals
};

#endif
