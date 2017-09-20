#include "square.h"
#include "line.h"

#include <ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <vector>
#include <list>

const int BOARD_SIZE = 100;
const int SIGHT_RADIUS = 5; 

void drawEverything();
void updateSightLines();
Line lineCast(int start_x, int start_y, int d_x, int d_y);
void initNCurses();

//TODO: make these non-global
std::vector<std::vector<Square>> board(BOARD_SIZE, std::vector<Square>(BOARD_SIZE);
std::vector<std::vector<Square>> board(SIGHT_RADIUS*2+1, std::vector<Square>(SIGHT_RADIUS*2+1);
std::list<Line> player_sight_lines();
int player_x = 0;
int player_y = 0;

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

}

Line lineCast(int start_x, int start_y, int d_x, int d_y)
{

}

void drawEverything()
{

}
