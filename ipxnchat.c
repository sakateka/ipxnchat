#include <curses.h>
#include <string.h>
#include <stdlib.h>

WINDOW* newBoxedWindow(int lines, int cols, int y, int x) {
	WINDOW *topOut = newwin(lines, cols,y,x);
	refresh();
    box(topOut, '|', '-');
	wrefresh(topOut);
    WINDOW *new = newwin(lines-2, cols-2,y+1,x+1);
	wrefresh(new);
    return new;
}

int main() {
	initscr();
    noecho();
    box(stdscr, '|', '=');
	refresh();

	int halfy = LINES/2;
	char text[]= "Hello World!!!";
    char *bigText = calloc(strlen(text), 200);
    for (int i = 0; i < 200 ; i++) {
        bigText = strcat(bigText, text);
    }

	WINDOW *top = newBoxedWindow(halfy-1, COLS-2, 1, 1);
    waddnstr(top, bigText, -1);
    wrefresh(top);

	WINDOW *bottom = newBoxedWindow(halfy-1, COLS-2, halfy, 1);
    waddnstr(bottom, bigText, -1);
    wrefresh(bottom);
	getch();

	endwin();
	return 0;
}
