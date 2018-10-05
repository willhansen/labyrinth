#include "square.h"
#include "line.h"
#include "portal.h"
#include "mote.h"
#include "geometry.h"

#include <ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <vector>
#include <utility>
#include <cmath>
#include <algorithm>

const int BOARD_SIZE = 100;
const int SIGHT_RADIUS = 50; 
const bool NAIVE_VIEW = false;
const bool PORTALS_OFF = false;

const int WHITE_ON_BLACK = 0;
const int RED_ON_BLACK = 1;
const int BLACK_ON_WHITE = 2;
const int BLACK_ON_RED = 3;

const int PLANT_MAX_HEALTH = 10;
const char PLANT_GLYPH = 'E';
const int AVG_PLANT_SPAWN_TIME = 20;
const int AVG_FIRE_SPREAD_TIME = 2;

const int SHALLOW_WATER_DEPTH = 3;
const int AVG_WATER_FLOW_TIME = 1;

const int BACKGROUND_COLOR = WHITE_ON_BLACK;
const char OUT_OF_VIEW = ' ';

const std::vector<char> GRASS_GLYPHS = {' ', ' ', ' ', '.', '\'', ',', '`'};
const std::vector<int> GRASS_COLORS = {COLOR_YELLOW, COLOR_YELLOW, COLOR_GREEN};

Line curveCast(std::vector<vect2Di> naive_squares, bool is_sight_line=false);
void drawEverything();
void updateSightLines();
Line lineCast(vect2Di start_pos, vect2Di d_pos, bool is_sight_line=false);
void initNCurses();
mat2Di transformFromStep(vect2Di start_pos, vect2Di step);
Square* getSquare(vect2Di pos);

//TODO: make these non-global
std::vector<std::shared_ptr<Mote>> motes;
std::vector<std::vector<Square>> board(BOARD_SIZE, std::vector<Square>(BOARD_SIZE));
std::vector<std::vector<Square>> sight_map(SIGHT_RADIUS*2+1, std::vector<Square>(SIGHT_RADIUS*2+1));
std::vector<Line> player_sight_lines;
std::vector<Line> lasers;
vect2Di player_pos;
int consecutive_laser_rounds = 0;
vect2Di player_faced_direction = RIGHT;
// This is visual only, its a transform for drawing to the screen and changing the direction of movement inputs.
mat2Di player_transform;
int mouse_x, mouse_y;
vect2Di mouse_pos;

int num_rows,num_cols;				/* to store the number of rows and */

bool onBoard(vect2Di p)
{
  return (p.x>=0 && p.y>=0 && p.x<BOARD_SIZE && p.y<BOARD_SIZE);
}

bool onScreen(int row, int col)
{
  return (row>=0 && col>=0 && row<num_rows && col<num_cols);
}

bool posIsEmpty(vect2Di pos)
{
  Square* squareptr = getSquare(pos);
  // Square must be empty and also actually be there
  if (squareptr == nullptr || 
      squareptr->mote.lock() != nullptr || 
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

int getColorPairIndex(int forground, int background)
{
  return forground * COLORS + background;
}

void attemptMove(vect2Di dp)
{
  player_faced_direction = dp;
  Line line = lineCast(player_pos, dp);
  if (line.mappings.size() > 0)
  {
    vect2Di pos = line.mappings[0].board_pos;
    if (posIsEmpty(pos))
    {
      player_transform *= transformFromStep(player_pos, dp);
      player_faced_direction *= transformFromStep(player_pos, dp);
      player_pos = pos;
    }
  }
}

void createMote(vect2Di pos)
{
  Square* squareptr = getSquare(pos);
  // Square must be empty
  if (squareptr == nullptr || !posIsEmpty(pos))
  {
    return;
  }
  std::shared_ptr<Mote> moteptr = std::make_shared<Mote>();
  moteptr->pos = pos;
  moteptr->rel_player_pos = vect2Di(0, 0);
  squareptr->mote = moteptr;
  motes.push_back(moteptr);
}

void createPlant(vect2Di pos)
{
  Square* squareptr = getSquare(pos);
  // Square must be empty
  if (squareptr == nullptr || !posIsEmpty(pos))
  {
    return;
  }
  squareptr->plant = PLANT_MAX_HEALTH;
}

void createWater(vect2Di pos, int depth)
{
  Square* squareptr = getSquare(pos);
  // Square must be empty
  if (!posIsEmpty(pos))
  {
    return;
  }
  squareptr->water = depth;
}

void deleteMote(std::shared_ptr<Mote> moteptr)
{
  // If 2 shared pointers are equal if they point to the same mote, this should work
  motes.erase(std::remove(motes.begin(), motes.end(), moteptr), motes.end());
}


void setMotePos(std::shared_ptr<Mote> moteptr, vect2Di new_pos)
{
  Square* newSquareptr = getSquare(new_pos);
  // Square must be empty
  if (newSquareptr == nullptr || newSquareptr->mote.lock() != nullptr || newSquareptr->wall != false || new_pos == player_pos)
  {
    return;
  }

  // this square exists
  Square* oldSquareptr = getSquare(moteptr->pos);
  newSquareptr->mote = oldSquareptr->mote;
  oldSquareptr->mote.reset();

  moteptr->pos = new_pos;
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

void tickMote(std::shared_ptr<Mote> moteptr)
{
  // The mote wants to move towards the player
  // But only if it has a destination
  if (moteptr->rel_player_pos != ZERO)
  {
    vect2Di step = firstStepInDirection(moteptr->rel_player_pos);
    Line step_line = lineCast(moteptr->pos, step);
    if (step_line.mappings.size() > 0)
    {
      vect2Di pos = step_line.mappings[0].board_pos;
      if (posIsEmpty(pos))
      {
        moteptr->rel_player_pos -= step;
        moteptr->rel_player_pos *= transformFromStep(moteptr->pos, step);
        setMotePos(moteptr, pos);
      }
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

void makePortalPair(vect2Di p1, vect2Di p2, bool left=true)
{
  if (!onBoard(p1) || !onBoard(p2))
    return;
  if (left)
  {
    board[p1.x][p1.y].left_portal.reset(new Portal());
    board[p1.x][p1.y].left_portal->new_pos = p2+LEFT;

    board[p1.x-1][p1.y].right_portal.reset(new Portal());
    board[p1.x-1][p1.y].right_portal->new_pos = p2;

    board[p2.x][p2.y].left_portal.reset(new Portal());
    board[p2.x][p2.y].left_portal->new_pos = p1+LEFT;

    board[p2.x-1][p2.y].right_portal.reset(new Portal());
    board[p2.x-1][p2.y].right_portal->new_pos = p1;
  }
  else
  {
    board[p1.x][p1.y].down_portal.reset(new Portal());
    board[p1.x][p1.y].down_portal->new_pos = p2+DOWN;

    board[p1.x][p1.y-1].up_portal.reset(new Portal());
    board[p1.x][p1.y-1].up_portal->new_pos = p2;

    board[p2.x][p2.y].down_portal.reset(new Portal());
    board[p2.x][p2.y].down_portal->new_pos = p1+DOWN;

    board[p2.x][p2.y-1].up_portal.reset(new Portal());
    board[p2.x][p2.y-1].up_portal->new_pos = p1;
  }
}


void makeOneWayPortalPair(vect2Di pos1, vect2Di step1, vect2Di pos2, vect2Di step2, bool flip)
{
  Square* square1 = getSquare(pos1);
  Square* square2 = getSquare(pos2);
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

void makePortalPair2(vect2Di pos1, vect2Di step1, vect2Di pos2, vect2Di step2, bool flip)
{
  makeOneWayPortalPair(pos1, step1, pos2, step2, flip);
  makeOneWayPortalPair(pos1+step1, -step1, pos2+step2, -step2, flip);
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

void makeNicePortalPair(int x1, int y1, int x2, int y2, int dx, int dy)
{
  board[x1][y1].wall = true;
  board[x2][y2].wall = true;
  board[x1+dx+1][y1+dy].wall = true;
  board[x2+dx+1][y2+dy].wall = true;
  for (int rx = 1; rx <= dx; rx++)
  {
    makePortalPair(vect2Di(x1+rx,y1),vect2Di(x2+rx,y2), false);
  }

}

void makeMirror(vect2Di square, vect2Di step)
{
  makePortalPair2(square, step, square, step, true);
}

int random(int min, int max) //range : [min, max)
{
   static bool first = true;
   if (first) 
   {  
      srand( time(NULL) ); //seeding for the first time only!
      first = false;
   }
   return min + rand() % (( max ) - min);
}

void initBoard()
{
  player_pos = vect2Di(5, 5);

  // pick random grass glyphs and colors for every tile
  for (int x=0; x < BOARD_SIZE; x++)
  {
    for (int y=0; y < BOARD_SIZE; y++)
    {
      getSquare(vect2Di(x, y))->grass_glyph = GRASS_GLYPHS[random(0, GRASS_GLYPHS.size())];
      getSquare(vect2Di(x, y))->grass_color = GRASS_COLORS[random(0, GRASS_COLORS.size())];
    }
  }
  
  rectToWall(0, 0, BOARD_SIZE-1, BOARD_SIZE-1);
  rectToWall(30, 5, 50, 20);

  //rectToWall(20,10,22,12);
  getSquare(vect2Di(20,12))->wall = true;
  getSquare(vect2Di(20,10))->wall = true;
  getSquare(vect2Di(22,10))->wall = true;
  makePortalPair2(vect2Di(20, 11), RIGHT, vect2Di(21, 10), UP, false);

  // corner portal on frame of square
  getSquare(vect2Di(30,8))->wall = false;
  getSquare(vect2Di(30,7))->wall = false;
  getSquare(vect2Di(30,6))->wall = false;
  getSquare(vect2Di(33,5))->wall = false;
  getSquare(vect2Di(32,5))->wall = false;
  getSquare(vect2Di(31,5))->wall = false;
  makePortalPair2(vect2Di(31, 8), LEFT, vect2Di(33, 6), DOWN, false);
  makePortalPair2(vect2Di(31, 7), LEFT, vect2Di(32, 6), DOWN, false);
  makePortalPair2(vect2Di(31, 6), LEFT, vect2Di(31, 6), DOWN, false);

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
  makeMirror(vect2Di(1,9), LEFT);
  makeMirror(vect2Di(1,8), LEFT);
  makeMirror(vect2Di(1,7), LEFT);
  makeMirror(vect2Di(1,6), LEFT);
  
  // this should be a retro-reflector
  makePortalPair2(vect2Di(1, 4), LEFT, vect2Di(1, 4), LEFT, false);

  makeNicePortalPair(20, 20, 20, 30, 7, 0);

  makeNicePortalPair(60, 20, 72, 20, 5, 0);
  for (int y = 10; y < 31; y++)
  {
    board[60][y].wall = true;
    board[66][y].wall = true;
    board[72][y].wall = true;
    board[78][y].wall = true;
  }

  createPlant(vect2Di(10, 40));
  createPlant(vect2Di(10, 41));
  createPlant(vect2Di(11, 41));

  createWater(vect2Di(10, 15), 300);
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
    Line laser_line = curveCast(naive_squares);
    // for every square of the laser
    for (int i = 0; i < static_cast<int>(laser_line.mappings.size()); i++)
    {
      Square* squareptr = getSquare(laser_line.mappings[i].board_pos);
      // Lasers don't go through walls
      if (squareptr->wall == true)
      {
        break;
      }
      squareptr->laser = true;
      if (squareptr->mote.lock() != nullptr)
      {
        deleteMote(squareptr->mote.lock());
      }
      if (squareptr->plant > 0)
      {
        squareptr->plant -= 1;
        squareptr->fire = true;
        break;
      }
    }
    lasers.push_back(laser_line);
    
  }
  // remove null pointers from the mote list
  motes.erase(std::remove(motes.begin(), motes.end(), nullptr), motes.end());
}

// remove the laser flag from all squares, and remove the laser beams
void cleanUpLaser()
{
  for (int i = 0; i < static_cast<int>(lasers.size()); i++)
  {
    Line laser = lasers[i];
    for (int j = 0; j < static_cast<int>(laser.mappings.size()); j++)
    {
      getSquare(laser.mappings[j].board_pos)->laser = false;
    }
  }
  lasers.clear();
}

void updateMotes()
{
  for (auto moteptr : motes)
  {
    tickMote(moteptr);
  }
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
    Line new_sightline = lineCast(player_pos, rel_p[i], is_sight_line);

    player_sight_lines.push_back(new_sightline);
  }
}

// Redirect an orthogonal step between adjacent squares.
// assumes step is exactly one square orthogonal
void orthogonalRedirect(vect2Di start_pos, vect2Di step, vect2Di& portal_offset, mat2Di& portal_transform, int& portal_color)
{
  vect2Di naive_end_pos = start_pos + step;

  if (!onBoard(start_pos))
  {
    return;
  }
  Square square = board[start_pos.x][start_pos.y];

  std::shared_ptr<Portal> portalptr = *getPortal(square, step);

  if (portalptr == nullptr)
  {
    // no offset to be had, nothing left to do
    return;
  }
  else
  {
    portal_offset = portalptr->new_pos - naive_end_pos;
    portal_transform = portalptr->transform;
    portal_color = portalptr->color;
  }
}
// overload to make the color optional
void orthogonalRedirect(vect2Di start_pos, vect2Di step, vect2Di& portal_offset, mat2Di& portal_transform)
{
  int color = COLOR_WHITE;
  return orthogonalRedirect(start_pos, step, portal_offset, portal_transform, color);
}

mat2Di transformFromStep(vect2Di start_pos, vect2Di step)
{
  mat2Di transform;
  vect2Di offset;
  orthogonalRedirect(start_pos, step, offset, transform);
  return transform;
}

vect2Di posFromStep(vect2Di start_pos, vect2Di step)
{
  mat2Di transform;
  vect2Di offset;
  orthogonalRedirect(start_pos, step, offset, transform);
  return start_pos + step + offset;
}

Line curveCast(std::vector<vect2Di> naive_squares, bool is_sight_line)
{
  Line line;

  // These represent how much the line has been teleported when going through a portal.
  // Take the real position, subtract the translation offset, then the rotation offset, and you have the line position.
  vect2Di offset;
  // This is the rotation and flips of portals travelled to.  Apply to each relative step.
  // a 2x2 matrix of ints.
  // New transforms are multiplied onto the right.
  mat2Di transform;

  int color = COLOR_WHITE; // As a sight line goes through a portal, if that portal has a non-white or non-black color, that color overwrites the old one (may have fancier interactions later)
  vect2Di board_pos = naive_squares[0];

  // Sight lines don't include the starting square.  They do include the ending square.
  for(int step_num = 1; step_num < static_cast<int>(naive_squares.size()); step_num++)
  {
    vect2Di naive_step = naive_squares[step_num] - naive_squares[step_num-1];
    // This takes into account rotations and flipping caused by portals
    vect2Di transformed_naive_step = naive_step * transform;
    // This does not yet account for a portal this next step may step through
    vect2Di naive_next_board_pos = board_pos + transformed_naive_step;

    vect2Di portal_offset;
    mat2Di portal_transform;
    int portal_color = COLOR_WHITE; // white means no change
    if (!PORTALS_OFF)
    { 
      orthogonalRedirect(board_pos, transformed_naive_step, portal_offset, portal_transform, portal_color);
    }
    if(portal_color != COLOR_WHITE)
    {
      color = portal_color;
    }

    offset += portal_offset; 
    transform *= portal_transform;

    vect2Di next_board_pos = naive_next_board_pos + portal_offset;

    // If the portal has sent us off the board, stop
    if (!onBoard(next_board_pos))
    {
      break;
    }

    SquareMap square_map;

    square_map.board_pos = next_board_pos;

    square_map.line_pos = naive_squares[step_num] - naive_squares[0];

    square_map.color = color;

    line.mappings.push_back(square_map);

    // sight lines don't need to go past the first block
    if (is_sight_line)
    {
      // if there is a mote here, it wants to go to the source of the sight line
      if (board[next_board_pos.x][next_board_pos.y].mote.lock() != nullptr)
      {
        board[next_board_pos.x][next_board_pos.y].mote.lock()->rel_player_pos = naive_squares[0] - naive_squares[step_num];
      }

      if (board[next_board_pos.x][next_board_pos.y].wall == true)
      {
        break;
      }
    }
    board_pos = next_board_pos;
  }
  return line;
}

Line lineCast(vect2Di start_board_pos, vect2Di rel_pos, bool is_sight_line)
{
  std::vector<vect2Di> naive_line = orthogonalBresneham(start_board_pos, start_board_pos + rel_pos);
  return curveCast(naive_line, is_sight_line);
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
      char glyph;
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

      attron(COLOR_PAIR(RED_ON_BLACK));
      mvaddch(row, col, glyph);
      attroff(COLOR_PAIR(RED_ON_BLACK));
    }
  }
}

void drawSightMap()
{
  // Fill background
  attron(COLOR_PAIR(BACKGROUND_COLOR));
  for (int row = 0; row < num_rows; row++)
  {
    for (int col = 0; col < num_cols; col++)
    {
      mvaddch(row, col, OUT_OF_VIEW);
    }
  }
  attroff(COLOR_PAIR(BACKGROUND_COLOR));
  // Draw the player at the center of the sightmap
  int row, col;
  sightMapToScreen(vect2Di(0, 0), row, col);
  mvaddch(row, col, '@');
  
  // For every sight line
  for(int line_num = 0; line_num < static_cast<int>(player_sight_lines.size()); line_num++)
  {
    Line line = player_sight_lines[line_num];
    // For every square on that line of sight
    for(int square_num = 0; square_num < static_cast<int>(line.mappings.size()); square_num++)
    {
      SquareMap mapping = line.mappings[square_num];
      Square board_square = board[mapping.board_pos.x][mapping.board_pos.y];
      int row, col;
      sightMapToScreen(mapping.line_pos, row, col);
      int forground_color = COLOR_WHITE;
      int background_color = COLOR_BLACK;
      char glyph;
      // if player, show the player.
      if (mapping.board_pos == player_pos)
      {
        glyph = '@';
      }
      else if (board_square.wall == true)
      {
        forground_color = COLOR_BLACK;
        background_color = COLOR_WHITE;
        glyph = ' ';
      }
      else if (board_square.laser == true)
      {
        forground_color = COLOR_BLACK;
        background_color = COLOR_RED;
        glyph = ' ';
      }
      else if (board_square.mote.lock() != nullptr)
      {
        // draw mote
        glyph = '#';
      }
      else if (board_square.plant > 0)
      {
        forground_color = COLOR_GREEN;
        glyph = PLANT_GLYPH;
      }
      else if (board_square.water > 0)
      {
        glyph = ' ';
        if (board_square.water <= SHALLOW_WATER_DEPTH)
        {
          background_color = COLOR_CYAN;
        }
        else
        {
          background_color = COLOR_BLUE;
        }

      }
      else
      {
        forground_color = board_square.grass_color;
        glyph = board_square.grass_glyph;
      }

      if (board_square.fire == true)
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
      mvaddch(row, col, glyph);
      attroff(COLOR_PAIR(getColorPairIndex(forground_color, background_color)));
      // if we just drew a wall, done with this line
      if (board_square.wall == true)
        break;
    }
  }
  // where the player is facing
  char aiming_indicator = 'o';
  vect2Di rel_faced_direction = player_faced_direction * player_transform.inversed();
  if (rel_faced_direction == DOWN)
  {
    aiming_indicator = 'v';
  }
  else if (rel_faced_direction == UP)
  {
    aiming_indicator = '^';
  }
  else if (rel_faced_direction == LEFT)
  {
    aiming_indicator = '<';
  }
  else if (rel_faced_direction == RIGHT)
  {
    aiming_indicator = '>';
  }
  sightMapToScreen(player_faced_direction, row, col);
  mvaddch(row, col, aiming_indicator);
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
      char glyph;
      int color = 0;
      if (!onBoard(pos))
        glyph = '.';
      else if (board[pos.x][pos.y].wall == true)
      {
        glyph = ' ';
        color = 2;
      }
      else if (board[pos.x][pos.y].mote.lock() != nullptr)
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

// Flow water
void updateWater()
{

  // each flow is one water moving from the first of the pair to the second.
  std::vector<std::pair<vect2Di, vect2Di>> flows;
  // for every square on the board
  for (int x=0; x < BOARD_SIZE; x++)
  {
    for (int y=0; y < BOARD_SIZE; y++)
    {
      vect2Di thispos = vect2Di(x, y);
      // if this square has water deeper than 1
      if(getSquare(thispos)->water > 1)
      {
        // check every adjacent square
        for (vect2Di dir : ORTHOGONALS)
        {
          vect2Di adjpos = posFromStep(thispos, dir);
          // if there can be a flow from here to there
          // TODO: different flow rules for shallow vs deep water?
          if (onBoard(adjpos) && 
              getSquare(adjpos)->wall==false &&
              getSquare(adjpos)->plant==false &&
              getSquare(adjpos)->water <= getSquare(thispos)->water-2)

          {
            if (random(0, AVG_WATER_FLOW_TIME * 2) == 0)
            {
              flows.push_back(std::make_pair(thispos, adjpos));
            }
          }
        }
      }
    }
  }
  // randomize the order of attempted flows to prevent directional bias
  std::random_shuffle(flows.begin(), flows.end());
  // actually flow the water the plants in the selected locations
  for (std::pair<vect2Di, vect2Di> flowpair : flows)
  {
    // if there is still enough of a water difference to allow a flow
    if (getSquare(flowpair.first)->water > getSquare(flowpair.second)->water+1)
    {
     getSquare(flowpair.first)->water -= 1;
     getSquare(flowpair.second)->water += 1;
    }
  }
}

// fire spreading and damaging plants
void updateFire()
{
  std::vector<vect2Di> newFires;
  // for every square on the board
  for (int x=0; x < BOARD_SIZE; x++)
  {
    for (int y=0; y < BOARD_SIZE; y++)
    {
      vect2Di thispos = vect2Di(x, y);
      // if this square has a fire
      if(getSquare(thispos)->fire == true)
      {
        // apply damage to the current plant, maybe destroying it and putting out the fire
        if (getSquare(thispos)->plant > 0)
        {
          getSquare(thispos)->plant -=1;
        }
        if (getSquare(thispos)->plant == 0)
        {
          getSquare(thispos)->fire = false;
        }
        // check every adjacent square
        for (vect2Di dir : ORTHOGONALS)
        {
          vect2Di adjpos = posFromStep(thispos, dir);
          // if the space has a plant but no fire
          if (onBoard(adjpos) && 
              getSquare(adjpos)->plant > 0 && 
              getSquare(adjpos)->fire == false)
          {
            if (random(0, AVG_FIRE_SPREAD_TIME * 2) == 0)
            {
              newFires.push_back(adjpos);
            }
          }
        }
      }
    }
  }
  // actually spawn the fires in the selected locations
  for (vect2Di pos : newFires)
  {
    getSquare(pos)->fire = true;
  }
}

// go through all the plants, and grow new ones or kill off old ones as rules dictate
// For now, simple expansion
void updatePlants()
{
  std::vector<vect2Di> whereToSpawnPlants;
  // for every square on the board
  for (int x=0; x < BOARD_SIZE; x++)
  {
    for (int y=0; y < BOARD_SIZE; y++)
    {
      vect2Di thispos = vect2Di(x, y);
      // if this square has a plant THAT IS NOT ON FIRE
      if(getSquare(thispos)->plant != 0 && getSquare(thispos)->fire == false)
      {
        // check every adjacent square
        for (vect2Di dir : ORTHOGONALS)
        {
          vect2Di adjpos = posFromStep(thispos, dir);
          // if the space is empty
          if (posIsEmpty(adjpos))
          {
            if (random(0, AVG_PLANT_SPAWN_TIME * 2) == 0)
            {
              whereToSpawnPlants.push_back(adjpos);
            }
          }
        }
      }
    }
  }
  // actually spawn the plants in the selected locations
  for (vect2Di pos : whereToSpawnPlants)
  {
    // need to check this because we don't want to try to double spawn a plant (a square with 2 adjacent plants has 2 chances to spawn)
    if (posIsEmpty(pos))
    {
      createPlant(pos);
    }
  }
}

int main()
{
  initNCurses();

  initBoard();

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
      shootLaser();
      laser_fired = true;
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
    updateSightLines();
    updateMotes();
    updatePlants();
    updateFire();
    updateWater();
      
    // draw things
    drawEverything();

    // a laser is only shown for the round that it is fired.
    cleanUpLaser();
  }

  endwin(); // End curses
  return 0;
}

