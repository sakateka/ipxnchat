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
#include <pthread.h>

typedef struct t_inputBuffer {
    int size;
    int idx;
    char *text;
} t_inputBuffer;

typedef struct t_Args {
    u_int32_t local_network;
    u_int32_t remote_network;
    unsigned char remote_addr[6];
} t_Args;
t_Args args = {0};

WINDOW *remoteWin, *localWin, *inputWin;
char remoteTitle[45] = "[ Remote ]\0";
char localTitle[45] = "[ Local ]\0";
// initial buf size is 80 chars
t_inputBuffer* buf = &(t_inputBuffer){.size=80};

// ipx sockaddr bind to
struct sockaddr_ipx sipx;
// ipx remote socket addr

// receiver
pthread_t rx_thread;

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
    mvprintw(0, 3, remoteTitle);

    mvaddch(halfy, 0, ACS_LTEE);
    mvhline(halfy, 1, 0, x-2);
    mvaddch(halfy, x-1, ACS_RTEE);

    mvprintw(halfy, 3, localTitle);

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
    int y = LINES, x = COLS;
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

int ipx_bind(){
    int fd = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
    if (fd < 0) {
        endwin();
        perror("IPX: socket: ");
        exit(EXIT_FAILURE);
    }
    socklen_t len = sizeof(sipx);

    sipx.sipx_family = AF_IPX;
    sipx.sipx_network = htonl(args.local_network);
    sipx.sipx_port = 0;
    sipx.sipx_type = 17;

    if (bind(fd, (struct sockaddr *) &sipx, len) < 0) {
        endwin();
        perror("IPX: bind: ");
        exit(EXIT_FAILURE);
    }
    // get addr where we are bound to
    getsockname(fd, (struct sockaddr *) &sipx, &len);
    snprintf(localTitle, 45, "[ Local addr: 0x%08X:%02X%02X%02X%02X%02X%02X:%04X ]",
            ntohl(sipx.sipx_network),
            sipx.sipx_node[0], sipx.sipx_node[1], sipx.sipx_node[2],
            sipx.sipx_node[3], sipx.sipx_node[4], sipx.sipx_node[5],
            ntohs(sipx.sipx_port));
    mvprintw(LINES/2, 3, localTitle);
    refresh();
    return fd;
}

void ipx_set_remote_addr() {
    memcpy(sipx.sipx_node, args.remote_addr, 6);
    sipx.sipx_port = htons(0x5000);
    snprintf(remoteTitle, 45, "[ Remote addr: 0x%08X:%02X%02X%02X%02X%02X%02X:%04X ]",
            ntohl(sipx.sipx_network),
            sipx.sipx_node[0], sipx.sipx_node[1], sipx.sipx_node[2],
            sipx.sipx_node[3], sipx.sipx_node[4], sipx.sipx_node[5],
            ntohs(sipx.sipx_port));
    mvprintw(0, 3, remoteTitle);
    refresh();
}

void ipx_send(int fd) {
    if (buf->idx > 0) {
        ssize_t result = sendto(
            fd,
            buf->text, buf->idx+1,
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

void get_args(int argc, char *argv[]) {
    char opt;
    char *end;
    ssize_t sscanf_result;
    char *delimiter;
    if (argc < 3) {
        usage(argv[0], EXIT_FAILURE);
    }
    while ((opt = getopt(argc, argv, "hn:r:")) != -1) {
        switch (opt) {
        case 'n':
            args.local_network = strtol(optarg, &end, 0);
            if (errno != 0 || *end != '\0') {
                fprintf(stderr, "Failed to parse network number %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'r':
            delimiter = strchr(optarg, ':');
            if (delimiter != NULL) {
                *delimiter = '\0';
                args.remote_network = strtol(optarg, &end, 0);
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
                    #pragma clang diagnostic push
                    #pragma clang diagnostic ignored "-Wformat"
                    &args.remote_addr[0], &args.remote_addr[1],
                    &args.remote_addr[2], &args.remote_addr[3],
                    &args.remote_addr[4], &args.remote_addr[5]
                    #pragma clang diagnostic pop
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
                    args.remote_addr[0], args.remote_addr[1],
                    args.remote_addr[2], args.remote_addr[3],
                    args.remote_addr[4], args.remote_addr[5]
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
}

void *receiver(void *arg) {
    char rBuf[2048];

    struct sockaddr_ipx ripx;

    wprintw(remoteWin, "Create socket: ");
    wrefresh(remoteWin);
	int s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
        wprintw(remoteWin, "IPX: socket: %s\n", strerror(errno));
        wrefresh(remoteWin);
        return NULL;
	}
    wprintw(remoteWin, "Success\n");
    wrefresh(remoteWin);

    ripx.sipx_family = AF_IPX;
    ripx.sipx_network = htonl(args.remote_network);
    ripx.sipx_port = htons(0x5000);
    ripx.sipx_type = 17;
    socklen_t len = sizeof(ripx);

    wprintw(remoteWin, "Bind receive socket: ");
    wrefresh(remoteWin);
	int result = bind(s, (struct sockaddr *) &ripx, len);
	if (result < 0) {
        wprintw(remoteWin, "IPX: bind: %s\n", strerror(errno));
        wrefresh(remoteWin);
        return NULL;
	}
    wprintw(remoteWin, "Success\n");
    wrefresh(remoteWin);
    wprintw(remoteWin, "Success start receive thread\n");
    wrefresh(remoteWin);
    while (1) {
        ssize_t result = recvfrom(s, &rBuf, sizeof(rBuf), 0, (struct sockaddr *) &ripx, &len);
        if (result < 0) {
            wprintw(remoteWin, "IPX: recvfrom: %s\n", strerror(errno));
            break;
        }
        rBuf[result] = '\0';
        wprintw(remoteWin, "%s\n", rBuf);
        wrefresh(remoteWin);
        refresh();
    }
    return NULL;
}

void start_receiver() {
    if (pthread_create(&rx_thread, NULL, receiver, NULL)) {
        char *errMsg = strerror(errno);
        endwin();
        fprintf(stderr, "Failed to start receive thread\n");
        fprintf(stderr, "pthread_create: %s\n", errMsg);
    }
}

int main(int argc, char *argv[]) {

    get_args(argc, argv);

    setlocale(LC_ALL, ""); /* make sure UTF8 */
    buf->text = calloc(buf->size, sizeof(char));

    init_ui();
    signal(SIGWINCH, resize_handler);
    int fd = ipx_bind();
    ipx_set_remote_addr();

    start_receiver();
    while (TRUE) {
        refresh_all_win();
        user_input();
        ipx_send(fd);
    };
    endwin();
    return 0;
}
