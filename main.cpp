#include "square.h"
#include "line.h"
#include "portal.h"

#include <ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <vector>
#include <cmath>

const int BOARD_SIZE = 100;
const int SIGHT_RADIUS = 5; 

void drawEverything();
void updateSightLines();
Line lineCast(int start_x, int start_y, int d_x, int d_y);
void initNCurses();

//TODO: make these non-global
std::vector<std::vector<Square>> board(BOARD_SIZE, std::vector<Square>(BOARD_SIZE));
std::vector<std::vector<Square>> sight_map(SIGHT_RADIUS*2+1, std::vector<Square>(SIGHT_RADIUS*2+1));
std::vector<Line> player_sight_lines;
int player_x = 0;
int player_y = 0;

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


int main()
{
  initscr();				/* start the curses mode */
  clear();
  noecho();
  cbreak();
  keypad(stdscr, true);
  getmaxyx(stdscr,num_rows,num_cols);		/* get the number of rows and columns */

  board[3][3].wall = true;
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
  for(int y = 0; y < SIGHT_RADIUS; y++)
  {
    rel_x.push_back(SIGHT_RADIUS);
    rel_y.push_back(y);
  }
  for(int x = SIGHT_RADIUS-1; x > -SIGHT_RADIUS; x--)
  {
    rel_x.push_back(x);
    rel_y.push_back(SIGHT_RADIUS);
  }
  for(int y = SIGHT_RADIUS-1; y > -SIGHT_RADIUS; y--)
  {
    rel_x.push_back(-SIGHT_RADIUS);
    rel_y.push_back(y);
  }
  for(int x = -SIGHT_RADIUS+1; x < SIGHT_RADIUS; x++)
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

Line lineCast(int start_x, int start_y, int rel_x, int rel_y)
{
  Line line;

  // For now, linecast with a bresneham equivalent method.
  const int num_steps = std::max(std::abs(rel_x), std::abs(rel_y));

  double dx = static_cast<double>(rel_x)/static_cast<double>(num_steps);
  double dy = static_cast<double>(rel_y)/static_cast<double>(num_steps);

  // These represent how much the line has been teleported when going through a portal.
  // TODO: Take into account portal rotation
  int x_offset = 0;
  int y_offset = 0;

  int current_square_x = start_x;
  int current_square_y = start_y;

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
    if (rounded_x != current_square_x || rounded_y != current_square_y)
    {
      SquareMap square_map;

      square_map.board_x = rounded_x;
      square_map.board_y = rounded_y;

      square_map.line_x = rounded_x - start_x;
      square_map.line_y = rounded_y - start_y;

      line.mappings.push_back(square_map);
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
  init_pair(1, COLOR_RED, COLOR_BLACK);
  attron(COLOR_PAIR(1));
  for (int i = 0; i < line.mappings.size(); i++)
  {
    int bx = line.mappings[i].board_x;
    int by = line.mappings[i].board_y;
    int lx = line.mappings[i].line_x;
    int ly = line.mappings[i].line_y;
    int row, col;
    sightMapToScreen(bx, by, row, col);
    if(onScreen(row, col))
    {
      int glyph;
      if (lx == 0)
        glyph = '|';
      else 
      {
        double slope = ly/lx;
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

      mvaddch(row, col, glyph);
    }
  }
  attroff(COLOR_PAIR(1));
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
      int glyph;
      if (!onBoard(x, y))
        glyph = ' ';
      else if (board[x][y].wall == true)
        glyph = '#';
      else
        glyph = '.';
      mvaddch(row, col, glyph);
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

