#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
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

typedef struct t_Args {
    u_int local_network;
    u_int remote_network;
    u_int remote_addr[6];
} t_Args;

WINDOW *remoteWin, *localWin, *inputWin;
// initial buf size is 80 chars
t_inputBuffer* buf = &(t_inputBuffer){.size=80};

// ipx sockaddr bind to
struct sockaddr_ipx sipx;
// ipx remote socket addr
struct sockaddr_ipx ripx;

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
    remoteWin = subwin(stdscr, halfy-2, x-4, 1, 2);
    wrefresh(remoteWin);
    scrollok(remoteWin, TRUE);
    localWin = subwin(stdscr, halfy-4, x-4, halfy+1, 2);
    wrefresh(localWin);
    scrollok(localWin, TRUE);
}

void draw_input_win() {
    int y = LINES, x = COLS, halfy = y/2;
    inputWin = subwin(stdscr, 1, x-4, y-2, 2);
    wrefresh(inputWin);
}

void init_ui() {
    if (initscr() == NULL) {
        fprintf(stderr, "Failed to initialize screen\n");
        exit(EXIT_FAILURE);
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
    wrefresh(remoteWin);
    wrefresh(localWin);
    wcursyncup(inputWin);
    wrefresh(inputWin);
}

void resize_handler(int sig) {
    endwin();
    refresh();
    clear();
    draw_main_box();
    draw_output_win();
    draw_input_win();
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
            wprintw(inputWin, "\b \b\0");
            if (buf->idx > 0) {
                full=FALSE;
                buf->text[buf->idx++] = '\b';
            }
            wrefresh(inputWin);
        } else {
            getyx(inputWin, cur_y, cur_x);
            if (!full && cur_x < COLS - 5) {
                if (buf->idx >= buf->size) {
                    buf->text = realloc(buf->text, sizeof(char)*(buf->size*2));
                    if (buf->text != NULL) {
                        buf->size = buf->size*2;
                    } else {
                        endwin();
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                }
                buf->text[buf->idx++] = c;
                wprintw(inputWin, "%s", (char*)&c);
            } else {
                full = TRUE;
                buf->text[buf->idx] = c;
                wprintw(inputWin, "\b%s", (char*)&c);
            }
            wrefresh(inputWin);
        }
    }
    buf->text[buf->idx] = '\0';
    wclear(inputWin);
    wrefresh(inputWin);
    wprintw(localWin, "%s\n", buf->text);
    wrefresh(localWin);
}

int ipx_bind(u_int net){
    int fd = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
    if (fd < 0) {
        endwin();
        perror("IPX: socket: ");
        exit(EXIT_FAILURE);
    }
    sipx.sipx_family = AF_IPX;
    sipx.sipx_network = htonl(net);
    sipx.sipx_port = htons(0x5000);
    sipx.sipx_type = 17;

    if (bind(fd, (struct sockaddr *) &sipx, sizeof(sipx)) < 0) {
        endwin();
        perror("IPX: bind: ");
        exit(EXIT_FAILURE);
    }
    socklen_t len = sizeof(sipx);
    // get addr where we are bound to
    getsockname(fd, (struct sockaddr *) &sipx, &len);
    return fd;
}

void ipx_set_remote_addr(u_int remote_addr[6]) {
    memcpy(ripx.sipx_node, remote_addr, 6);
    wprintw(remoteWin, "Remote addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
            remote_addr[0], remote_addr[1], remote_addr[2],
            remote_addr[3], remote_addr[4], remote_addr[5]);
    wrefresh(remoteWin);
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
            exit(EXIT_FAILURE);
        }
    }
}

void usage(char *name, int exit_status) {
    fprintf(stderr, "Usage: %s [-n network] [-r] [network:]remote_addr\n"
            "   -n NETWORK    - ipx network number like 0x1 or 0x33234 (hex)\n"
            "   -r [NET]:ADDR - remote network and addr (addr is MAC add in hex format)\n",
            name);
    exit(exit_status);
}

t_Args* get_args(int argc, char *argv[]) {
    char opt;
    char *end;
    ssize_t sscanf_result;
    char *delimiter;
    if (argc < 3) {
        usage(argv[0], EXIT_FAILURE);
    }
    t_Args* args = (t_Args*)calloc(1, sizeof(t_Args));
    while ((opt = getopt(argc, argv, "hn:r:")) != -1) {
        switch (opt) {
        case 'n':
            args->local_network = strtoul(optarg, &end, 0);
            if (errno != 0 || *end != '\0') {
                fprintf(stderr, "Failed to parse network number %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'r':
            delimiter = strchr(optarg, ':');
            if (delimiter != NULL) {
                *delimiter = '\0';
                args->local_network = strtoul(optarg, &end, 0);
                if (errno != 0 || *end != '\0') {
                    fprintf(stderr,
                        "Failed to parse remte network number %s\n", optarg
                    );
                    exit(EXIT_FAILURE);
                }
                *delimiter = ':';
                delimiter++;
            } else {
                delimiter = optarg;
            }
            sscanf_result = strspn(delimiter, "0123456789abcdefABCDEF");
            if (sscanf_result == 12) {
                sscanf_result = sscanf(
                    delimiter, "%2x%2x%2x%2x%2x%2x",
                    &args->remote_addr[0], &args->remote_addr[1],
                    &args->remote_addr[2], &args->remote_addr[3],
                    &args->remote_addr[4], &args->remote_addr[5]
                );
            } else {
                if (sscanf_result > 12) {
                    fprintf(stderr, "Max length is 12 hex digits\n");
                }
                fprintf(stderr, "Invalid remote addr, char at %ld, '%c'\n",
                        sscanf_result,
                        delimiter[sscanf_result]);
                exit(EXIT_FAILURE);
            }
            if (errno != 0 || sscanf_result < 6) {
                fprintf(stderr,
                    "Invalid remote addr %s"
                    ", expect MAC address in format 'AABBCCDDEEFF'\n"
                    "Parsed part is: %02X%02X%02X%02X%02X%02X\n",
                    delimiter,
                    args->remote_addr[0], args->remote_addr[1],
                    args->remote_addr[2], args->remote_addr[3],
                    args->remote_addr[4], args->remote_addr[5]
                );
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            usage(argv[0], EXIT_SUCCESS);
        default: /* '?' */
            usage(argv[0], EXIT_FAILURE);
        }
    }
    return args;
}

int main(int argc, char *argv[]) {

    t_Args* args = get_args(argc, argv);

    signal(SIGWINCH, resize_handler);
    setlocale(LC_ALL, ""); /* make sure UTF8 */
    buf->text = calloc(buf->size, sizeof(char));

    init_ui();
    int fd = ipx_bind(args->local_network);
    ipx_set_remote_addr(args->remote_addr);

    while (TRUE) {
        refresh_all_win();
        user_input();
        ipx_send(fd, buf->text, buf->idx+1);
    };
    endwin();
    return 0;
}
