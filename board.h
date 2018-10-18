#ifndef SQUARE_H
#define SQUARE_H

#include "portal.h"
#include "line.h"
#include "entity.h"
#include <utility>
#include <memory>
#include <list>
#include <algorithm>

const std::vector<const wchar_t*> GRASS_GLYPHS = {L" ", L" ", L" ", L".", L"'", L",", L"`"};
const std::vector<int> GRASS_COLORS = {COLOR_YELLOW, COLOR_YELLOW, COLOR_GREEN};

int random(int min, int max) //range : [min, max)
{
   static bool first = true;
   if (first) 
   {  
      srand( time(NULL) ); //seeding for the first time only!
      first = false;
   }
   if (max == min)
   {
     return min;
   }
   return min + rand() % (( max ) - min);
}

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
  std::weak_ptr<Entity> entity;
};

struct Board
{
  const int board_size;
  std::vector<std::vector<Square>> board;
  std::vector<std::shared_ptr<Entity>> entities;

  Board(int board_size)
    : board(board_size, std::vector<Square>(board_size))
      , board_size(board_size)
  {
    // pick random grass glyphs and colors for every tile
    for (int x=0; x < board_size; x++)
    {
      for (int y=0; y < board_size; y++)
      {
        getSquare(vect2Di(x, y))->grass_glyph = GRASS_GLYPHS[random(0, GRASS_GLYPHS.size())];
        getSquare(vect2Di(x, y))->grass_color = GRASS_COLORS[random(0, GRASS_COLORS.size())];
      }
    }

    rectToWall(0, 0, board_size-1, board_size-1);
  }

  bool onBoard(vect2Di p)
  {
    return (p.x>=0 && p.y>=0 && p.x<board_size && p.y<board_size);
  }

  Square* getSquare(vect2Di pos)
  {
    if (!onBoard(pos))
    {
      return nullptr;
    }
    else
    {
      Square* ptr = &board[pos.x][pos.y];
      return ptr;
    }
  }

  void rectToWall(int left, int bottom, int right, int top)
  {
    for (int x = left; x < right+1; x++)
    {
      board[x][bottom].wall = true;
      board[x][top].wall = true;
    }
    for (int y = bottom; y < top+1; y++)
    {
      board[left][y].wall = true;
      board[right][y].wall = true;
    }
  }

  void deleteEntity(std::shared_ptr<Entity> entity)
  {
    // If 2 shared pointers are equal if they point to the same mote, this should work
    entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
  }
};


#endif
