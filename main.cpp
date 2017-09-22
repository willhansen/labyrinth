#include "square.h"
#include "line.h"
#include "portal.h"

#include <ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <vector>
#include <cmath>

const int BOARD_SIZE = 100;
const int SIGHT_RADIUS = 10; 

void drawEverything();
void updateSightLines();
Line lineCast(int start_x, int start_y, int d_x, int d_y, bool stop_at_wall = false);
void initNCurses();

//TODO: make these non-global
std::vector<std::vector<Square>> board(BOARD_SIZE, std::vector<Square>(BOARD_SIZE));
std::vector<std::vector<Square>> sight_map(SIGHT_RADIUS*2+1, std::vector<Square>(SIGHT_RADIUS*2+1));
std::vector<Line> player_sight_lines;
int player_x = 1;
int player_y = 1;

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

void makePortalPair(int x1, int y1, int x2, int y2)
{
  if (!onBoard(x1, y1) || !onBoard(x2, y2))
    return;
  board[x1][y1].portal.reset(new Portal());
  board[x1][y1].portal->new_x = x2;
  board[x1][y1].portal->new_y = y2;

  board[x2][y2].portal.reset(new Portal());
  board[x2][y2].portal->new_x = x1;
  board[x2][y2].portal->new_y = y1;
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

  init_pair(0, COLOR_WHITE, COLOR_BLACK);
  init_pair(1, COLOR_RED, COLOR_BLACK);
  init_pair(2, COLOR_BLACK, COLOR_WHITE);
}

void initBoard()
{
  board[3][3].wall = true;
  for (int x = 0; x < BOARD_SIZE; x++)
  {
    board[x][0].wall = true;
    board[x][BOARD_SIZE-1].wall = true;
  }
  for (int y = 0; y < BOARD_SIZE; y++)
  {
    board[0][y].wall = true;
    board[BOARD_SIZE-1][y].wall = true;
  }
  makePortalPair(20,10,40,10);
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
    player_sight_lines.push_back(lineCast(player_x, player_y, rel_x[i], rel_y[i], true));
  }
}

Line lineCast(int start_x, int start_y, int rel_x, int rel_y, bool stop_at_wall)
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

    int rounded_x = static_cast<double>(x);
    int rounded_y = static_cast<double>(y);

    if (!onBoard(rounded_x, rounded_y))
    {
      break;
    }

    // If the line has entered a new square.
    if (rounded_x != prev_square_x || rounded_y != prev_square_y)
    {
      Square square = board[rounded_x][rounded_y];
      // If a portal goes directly to another portal, we need to go through that one too, right away.
      while (square.portal != nullptr)
      {
        int x_portal_offset = square.portal->new_x - rounded_x;
        int y_portal_offset = square.portal->new_y - rounded_y;
        // TODO: portal rotation here
        int portal_rotation = square.portal->rotation;

        // TODO: These will need to be rotated, because the autostep is coming out of a maybe rotated portal.
        int x_step_offset = rounded_x - prev_square_x;
        int y_step_offset = rounded_y - prev_square_y;

        x_offset += x_portal_offset + x_step_offset;
        y_offset += y_portal_offset + y_step_offset;
        rounded_x += x_offset;
        rounded_y += y_offset;
        x += x_offset;
        y += y_offset;

        square = board[rounded_x][rounded_y];
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

    // This check is at the end so that we include the wall in the line, but it is the end of the line.
    if(stop_at_wall && board[rounded_x][rounded_y].wall == true)
    {
      break;
    }
  }

  return line;
}

void screenToSightMap(int row, int col, int& x, int& y)
{
  // The player is at the center of the sightmap, coordinates are (x, y) in the first quadrant
  // The screen is (y, x) in the fourth quadrant (but y is positive)
  x = player_x - num_cols/2 + col;
  y = player_y + num_rows/2 - row;
  return;
}

void sightMapToScreen(int x, int y, int& row, int& col)
{
  row = player_y + num_rows/2 - y;
  col = x - player_x + num_cols/2;
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
    sightMapToScreen(bx, by, row, col);
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

      attron(COLOR_PAIR(1));
      mvaddch(row, col, glyph);
      attroff(COLOR_PAIR(1));
    }
  }
}

void drawEverything()
{
  // Draw all the floor and walls
  // For every square on the screen
  for (int row = 0; row < num_rows; row++)
  {
    for (int col = 0; col < num_cols; col++)
    {
      int x, y;
      screenToSightMap(row, col, x, y);
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
  sightMapToScreen(player_x, player_y, row, col);
  if (onScreen(row, col))
  {
    mvaddch(row, col, '@');
  }

  for(int i = 0; i < player_sight_lines.size(); i++)
  {
    drawLine(player_sight_lines[i]);
  }

  refresh();
}

