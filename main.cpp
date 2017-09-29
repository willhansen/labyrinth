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

const int BOARD_SIZE = 100;
const int SIGHT_RADIUS = 50; 
const bool NAIVE_VIEW = false;

const int WHITE_ON_BLACK = 0;
const int RED_ON_BLACK = 1;
const int BLACK_ON_WHITE = 2;

//const int BACKGROUND_COLOR = BLACK_ON_WHITE;
//const char OUT_OF_VIEW = ' ';
const int BACKGROUND_COLOR = WHITE_ON_BLACK;
const char OUT_OF_VIEW = '.';

const char FLOOR = ' ';

void drawEverything();
void updateSightLines();
Line lineCast(vect2Di start_pos, vect2Di d_pos, bool is_sight_line=true);
void initNCurses();

//TODO: make these non-global
std::vector<std::vector<Square>> board(BOARD_SIZE, std::vector<Square>(BOARD_SIZE));
std::vector<std::vector<Square>> sight_map(SIGHT_RADIUS*2+1, std::vector<Square>(SIGHT_RADIUS*2+1));
std::vector<Line> player_sight_lines;
vect2Di player_pos;
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

void attemptMove(vect2Di dp)
{
  Line line = lineCast(player_pos, dp);
  if (line.mappings.size() > 0)
  {
    vect2Di pos = line.mappings[0].board_pos;
    if (board[pos.x][pos.y].wall == false)
    {
      player_pos = pos;
    }
  }
}

// TODO: allow double start and end positions in order to allow slight perturbations to avoid needing to break ties.
std::vector<vect2Di> orthogonalBresneham(vect2Di goal_pos)
{
  // this line starts at zero
  // ties are broken towards y=+/-inf
  std::vector<vect2Di> output;
  // include the first step
  const int num_steps = std::max(std::abs(goal_pos.x), std::abs(goal_pos.y)) + 1;
  vect2Di pos;
  double x=0;
  double y=0;
  double dx = static_cast<double>(goal_pos.x)/static_cast<double>(num_steps);
  double dy = static_cast<double>(goal_pos.y)/static_cast<double>(num_steps);
  output.push_back(pos);
  for (int step_num = 1; step_num < num_steps; step_num++)
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
  }
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
  row = num_rows/2 - pos.y;
  col = pos.x + num_cols/2;
}

// This gives board to screen coordinates, player is at center, PORTALS ARE IGNORED
void naiveBoardToScreen(vect2Di pos, int& row, int& col)
{
  row = player_pos.y + num_rows/2 - pos.y;
  col = pos.x - player_pos.x + num_cols/2;
}

void makePortalPair(vect2Di p1, vect2Di p2, bool left=true)
{
  if (!onBoard(p1) || !onBoard(p2))
    return;
  if (left)
  {
    board[p1.x][p1.y].left_portal.reset(new Portal());
    board[p1.x][p1.y].left_portal->new_pos = p2;

    board[p2.x][p2.y].left_portal.reset(new Portal());
    board[p2.x][p2.y].left_portal->new_pos = p1;
  }
  else
  {
    board[p1.x][p1.y].down_portal.reset(new Portal());
    board[p1.x][p1.y].down_portal->new_pos = p2;

    board[p2.x][p2.y].down_portal.reset(new Portal());
    board[p2.x][p2.y].down_portal->new_pos = p1;
  }
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

void initBoard()
{

  player_pos = vect2Di(5, 5);
  
  rectToWall(0, 0, BOARD_SIZE-1, BOARD_SIZE-1);
  rectToWall(30, 5, 50, 20);

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

  makePortalPair(vect2Di(10, 12), vect2Di(20, 12));
  makePortalPair(vect2Di(10, 11), vect2Di(20, 11));
  makePortalPair(vect2Di(10, 10), vect2Di(20, 10));

  makeNicePortalPair(20, 20, 20, 30, 7, 0);

  makeNicePortalPair(60, 20, 72, 20, 5, 0);
  for (int y = 10; y < 31; y++)
  {
    board[60][y].wall = true;
    board[66][y].wall = true;
    board[72][y].wall = true;
    board[78][y].wall = true;
  }

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
      attemptMove(vect2Di(-1, 0));
    else if (in == 'j')
      attemptMove(vect2Di(0, -1));
    else if (in == 'k')
      attemptMove(vect2Di(0, 1));
    else if (in == 'l')
      attemptMove(vect2Di(1, 0));
    else if (in == 'y')
      attemptMove(vect2Di(-1, 1));
    else if (in == 'u')
      attemptMove(vect2Di(1, 1));
    else if (in == 'b')
      attemptMove(vect2Di(-1, -1));
    else if (in == 'n')
      attemptMove(vect2Di(1, -1));

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
void orthogonalRedirect(vect2Di start_pos, vect2Di& dp)
{
  vect2Di end_pos = start_pos + dp;
  // X step
  if (std::abs(start_pos.x - end_pos.x) > 0)
  {
    int big_x = std::max(start_pos.x, end_pos.x);
    int y = start_pos.y;
    // only bigger number square could have a portal for this transition 
    if (onBoard(vect2Di(big_x, y)) && board[big_x][y].left_portal != nullptr)
    {
      dp.x += board[big_x][y].left_portal->new_pos.x - big_x;
      dp.y += board[big_x][y].left_portal->new_pos.y - y;
    }
  }
  // Y step
  else
  {
    int big_y = std::max(start_pos.y, end_pos.y);
    int x = start_pos.x;
    // only bigger number square could have a portal for this transition 
    if (onBoard(vect2Di(x, big_y)) && board[x][big_y].down_portal != nullptr)
    {
      dp.x += board[x][big_y].down_portal->new_pos.x - x;
      dp.y += board[x][big_y].down_portal->new_pos.y - big_y;
    }
  }
}


Line lineCast(vect2Di start_pos, vect2Di rel_pos, bool is_sight_line)
{
  Line line;
  std::vector<vect2Di> naive_line = orthogonalBresneham(rel_pos);

  // These represent how much the line has been teleported when going through a portal.
  // Take the real position, subtract the translation offset, then the rotation offset, and you have the line position.
  vect2Di offset;
  vect2Di real_pos = start_pos;

  // Sight lines don't include the starting square.  They do include the ending square.
  for(int step_num = 1; step_num < naive_line.size(); step_num++)
  {
    // This does not yet account for a portal this next step may step through
    vect2Di next_real_pos = naive_line[step_num] + start_pos + offset;
    // don't cast off the board
    if (!onBoard(next_real_pos))
    {
      break;
    }

    Square new_square = board[next_real_pos.x][next_real_pos.y];

    // Portals may be found on the bottom and left of any square
    // In the case of exact diagonal, go though the portal on the bottom of a square

    // if only orthogonal step
    vect2Di step = next_real_pos - real_pos;
    //orthogonalRedirect(real_pos, step);

    vect2Di portal_dp = step - (next_real_pos - real_pos);

    offset += portal_dp; 
    next_real_pos += portal_dp;

    // If the portal has sent us off the board, stop
    if (!onBoard(next_real_pos))
    {
      break;
    }

    SquareMap square_map;

    square_map.board_pos = next_real_pos;

    square_map.line_pos = naive_line[step_num];

    line.mappings.push_back(square_map);

    // sight lines don't need to go past the first block
    if (is_sight_line)
    {
      // if there is a mote here, it wants to go to the source of the sight line
      if (board[next_real_pos.x][next_real_pos.y].mote != nullptr)
      {
        board[next_real_pos.x][next_real_pos.y].mote->rel_player_pos = naive_line[0] - naive_line[step_num];
      }

      if (board[next_real_pos.x][next_real_pos.y].wall == true)
      {
        break;
      }
    }

    real_pos = next_real_pos;
  }

  return line;
}

void drawLine(Line line)
{
  for (int i = 0; i < line.mappings.size(); i++)
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
  for(int line_num = 0; line_num < player_sight_lines.size(); line_num++)
  {
    Line line = player_sight_lines[line_num];
    for(int square_num = 0; square_num < line.mappings.size(); square_num++)
    {
      SquareMap mapping = line.mappings[square_num];
      Square board_square = board[mapping.board_pos.x][mapping.board_pos.y];
      int row, col;
      sightMapToScreen(mapping.line_pos, row, col);
      int color = WHITE_ON_BLACK;
      char glyph;
      // if player, show the player.
      if (mapping.board_pos == player_pos)
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

  for(int i = 0; i < player_sight_lines.size(); i++)
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

