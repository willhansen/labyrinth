#ifndef PORTAL_H
#define PORTAL_H

#include "geometry.h"

struct Portal
{
  vect2Di new_pos;
  // number of 90 degree rotations counter clockwise
  mat2Di transform;
};

#endif
