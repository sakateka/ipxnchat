#include <curses.h>

void draw_screen(WINDOW **top, WINDOW **bottom) {
    clear();

    int y, x;
    getmaxyx(stdscr, y, x);
    int halfy = y/2;

    box(stdscr, 0, 0);
    mvaddch(halfy, 0, ACS_LTEE);
    mvaddch(halfy, x-1, ACS_RTEE);
    mvhline(halfy, 1, 0, x-2);
    mvaddch(y-3, 0, ACS_LTEE);
    mvaddch(y-3, x-1, ACS_RTEE);
    mvhline(y-3, 1, 0, x-2);
    refresh();
    if (*top == NULL) {
        *top = newwin(halfy-2, x-2, 1, 1);
    }
    wresize(*top, halfy-2, x-2);
    wrefresh(*top);
    if (*bottom == NULL) {
        *bottom = newwin(halfy-4, x-2, halfy+1, 1);
    }
    wresize(*bottom, halfy-4, x-2);
    wrefresh(*bottom);
}

int main() {
    WINDOW *top = NULL, *bottom = NULL;
    initscr();
    noecho();
    draw_screen(&top, &bottom);
    int key;
    while ((key = getch()) != 'q') {
        if (key == KEY_RESIZE) {
            draw_screen(&top, &bottom);
        }
    };
    endwin();
    return 0;
}
