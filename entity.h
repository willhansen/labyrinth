#ifndef MOTE_H
#define MOTE_H

#include "geometry.h"
#include <memory>

struct Board;

struct Entity
{
  vect2Di pos;
  std::weak_ptr<Board> board;
  // These values are relative to the current position. 
  // if this vector is zero, the mote cannot see the player
  // Set as the player's sight lines are updated
  vect2Di rel_player_pos = vect2Di(0, 0);
  vect2Di faced_direction = LEFT; // This is so there is consistency when stepping through portals
  bool moving = false;
  bool homing = false;
  bool can_shoot = false;
  // Does it disappear if it runs into a wall?
  bool die_on_touch = false;
  int max_cooldown = 4;
  int cooldown = 0;
  int detection_range=10;

  static Entity arrow(std::shared_ptr<Board> board, vect2Di pos, vect2Di dir)
  {
    Entity arrow;
    arrow.board = board;
    arrow.pos = pos;
    arrow.faced_direction = dir;
    arrow.moving = true;
    arrow.die_on_touch = true;
    return arrow;
  }

  static Entity mote(std::shared_ptr<Board> board, vect2Di pos)
  {
    Entity mote;
    mote.board = board;
    mote.pos = pos;
    mote.moving = true;
    mote.homing = true;
    return mote;
  }

  static Entity turret(std::shared_ptr<Board> board, vect2Di pos, vect2Di dir)
  {
    Entity turret;
    turret.board = board;
    turret.pos = pos;
    turret.faced_direction = dir;
    turret.can_shoot = true;
    return turret;
  }
};

#endif
