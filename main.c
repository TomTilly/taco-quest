#include <ncurses.h>

int main() 
{
	int max_x, max_y;
	
	initscr(); 		  // ncurses takes over!
	noecho();  		  // no display user typing!!
	curs_set(false);  // don't show cursor
	
	getmaxyx(stdscr, max_y, max_x);
	
	clear();		  // clear screen
	
	// move, print, and refresh so it appears
	int y = 5;
	int x = 5;
	mvprintw(y, x, "O");
	refresh();
	
	int key;
	do
	{
		key = getch();
		clear();
		
		if (key == 'd' && x < (max_x - 1))
		{
			x++;
		}
		if (key == 'a' && x > 0)
		{
			x--;
		}
		if (key == 's' && y < (max_y - 1))
		{
			y++;
		}
		if (key == 'w' && y > 0)
		{
			y--;
		}
		
		mvprintw(y,x,"O");
		refresh();
		
	} while (key != 'q');
		
	endwin();  // clean up
	
	return 0;
}

