// idobata.c
// 21122051 MIZUTANI Kota

#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "mynet.h"

#define HELLO 1
#define HERE 2
#define JOIN 3
#define POST 4
#define MESSAGE 5
#define QUIT 6

#define USERNAME_LEN 16     // ユーザ名格納用バッファサイズ
#define MESSAGE_LEN 256     // メッセージ格納用バッファサイズ
#define PACKET_LEN 512      // パケット格納用バッファサイズ
#define DEFAULT_PORT 50001  // ポート番号のデフォルト値
#define TIMEOUT_SEC 1       // タイムアウト時間

// パケットの構造を表す構造体
typedef struct {
    char header[4];
    char sep;
    char data[];
} idobata_packet;

// ユーザの情報を格納する構造体
typedef struct imember {
    char username[USERNAME_LEN]; /* ユーザ名 */
    int sock;                    /* ソケット番号 */
    struct imember *next;        /* 次のユーザ */
} imember;

// スレッド関数の引数
struct myarg {
    char username[USERNAME_LEN];
    int sock;
    int port;
    pthread_t *tid;
};

// オプション用文字列
extern char *optarg;
extern int optind, opterr, optopt;

// グローバル変数
char server_address[512];  // HEREパケットを受信したサーバのIPアドレス

// imember構造体の追加関数
void add_imember(imember *head, char *username, int sock) {
    imember *p;

    // リストの末尾まで移動する
    p = head;
    while (p->next != NULL) {
        p = p->next;
    }

    // 新しいノードを作成する
    p->next = (imember *)malloc(sizeof(imember));
    p = p->next;
    if (username != NULL) {
        strcpy(p->username, username);
    }
    p->sock = sock;
    p->next = NULL;
}

// imember構造体の削除関数
void delete_imember(imember *head, int sock) {
    imember *p, *q;

    // リストの先頭から探索する
    p = head;
    while (p->next != NULL) {
        if (p->next->sock == sock) {
            // ノードを削除する
            q = p->next;
            p->next = p->next->next;
            free(q);
            return;
        }
        p = p->next;
    }
}

// imember構造体のsocket番号からusernameを取得する関数
char *get_username(imember *head, int sock) {
    imember *p;

    // リストの先頭から探索する
    p = head;
    while (p->next != NULL) {
        if (p->next->sock == sock) {
            return p->next->username;
        }
        p = p->next;
    }
    return NULL;
}

// パケットを作成する関数
char *create_packet(char *buffer, u_int32_t type, char *message) {
    switch (type) {
        case HELLO:
            snprintf(buffer, PACKET_LEN, "HELO");
            break;
        case HERE:
            snprintf(buffer, PACKET_LEN, "HERE");
            break;
        case JOIN:
            snprintf(buffer, PACKET_LEN, "JOIN %s", message);
            break;
        case POST:
            snprintf(buffer, PACKET_LEN, "POST %s", message);
            break;
        case MESSAGE:
            snprintf(buffer, PACKET_LEN, "MESG %s", message);
            break;
        case QUIT:
            snprintf(buffer, PACKET_LEN, "QUIT");
            break;
        default:
            /* Undefined packet type */
            return (NULL);
            break;
    }
    return (buffer);
}

// パケットのヘッダを解析する関数
u_int32_t analyze_header(char *header) {
    if (strncmp(header, "HELO", 4) == 0) return (HELLO);
    if (strncmp(header, "HERE", 4) == 0) return (HERE);
    if (strncmp(header, "JOIN", 4) == 0) return (JOIN);
    if (strncmp(header, "POST", 4) == 0) return (POST);
    if (strncmp(header, "MESG", 4) == 0) return (MESSAGE);
    if (strncmp(header, "QUIT", 4) == 0) return (QUIT);
    return 0;
}

// プログラムの起動処理を行う関数
int start_idobata(int port_number) {
    // 変数定義
    struct sockaddr_in broadcast_adrs;
    struct sockaddr_in from_adrs;
    socklen_t from_len;

    int sock;
    int broadcast_sw = 1;
    fd_set mask, readfds;
    struct timeval timeout;

    char s_buf[PACKET_LEN], r_buf[PACKET_LEN];
    int strsize;

    // ブロードキャストアドレスの情報をsockaddr_in構造体に格納する
    memset(&broadcast_adrs, 0, sizeof(broadcast_adrs));
    broadcast_adrs.sin_family = AF_INET;
    broadcast_adrs.sin_port = htons(port_number);
    broadcast_adrs.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // ソケットをDGRAMモードで作成する
    sock = init_udpclient();

    // ソケットをブロードキャスト可能にする
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *)&broadcast_sw,
                   sizeof(broadcast_sw)) == -1) {
        exit_errmesg("setsockopt()");
    }

    // ビットマスクの準備
    FD_ZERO(&mask);
    FD_SET(sock, &mask);

    // 送信するパケットを作成する
    create_packet(s_buf, HELLO, NULL);
    strsize = strlen(s_buf);

    // パケットを送信する
    Sendto(sock, s_buf, strsize, 0, (struct sockaddr *)&broadcast_adrs,
           sizeof(broadcast_adrs));

    int retries = 2;  // 2回まで再送する
    for (;;) {
        // 受信データの有無をチェック
        readfds = mask;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        if (select(sock + 1, &readfds, NULL, NULL, &timeout) == 0) {
            // タイムアウト
            if (retries > 0) {
                // 再送
                fprintf(stdout, "Retrying...\n");
                Sendto(sock, s_buf, strsize, 0,
                       (struct sockaddr *)&broadcast_adrs,
                       sizeof(broadcast_adrs));
                retries--;
            } else {
                // タイムアウト回数を超えたので終了
                fprintf(stdout, "Timeout.\n");
                return -1;  // サーバとして起動
            }
        } else {
            // パケットを受信
            from_len = sizeof(from_adrs);
            strsize = Recvfrom(sock, r_buf, MESSAGE_LEN - 1, 0,
                               (struct sockaddr *)&from_adrs, &from_len);
            r_buf[strsize] = '\0';
            if (analyze_header(r_buf) == HERE) {
                // 受信したパケットからサーバのIPアドレスを取得
                fprintf(stdout, "Received HERE packet from %s\n",
                        inet_ntoa(from_adrs.sin_addr));
                strcpy(server_address, inet_ntoa(from_adrs.sin_addr));
                // snprintf(server_ip, INET_ADDRSTRLEN, "%s",
                // inet_ntoa(from_adrs.sin_addr)); printf("%s\n", server_ip);
                return 0;  // クライアントとして起動
            }
        }
    }
}

// クライアントとしての処理を行う関数
void idobata_client(char *username, int port_number) {
    int sock = -1;
    char s_buf[PACKET_LEN], r_buf[PACKET_LEN], k_buf[MESSAGE_LEN];
    int strsize;

    fd_set mask, readfds;

    idobata_packet *packet;
    printf("%s\n%d\n", server_address, port_number);  // debug
    // サーバに接続する
    sock = init_tcpclient(server_address, port_number);

    // サーバに接続完了したら，JOIN usernameを送信する
    if (sock != -1) {
        // 送信するパケットを作成する
        create_packet(s_buf, JOIN, username);
        strsize = strlen(s_buf);
        printf("%s\n", s_buf);  // debug

        // パケットを送信する
        if (send(sock, s_buf, strsize, 0) == -1) {
            exit_errmesg("send()");
        }
    }
    // ビットマスクの準備
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(sock, &mask);

    for (;;) {
        // 受信データの有無をチェック
        readfds = mask;
        select(sock + 1, &readfds, NULL, NULL, NULL);

        // キーボードからの入力をチェック
        if (FD_ISSET(0, &readfds)) {
            fgets(k_buf, MESSAGE_LEN, stdin);
            k_buf[strlen(k_buf) - 1] = '\0';

            // コマンドのチェック
            if (strcmp(k_buf, "QUIT") == 0) {
                break;
            } else {
                // 送信するパケットを作成する
                create_packet(s_buf, POST, k_buf);
                strsize = strlen(s_buf);

                // パケットを送信する
                if (send(sock, s_buf, strsize, 0) == -1) {
                    exit_errmesg("send()");
                }
            }
        }

        // サーバからの受信をチェック
        if (FD_ISSET(sock, &readfds)) {
            // パケットを受信する
            if ((strsize = recv(sock, r_buf, PACKET_LEN, 0)) == -1) {
                exit_errmesg("recv()");
            }
            r_buf[strsize] = '\0';
            packet = (idobata_packet *)r_buf;

            // パケットのヘッダを解析する
            switch (analyze_header(packet->header)) {
                case MESSAGE:
                    // メッセージを表示する
                    packet = (idobata_packet *)r_buf;
                    fprintf(stdout, "%s\n", packet->data);
                    break;
                default:
                    break;
                    // 何もしない
            }
        }
    }
    // ソケットをクローズする
    close(sock);

    exit(EXIT_SUCCESS);
}

// UDPスレッドの本体
void *udp_thread(void *arg) {
    printf("*** udp_thread called\n");
    // 変数定義
    struct myarg **tharg;
    tharg = (struct myarg **)arg;

    struct sockaddr_in from_adrs;
    socklen_t from_len = sizeof(from_adrs);
    int sock_udp;

    char s_buf[PACKET_LEN], r_buf[PACKET_LEN];

    // UDPの準備
    sock_udp = init_udpserver((*tharg)->port);

    for (;;) {
        // パケットを受信する
        Recvfrom(sock_udp, r_buf, PACKET_LEN, 0, (struct sockaddr *)&from_adrs,
                 &from_len);
        r_buf[PACKET_LEN - 1] = '\0';

        // パケットのヘッダを解析する
        if (analyze_header(r_buf) == HELLO) {
            printf("*** R:%s\n", r_buf);  // debug
            // HELLOパケットを受信したら，HEREパケットを返信する
            create_packet(s_buf, HERE, NULL);
            printf("*** S:%s\n", s_buf);  // debug
            Sendto(sock_udp, s_buf, strlen(s_buf), 0,
                   (struct sockaddr *)&from_adrs, sizeof(from_adrs));
        }
    }
}

// TCPスレッドの本体
void *tcp_thread(void *arg) {
    printf("*** tcp_thread called\n");
    // 変数定義
    struct myarg **tharg;
    tharg = (struct myarg **)arg;

    int sock_tcp, sd;

    imember *head = (imember *)malloc(sizeof(imember));
    imember *p = head;

    char s_buf[PACKET_LEN], r_buf[PACKET_LEN], k_buf[MESSAGE_LEN];

    // TCPの準備
    sock_tcp = init_tcpserver((*tharg)->port, 5);
    printf("*** sock_tcp:%d\n", sock_tcp);  // debug

    fd_set readfds;
    int maxfd;

    for (;;) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock_tcp, &readfds);
        maxfd = sock_tcp;

        p = head;
        while (p != NULL) {
            FD_SET(p->sock, &readfds);
            if (maxfd < p->sock) {
                maxfd = p->sock;
            }
            p = p->next;
        }

        select(maxfd + 1, &readfds, NULL, NULL, NULL);
        printf("*** select called\n");

        // キーボードからの入力をチェック
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(k_buf, MESSAGE_LEN, stdin);
            k_buf[strlen(k_buf) - 1] = '\0';

            if (strcmp(k_buf, "QUIT") == 0) {
                close((*tharg)->sock);
                exit(EXIT_SUCCESS);
            }
            char str1[USERNAME_LEN + 4];
            snprintf(str1, USERNAME_LEN, "[%s] ", (*tharg)->username);
            strcat(str1, k_buf);

            // パケットの作成
            create_packet(s_buf, MESSAGE, str1);

            // ユーザリストの全ユーザにパケットを送信する
            imember *q = head->next;
            while (q != NULL) {
                if (q->sock != sock_tcp) {
                    printf("*** S:%s\n", s_buf);  // debug
                    send(q->sock, s_buf, strlen(s_buf), 0);
                }
                q = q->next;
            }
        } else {
            if (FD_ISSET(sock_tcp, &readfds)) {
                // クライアントからの接続を受け付ける
                sd = accept(sock_tcp, NULL, NULL);
                printf("*** accept called\n");

                add_imember(head, "unknown", sd);
            }

            p = head;
            while (p != NULL) {
                sd = p->sock;

                if (FD_ISSET(p->sock, &readfds)) {
                    // パケットを受信する
                    int strsize;
                    printf("*** recv around 424 start\n");
                    if ((strsize = recv(p->sock, r_buf, PACKET_LEN, 0)) == -1) {
                        exit_errmesg("recv()");
                    }
                    printf("*** recv around 424 end\n");
                    printf("*** recv called\n");
                    r_buf[strsize] = '\0';
                    idobata_packet *packet = (idobata_packet *)r_buf;

                    // パケットのヘッダを解析する
                    switch (analyze_header(packet->header)) {
                        case JOIN:
                            printf("*** R:JOIN %s\n", packet->data);
                            // JOINパケットを受信したら，usernameを設定(最大文字数はUSERNAME_LEN)
                            strncpy(p->username, packet->data, USERNAME_LEN);
                            printf("*** username registered :%s\n",
                                   packet->data);  // debug

                            // 現在のユーザリストを表示する
                            printf("*** current users:\n");
                            imember *q = head;
                            while (q != NULL) {
                                printf("*** %s\n", q->username);
                                q = q->next;
                            }
                            printf("*** end of users\n");

                            break;
                        case POST:
                            printf("*** R:POST %s\n", packet->data);
                            // POSTパケットを受信したら，メッセージを表示する
                            // メッセージを作成
                            char str1[MESSAGE_LEN];
                            snprintf(str1, PACKET_LEN, "[%s] %s",
                                     get_username(head, p->sock), packet->data);
                            create_packet(s_buf, MESSAGE, str1);

                            // メッセージ他メンバーに送信する
                            q = head;
                            while (q != NULL) {
                                if (q->sock != p->sock) {
                                    send(q->sock, s_buf, strlen(s_buf), 0);
                                }
                                q = q->next;
                            }
                            // メッセージを表示する
                            printf("%s\n", str1);
                            break;
                        case QUIT:
                            // QUITパケットを受信したら，接続を閉じてユーザを削除する
                            printf("%s has left.\n", p->username);
                            close(p->sock);
                            delete_imember(head, p->sock);

                            // 現在のユーザリストを表示する
                            printf("*** current users:\n");
                            imember *r = head;
                            while (r != NULL) {
                                printf("*** %s\n", r->username);
                                r = r->next;
                            }
                            printf("*** end of users\n");
                            break;
                    }
                }
                p = p->next;
            }
        }
    }
}

// サーバとしての処理を行う関数
void idobata_server(char username[USERNAME_LEN], int port_number) {
    printf("*** idobata server called\n");
    // 変数定義
    pthread_t tid;
    struct myarg arg;
    strcpy(arg.username, username);
    arg.port = port_number;
    arg.tid = &tid;
    struct myarg *arg_p = &arg;

    pthread_create(arg.tid, NULL, udp_thread, (void *)&arg_p);
    pthread_create(arg.tid, NULL, tcp_thread, (void *)&arg_p);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int port_number = DEFAULT_PORT;  // ポート番号
    char username[USERNAME_LEN];     // ユーザ名

    int c;
    // オプション文字列の取得
    while (1) {
        c = getopt(argc, argv, "u:p:h");
        if (c == -1) break;
        switch (c) {
            case 'u':  // ユーザ名の指定
                strncpy(username, optarg, USERNAME_LEN);
                break;

            case 'p':  // ポート番号の指定
                port_number = atoi(optarg);
                break;

            case '?':  // エラー
                fprintf(stderr, "Unknown option '%c'\n", optopt);
                exit(EXIT_FAILURE);
                break;

            case 'h':  // ヘルプ
                fprintf(stderr, "Usage: %s -u username [-p port_number]\n",
                        argv[0]);
                exit(EXIT_SUCCESS);
                break;
        }
    }

    if (start_idobata(port_number) == 0) {
        // クライアントとして起動
        printf("Start as a client.\n");
        idobata_client(username, port_number);

    } else {
        // サーバとして起動
        printf("Start as a server.\n");
        idobata_server(username, port_number);
    }
    exit(EXIT_SUCCESS);
}