#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <ncursesw/ncurses.h>
#include <sys/types.h>
#include <linux/ipx.h>
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct t_inputBuffer {
    int size;
    int idx;
    char *text;
} t_inputBuffer;

WINDOW *remote, *local, *input;
// initial buf size is 80 chars
t_inputBuffer* buf = &(t_inputBuffer){.size=80};

// ipx sockaddr bind to
struct sockaddr_ipx sipx;
struct sockaddr_ipx raddr;

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
    scrollok(local, TRUE);
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

void user_input() {
    int cur_y, cur_x, c = 0;
    buf->idx = 0;
    _Bool full = FALSE;

    while ((c = getch()) != '\n') {
        if (c == KEY_RESIZE || c == ERR) {
           continue;
        } else if (c == KEY_BACKSPACE || c == KEY_LEFT) {
            wprintw(input, "\b \b\0");
            if (buf->idx > 0) {
                full=FALSE;
                buf->text[buf->idx++] = '\b';
            }
            wrefresh(input);
        } else {
            getyx(input, cur_y, cur_x);
            if (!full && cur_x < COLS - 5) {
                if (buf->idx >= buf->size) {
                    buf->text = realloc(buf->text, sizeof(char)*(buf->size*2));
                    if (buf->text != NULL) {
                        buf->size = buf->size*2;
                    } else {
                        endwin();
                        perror("realloc");
                        exit(1);
                    }
                }
                buf->text[buf->idx++] = c;
                wprintw(input, "%s", (char*)&c);
            } else {
                full = TRUE;
                buf->text[buf->idx] = c;
                wprintw(input, "\b%s", (char*)&c);
            }
            wrefresh(input);
        }
    }
    buf->text[buf->idx] = '\0';
    wclear(input);
    wrefresh(input);
    wprintw(local, "%s\n", buf->text);
    wrefresh(local);
}

int ipx_bind(int net){
    int fd = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
    if (fd < 0) {
        endwin();
        perror("IPX: socket: ");
        exit(1);
    }
    sipx.sipx_family = AF_IPX;
    sipx.sipx_network = htonl(net);
    sipx.sipx_port = htons(0x5000);
    sipx.sipx_type = 17;

    if (bind(fd, (struct sockaddr *) &sipx, sizeof(sipx)) < 0) {
        endwin();
        perror("IPX: bind: ");
        exit(1);
    }
    socklen_t len = sizeof(sipx);
    // get addr where we are bound to
    getsockname(fd, (struct sockaddr *) &sipx, &len);
    return fd;
}

_Bool is_valid(char c) {
    return (c >= 'a' && c <= 'f') || (c >= 'a' && c <= 'F') || (c >= '0' && c <= '9');
}

void ipx_set_remote_addr(char remote_addr[12]) {
    char hex[5] = "0x00\0";
    for (int i = 0; i < 6; i++) {
        char f = remote_addr[i*2];
        char s = remote_addr[i*2+1];
        if (is_valid(f) && is_valid (s)) {
            hex[2] = f;
            hex[3] = s;
        } else {
            endwin();
            printf(
                "Invalid remote addr %12s, expect MAC address in format 'AABBCCDDEEFF'\n",
                remote_addr
            );
            exit(1);
        }
        wprintw(remote, "%s\n", hex);
        sipx.sipx_node[i] = atoi(hex);
    }
}

void ipx_send(int fd, const char *msg, uint msg_size) {
    if (buf->idx > 0) {
        ssize_t result = sendto(
            fd,
            msg, msg_size,
            0,
            (struct sockaddr *)&sipx, sizeof(sipx)
        );
        if (result < 0) {
            endwin();
            perror("IPX: send: ");
            exit(1);
        }
    }
}

int main() {
    signal(SIGWINCH, resize_handler);
    setlocale(LC_ALL, ""); /* make sure UTF8 */
    buf->text = calloc(buf->size, sizeof(char));

    init_ui();
    int net_num = 0;
    int fd = ipx_bind(net_num);
    char remote_addr[12] = "abcdeffedcba";
    ipx_set_remote_addr(remote_addr);

    while (TRUE) {
        refresh_all_win();
        user_input();
        ipx_send(fd, buf->text, buf->idx+1);
    };
    endwin();
    return 0;
}
