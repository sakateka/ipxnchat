#include <curses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

# define MAXINPUT 300

WINDOW *remote, *local, *input;
char inputText[MAXINPUT] = {0};

void init_colors() {
   start_color();
   use_default_colors();
   init_pair(1, -1, -1);
   init_pair(2, COLOR_RED, -1);
   init_pair(3, COLOR_GREEN, -1);
   init_pair(4, COLOR_BLUE, -1);
   init_pair(5, COLOR_MAGENTA, -1);
   init_pair(6, COLOR_CYAN, -1);
   init_pair(7, COLOR_YELLOW, -1);
}

void draw_main_box() {
    int y = LINES, x = COLS, halfy = y/2;
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
    refresh();
}

void draw_output_win() {
    int y = LINES, x = COLS, halfy = y/2;
    remote = subwin(stdscr, halfy-2, x-4, 1, 2);
    wrefresh(remote);
    scrollok(remote, TRUE);
    local = subwin(stdscr, halfy-4, x-4, halfy+1, 2);
    wrefresh(local);
    scrollok(remote, TRUE);
}

void draw_input_win() {
    int y = LINES, x = COLS, halfy = y/2;
    input = subwin(stdscr, 1, x-4, y-2, 2);
    wrefresh(input);
}

void init_ui() {
    if (initscr() == NULL) {
        fprintf(stderr, "Failed to initialize screen");
        exit(1);
    }
    init_colors();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    draw_main_box();
    draw_output_win();
    draw_input_win();
}

void refresh_all_win(){
    wrefresh(remote);
    wrefresh(local);
    wcursyncup(input);
    wrefresh(input);
}

void resize_handler(int sig) {
    endwin();
    refresh();
    clear();
    draw_main_box();
    draw_output_win();
    draw_input_win();
    static int counter = 0;
    wprintw(input, "");
    wprintw(input, "sig handler %d", ++counter);
    refresh_all_win();
    signal(SIGWINCH, resize_handler);
}

int user_input() {
    int t_idx = 0;
    int c;
    int max_input = COLS;
    while ((c = getch()) != '\n') {
        if (c == KEY_RESIZE || c == ERR) {
           continue;
        } else if (c == KEY_BACKSPACE || c == KEY_LEFT) {
            wprintw(input, "\b \b\0");
            if (t_idx > 0) {
                inputText[--t_idx] = '\0';
            }
            wrefresh(input);
        } else {
            max_input = (COLS - 5) >= MAXINPUT
                ? MAXINPUT-1 /*screen bigger then max input*/
                : COLS - 5 /* screen small enought */;

            if (t_idx < max_input) {
                inputText[t_idx++] = c;
                wprintw(input, (char *)&c);
            } else {
                inputText[t_idx] = c;
                wprintw(input, "\b%s", (char *)&c);
            }
            wrefresh(input);
        }
    }
    inputText[t_idx] = '\0';
    wclear(input);
    wrefresh(input);
    return t_idx;
}

int main() {
    signal(SIGWINCH, resize_handler);
    init_ui();

    // TODO bind

    while (TRUE) {
        refresh_all_win();
        user_input();
    };
    endwin();
    return 0;
}
