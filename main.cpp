
#define _XOPEN_SOURCE_EXTENDED 1
#include "board.h"
#include "line.h"
#include "portal.h"
#include "entity.h"
#include "geometry.h"

#include <ncursesw/ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <locale.h>
#include <vector>
#include <tuple>
#include <utility>
#include <cmath>
#include <algorithm>

const int BOARD_SIZE = 100;
const int MEMORY_MAP_SIZE = 101;
const int SIGHT_RADIUS = 50; 
const bool NAIVE_VIEW = false;
const bool PORTALS_OFF = false;

const int WHITE_ON_BLACK = 0;
const int RED_ON_BLACK = 1;
const int BLACK_ON_WHITE = 2;
const int BLACK_ON_RED = 3;

const wchar_t* WALL_GLYPH = L"█";

// These are in order from RIGHT going ccw
const std::vector<const wchar_t*> MOTE_GLYPHS = {L"▶", L"▲", L"◀", L"▼"};

const std::vector<const wchar_t*> ARROW_GLYPHS = { L"→", L"↑", L"←", L"↓"};
const std::vector<const wchar_t*> TURRET_GLYPHS = { L"→", L"↑", L"←", L"↓"};

const int PLANT_MAX_HEALTH = 10;
const wchar_t* PLANT_GLYPH = L"♣";
const int AVG_PLANT_SPAWN_TIME = 20;
const int AVG_FIRE_SPREAD_TIME = 2;

const int SHALLOW_WATER_DEPTH = 3;
const int AVG_WATER_FLOW_TIME = 1;
const wchar_t* WATER_GLYPH = L"≈";

const int STEAM_PER_WATER = 100;
const wchar_t* STEAM_GLYPH = L"▒";

const int BACKGROUND_COLOR = WHITE_ON_BLACK;
const wchar_t* OUT_OF_VIEW = L" ";


std::pair<std::shared_ptr<Board>, vect2Di> posFromStep(std::shared_ptr<Board> start_board, vect2Di start_pos, vect2Di step);
Line curveCast(std::shared_ptr<Board> board, std::vector<vect2Di> naive_squares, bool is_sight_line=false);
void drawEverything();
void updateSightLines();
Line lineCast(std::shared_ptr<Board> start_board, vect2Di start_pos, vect2Di d_pos, bool is_sight_line=false);
void initNCurses();
mat2Di transformFromStep(std::shared_ptr<Board> start_board, vect2Di start_pos, vect2Di step);
void shiftMemoryMap(vect2Di);

//TODO: make these non-global
std::vector<std::shared_ptr<Board>> boards;
std::vector<std::vector<const wchar_t*>> memory_map(MEMORY_MAP_SIZE, std::vector<const wchar_t*>(MEMORY_MAP_SIZE, L" "));
std::vector<Line> player_sight_lines;
vect2Di player_pos;
std::shared_ptr<Board> player_board;
int consecutive_laser_rounds = 0;
vect2Di player_faced_direction = RIGHT;
// This is visual only, its a transform for drawing to the screen and changing the direction of movement inputs.
mat2Di player_transform;
int mouse_x, mouse_y;
vect2Di mouse_pos;

int num_rows,num_cols;				/* to store the number of rows and */


bool onScreen(int row, int col)
{
  return (row>=0 && col>=0 && row<num_rows && col<num_cols);
}

bool posIsEmpty(std::shared_ptr<Board> board, vect2Di pos)
{
  Square* squareptr = board->getSquare(pos);
  // Square must be empty and also actually be there
  if (squareptr == nullptr || 
      squareptr->entity.lock() != nullptr || 
      squareptr->wall != false || 
      squareptr->water != 0 ||
      squareptr->plant != 0 ||
      squareptr->fire != false ||
      pos == player_pos)
  {
    return false;
  }
  else
  {
    return true;
  }
}

bool posIsWalkable(std::shared_ptr<Board> board, vect2Di pos)
{
  Square* squareptr = board->getSquare(pos);
  // Square must be empty and also actually be there
  if (squareptr == nullptr || 
      squareptr->entity.lock() != nullptr || 
      squareptr->wall != false || 
      squareptr->water > SHALLOW_WATER_DEPTH ||
      squareptr->plant != 0 ||
      squareptr->fire != false ||
      pos == player_pos)
  {
    return false;
  }
  else
  {
    return true;
  }
}

bool posIsFlyable(std::shared_ptr<Board> board, vect2Di pos)
{
  Square* squareptr = board->getSquare(pos);
  // Square must be empty and also actually be there
  if (squareptr == nullptr || 
      squareptr->entity.lock() != nullptr || 
      squareptr->wall != false || 
      squareptr->plant != 0 ||
      pos == player_pos)
  {
    return false;
  }
  else
  {
    return true;
  }
}

int getColorPairIndex(int forground, int background)
{
  return forground * COLORS + background;
}

void attemptMove(vect2Di dp, bool voluntaryMove = true)
{
  if (voluntaryMove)
  {
    player_faced_direction = dp;
  }
  Line line = lineCast(player_board, player_pos, dp);
  if (line.mappings.size() > 0)
  {
    vect2Di pos = line.mappings[0].board_pos;
    std::shared_ptr<Board> board = line.mappings[0].board;
    if (posIsWalkable(board, pos))
    {
      shiftMemoryMap(dp * player_transform.inversed());
      player_transform *= transformFromStep(player_board, player_pos, dp);
      player_faced_direction *= transformFromStep(player_board, player_pos, dp);
      player_pos = pos;
      player_board = board;
    }
  }
}

void createMote(std::shared_ptr<Board> board, vect2Di pos)
{
  Square* squareptr = board->getSquare(pos);
  // Square must be empty
  if (squareptr == nullptr || !posIsEmpty(board, pos))
  {
    return;
  }
  std::shared_ptr<Entity> moteptr = std::make_shared<Entity>(Entity::mote(board, pos));
  squareptr->entity = moteptr;
  board->entities.push_back(moteptr);
}

void createPlant(std::shared_ptr<Board> board, vect2Di pos)
{
  Square* squareptr = board->getSquare(pos);
  // Square must be empty
  if (squareptr == nullptr || !posIsWalkable(board, pos))
  {
    return;
  }
  squareptr->plant = PLANT_MAX_HEALTH;
}

void createWater(std::shared_ptr<Board> board, vect2Di pos, int depth)
{
  Square* squareptr = board->getSquare(pos);
  // Square must be empty
  if (!posIsEmpty(board, pos))
  {
    return;
  }
  squareptr->water = depth;
}



// This assumes validity checks have been done, and just handles the pointer movements
void moveEntity(std::shared_ptr<Entity> entityptr, std::shared_ptr<Board> new_board, vect2Di new_pos)
{
  std::shared_ptr<Board> old_board = entityptr->board.lock();

  Square* newSquareptr = new_board->getSquare(new_pos);
  Square* oldSquareptr = old_board->getSquare(entityptr->pos);

  newSquareptr->entity = oldSquareptr->entity;
  oldSquareptr->entity.reset();

  entityptr->pos = new_pos;

  // if the entity has crossed over to a new board
  if (new_board != old_board)
  {
    entityptr->board = new_board;
    new_board->entities.push_back(entityptr);
    old_board->deleteEntity(entityptr);
  }
}


vect2Di firstStepInDirection(vect2Di far_step)
{
  if (far_step == vect2Di(0, 0))
  {
    return far_step;
  }
  if (std::abs(far_step.x) > std::abs(far_step.y))
  {
    if (far_step.x > 0)
    {
      return RIGHT;
    }
    else
    {
      return LEFT;
    }
  }
  else
  {
    if (far_step.y > 0)
    {
      return UP;
    }
    else
    {
      return DOWN;
    }
  }
}

std::shared_ptr<Portal>* getPortal(Square& square, const vect2Di& step)
{
  std::shared_ptr<Portal>* portalptrptr = nullptr;
  if (step.x  == 1)
  {
    portalptrptr = &(square.right_portal);
  }
  else if (step.x == -1)
  {
    portalptrptr = &(square.left_portal);
    //portalptrptr = std::make_shared<std::shared_ptr<Portal>>(square.left_portal);
  }
  else if (step.y == 1)
  {
    portalptrptr = &(square.up_portal);
    //portalptrptr = std::make_shared<std::shared_ptr<Portal>>(square.up_portal);
  }
  else if (step.y == -1)
  {
    portalptrptr = &(square.down_portal);
    //portalptrptr = std::make_shared<std::shared_ptr<Portal>>(square.down_portal);
  }
  return portalptrptr;
}

// TODO: allow double start and end positions in order to allow slight perturbations to avoid needing to break ties.
// This returns all the squares that fall on the line between (0, 0) and the given point the points are at the center of squares.  All squares on the line are orthogonally connected exactly once in a chain, with tie-breaking for diagonals.
std::vector<vect2Di> orthogonalBresneham(vect2Di goal_pos)
{
  // this line starts at zero
  // ties are broken towards y=+/-inf
  std::vector<vect2Di> output;
  // include the first step
  const int num_steps = std::max(std::abs(goal_pos.x), std::abs(goal_pos.y));
  vect2Di pos;
  double x=0;
  double y=0;
  double dx = static_cast<double>(goal_pos.x)/static_cast<double>(num_steps);
  double dy = static_cast<double>(goal_pos.y)/static_cast<double>(num_steps);
  output.push_back(pos);
  for (int step_num = 0; step_num < num_steps; step_num++)
  {
    // every step will enter a new square.  The question is: was it a diagonal step?
    double next_x = x+dx;
    double next_y = y+dy;
    vect2Di next_pos = vect2Di(static_cast<int>(std::round(next_x)), static_cast<int>(std::round(next_y)));
    // if diagonal step, there is another square before the next_pos square
    if (std::abs(next_pos.x - pos.x) + std::abs(next_pos.y - pos.y) > 1)
    {
      // need to find which orthogonal square this went through, If a tie, pick the vertical
      double y_division = std::round(std::min(y, next_y)) + 0.5;
      double x_division = std::round(std::min(x, next_x)) + 0.5;
      double step_slope = (next_y - y)/(next_x - x);
      double y_at_x_division = y + step_slope * (x_division - x);

      // This line decides diagonal tie breaks
      // If the intermediate step is horizontal first
      if ((next_y > y && y_at_x_division < y_division) || (next_y < y && y_at_x_division > y_division))
      {
        output.push_back(vect2Di(next_pos.x, pos.y));
      }
      else
      {
        output.push_back(vect2Di(pos.x, next_pos.y));
      }

    }
    output.push_back(next_pos);
    x = next_x;
    y = next_y;
    pos = next_pos;
  }
  return output;
}

std::vector<vect2Di> orthogonalBresneham(vect2Di start, vect2Di end)
{
  vect2Di rel_end = end-start;
  std::vector<vect2Di> rel_points = orthogonalBresneham(rel_end);
  for (int i = 0; i < static_cast<int>(rel_points.size()); i++)
  {
    rel_points[i] += start;
  }
  return rel_points;
}

void screenToBoard(int row, int col, vect2Di& pos)
{
  // The player is at the center of the sightmap, coordinates are (x, y) in the first quadrant
  // The screen is (y, x) in the fourth quadrant (but y is positive)
  pos.x = player_pos.x - num_cols/2 + col;
  pos.y = player_pos.y + num_rows/2 - row;
  return;
}

void screenToMemoryMap(int row, int col, vect2Di& pos)
{
  pos.x = (MEMORY_MAP_SIZE/2 + 1) - num_cols/2 + col;
  pos.y = (MEMORY_MAP_SIZE/2 + 1) + num_rows/2 - row;
}

// Sight map positions are relative to its center
// Screen positions are relative to top left
void sightMapToScreen(vect2Di pos, int& row, int& col)
{
  vect2Di corrected_pos = pos * player_transform.inversed();
  row = num_rows/2 - corrected_pos.y;
  col = corrected_pos.x + num_cols/2;
}

// This gives board to screen coordinates, player is at center, PORTALS ARE IGNORED
void naiveBoardToScreen(vect2Di pos, int& row, int& col)
{
  row = player_pos.y + num_rows/2 - pos.y;
  col = pos.x - player_pos.x + num_cols/2;
}

void makePortalPair(std::shared_ptr<Board> b1 ,vect2Di p1, std::shared_ptr<Board> b2, vect2Di p2, bool left=true)
{
  if (!b1->onBoard(p1) || !b2->onBoard(p2))
    return;
  if (left)
  {
    b1->board[p1.x][p1.y].left_portal.reset(new Portal());
    b1->board[p1.x][p1.y].left_portal->new_pos = p2+LEFT;
    b1->board[p1.x][p1.y].left_portal->new_board = b2;

    b1->board[p1.x-1][p1.y].right_portal.reset(new Portal());
    b1->board[p1.x-1][p1.y].right_portal->new_pos = p2;
    b1->board[p1.x-1][p1.y].right_portal->new_board = b2;

    b2->board[p2.x][p2.y].left_portal.reset(new Portal());
    b2->board[p2.x][p2.y].left_portal->new_pos = p1+LEFT;
    b2->board[p2.x][p2.y].left_portal->new_board = b1;

    b2->board[p2.x-1][p2.y].right_portal.reset(new Portal());
    b2->board[p2.x-1][p2.y].right_portal->new_pos = p1;
    b2->board[p2.x-1][p2.y].right_portal->new_board = b1;
  }
  else
  {
    b1->board[p1.x][p1.y].down_portal.reset(new Portal());
    b1->board[p1.x][p1.y].down_portal->new_pos = p2+DOWN;
    b1->board[p1.x][p1.y].down_portal->new_board = b2;

    b1->board[p1.x][p1.y-1].up_portal.reset(new Portal());
    b1->board[p1.x][p1.y-1].up_portal->new_pos = p2;
    b1->board[p1.x][p1.y-1].up_portal->new_board = b2;

    b2->board[p2.x][p2.y].down_portal.reset(new Portal());
    b2->board[p2.x][p2.y].down_portal->new_pos = p1+DOWN;
    b2->board[p2.x][p2.y].down_portal->new_board = b1;

    b2->board[p2.x][p2.y-1].up_portal.reset(new Portal());
    b2->board[p2.x][p2.y-1].up_portal->new_pos = p1;
    b2->board[p2.x][p2.y-1].up_portal->new_board = b1;
  }
}


void makeOneWayPortalPair( std::shared_ptr<Board> board1,vect2Di pos1, vect2Di step1, std::shared_ptr<Board> board2, vect2Di pos2, vect2Di step2, bool flip)
{
  Square* square1 = board1->getSquare(pos1);
  Square* square2 = board2->getSquare(pos2);
  // both squares must be on the board
  if (square1==nullptr || square2==nullptr)
  {
    return;
  }
  // The steps indicate direction and must be exactly one step orthogonal
  if (abs(step1.x)+abs(step1.y) != 1 || abs(step2.x)+abs(step2.y) != 1)
  {
    return;
  }

  vect2Di v = step1;
  mat2Di rotation1to2 = IDENTITY;
  while(v != -step2)
  {
    v *= CCW;
    rotation1to2 *= CCW;
  }

  // do all the logic for one portal at a time, just in case they are the same goddamn portal.
  std::shared_ptr<Portal>* portal1 = getPortal(*square1, step1);
  (*portal1).reset(new Portal());
  (*portal1)->new_pos = pos2;
  (*portal1)->new_board = board2;
  (*portal1)->transform = rotation1to2;

  if (flip == true)
  {
    if (step1.x != 0)
    {
      (*portal1)->transform *= FLIP_Y;
    }
    else
    {
      (*portal1)->transform *= FLIP_X;
    }
  }

  std::shared_ptr<Portal>* portal2 = getPortal(*square2, step2);
  (*portal2).reset(new Portal());
  (*portal2)->new_pos = pos1;
  (*portal2)->new_board = board1;
  (*portal2)->transform = rotation1to2.inversed();

  if (flip == true)
  {
    if (step2.x != 0)
    {
      (*portal2)->transform *= FLIP_Y;
    }
    else
    {
      (*portal2)->transform *= FLIP_X;
    }
  }
}

void makePortalPair2( std::shared_ptr<Board> board1, vect2Di pos1, vect2Di step1, std::shared_ptr<Board> board2, vect2Di pos2, vect2Di step2, bool flip=false)
{
  makeOneWayPortalPair( board1,pos1, step1, board2, pos2, step2, flip);
  makeOneWayPortalPair( board1,pos1+step1, -step1, board2, pos2+step2, -step2, flip);
}

void initNCurses()
{
  initscr();				/* start the curses mode */
  start_color();
  clear();
  noecho();
  cbreak();
  keypad(stdscr, true);
  getmaxyx(stdscr,num_rows,num_cols);		/* get the number of rows and columns */
  mousemask(ALL_MOUSE_EVENTS, NULL);

  // init all the color pairs
  for(int forground=0; forground < COLORS; ++forground)
  {
    for(int background=0; background < COLORS; ++background)
    {
      init_pair(getColorPairIndex(forground, background), forground, background);
    }
  }
}


void makeNicePortalPair( std::shared_ptr<Board> b1,int x1, int y1, std::shared_ptr<Board> b2, int x2, int y2, int dx, int dy)
{
  b1->board[x1][y1].wall = true;
  b2->board[x2][y2].wall = true;
  b1->board[x1+dx+1][y1+dy].wall = true;
  b2->board[x2+dx+1][y2+dy].wall = true;
  for (int rx = 1; rx <= dx; rx++)
  {
    makePortalPair( b1,vect2Di(x1+rx,y1), b2,vect2Di(x2+rx,y2), false);
  }

}

void makeMirror(std::shared_ptr<Board> board, vect2Di square, vect2Di step)
{
  makePortalPair2( board,square, step, board, square, step, true);
}

void initWorld()
{
  // make the boards
  for (int i = 0; i < 2; i++)
  {
    boards.push_back(std::make_shared<Board>(Board(BOARD_SIZE)));
  }
  player_board = boards[1];
  player_pos = vect2Di(5, 5);

  
  boards[0]->rectToWall(30, 5, 50, 20);

  //rectToWall(20,10,22,12);
  boards[0]->getSquare(vect2Di(20,12))->wall = true;
  boards[0]->getSquare(vect2Di(20,10))->wall = true;
  boards[0]->getSquare(vect2Di(22,10))->wall = true;
  makePortalPair2(boards[0] ,vect2Di(20, 11), RIGHT, boards[0], vect2Di(21, 10), UP, false);

  // corner portal on frame of square
  boards[0]->getSquare(vect2Di(30,8))->wall = false;
  boards[0]->getSquare(vect2Di(30,7))->wall = false;
  boards[0]->getSquare(vect2Di(30,6))->wall = false;
  boards[0]->getSquare(vect2Di(33,5))->wall = false;
  boards[0]->getSquare(vect2Di(32,5))->wall = false;
  boards[0]->getSquare(vect2Di(31,5))->wall = false;
  makePortalPair2( boards[0],vect2Di(31, 8), LEFT, boards[0], vect2Di(33, 6), DOWN, false);
  makePortalPair2( boards[0],vect2Di(31, 7), LEFT, boards[0], vect2Di(32, 6), DOWN, false);
  makePortalPair2( boards[0],vect2Di(31, 6), LEFT, boards[0], vect2Di(31, 6), DOWN, false);

  //createMote(vect2Di(10, 20));
  //createMote(vect2Di(10, 21));
  //createMote(vect2Di(10, 22));
  //createMote(vect2Di(11, 20));
  //createMote(vect2Di(11, 21));

  /*
  board[45][13].wall = true;
  board[20][13].wall = true;
  makePortalPair(20,12,45,12);
  makePortalPair(20,11,45,11);
  makePortalPair(20,10,45,10);
  makePortalPair(20,9,45,9);
  board[45][8].wall = true;
  board[20][8].wall = true;
  */

  //makePortalPair(vect2Di(10, 12), vect2Di(20, 12));
  //makePortalPair(vect2Di(10, 11), vect2Di(20, 11));
  //makePortalPair(vect2Di(10, 10), vect2Di(20, 10));
  
  // This should be a mirror
  makeMirror(boards[0], vect2Di(1,9), LEFT);
  makeMirror(boards[0], vect2Di(1,8), LEFT);
  makeMirror(boards[0], vect2Di(1,7), LEFT);
  makeMirror(boards[0], vect2Di(1,6), LEFT);
  
  // this should be a retro-reflector
  makePortalPair2(boards[0], vect2Di(1, 4), LEFT, boards[0], vect2Di(1, 4), LEFT, false);

  // the infinite tunnel
  makeNicePortalPair(boards[0], 20, 20, boards[0], 20, 30, 7, 0);

  // to the second board
  makeNicePortalPair(boards[0], 20, 40, boards[1], 20, 40, 7, 0);

  makeNicePortalPair(boards[0], 60, 20, boards[0], 72, 20, 5, 0);
  for (int y = 10; y < 31; y++)
  {
    boards[0]->board[60][y].wall = true;
    boards[0]->board[66][y].wall = true;
    boards[0]->board[72][y].wall = true;
    boards[0]->board[78][y].wall = true;
  }

  createPlant(boards[0], vect2Di(10, 40));
  createPlant(boards[0], vect2Di(10, 41));
  createPlant(boards[0], vect2Di(11, 41));

  createWater(boards[0], vect2Di(10, 15), 300);
}

// x is in squares to the right
// t is in turns
// phase is scaled to full circle at 1
double laserShape(double x, double t, double phase)
{
  const double WAVELENGTH = 5;
  const double PERIOD = 5;
  const double GROWTH_SCALE = 0.01;
  const double GROWTH_MAX = 2;
  const double DISTANCE_SCALE = 0.2;
  return std::sin(x/WAVELENGTH - t/PERIOD + phase*2*M_PI) * x*DISTANCE_SCALE * std::min(std::exp(t*GROWTH_SCALE)-1, GROWTH_MAX);
}

std::vector<vect2Di> naiveLaserSquares(int t, double phase)
{
  const int LASER_RANGE = SIGHT_RADIUS * 2;
  std::vector<vect2Di> laser_squares;
  vect2Di prev_laser_point = ZERO;
  laser_squares.push_back(prev_laser_point);
  for (int x = 1; x <= LASER_RANGE; x+=3)
  {
    vect2Di laser_point = vect2Di(x, std::round(laserShape(x, t, phase)));
    std::vector<vect2Di> new_points = orthogonalBresneham(prev_laser_point, laser_point);
    // append the new squares, but not the first one, that would be redundant.
    laser_squares.insert(laser_squares.end(), new_points.begin()+1, new_points.end());
    prev_laser_point = laser_point;
  }
  return laser_squares;
}

void createArrow(std::shared_ptr<Board> board, vect2Di world_pos, vect2Di direction)
{
  Square* squareptr = board->getSquare(world_pos);
  // Square must be empty
  if (squareptr == nullptr || !posIsFlyable(board, world_pos))
  {
    return;
  }
  std::shared_ptr<Entity> arrowptr = std::make_shared<Entity>(Entity::arrow(board, world_pos, direction));
  squareptr->entity = arrowptr;
  board->entities.push_back(arrowptr);
}

void createTurret(std::shared_ptr<Board> board, vect2Di world_pos, vect2Di direction)
{
  Square* squareptr = board->getSquare(world_pos);
  // Square must be empty
  if (squareptr == nullptr || !posIsWalkable(board, world_pos))
  {
    return;
  }
  std::shared_ptr<Entity> turretptr = std::make_shared<Entity>(Entity::turret(board, world_pos, direction));
  squareptr->entity = turretptr;
  board->entities.push_back(turretptr);
}

// attempt to spawn an arrow just in front of the player
void shootArrow()
{
  // first need to find the square and direction that is directly in front of the player
  vect2Di step = player_faced_direction;
  Line step_line = lineCast(player_board, player_pos, step);
  if (step_line.mappings.size() > 0)
  {
    vect2Di newpos = step_line.mappings[0].board_pos;
    std::shared_ptr<Board> newboard = step_line.mappings[0].board;
    if (posIsFlyable(newboard, newpos))
    {
      mat2Di T = transformFromStep(player_board, player_pos, step);
      createArrow(player_board, newpos, player_faced_direction * T);
    }
  }
}

// attempt to spawn a turret just in front of the player
void buildTurret()
{
  // first need to find the square and direction that is directly in front of the player
  vect2Di step = player_faced_direction;
  Line step_line = lineCast(player_board, player_pos, step);
  if (step_line.mappings.size() > 0)
  {
    vect2Di newpos = step_line.mappings[0].board_pos;
    std::shared_ptr<Board> newboard = step_line.mappings[0].board;
    if (posIsWalkable(newboard, newpos))
    {
      mat2Di T = transformFromStep(player_board, player_pos, step);
      createTurret(newboard, newpos, player_faced_direction * T);
    }
  }
}

// make the laser beams, kill motes, and mark squares as laser
void shootLaser()
{
  int t = consecutive_laser_rounds;
  const int NUM_STREAMS = 5;
  mat2Di rot_to_player_faced;
  for (int i = 0; i < player_faced_direction.ccwRotations(); i++)
  {
    rot_to_player_faced *= CCW;
  }
  for (int p = 0; p < NUM_STREAMS; p++)
  {
    double phase = static_cast<double>(p)/static_cast<double>(NUM_STREAMS+25);
    std::vector<vect2Di> naive_squares = naiveLaserSquares(t, phase);
    // adapt the naive squares to the player's location and faced direction
    for (int i = 0; i < static_cast<int>(naive_squares.size()); i++)
    {
      naive_squares[i] *= rot_to_player_faced;
      naive_squares[i] += player_pos;
    }
    Line laser_line = curveCast(player_board, naive_squares);
    // for every square of the laser
    for (int i = 0; i < static_cast<int>(laser_line.mappings.size()); i++)
    {
      std::shared_ptr<Board> board = laser_line.mappings[i].board;
      Square* squareptr = board->getSquare(laser_line.mappings[i].board_pos);
      // Lasers don't go through walls
      if (squareptr->wall == true)
      {
        break;
      }
      squareptr->fire = true;
      if (squareptr->entity.lock() != nullptr)
      {
        board->deleteEntity(squareptr->entity.lock());
      }
      if (squareptr->plant > 0)
      {
        squareptr->plant -= 1;
        // plants stop lasers
        break;
      }
    }
  }
}

// if the entity knows where the player is, face the player
void facePlayer(std::shared_ptr<Entity> entityptr)
{
  vect2Di dir = entityptr->rel_player_pos;
  if (dir != ZERO)
  {
    vect2Di newfaced;
    // if along x axis
    if (std::abs(dir.x) > std::abs(dir.y) || (std::abs(dir.x) == std::abs(dir.y) && random(0, 2) == 0)) // tiebreak random because why not
    {
      if (dir.x > 0)
      {
        newfaced = RIGHT;
      }
      else
      {
        newfaced = LEFT;
      }
    }
    else 
    {
      if (dir.y > 0)
      {
        newfaced = UP;
      }
      else
      {
        newfaced = DOWN;
      }
    }
    entityptr->faced_direction = newfaced;
  }
}

void updateEntities()
{
  std::vector<std::shared_ptr<Entity>> todelete;
  for (auto board : boards)
  {
    int i = 0;
    while (i < static_cast<int>(board->entities.size()))
    {
      std::shared_ptr<Entity> entityptr = board->entities[i];
      // face the player if can turn
      if (entityptr->homing == true)
      {
        facePlayer(entityptr);
      }
      if (entityptr->moving == true)
      {
        vect2Di step = entityptr->faced_direction;
        vect2Di newpos;
        std::shared_ptr<Board> newboard;
        std::tie(newboard, newpos) = posFromStep(entityptr->board.lock(), entityptr->pos, step);
        if (posIsFlyable(newboard, newpos))
        {
          mat2Di T = transformFromStep(entityptr->board.lock(), entityptr->pos, step);
          entityptr->faced_direction *= T;
          moveEntity(entityptr, newboard, newpos);
          if (entityptr->rel_player_pos != ZERO)
          {
            entityptr->rel_player_pos -= step;
            entityptr->rel_player_pos *= T;
          }
        }
        // if the new position is not clear, the arrow dies, and maybe does some damage
        else if (entityptr->die_on_touch)
        {
          if (newboard->getSquare(newpos)->plant > 0)
          {
            newboard->getSquare(newpos)->plant -= 1;
          }
          else if (newboard->getSquare(newpos)->wall == true)
          {
            // can't damage a wall with a simple arrow
          }
          else if (newboard->getSquare(newpos)->entity.lock() != nullptr)
          {
            todelete.push_back(newboard->getSquare(newpos)->entity.lock());
          }
          // no mater what the arrow has hit, the arrow dies
          todelete.push_back(entityptr);
        }

      }
      if (entityptr->can_shoot)
      {
        if (entityptr->cooldown > 0)
        {
          entityptr->cooldown -= 1; 
        }
        else
        {
          // raycast ahead of the entity, and if it sees another entity, shoot it and set the cooldown
          vect2Di step = entityptr->faced_direction * entityptr->detection_range;
          Line detection_line = lineCast(entityptr->board.lock(), entityptr->pos, step);
          for (SquareMap mapping : detection_line.mappings)
          {
            Square* squareptr = mapping.board->getSquare(mapping.board_pos);
            if (squareptr->wall == true)
            {
              break;
            }
            else if (squareptr->entity.lock() != nullptr || mapping.board_pos == player_pos)
            {
              // if there is space in front of the entity
              if (posIsFlyable(detection_line.mappings[0].board, detection_line.mappings[0].board_pos))
              {
                // shoot an arrow
                mat2Di T = transformFromStep(entityptr->board.lock(), entityptr->pos, entityptr->faced_direction);
                createArrow(detection_line.mappings[0].board, detection_line.mappings[0].board_pos, entityptr->faced_direction * T);
                entityptr->cooldown = entityptr->max_cooldown;
                break;
              }
            }
          }
        }
      }
      i++;
    }
  }
  for (auto entityptr : todelete)
  {
    entityptr->board.lock()->deleteEntity(entityptr);
  }
}

bool onMemoryMap(vect2Di pos)
{
  return (pos.x >= 0 &&
      pos.x < MEMORY_MAP_SIZE &&
      pos.y >= 0 &&
      pos.y < MEMORY_MAP_SIZE);
}

void shiftMemoryMap(vect2Di player_movement)
{
  // The player has just moved by player_movement, so the map shifts in the opposite direction
  // Edges are filled with spaces.
  //
  auto newmap = memory_map;
  
  // For every square of the memory map
  for (int x=0; x < MEMORY_MAP_SIZE; x++)
  {
    for (int y=0; y < MEMORY_MAP_SIZE; y++)
    {
      if (x+player_movement.x >= 0 &&
          x+player_movement.x < MEMORY_MAP_SIZE &&
          y+player_movement.y >= 0 &&
          y+player_movement.y < MEMORY_MAP_SIZE)
      {
        newmap[x][y] = memory_map[x+player_movement.x][y+player_movement.y];
      }
      else
      {
        newmap[x][y] = L" ";
      }
    }
  }
  memory_map = newmap;
}

void updateSightLines()
{
  // the order these are updated is essentially bottom to top in terms of draw order
  
  std::vector<vect2Di> rel_p;

  // The orthogonals
  
  rel_p.push_back(vect2Di(SIGHT_RADIUS, 0));

  rel_p.push_back(vect2Di(0, SIGHT_RADIUS));

  rel_p.push_back(vect2Di(-SIGHT_RADIUS, 0));

  rel_p.push_back(vect2Di(0, -SIGHT_RADIUS));

  // All the non-diagonals and non-orthogonals, moving from diagonal to orthogonal
  for(int i = 1; i < SIGHT_RADIUS; i++)
  {
    // all 8 octants
    rel_p.push_back(vect2Di(SIGHT_RADIUS, i));
    rel_p.push_back(vect2Di(i, SIGHT_RADIUS));
    rel_p.push_back(vect2Di(-i, SIGHT_RADIUS));
    rel_p.push_back(vect2Di(-SIGHT_RADIUS, i));
    rel_p.push_back(vect2Di(-SIGHT_RADIUS, -i));
    rel_p.push_back(vect2Di(-i, -SIGHT_RADIUS));
    rel_p.push_back(vect2Di(i, -SIGHT_RADIUS));
    rel_p.push_back(vect2Di(SIGHT_RADIUS, -i));
  }
  // The diagonals
  rel_p.push_back(vect2Di(SIGHT_RADIUS, SIGHT_RADIUS));
  rel_p.push_back(vect2Di(-SIGHT_RADIUS, SIGHT_RADIUS));
  rel_p.push_back(vect2Di(SIGHT_RADIUS, -SIGHT_RADIUS));
  rel_p.push_back(vect2Di(-SIGHT_RADIUS, -SIGHT_RADIUS));

  player_sight_lines.clear();
  // Find the sight lines in this order
  for(int i = 0; i < static_cast<int>(rel_p.size()); i++)
  {
    bool is_sight_line = true;
    Line new_sightline = lineCast(player_board, player_pos, rel_p[i], is_sight_line);

    player_sight_lines.push_back(new_sightline);
  }
}

// Redirect an orthogonal step between adjacent squares.
// assumes step is exactly one square orthogonal
void orthogonalRedirect(std::shared_ptr<Board> start_board, vect2Di start_pos, vect2Di step, std::shared_ptr<Board>& end_board, vect2Di& end_pos, mat2Di& portal_transform, int& portal_color)
{
  if (!start_board->onBoard(start_pos))
  {
    return;
  }
  Square square = start_board->board[start_pos.x][start_pos.y];

  std::shared_ptr<Portal> portalptr = *getPortal(square, step);

  if (portalptr == nullptr)
  {
    // Nice and simple
    end_board = start_board;
    end_pos = start_pos + step;
    portal_transform = IDENTITY;
    portal_color = COLOR_WHITE;
  }
  else
  {
    // take redirect, transform, and color from the portal
    end_pos = portalptr->new_pos;
    end_board = portalptr->new_board.lock();
    portal_transform = portalptr->transform;
    portal_color = portalptr->color;
  }
}

// overload to make the color optional
void orthogonalRedirect(std::shared_ptr<Board> start_board, vect2Di start_pos, vect2Di step, std::shared_ptr<Board>& end_board, vect2Di& end_pos, mat2Di& portal_transform)
{
  int color = COLOR_WHITE;
  return orthogonalRedirect(start_board, start_pos, step, end_board, end_pos, portal_transform, color);
}

mat2Di transformFromStep(std::shared_ptr<Board> start_board, vect2Di start_pos, vect2Di step)
{
  mat2Di transform;
  vect2Di end_pos;
  std::shared_ptr<Board> end_board;
  orthogonalRedirect(start_board, start_pos, step, end_board, end_pos, transform);
  return transform;
}

std::pair<std::shared_ptr<Board>, vect2Di> posFromStep(std::shared_ptr<Board> start_board, vect2Di start_pos, vect2Di step)
{
  mat2Di transform;
  vect2Di end_pos;
  std::shared_ptr<Board> end_board;
  orthogonalRedirect(start_board, start_pos, step, end_board, end_pos, transform);
  return std::make_pair(end_board, end_pos);
}

Line curveCast(std::shared_ptr<Board> start_board, std::vector<vect2Di> naive_squares, bool is_sight_line)
{
  Line line;

  // This is the rotation and flips of portals travelled to.  Apply to each relative step.
  // a 2x2 matrix of ints.
  // New transforms are multiplied onto the right.
  mat2Di cumulative_transform = IDENTITY;

  int current_color = COLOR_WHITE; // As a sight line goes through a portal, if that portal has a non-white or non-black color, that color overwrites the old one (may have fancier interactions later)
  vect2Di current_pos = naive_squares[0];
  vect2Di next_pos;

  std::shared_ptr<Board> current_board = start_board;
  std::shared_ptr<Board> next_board;

  // Sight lines don't include the starting square.  They do include the ending square.
  for(int step_num = 1; step_num < static_cast<int>(naive_squares.size()); step_num++)
  {
    vect2Di naive_step = naive_squares[step_num] - naive_squares[step_num-1];
    // This takes into account rotations and flipping caused by portals
    vect2Di transformed_naive_step = naive_step * cumulative_transform;

    mat2Di portal_transform;
    int portal_color = COLOR_WHITE; // white means no change
    if (!PORTALS_OFF)
    { 
      orthogonalRedirect(current_board, current_pos, transformed_naive_step, next_board, next_pos, portal_transform, portal_color);
    }
    if(portal_color != COLOR_WHITE)
    {
      current_color = portal_color;
    }

    cumulative_transform *= portal_transform;


    // If the portal has sent us off a board, stop
    if (!next_board->onBoard(next_pos))
    {
      break;
    }

    SquareMap square_map;

    square_map.board = next_board;
    square_map.board_pos = next_pos;

    square_map.line_pos = naive_squares[step_num] - naive_squares[0];

    square_map.transform = cumulative_transform;

    square_map.color = current_color;

    line.mappings.push_back(square_map);

    // sight lines don't need to go past the first block
    if (is_sight_line)
    {
      Square* new_square = next_board->getSquare(next_pos);
      // if there is a mote here, it wants to go to the source of the sight line
      if (new_square->entity.lock() != nullptr)
      {
        new_square->entity.lock()->rel_player_pos = naive_squares[0] - naive_squares[step_num];
      }

      // Walls, plants, and steam all block sight
      if (new_square->wall == true ||
          new_square->plant > 0 ||
          new_square->steam > 0 )
      {
        break;
      }
    }
    current_pos = next_pos;
    current_board = next_board;
  }
  return line;
}

Line lineCast(std::shared_ptr<Board> board, vect2Di start_board_pos, vect2Di rel_pos, bool is_sight_line)
{
  std::vector<vect2Di> naive_line = orthogonalBresneham(start_board_pos, start_board_pos + rel_pos);
  return curveCast(board, naive_line, is_sight_line);
}

void drawLine(Line line)
{
  for (int i = 0; i < static_cast<int>(line.mappings.size()); i++)
  {
    vect2Di board_pos = line.mappings[i].board_pos;
    vect2Di line_pos = line.mappings[i].line_pos;
    vect2Di prev_line_pos;
    if (i != 0)
    {
      prev_line_pos = line.mappings[i-1].line_pos;
    }
    else
    {
      prev_line_pos = vect2Di(0, 0);
    }
    int row, col;
    naiveBoardToScreen(board_pos, row, col);
    if(onScreen(row, col))
    {
      wchar_t glyph;
      vect2Di dpos = line_pos - prev_line_pos;
      if (dpos.x == 0)
        glyph = '|';
      else 
      {
        double slope = static_cast<double>(dpos.y)/static_cast<double>(dpos.x);
        if (slope > 2)
          glyph = '|';
        else if (slope > 1.0/2.0)
          glyph = '/';
        else if (slope > -1.0/2.0)
          glyph = '-';
        else if (slope > -2)
          glyph = '\\';
        else
          glyph = '|';
      }

      const wchar_t* glyphptr = &glyph;
      attron(COLOR_PAIR(RED_ON_BLACK));
      mvaddwstr(row, col, glyphptr);
      attroff(COLOR_PAIR(RED_ON_BLACK));
    }
  }
}

void drawSightMap()
{
  // Draw the memory map
  attron(COLOR_PAIR(getColorPairIndex(COLOR_BLUE, COLOR_BLACK)));
  for (int row = 0; row < num_rows; row++)
  {
    for (int col = 0; col < num_cols; col++)
    {
      vect2Di memmappos;
      screenToMemoryMap(row, col, memmappos);
      mvaddwstr(row, col, memory_map[memmappos.x][memmappos.y]);
    }
  }
  attroff(COLOR_PAIR(getColorPairIndex(COLOR_BLUE, COLOR_BLACK)));
  // Draw the player at the center of the sightmap
  int row, col;
  sightMapToScreen(vect2Di(0, 0), row, col);
  mvaddwstr(row, col, L"@");
  
  // For every sight line
  for(int line_num = 0; line_num < static_cast<int>(player_sight_lines.size()); line_num++)
  {
    Line line = player_sight_lines[line_num];
    // For every square on that line of sight
    for(int square_num = 0; square_num < static_cast<int>(line.mappings.size()); square_num++)
    {
      SquareMap mapping = line.mappings[square_num];
      Square* board_square = mapping.board->getSquare(mapping.board_pos);
      int row, col;
      sightMapToScreen(mapping.line_pos, row, col);
      int forground_color = COLOR_WHITE;
      int background_color = COLOR_BLACK;
      const wchar_t* glyph;
      // if player, show the player.
      if (mapping.board_pos == player_pos)
      {
        glyph = L"@";
      }
      else if (board_square->wall == true)
      {
        background_color = COLOR_BLACK;
        forground_color = COLOR_WHITE;
        glyph = WALL_GLYPH;
      }
      else if (board_square->steam > 0)
      {
        glyph = STEAM_GLYPH;
      }
      else if (board_square->entity.lock() != nullptr)
      {
        int ccw_rotations_from_right = ((board_square->entity.lock()->faced_direction * mapping.transform.inversed()).ccwRotations() + player_transform.inversed().ccwRotations())%4;
        // if we are dealing with a mote
        if (board_square->entity.lock()->homing == true)
        {

          // draw mote
          // Need to account for rotation of the entity, portals, and the player
          glyph = MOTE_GLYPHS[ccw_rotations_from_right];
        }
        else if (board_square->entity.lock()->can_shoot == true) // if we're dealing with a turret
        {
          forground_color = COLOR_BLACK;
          background_color = COLOR_WHITE;
          glyph = TURRET_GLYPHS[ccw_rotations_from_right];
        }
        else // if we are dealing with an arrow
        {
          // draw arrow
          glyph = ARROW_GLYPHS[ccw_rotations_from_right];
        }
      }
      else if (board_square->water > 0)
      {
        glyph = WATER_GLYPH;
        if (board_square->water <= SHALLOW_WATER_DEPTH)
        {
          forground_color = COLOR_CYAN;
        }
        else
        {
          forground_color = COLOR_BLUE;
        }

        if (board_square->plant > 0)
        {
          forground_color = COLOR_BLACK;
          glyph = PLANT_GLYPH;
        }
      }
      else if (board_square->plant > 0)
      {
        forground_color = COLOR_GREEN;
        glyph = PLANT_GLYPH;
      }
      else
      {
        forground_color = board_square->grass_color;
        glyph = board_square->grass_glyph;
      }


      if (board_square->fire == true)
      {
        background_color = COLOR_RED;
      }

      // if the sight line is tinted by a portal, apply the color modifications here (for now at least)
      if(mapping.color != COLOR_WHITE && mapping.color != COLOR_BLACK)
      {
        if(forground_color != COLOR_BLACK)
        {
          forground_color = mapping.color;
        }
        if(background_color != COLOR_BLACK)
        {
          background_color = mapping.color;
        }
      }

      // actually draw the thing
      attron(COLOR_PAIR(getColorPairIndex(forground_color, background_color)));
      mvaddwstr(row, col, glyph);
      attroff(COLOR_PAIR(getColorPairIndex(forground_color, background_color)));
      // Put the drawn glyph on the memory map
      vect2Di memmappos;
      screenToMemoryMap(row, col, memmappos);
      if (onMemoryMap(memmappos))
      {
        memory_map[memmappos.x][memmappos.y] = glyph;
      }
      // if we just drew a wall, done with this line
      if (board_square->wall == true)
        break;
    }
  }
  // where the player is facing
  const wchar_t* aiming_indicator;
  vect2Di rel_faced_direction = player_faced_direction * player_transform.inversed();
  if (rel_faced_direction == DOWN)
  {
    aiming_indicator = L"↓";
  }
  else if (rel_faced_direction == UP)
  {
    aiming_indicator = L"↑";
  }
  else if (rel_faced_direction == LEFT)
  {
    aiming_indicator = L"←";
  }
  else if (rel_faced_direction == RIGHT)
  {
    aiming_indicator = L"→";
  }
  sightMapToScreen(player_faced_direction, row, col);
  mvaddwstr(row, col, aiming_indicator);
}

void drawBoard()
{
  // Draw all the floor and walls
  // For every square on the screen
  
  for (int row = 0; row < num_rows; row++)
  {
    for (int col = 0; col < num_cols; col++)
    {
      vect2Di pos;
      screenToBoard(row, col, pos);
      wchar_t glyph;
      int color = 0;
      if (!player_board->onBoard(pos))
        glyph = '.';
      else if (player_board->board[pos.x][pos.y].wall == true)
      {
        glyph = ' ';
        color = 2;
      }
      else if (player_board->board[pos.x][pos.y].entity.lock() != nullptr)
      {
        color = BLACK_ON_WHITE; 
        glyph = '*';
      }
      else
        glyph = ' ';
      attron(COLOR_PAIR(color));
      mvaddch(row, col, glyph);
      attroff(COLOR_PAIR(color));
    }
  }
  // Draw the player
  int row, col;
  naiveBoardToScreen(player_pos, row, col);
  if (onScreen(row, col))
  {
    mvaddch(row, col, '@');
  }

  for(int i = 0; i < static_cast<int>(player_sight_lines.size()); i++)
  {
    //if (i%4 ==1)
      drawLine(player_sight_lines[i]);
  }
}

void drawEverything()
{
  if (NAIVE_VIEW)
  {
    drawBoard();
  }
  else
  {
    drawSightMap();
  }

  // move the cursor
  move(0,0);
  refresh();
}

void updateSteam()
{
  for (auto board : boards)
  {
    // each flow is a bunch of steam moving from the first of the tuple to the second.
    // The third element is the magnitude of the flow
    std::vector<
      std::tuple<
        std::pair<std::shared_ptr<Board>, vect2Di>,
        std::pair<std::shared_ptr<Board>, vect2Di>, 
        int
        >
      > flows;
    // for every square on the board
    for (int x=0; x < board->board_size; x++)
    {
      for (int y=0; y < board->board_size; y++)
      {
        vect2Di thispos = vect2Di(x, y);
        Square* thissquare = board->getSquare(thispos);
        // if this square has steam and fire, there is no more fire
        if(thissquare->steam > 0 && thissquare->fire == true)
        {
          thissquare->fire = false;
        }
        // if this square only has 1 steam, the steam fades away to nothing
        if(thissquare->steam == 1)
        {
          thissquare->steam = 0;
        }
        // if this square has enough steam to possibly flow elsewhere
        if(thissquare->steam > 1)
        {
          std::vector<std::pair<std::shared_ptr<Board>, vect2Di>> downhills;
          // check every adjacent square
          for (vect2Di dir : ORTHOGONALS)
          {
            std::pair<std::shared_ptr<Board>, vect2Di> adjloc = posFromStep(board, thispos, dir);
            auto adjboard = adjloc.first;
            auto adjpos = adjloc.second;
            auto adjsquare = adjboard->getSquare(adjpos);
            // if there can be a flow from here to there
            if (adjsquare && 
                adjsquare->wall==false &&
                adjsquare->steam <= thissquare->steam-2)

            {
              downhills.push_back(std::make_pair(adjboard, adjpos));
            }
          }
          // Now look through the adjacent squares that have less steam, and find out how much steam this square has to give to the other squares for all the squares to have the same amount of steam.
          int totalSteam = thissquare->steam;
          for (std::pair<std::shared_ptr<Board>, vect2Di> downhillloc : downhills)
          {
            auto adjboard = downhillloc.first;
            auto adjpos = downhillloc.second;
            totalSteam += adjboard->getSquare(adjpos)->steam;
          }
          int avgSteam = totalSteam / (1 + downhills.size()); 
          int extrasteam = totalSteam - (avgSteam * (1+downhills.size())); // TODO: make this not be.
          // extrasteam can be 1, 2, or 3.  We don't need to do anything if it's 1.
          extrasteam -=1;
          // shuffle the downhills to prevent direction bias of distribution of extrasteams 
          std::random_shuffle(downhills.begin(), downhills.end());
          for (std::pair<std::shared_ptr<Board>, vect2Di> downhillloc : downhills)
          {
            auto adjboard = downhillloc.first;
            auto adjpos = downhillloc.second;
            int magnitude = avgSteam - adjboard->getSquare(adjpos)->steam;
            if (extrasteam > 0)
            {
              magnitude += 1;
              extrasteam -= 1;
            }
            flows.push_back(std::make_tuple(
                  std::make_pair(board, thispos), 
                  std::make_pair(adjboard, adjpos),
                  magnitude
                  ));
          }
        }
      }
    }
    // randomize the order of attempted flows to prevent directional bias
    std::random_shuffle(flows.begin(), flows.end());
    // actually flow the steam
    // REMINDER: the tuple is (absoluteSourcePosition, absoluteEndPosition, flowMagnitude)
    for (std::tuple<std::pair<std::shared_ptr<Board>, vect2Di>, std::pair<std::shared_ptr<Board>, vect2Di>, int> flowtuple : flows)
    {
      // unpack this abomination of a tuple
      std::pair<std::shared_ptr<Board>, vect2Di> start_loc = std::get<0>(flowtuple);
      auto start_board = start_loc.first;
      auto start_pos = start_loc.second;
      auto start_square = start_board->getSquare(start_pos);

      std::pair<std::shared_ptr<Board>, vect2Di> end_loc = std::get<1>(flowtuple);
      auto end_board = end_loc.first;
      auto end_pos = end_loc.second;
      auto end_square = end_board->getSquare(end_pos);

      int magnitude = std::get<2>(flowtuple);

      // if there is still enough of a steam difference to allow a flow
      if (start_square->steam > end_square->steam+1)
      {
        // Reduce the flow magnitude if we need to
        if (end_square->steam + magnitude > start_square->steam - magnitude)
        {
          magnitude = (start_square->steam + end_square->steam)/2 - end_square->steam;
        }
        start_square->steam -= magnitude;
        end_square->steam += magnitude;
      }
    }
  }
}

// Flow water
void updateWater()
{
  for (auto board : boards)
  {
    // First check for water->steam from fire
    // for every square on the board
    for (int x=0; x < board->board_size; x++)
    {
      for (int y=0; y < board->board_size; y++)
      {
        vect2Di thispos = vect2Di(x, y);
        auto thissquare = board->getSquare(thispos);
        // if this square has water and fire, water turns into steam (the steam takes care of putting out fires)
        if(thissquare->water > 0 && thissquare->fire==true)
        {
          thissquare->water  -= 1;
          thissquare->steam  += STEAM_PER_WATER;
        }
      }
    }

    // each flow is one water moving from the first of the tuple to the second.
    // The third element is the relative direction of the flow from the first square
    std::vector<std::tuple<std::pair<std::shared_ptr<Board>, vect2Di>,std::pair<std::shared_ptr<Board>, vect2Di>, vect2Di>> flows;
    // for every square on the board
    for (int x=0; x < board->board_size; x++)
    {
      for (int y=0; y < board->board_size; y++)
      {
        vect2Di thispos = vect2Di(x, y);
        Square* thissquare = board->getSquare(thispos);
        // if this square has water deeper than 1
        if(thissquare->water > 1)
        {
          // check every adjacent square
          for (vect2Di dir : ORTHOGONALS)
          {
            std::shared_ptr<Board> adjboard;
            vect2Di adjpos; 
            std::tie(adjboard, adjpos) = posFromStep(board, thispos, dir);
            Square* adjsquare = adjboard->getSquare(adjpos);
            // if there can be a flow from here to there
            // TODO: different flow rules for shallow vs deep water?
            if (adjsquare != nullptr && 
                adjsquare->wall==false &&
                adjsquare->plant==false &&
                adjsquare->water <= thissquare->water-2)

            {
              if (random(0, (AVG_WATER_FLOW_TIME-1) * 2) == 0)
              {
                flows.push_back(std::make_tuple(
                      std::make_pair(board, thispos), 
                      std::make_pair(adjboard, adjpos), 
                      dir
                      ));
              }
            }
          }
        }
      }
    }
    // randomize the order of attempted flows to prevent directional bias
    std::random_shuffle(flows.begin(), flows.end());
    // actually flow the water the plants in the selected locations
    // REMINDER: the tuple is (absoluteSourcePosition, absoluteEndPosition, relativeDirectionOfFlowFromTheSourceSquare)
    for (std::tuple<std::pair<std::shared_ptr<Board>, vect2Di>,std::pair<std::shared_ptr<Board>, vect2Di>, vect2Di> flowtuple : flows)
    {
      // unpack the tuple
      vect2Di start_pos, end_pos, flow_dir;
      std::shared_ptr<Board> start_board, end_board;
      std::pair<std::shared_ptr<Board>, vect2Di> start_loc, end_loc;

      // Can't nest "tie" :-(
      std::tie(start_loc, end_loc, flow_dir) = flowtuple;
      std::tie(start_board, start_pos) = start_loc;
      std::tie(end_board, end_pos) = end_loc;

      Square* start_square = start_board->getSquare(start_pos);
      Square* end_square = end_board->getSquare(end_pos);

      // if there is still enough of a water difference to allow a flow
      if (start_square->water > end_square->water+1)
      {
        start_square->water -= 1;
        end_square->water += 1;
        // Also push the player if the player is there
        if (start_pos == player_pos)
        {
          attemptMove(flow_dir, false);
        }
      }
    }
  }
}

// fire spreading and damaging plants
void updateFire()
{
  std::vector<std::pair<std::shared_ptr<Board>, vect2Di>> newFires;
  for (auto board : boards)
  {
    // for every square on the board
    for (int x=0; x < board->board_size; x++)
    {
      for (int y=0; y < board->board_size; y++)
      {
        vect2Di thispos = vect2Di(x, y);
        Square* thissquare = board->getSquare(thispos);
        // if this square has a fire
        if(thissquare->fire == true)
        {
          // apply damage to the current plant, maybe destroying it and putting out the fire
          if (thissquare->plant > 0)
          {
            thissquare->plant -=1;
          }
          // Fire without fuel can't spread
          if (thissquare->plant == 0)
          {
            thissquare->fire = false;
          }
          else 
          {
            // check every adjacent square
            for (vect2Di dir : ORTHOGONALS)
            {
              vect2Di adjpos; 
              std::shared_ptr<Board> adjboard;
              std::tie(adjboard, adjpos) = posFromStep(board, thispos, dir);
              Square* adjsquare = adjboard->getSquare(adjpos);
              // if the space has no fire, the fire may spread
              if (adjsquare != nullptr && 
                  adjsquare->wall == false && 
                  adjsquare->fire == false)
              {
                if (random(0, (AVG_FIRE_SPREAD_TIME-1) * 2) == 0)
                {
                  newFires.push_back(std::make_pair(adjboard, adjpos));
                }
              }
            }
          }
        }
      }
    }
  }
  // actually spawn the fires in the selected locations
  for (auto loc : newFires)
  {
    loc.first->getSquare(loc.second)->fire = true;
  }
}

// go through all the plants, and grow new ones or kill off old ones as rules dictate
// For now, simple expansion
void updatePlants()
{
  std::vector<std::pair<std::shared_ptr<Board>, vect2Di>> whereToSpawnPlants;
  for (auto board : boards)
  {
    // for every square on the board
    for (int x=0; x < board->board_size; x++)
    {
      for (int y=0; y < board->board_size; y++)
      {
        vect2Di thispos = vect2Di(x, y);
        Square* thissquare = board->getSquare(thispos);
        // if this square has a plant THAT IS NOT ON FIRE
        if(thissquare->plant != 0 && thissquare->fire == false)
        {
          // check every adjacent square
          for (vect2Di dir : ORTHOGONALS)
          {
            vect2Di adjpos;
            std::shared_ptr<Board> adjboard;
            std::tie(adjboard, adjpos) = posFromStep(board, thispos, dir);
            // if the space is empty
            if (posIsWalkable(adjboard, adjpos))
            {
              if (random(0, (AVG_PLANT_SPAWN_TIME-1) * 2) == 0)
              {
                whereToSpawnPlants.push_back(std::make_pair(adjboard, adjpos));
              }
            }
          }
        }
      }
    }
  }
  // actually spawn the plants in the selected locations
  for (auto loc : whereToSpawnPlants)
  {
    // need to check this because we don't want to try to double spawn a plant (a square with 2 adjacent plants has 2 chances to spawn)
    if (posIsWalkable(loc.first, loc.second))
    {
      createPlant(loc.first, loc.second);
    }
  }
}

int main()
{
  setlocale(LC_ALL, "");
  initNCurses();

  initWorld();

  while(true)
  {
    bool laser_fired = false;
    // Get input
    int in = getch();
    // Process input
    if (in == 'q')
      break;
    else if (in == ' ')
    {
      laser_fired = true;
    }
    else if (in == 'f')
    {
      shootArrow();
    }
    else if (in == 'b')
    {
      buildTurret();
    }
    else if (in == 'h')
      attemptMove(vect2Di(-1, 0)*player_transform);
    else if (in == 'j')
      attemptMove(vect2Di(0, -1)*player_transform);
    else if (in == 'k')
      attemptMove(vect2Di(0, 1)*player_transform);
    else if (in == 'l')
      attemptMove(vect2Di(1, 0)*player_transform);
    /*
    else if (in == 'y')
      attemptMove(vect2Di(-1, 1));
    else if (in == 'u')
      attemptMove(vect2Di(1, 1));
    else if (in == 'b')
      attemptMove(vect2Di(-1, -1));
    else if (in == 'n')
      attemptMove(vect2Di(1, -1));
      */
    if (!laser_fired)
    {
      consecutive_laser_rounds = 0;
    }
    else
    {
      consecutive_laser_rounds++;
    }

    // Tick everything
    updateFire();
    if (laser_fired)
    {
      shootLaser();
    }
    updatePlants();
    updateWater();
    updateSteam();
    updateSightLines();
    updateEntities();
      
    // draw things
    drawEverything();
  }

  endwin(); // End curses
  return 0;
}

