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

bool on_board(int x, int y)
{
  return (x>=0 && y>=0 && x<BOARD_SIZE && y<BOARD_SIZE);
}


int main()
{
  initNCurses();
  return 0;
}

void initNCurses()
{
  char mesg[]="Just a string";		/* message to be appeared on the screen */
  int row,col;				/* to store the number of rows and *
                       * the number of colums of the screen */
  initscr();				/* start the curses mode */
  getmaxyx(stdscr,row,col);		/* get the number of rows and columns */
  mvprintw(row/2,(col-strlen(mesg))/2,"%s",mesg);
  /* print the message at the center of the screen */
  mvprintw(row-2,0,"This screen has %d rows and %d columns\n",row,col);
  printw("Try resizing your window(if possible) and then run this program again");
  refresh();
  getch();
  endwin();
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

    if (!on_board(rounded_x, rounded_y))
    {
      break;
    }

    // If the line has entered a new square.
    if (rounded_x != current_square_x || rounded_y != current_square_y)
    {
      SquareMap square_map;

      square_map.board_x = rounded_x;
      square_map.board_y = rounded_y;

      square_map.line_x = rounded_x;
      square_map.line_y = rounded_y;

      line.mappings.push_back(square_map);
    }
  }

  return line;
}

void drawEverything()
{

}
