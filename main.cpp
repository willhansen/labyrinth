#include "square.h"
#include "line.h"
#include "portal.h"

#include <ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <vector>
#include <cmath>

const int BOARD_SIZE = 100;
const int SIGHT_RADIUS = 50; 

const int WHITE_ON_BLACK = 0;
const int RED_ON_BLACK = 1;
const int BLACK_ON_WHITE = 2;

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

void screenToSightMap(int row, int col, int& x, int& y)
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
  makePortalPair(20,12,40,9);
  board[20][11].wall = true;
  makePortalPair(20,10,40,10);
  board[20][9].wall = true;
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
    else if (in == KEY_MOUSE)
    {
      MEVENT mouse_event;
      getmouse(&mouse_event);
      screenToSightMap(mouse_event.y, mouse_event.x, mouse_x, mouse_y);
    }
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
      int x_step_taken = rounded_x - prev_square_x;
      int y_step_taken = rounded_y - prev_square_y;
      // If a portal goes directly to another portal, we need to go through that one too, right away.
      while (square.portal != nullptr)
      {
        // These are the straight translational offset given by portals.  Not including the step out of the portal, or rotation.
        int x_portal_offset = square.portal->new_x - rounded_x;
        int y_portal_offset = square.portal->new_y - rounded_y;
        // TODO: portal rotation here
        
        
        int portal_dx = x_portal_offset + x_step_taken;
        int portal_dy = y_portal_offset + y_step_taken;
        x_offset += portal_dx; // TODO: rotate the step taken here
        y_offset += portal_dy;
        rounded_x += portal_dx;
        rounded_y += portal_dy;
        x += portal_dx;
        y += portal_dy;

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

      attron(COLOR_PAIR(RED_ON_BLACK));
      mvaddch(row, col, glyph);
      attroff(COLOR_PAIR(RED_ON_BLACK));
    }
  }
}

void drawEverything()
{
  // Fill background
  for (int row = 0; row < num_rows; row++)
  {
    for (int col = 0; col < num_cols; col++)
    {
      mvaddch(row, col, '.');
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
        glyph = ' ';
      }
      attron(COLOR_PAIR(color));
      mvaddch(row, col, glyph);
      attroff(COLOR_PAIR(color));
      // if we just drew a wall, done with this line
      if (board_square.wall == true)
        break;
    }
  }
  // Draw all the floor and walls
  // For every square on the screen
  /*
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
  */

  // move the cursor
  move(0,0);
  refresh();
}

