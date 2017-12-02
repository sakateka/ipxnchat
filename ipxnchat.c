#include <curses.h>
#include <string.h>
#include <stdio.h>

# define MAXCHAR 65535

void draw_screen(WINDOW **top, WINDOW **bottom, char *text) {
    int y = LINES, x = COLS;
    int halfy = y/2;

    wclear(stdscr);
    box(stdscr, 0, 0);
    mvprintw(0, 3, "[ Remote ]");
    mvaddch(halfy, 0, ACS_LTEE);
    mvhline(halfy, 1, 0, x-2);
    mvaddch(halfy, x-1, ACS_RTEE);
    mvprintw(halfy, 3, "[ You ]");

    mvaddch(y-3, 0, ACS_LTEE);
    mvhline(y-3, 1, 0, x-2);
    mvaddch(y-3, x-1, ACS_RTEE);
    mvprintw(y-3, 3, "[ Send ]");

    if (*top == NULL) {
        *top = newwin(halfy-2, x-4, 1, 2);
    } else {
        wclear(*top);
        wresize(*top, halfy-2, x-4);
        mvwin(*top, 1, 2);
    }
    wprintw(*top, "%s", text);

    if (*bottom == NULL) {
        *bottom = newwin(halfy-4, x-4, halfy+1, 2);
    } else {
        wclear(*bottom);
        wresize(*bottom, halfy-4, x-4);
        mvwin(*bottom, halfy+1, 2);
    }
    wprintw(*bottom, "%s", text);

    move(y-2, 1);
    printw("Size = %d x %d", y, x);
    refresh();
    wrefresh(*top);
    wrefresh(*bottom);

}

int main() {
    char text[MAXCHAR];
    memset(text, 'a', MAXCHAR-2);
    text[MAXCHAR-1] = '\0';


    WINDOW *top = NULL, *bottom = NULL;
    initscr();
    noecho();
    draw_screen(&top, &bottom, text);
    int key;
    while ((key = getch()) != 'q') {
        if (key == KEY_RESIZE || key == 'r') {
            draw_screen(&top, &bottom, text);
        }
        if (key == 'c') {
            wclear(bottom);
            wrefresh(bottom);
        }
        if (key == 'w') {
            wprintw(bottom, "hello!");
            wrefresh(bottom);
        }
    };
    endwin();
    return 0;
}
