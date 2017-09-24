#include "square.h"
#include "line.h"
#include "portal.h"

#include <ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <vector>
#include <cmath>

const int BOARD_SIZE = 100;
const int SIGHT_RADIUS = 10; 

const int WHITE_ON_BLACK = 0;
const int RED_ON_BLACK = 1;
const int BLACK_ON_WHITE = 2;

const char OUT_OF_VIEW = '.';
const char FLOOR = ' ';

void drawEverything();
void updateSightLines();
Line lineCast(int start_x, int start_y, int d_x, int d_y);
void initNCurses();

//TODO: make these non-global
std::vector<std::vector<Square>> board(BOARD_SIZE, std::vector<Square>(BOARD_SIZE));
std::vector<std::vector<Square>> sight_map(SIGHT_RADIUS*2+1, std::vector<Square>(SIGHT_RADIUS*2+1));
std::vector<Line> player_sight_lines;
int player_x = 1;
int player_y = 1;
int mouse_x, mouse_y;

int num_rows,num_cols;				/* to store the number of rows and */

bool onBoard(int x, int y)
{
  return (x>=0 && y>=0 && x<BOARD_SIZE && y<BOARD_SIZE);
}

bool onScreen(int row, int col)
{
  return (row>=0 && col>=0 && row<num_rows && col<num_cols);
}

void attemptMove(int dx, int dy)
{
  Line line = lineCast(player_x, player_y, dx, dy);
  if (line.mappings.size() > 0)
  {
    int x = line.mappings[0].board_x;
    int y = line.mappings[0].board_y;
    if (board[x][y].wall == false)
    {
      player_x = x;
      player_y = y;
    }
  }
}

void screenToBoard(int row, int col, int& x, int& y)
{
  // The player is at the center of the sightmap, coordinates are (x, y) in the first quadrant
  // The screen is (y, x) in the fourth quadrant (but y is positive)
  x = player_x - num_cols/2 + col;
  y = player_y + num_rows/2 - row;
  return;
}

// Sight map positions are relative to its center
// Screen positions are relative to top left
void sightMapToScreen(int x, int y, int& row, int& col)
{
  row = num_rows/2 - y;
  col = x + num_cols/2;
}

// This gives board to screen coordinates, player is at center, PORTALS ARE IGNORED
void naiveBoardToScreen(int x, int y, int& row, int& col)
{
  row = player_y + num_rows/2 - y;
  col = x - player_x + num_cols/2;
}

void makePortalPair(int x1, int y1, int x2, int y2)
{
  if (!onBoard(x1, y1) || !onBoard(x2, y2))
    return;
  board[x1][y1].left_portal.reset(new Portal());
  board[x1][y1].left_portal->new_x = x2;
  board[x1][y1].left_portal->new_y = y2;

  board[x2][y2].left_portal.reset(new Portal());
  board[x2][y2].left_portal->new_x = x1;
  board[x2][y2].left_portal->new_y = y1;
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

  init_pair(WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);
  init_pair(RED_ON_BLACK, COLOR_RED, COLOR_BLACK);
  init_pair(BLACK_ON_WHITE, COLOR_BLACK, COLOR_WHITE);
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


void initBoard()
{
  rectToWall(0, 0, BOARD_SIZE-1, BOARD_SIZE-1);
  rectToWall(30, 5, 50, 20);

  board[20][13].wall = true;
  makePortalPair(20,12,45,12);
  board[20][11].wall = true;
}

int main()
{
  initNCurses();

  initBoard();

  while(true)
  {
    // Get input
    int in = getch();
    // Process input
    if (in == 'q')
      break;
    else if (in == 'h')
      attemptMove(-1, 0);
    else if (in == 'j')
      attemptMove(0, -1);
    else if (in == 'k')
      attemptMove(0, 1);
    else if (in == 'l')
      attemptMove(1, 0);
    else if (in == 'y')
      attemptMove(-1, 1);
    else if (in == 'u')
      attemptMove(1, 1);
    else if (in == 'b')
      attemptMove(-1, -1);
    else if (in == 'n')
      attemptMove(1, -1);

    // Tick everything
    updateSightLines();
      
    // draw things
    drawEverything();
  }

  endwin(); // End curses
  return 0;
}

void updateSightLines()
{
  std::vector<int> rel_x;
  std::vector<int> rel_y;
  // all the relative positions in a square around (0,0)
  for(int y = 0; y <= SIGHT_RADIUS; y++)
  {
    rel_x.push_back(SIGHT_RADIUS);
    rel_y.push_back(y);
  }
  for(int x = SIGHT_RADIUS-1; x >= -SIGHT_RADIUS; x--)
  {
    rel_x.push_back(x);
    rel_y.push_back(SIGHT_RADIUS);
  }
  for(int y = SIGHT_RADIUS-1; y >= -SIGHT_RADIUS; y--)
  {
    rel_x.push_back(-SIGHT_RADIUS);
    rel_y.push_back(y);
  }
  for(int x = -SIGHT_RADIUS+1; x <= SIGHT_RADIUS; x++)
  {
    rel_x.push_back(x);
    rel_y.push_back(-SIGHT_RADIUS);
  }
  for(int y = -SIGHT_RADIUS+1; y < 0; y++)
  {
    rel_x.push_back(SIGHT_RADIUS);
    rel_y.push_back(y);
  }

  player_sight_lines.clear();
  // Find the sight lines in this order
  for(int i = 0; i < static_cast<int>(rel_x.size()); i++)
  {
    player_sight_lines.push_back(lineCast(player_x, player_y, rel_x[i], rel_y[i]));
  }
}

// Redirect an orthogonal step between adjacent squares.
void orthogonalRedirect(int start_x, int start_y, int& dx, int& dy)
{
  int end_x = start_x + dx;
  int end_y = start_y + dy;
  // X step
  if (std::abs(start_x - end_x) > 0)
  {
    int small_x = std::min(start_x, end_x);
    int big_x = std::max(start_x, end_x);
    int y = start_y;
    // only bigger number square could have a portal for this transition 
    if (onBoard(big_x, y) && board[big_x][y].left_portal != nullptr)
    {
      dx += board[big_x][y].left_portal->new_x - big_x;
      dy += board[big_x][y].left_portal->new_y - y;
    }
  }
  // Y step
  else
  {
    int small_y = std::min(start_y, end_y);
    int big_y = std::max(start_y, end_y);
    int x = start_x;
    // only bigger number square could have a portal for this transition 
    if (onBoard(x, big_y) && board[x][big_y].down_portal != nullptr)
    {
      dx += board[x][big_y].down_portal->new_x - x;
      dy += board[x][big_y].down_portal->new_y - big_y;
    }
  }
}

// given a small step, redirect it accordingly if it goes through portals
// Assumes this step is less than 1 square long
// Assumes this step crosses a square boundary
void redirect(double start_x, double start_y, double& dx, double& dy)
{
  double end_x = start_x + dx;
  double end_y = start_y + dy;
  // Portals may be found on the bottom and left of any square
  // In the case of exact diagonal, go though the portal on the bottom of a square
  int start_x_int = static_cast<int>(std::round(start_x));
  int start_y_int = static_cast<int>(std::round(start_y));
  int end_x_int = static_cast<int>(std::round(end_x));
  int end_y_int = static_cast<int>(std::round(end_y));

  std::vector<int> x_steps_int;
  std::vector<int> y_steps_int;
  // if only orthogonal step
  if (std::abs(end_x_int - start_x_int) + std::abs(end_y_int - start_y_int) < 2)
  {
    x_steps_int.push_back(end_x_int-start_x_int);
    y_steps_int.push_back(end_y_int-start_y_int);
  }
  // if diagonal step
  else
  {
    // need to find which orthogonal square this went through, If a tie, pick the vertical
    double y_division = std::round(std::min(start_y, end_y)) + 0.5;
    double x_division = std::round(std::min(start_x, end_x)) + 0.5;
    double step_slope = (end_y - start_y)/(end_x - start_x);
    double y_at_x_division = start_y + step_slope * (x_division - start_x);

    int mid_x_int, mid_y_int; // for the intermediate step
    // This line decides diagonal tie breaks
    if ((end_y > start_y && y_at_x_division < y_division) || ( end_y < start_y && y_at_x_division > y_division))
    {
      // Horizontal then vertical
      x_steps_int.push_back(end_x_int-start_x_int);
      y_steps_int.push_back(0);

      x_steps_int.push_back(0);
      y_steps_int.push_back(end_y_int-start_y_int);
    }
    else
    {
      // vertical then horizontal
      x_steps_int.push_back(0);
      y_steps_int.push_back(end_y_int-start_y_int);

      x_steps_int.push_back(end_x_int-start_x_int);
      y_steps_int.push_back(0);
    }
  }

  int temp_start_x_int = start_x_int;
  int temp_start_y_int = start_y_int;
  for (int i=0; i < x_steps_int.size(); i++)
  {
    orthogonalRedirect(temp_start_x_int, temp_start_y_int, x_steps_int[i], y_steps_int[i]);
    temp_start_x_int += x_steps_int[i];
    temp_start_y_int += y_steps_int[i];
  }

  dx += temp_start_x_int - end_x;
  dy += temp_start_y_int - end_y;
}

Line lineCast(int start_x, int start_y, int rel_x, int rel_y)
{
  Line line;

  // For now, linecast with a bresneham equivalent method.
  const int num_steps = std::max(std::abs(rel_x), std::abs(rel_y));

  double dx = static_cast<double>(rel_x)/static_cast<double>(num_steps);
  double dy = static_cast<double>(rel_y)/static_cast<double>(num_steps);

  // These represent how much the line has been teleported when going through a portal.
  // Take the real position, subtract the translation offset, then the rotation offset, and you have the line position.
  int x_offset = 0;
  int y_offset = 0;
  int rotation_offset = 0;

  int prev_square_x = start_x;
  int prev_square_y = start_y;

  double x = start_x;
  double y = start_y;

  // Rounding to integer coordinates is done with truncation.
  // line casts do not include the first square, they do include the end.
  for(int step_num = 0; step_num < num_steps; step_num++)
  {
    // TODO: Account for portals
    x += dx;
    y += dy;

    int rounded_x = static_cast<double>(std::round(x));
    int rounded_y = static_cast<double>(std::round(y));

    if (!onBoard(rounded_x, rounded_y))
    {
      break;
    }

    // If the line has entered a new square.
    if (rounded_x != prev_square_x || rounded_y != prev_square_y)
    {
      Square square = board[rounded_x][rounded_y];
      // Note: these will have to be rotated if going through a rotated portal
      double new_dx = dx;
      double new_dy = dy;
      redirect(x-dx, y-dy, new_dx, new_dy);
      int portal_dx = static_cast<int>(std::round(new_dx - dx));
      int portal_dy = static_cast<int>(std::round(new_dy - dy));
      
      x_offset += portal_dx; 
      y_offset += portal_dy;
      rounded_x += portal_dx;
      rounded_y += portal_dy;
      x += portal_dx;
      y += portal_dy;
      
      if (!onBoard(rounded_x, rounded_y))
      {
        break;
      }


      SquareMap square_map;

      square_map.board_x = rounded_x;
      square_map.board_y = rounded_y;

      square_map.line_x = (rounded_x - x_offset) - start_x;
      square_map.line_y = (rounded_y - y_offset) - start_y;

      line.mappings.push_back(square_map);

      prev_square_x = rounded_x;
      prev_square_y = rounded_y;
    }
  }

  return line;
}

void drawLine(Line line)
{
  for (int i = 0; i < line.mappings.size(); i++)
  {
    int bx = line.mappings[i].board_x;
    int by = line.mappings[i].board_y;
    int lx = line.mappings[i].line_x;
    int ly = line.mappings[i].line_y;
    int prev_lx, prev_ly;
    if (i != 0)
    {
      prev_lx = line.mappings[i-1].line_x;
      prev_ly = line.mappings[i-1].line_y;
    }
    else
    {
      prev_lx = 0;
      prev_ly = 0;
    }
    int row, col;
    naiveBoardToScreen(bx, by, row, col);
    if(onScreen(row, col))
    {
      char glyph;
      int dx = lx - prev_lx;
      int dy = ly - prev_ly;
      if (dx == 0)
        glyph = '|';
      else 
      {
        double slope = static_cast<double>(dy)/static_cast<double>(dx);
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

void drawEverything()
{
  /*
  // Fill background
  for (int row = 0; row < num_rows; row++)
  {
    for (int col = 0; col < num_cols; col++)
    {
      mvaddch(row, col, OUT_OF_VIEW);
    }
  }
  // Draw the player at the center of the sightmap
  int row, col;
  sightMapToScreen(0, 0, row, col);
  mvaddch(row, col, '@');
  
  // For every sight line
  for(int line_num = 0; line_num < player_sight_lines.size(); line_num++)
  {
    Line line = player_sight_lines[line_num];
    for(int square_num = 0; square_num < line.mappings.size(); square_num++)
    {
      SquareMap mapping = line.mappings[square_num];
      Square board_square = board[mapping.board_x][mapping.board_y];
      int row, col;
      sightMapToScreen(mapping.line_x, mapping.line_y, row, col);
      int color = WHITE_ON_BLACK;
      char glyph;
      // if player, show the player.
      if (mapping.board_x == player_x && mapping.board_y == player_y)
      {
        glyph = '@';
      }
      else if (board_square.wall == true)
      {
        color = BLACK_ON_WHITE;
        glyph = ' ';
      }
      else
      {
        glyph = FLOOR;
      }
      attron(COLOR_PAIR(color));
      mvaddch(row, col, glyph);
      attroff(COLOR_PAIR(color));
      // if we just drew a wall, done with this line
      if (board_square.wall == true)
        break;
    }
  }
  */
  // Draw all the floor and walls
  // For every square on the screen
  
  for (int row = 0; row < num_rows; row++)
  {
    for (int col = 0; col < num_cols; col++)
    {
      int x, y;
      screenToBoard(row, col, x, y);
      char glyph;
      int color = 0;
      if (!onBoard(x, y))
        glyph = '.';
      else if (board[x][y].wall == true)
      {
        glyph = ' ';
        color = 2;
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
  naiveBoardToScreen(player_x, player_y, row, col);
  if (onScreen(row, col))
  {
    mvaddch(row, col, '@');
  }

  for(int i = 0; i < player_sight_lines.size(); i++)
  {
    drawLine(player_sight_lines[i]);
  }
  

  // move the cursor
  move(0,0);
  refresh();
}

