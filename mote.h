#ifndef MOTE_H
#define MOTE_H
struct Mote
{
  int x;
  int y;
  // These values are relative to the current position.  The mote will walk in a relatively straight line.  PUN!
  int goal_x;
  int goal_y;
};

#endif
