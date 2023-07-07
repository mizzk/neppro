//
// Created by li-xinjia on 23/06/08.
//

/**
 * 起動は、./client --p 50001 --u anonymousで、ポート番号と自分のユーザー名を指定することができる。
 * ./clientそのまま起動すると、ポート50001、ユーザー名をanonymousに設定し起動する。
 * 入力したいメッセージをそのまま入力し、enterを押したら発信する。
 * ウスのスクロールやキーボードのUPとDOWNキーで、今のChat Roomの表示するメッセージをスクロールできる変える。
 * 演習中に作成したmynetライブラリを使用して作成しました。
 * 問題があるクライアントへの対応
    * 1 : 1回目のHEREを無視する
       * HERE送る機能とconnect機能は独立で実行しているので特に問題ない
    * 2 : HEREを受け取った後、connectしない
        * 1と同じ
    * 3 : connectした後、JOINを送らない
        * JOINを送らない場合、JOINを送るまで発言できない
    * 4 : JOINの時、15文字を超えるユーザ名を送る
        * 強制的に15字以内のusernameにする（usernameを切る）
    * 5 : メッセージを送る時、POSTを付けない
        * そのメッセージを無視
    * 6 : メッセージを送る時、MESGを付ける
        * そのメッセージを無視
 */

#include "mynet.h"

#include <curses.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <arpa/inet.h>

#define LOCALHOST           "localhost"    /* localホスト名 */
#define PORT                50001          /* ポート番号 */
#define USERNAME            "anonymous"    /* デフォルトusername */
#define BUFSIZE             512            /* 最大message長さ */
#define MAX_USERNAME_LENGTH 15
#define MAX_MESSAGE_LENGTH  488
#define MAX_RETRIES         2
#define TIMEOUT_SEC         2

#define KEY_ESC             27

#define HELLO       1
#define HERE        2
#define JOIN        3
#define POST        4
#define MESSAGE     5
#define QUIT        6

struct message_list {
    unsigned char is_my_message;
    char username[MAX_USERNAME_LENGTH];
    char message[MAX_MESSAGE_LENGTH];
    struct message_list *next;
};

struct connect_info {
    unsigned char as_server;
    unsigned char is_connected;
    unsigned char request_shutdown;
    int sock;
    int port;
    int retry;
    char my_username[MAX_USERNAME_LENGTH];
    pthread_t *tid;
};

struct imember {
    char username[MAX_USERNAME_LENGTH];     /* ユーザ名 */
    int sock;                               /* ソケット番号 */
    struct imember *next;                   /* 次のユーザ */
};

struct idobata {
    char header[4];                         /* パケットのヘッダ部(4バイト) */
    char sep;                               /* セパレータ(空白、またはゼロ) */
    char data[MAX_MESSAGE_LENGTH];          /* データ部分(メッセージ本体) */
};

extern char *optarg;
extern int optind, opterr, optopt;

/*
 *  Help情報を出力関数
 */
void print_help() {
    printf("Usage: ./server_fork [options]\n");
    printf("Options:\n");
    printf("  --h --help                       Display this help message.\n");
    printf("  --p --port [port]                Set port number. Default is 50001.\n");
    printf("  --u --username [username]        Set username. Default is anonymous.\n");
}

/*
 *  Serverにメッセージ情報発信
 */
long net_send(int socket, char *msg) {
    return send(socket, msg, BUFSIZE, 0);
}

/*
 *  Serverから情報受信
 */
long net_read(int socket, char *buf) {
    return recv(socket, buf, BUFSIZE, 0);
}

/*
 *  Member列にmemberを追加
 */
void add_member(struct imember *imember, int sock) {
    while (imember->next != NULL) {
        imember = imember->next;
    }
    imember->next = calloc(sizeof(struct imember), 1);
    imember->next->sock = sock;
}

/*
 *  Member列にmemberを削除
 */
void remove_member(struct imember *imember, int sock) {
    struct imember *buff;
    while (imember->next != NULL) {
        if (imember->next->sock == sock) {
            buff = imember->next;
            imember->next = imember->next->next;
            free(buff);
            return;
        }
        imember = imember->next;
    }
}

/*
 *  受信したheaderを解析
 */
u_int32_t analyze_header(char *header) {
    if (strncmp(header, "HELO", 4) == 0) return (HELLO);
    if (strncmp(header, "HERE", 4) == 0) return (HERE);
    if (strncmp(header, "JOIN", 4) == 0) return (JOIN);
    if (strncmp(header, "POST", 4) == 0) return (POST);
    if (strncmp(header, "MESG", 4) == 0) return (MESSAGE);
    if (strncmp(header, "QUIT", 4) == 0) return (QUIT);
    return 0;
}

/*
 *  usernameのチェック
 */
unsigned char check_username(char *str) {
    for (int i = 0; i < strlen(str); ++i) {
        if (str[i] == '\n') {
            str[i] = '\0';
        }
    }
    if (strlen(str) > 15) {
        str[14] = '\0';
    }
    return str[0];
}

/*
 *  udp serverとしてのスレッド
 */
void *act_udp_server(void *arg) {
    struct connect_info **tharg = (struct connect_info **) arg;
    int udp_sock = init_udpserver((*tharg)->port);
    char buffer[BUFSIZ];
    struct sockaddr_in server_addr, from_adrs;
    socklen_t from_len = sizeof(from_adrs);
    char *message = "HERE";

    while (1) {
        if (recvfrom(udp_sock, buffer, 4, 0, (struct sockaddr *) &from_adrs, &from_len) < 0) {
            (*tharg)->request_shutdown = 1;
        }

        if (strcmp(buffer, "HELO") != 0) {
            continue;
        }

        if (sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *) &from_adrs, sizeof(from_adrs)) < 0) {
            (*tharg)->request_shutdown = 1;
        }

        if ((*tharg)->request_shutdown) {
            return NULL;
        }
    }
}

/*
 *  tcp serverとしてのスレッド
 */
void *act_tcp_server(void *arg) {
    struct connect_info **tharg = (struct connect_info **) arg;
    int tcp_sock = init_tcpserver((*tharg)->port, 3);
    struct imember *imember_p = calloc(sizeof(struct imember), 1), *first_p = imember_p, *buff_p;
    int sd;
    struct sockaddr_in address;
    int addrlen;
    char buff[BUFSIZ];
    char message[512];

    fd_set readfds;
    struct timeval tv;
    int max_fd;

    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_sock, &readfds);
        max_fd = tcp_sock;

        imember_p = first_p;
        while (imember_p != NULL) {
            if (imember_p->sock > 0) {
                FD_SET(imember_p->sock, &readfds);
            }

            if (imember_p->sock > max_fd) {
                max_fd = imember_p->sock;
            }

            imember_p = imember_p->next;
        }

        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(tcp_sock, &readfds)) {
            if ((sd = accept(tcp_sock, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }

            add_member(first_p, sd);
        }

        imember_p = first_p;
        while (imember_p != NULL) {
            sd = imember_p->sock;
            memset(buff, 0, BUFSIZE);

            if (sd == 0) {
                imember_p = imember_p->next;
                continue;
            }

            if (FD_ISSET(sd, &readfds)) {
                if ((read(sd, buff, BUFSIZE)) == 0) {
                    close(sd);
                    remove_member(first_p, imember_p->sock);
                } else {
                    struct idobata *packet;

                    packet = (struct idobata *) buff;

                    switch (analyze_header(packet->header)) {
                        case HELLO:
                        case HERE:
                        case MESSAGE:
                            break;
                        case JOIN:
                            if (!check_username(packet->data)) { break; }
                            strcpy(imember_p->username, packet->data);
                            break;
                        case POST:
                            if (!imember_p->username[0]) { break; }
                            buff_p = first_p;
                            sprintf(message, "MESG [%s]%s\n", imember_p->username, packet->data);
                            while (buff_p != NULL) {
                                net_send(buff_p->sock, message);
                                buff_p = buff_p->next;
                                if (buff_p == imember_p) { buff_p = buff_p->next; }
                            }
                            break;
                        case QUIT:
                            close(sd);
                            remove_member(first_p, imember_p->sock);
                            break;
                    }
                }
            }
            imember_p = imember_p->next;
        }
        if ((*tharg)->request_shutdown) {
            return NULL;
        }
    }
}

/*
 *  net機能としてのスレッド
 */
void *init_net(void *arg) {
    struct connect_info **tharg = (struct connect_info **) arg;
    (*tharg)->is_connected = 0;
    (*tharg)->retry = 0;
    struct sockaddr_in server_addr, from_adrs;
    struct timeval timeout;
    int udp_sock = init_udpclient();
    int broadcast_sw = 1;
    fd_set readfds;
    socklen_t from_len = sizeof(from_adrs);

    if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcast_sw, sizeof(broadcast_sw)) == -1) {
        (*tharg)->request_shutdown = 1;
        return NULL;
    }

    char *message = "HELO";
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((*tharg)->port);
    server_addr.sin_addr.s_addr = INADDR_BROADCAST;

    while ((*tharg)->retry <= MAX_RETRIES && !(*tharg)->is_connected) {
        if (sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            (*tharg)->request_shutdown = 1;
            return NULL;
        }

        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(udp_sock, &readfds);

        int activity = select(udp_sock + 1, &readfds, NULL, NULL, &timeout);
        if (activity == -1) {
            (*tharg)->request_shutdown = 1;
            return NULL;
        } else if (activity == 0) {
            (*tharg)->retry++;
        } else {
            if (FD_ISSET(udp_sock, &readfds)) {
                char buffer[BUFSIZ];
                memset(buffer, 0, sizeof(buffer));

                if (recvfrom(udp_sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &from_adrs, &from_len) < 0) {
                    (*tharg)->request_shutdown = 1;
                    return NULL;
                }

                if (strcmp(buffer, "HERE") != 0) {
                    (*tharg)->request_shutdown = 1;
                    return NULL;
                }

                (*tharg)->sock = init_tcpclient(inet_ntoa(from_adrs.sin_addr), (*tharg)->port);
                (*tharg)->as_server = 0;
                (*tharg)->is_connected = 1;
            }
        }
    }

    if (!(*tharg)->is_connected) {
        (*tharg)->as_server = 1;
        pthread_create((*tharg)->tid, NULL, act_udp_server, arg);
        pthread_create((*tharg)->tid, NULL, act_tcp_server, arg);
        (*tharg)->sock = init_tcpclient(LOCALHOST, (*tharg)->port);
        (*tharg)->is_connected = 1;
    }

    while (1) {
        if ((*tharg)->request_shutdown) { return NULL; }
    }
}

/*
 *  GUI機能としてのスレッド
 */
void *init_GUI(void *arg) {
    struct connect_info **tharg = (struct connect_info **) arg;
    int roll_x = 9;
    fd_set readfds;
    struct timeval tv;
    int ch;

    initscr();
    noecho();
    curs_set(0);

    start_color();

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);

    int win_width = 40;
    int win_height = 5;
    int win_x = (max_x - win_width) / 2;
    int win_y = (max_y - win_height) / 2;

    WINDOW *win = newwin(win_height, win_width, win_y, win_x);
    box(win, 0, 0);
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);

    refresh();
    wrefresh(win);

    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    while (!(*tharg)->is_connected) {
        if ((*tharg)->request_shutdown) {
            delwin(win);
            endwin();
            exit(EXIT_SUCCESS);
        }

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            ch = getch();
            if (ch == KEY_ESC) {
                delwin(win);
                endwin();
                exit(EXIT_SUCCESS);
            }
        }

        getmaxyx(stdscr, max_y, max_x);

        win_x = (max_x - win_width) / 2;
        win_y = (max_y - win_height) / 2;

        mvwin(win, win_y, win_x);

        clear();
        box(win, 0, 0);
        mvwprintw(win, 1, 1, "Waiting ........ (press Esc to quit)");
        mvwprintw(win, 1, roll_x++, "'");
        if (roll_x > 16) { roll_x = 9; }
        mvwprintw(win, 2, 1, "Retry (%d/%d)", (*tharg)->retry, MAX_RETRIES);
        mvwprintw(win, 3, 1, "Username: %s", (*tharg)->my_username);

        refresh();
        wrefresh(win);

        usleep(50000);
    }

    wclear(win);
    refresh();
    WINDOW *sub_win = newwin(1, 1, 0, 0);
    WINDOW *input_win = newwin(1, 1, 0, 0);
    WINDOW *input_sub_win = newwin(1, 1, 0, 0);
    char input_buff[MAX_MESSAGE_LENGTH] = {0};
    char trans_buff[BUFSIZE] = {0};
    keypad(stdscr, TRUE);

    int old_x = 0, old_y = 0;
    struct message_list list;
    list.next = NULL;
    unsigned int now_line = 0;
    unsigned char get_msg = 0, max_msg = 0;

    sprintf(trans_buff, "JOIN %s", (*tharg)->my_username);
    net_send((*tharg)->sock, trans_buff);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET((*tharg)->sock, &readfds);

        select((*tharg)->sock + 1, &readfds, NULL, NULL, &tv);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            ch = getch();

            if (ch == KEY_ESC) {
                (*tharg)->request_shutdown = 1;
                sprintf(input_buff, "QUIT");
                net_send((*tharg)->sock, input_buff);
                delwin(win);
                delwin(input_win);
                endwin();
                close((*tharg)->sock);
                exit(EXIT_SUCCESS);
            } else if (ch == 10) {                         // Enter
                if (strcmp(input_buff, "QUIT") == 0) {
                    (*tharg)->request_shutdown = 1;
                    net_send((*tharg)->sock, input_buff);
                    delwin(win);
                    delwin(input_win);
                    endwin();
                    close((*tharg)->sock);
                    exit(EXIT_SUCCESS);
                }

                struct message_list *p = &list;
                while (p->next != NULL) { p = p->next; }
                strcpy(p->username, (*tharg)->my_username);
                strcpy(p->message, input_buff);
                p->is_my_message = 1;
                p->next = malloc(sizeof(struct message_list));
                p->next->next = NULL;
                get_msg = 1;
                max_msg++;

                sprintf(trans_buff, "POST %s", input_buff);
                net_send((*tharg)->sock, trans_buff);
                memset(input_buff, 0, MAX_MESSAGE_LENGTH);
                wclear(input_sub_win);
            } else if (ch == 263) {                         //Backspace
                if (strlen(input_buff) > 0) {
                    input_buff[strlen(input_buff) - 1] = '\0';
                    wclear(input_sub_win);
                }
            } else if (ch == 258) {                          //DOWN
                if (now_line + 1 <= max_msg) {
                    now_line++;
                    werase(sub_win);
                }
            } else if (ch == 259) {                          //UP
                if (now_line != 0) {
                    now_line--;
                    werase(sub_win);
                }
            } else {
                char str[2];
                str[0] = (char) ch;
                if (str[0] == -102) { str[0] = '\0'; }       //~Z
                str[1] = '\0';
                strcat(input_buff, str);
            }
        }

        if (FD_ISSET((*tharg)->sock, &readfds)) {
            char buff[BUFSIZE];
            if (!net_read((*tharg)->sock, buff)) {
                close((*tharg)->sock);
                (*tharg)->request_shutdown = 1;
                delwin(win);
                endwin();
                printf("Server is down.\n");
                exit(EXIT_FAILURE);
            }
            char index[10] = {0};
            strncpy(index, buff, 4);
            if (strcmp(index, "MESG") == 0) {
                struct message_list *p = &list;
                while (p->next != NULL) { p = p->next; }
                sscanf(buff, "%*[^[][%[^]]]%[^\n]",p->username, p->message);
                p->is_my_message = 0;

                p->next = malloc(sizeof(struct message_list));
                p->next->next = NULL;

                get_msg = 1;
                max_msg++;
            }
        }

        getmaxyx(stdscr, max_y, max_x);
        if (max_x != old_x || max_y != old_y) {
            wclear(stdscr);

            wclear(win);
            mvwin(win, 1, 0);
            wresize(win, 3 * max_y / 4 - 1, max_x);
            box(win, 0, 0);

            wclear(sub_win);
            mvwin(sub_win, 2, 1);
            wresize(sub_win, 3 * max_y / 4 - 3, max_x - 2);

            wclear(input_win);
            mvwin(input_win, 3 * max_y / 4, 0);
            wresize(input_win, max_y / 4, max_x);
            box(input_win, 0, 0);

            wclear(input_sub_win);
            mvwin(input_sub_win, 3 * max_y / 4 + 1, 1);
            wresize(input_sub_win, max_y / 4 - 2, max_x - 2);

            refresh();
            wrefresh(win);
            wrefresh(sub_win);
            wrefresh(input_win);
            wrefresh(input_sub_win);

            old_x = max_x;
            old_y = max_y;
        }
        attron(A_BOLD | COLOR_PAIR(1));
        mvwprintw(stdscr, 0, max_x / 2 - 25, "Idobata Chat by Li-xinjia (press Esc or type QUIT to quit)");
        attroff(A_BOLD | COLOR_PAIR(1));
        if ((*tharg)->as_server) {
            mvwprintw(win, 0, 1, "Chat room (as server)");
        } else {
            mvwprintw(win, 0, 1, "Chat room (as client)");
        }

        mvwprintw(input_win, 0, 1, "Message");

        wprintw(input_sub_win, "%s", input_buff);
        wmove(input_sub_win, 0, 0);

        struct message_list *p = &list;
        for (int i = 0; i < now_line; ++i) {
            p = p->next;
        }

        if (now_line != 0) {
            wattron(sub_win, COLOR_PAIR(1) | A_BOLD);
            wprintw(sub_win, "<------Continue------>\n");
            wattroff(sub_win, COLOR_PAIR(1) | A_BOLD);
        }

        while (p->next != NULL) {
            if ((getcury(sub_win) == getmaxy(sub_win) - 2) || (getcury(sub_win) == getmaxy(sub_win) - 1)) {
                wattron(sub_win, COLOR_PAIR(1) | A_BOLD);
                mvwprintw(sub_win, getmaxy(sub_win) - 1, 0, "<------Continue------>");
                wattroff(sub_win, COLOR_PAIR(1) | A_BOLD);
                break;
            }
            if (p->is_my_message) {
                wattron(sub_win, COLOR_PAIR(2) | A_BOLD);
            } else {
                wattron(sub_win, COLOR_PAIR(3) | A_BOLD);
            }
            wprintw(sub_win, "Name:%s\n", p->username);
            wprintw(sub_win, "  %s\n\n", p->message);
            if (p->is_my_message) {
                wattroff(sub_win, COLOR_PAIR(2) | A_BOLD);
            } else {
                wattroff(sub_win, COLOR_PAIR(3) | A_BOLD);
            }
            p = p->next;
        }

        if (((getcury(sub_win) == getmaxy(sub_win) - 2) || (getcury(sub_win) == getmaxy(sub_win) - 1)) && get_msg) {
            now_line++;
            werase(sub_win);
        } else {
            get_msg = 0;
        }

        wmove(sub_win, 0, 0);

        refresh();
        wrefresh(win);
        wrefresh(sub_win);
        wrefresh(input_win);
        wrefresh(input_sub_win);
    }
}

int main(int argc, char *argv[]) {
    int sock;

    pthread_t tid;
    struct connect_info connect, *connect_p = &connect;
    connect.port = PORT;
    connect.request_shutdown = 0;
    connect.tid = &tid;
    snprintf(connect.my_username, MAX_USERNAME_LENGTH, USERNAME);

    struct option long_options[] = {
            {"h",        no_argument,       0, 0},
            {"help",     no_argument,       0, 0},
            {"t",        required_argument, 0, 0},
            {"port",     required_argument, 0, 0},
            {"u",        required_argument, 0, 0},
            {"username", required_argument, 0, 0},
    };
    opterr = 0;

    int c, index;
    while (1) {
        c = getopt_long(argc, argv, "", long_options, &index);
        if (c == -1) break;

        switch (index) {
            default:
            case 0:
            case 1:
                print_help();
                exit(EXIT_SUCCESS);
            case 2:
            case 3:
                connect.port = atoi(optarg);
                break;
            case 4:
            case 5:
                snprintf(connect.my_username, MAX_USERNAME_LENGTH, "%s", optarg);
                break;
        }
    }

    pthread_create(&tid, NULL, init_net, (void *) &connect_p);
    pthread_create(&tid, NULL, init_GUI, (void *) &connect_p);

    pthread_exit(NULL);
    return 0;
}
