// idobata.c
// 21122051 MIZUTANI Kota

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>

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
typedef struct _imember {
    char username[USERNAME_LEN]; /* ユーザ名 */
    int sock;                    /* ソケット番号 */
    struct _imember *next;       /* 次のユーザ */
} *imember;

// オプション用文字列
extern char *optarg;
extern int optind, opterr, optopt;

// グローバル変数
char server_address[512];  // HEREパケットを受信したサーバのIPアドレス

// imember構造体の追加関数
void add_imember(imember *head, char *username, int sock) {
    imember p, q;

    // メモリの確保
    if ((p = (imember)malloc(sizeof(struct _imember))) == NULL) {
        exit_errmesg("malloc()");
    }

    // メンバの設定
    snprintf(p->username, USERNAME_LEN, "%s", username);
    p->sock = sock;
    p->next = NULL;

    // リストの最後に追加する
    if (*head == NULL) {
        *head = p;
    } else {
        q = *head;
        while (q->next != NULL) {
            q = q->next;
        }
        q->next = p;
    }
}

// imember構造体の削除関数
void delete_imember(imember *head, int sock) {
    imember p, q;

    // リストの先頭から探索する
    p = *head;
    q = NULL;
    while (p != NULL) {
        if (p->sock == sock) {
            // 見つかったらリストから削除する
            if (q == NULL) {
                *head = p->next;
            } else {
                q->next = p->next;
            }
            free(p);
            return;
        }
        q = p;
        p = p->next;
    }
}

// imember構造体のsocket番号からusernameを取得する関数
char *get_username(imember head, int sock) {
    imember p;
    p = head;
    while (p != NULL) {
        if (p->sock == sock) {
            return p->username;
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

void idobata_server(char *username, int port_numeber) {
    struct sockaddr_in from_adrs;
    int sock_listen, sock_udp, sock_tcp, sock_accepted, from_len, strsize, max_fd;
    char s_buf[PACKET_LEN], r_buf[PACKET_LEN], k_buf[MESSAGE_LEN];
    idobata_packet *packet;
    imember imember_tmp, userlist_head = NULL;
    fd_set mask, readfds;

    // 1.UDPポート(デフォルト50001番)を監視し、「HELO」パケットが送られてきたら
    // 送ってきた相手に「HERE」パケットを送り返す。
    sock_udp = init_udpserver(port_numeber);

    // 文字列をクライアントから受信する
    from_len = sizeof(from_adrs);

    sock_tcp = init_tcpserver(port_numeber, 5);

    for (;;) {
        Recvfrom(sock_udp, r_buf, PACKET_LEN, 0, (struct sockaddr *)&from_adrs,
                 &from_len);
        if (analyze_header(r_buf) == HELLO) {
            // 送信するパケットを作成する
            create_packet(s_buf, HERE, NULL);
            strsize = strlen(s_buf);
            printf("%s\n", s_buf);  // debug
            // 文字列をクライアントに送信する
            Sendto(sock_udp, s_buf, strsize, 0, (struct sockaddr *)&from_adrs,
                   sizeof(from_adrs));
        }

        FD_ZERO(&mask);
        FD_SET(sock_tcp, &mask);

        max_fd = sock_tcp;
        imember_tmp = userlist_head;
        while(imember_tmp != NULL){
            FD_SET(imember_tmp->sock, &mask);
            if(imember_tmp->sock > max_fd){
                max_fd = imember_tmp->sock;
            }
            imember_tmp = imember_tmp->next;
        }

        select(max_fd + 1, &mask, NULL, NULL, NULL);

        if(FD_ISSET(sock_tcp, &mask)){
            sock_accepted = accept(sock_tcp, NULL, NULL);
            add_imember(&userlist_head, NULL, sock_accepted);
        }

        // キーボードからの入力をチェック
        if (FD_ISSET(0, &mask)) {
            fgets(k_buf, MESSAGE_LEN, stdin);
            k_buf[strlen(k_buf) - 1] = '\0';

            char str1[USERNAME_LEN + 4];
            snprintf(str1, USERNAME_LEN, "[%s] ", username);
            strcat(str1, k_buf);

            // パケットの作成
            create_packet(s_buf, MESSAGE, str1);
            strsize = strlen(s_buf);

            // ユーザリストの全ユーザにパケットを送信する
            puts("send to all users");
            imember p;
            p = userlist_head;
            while (p != NULL) {
                if (send(p->sock, s_buf, strsize, 0) == -1) {
                    exit_errmesg("send()");
                }
                p = p->next;
            }
        }

        imember_tmp = userlist_head;
        while(imember_tmp != NULL){
            sock_accepted = imember_tmp->sock;
            if(FD_ISSET(sock_accepted, &mask)){
                if((strsize = recv(sock_accepted, r_buf, PACKET_LEN, 0)) == -1){
                    exit_errmesg("recv()");
                }
                r_buf[strsize] = '\0';
                packet = (idobata_packet *)r_buf;

                switch(analyze_header(packet->header)){
                    case JOIN:
                        // usernameを接続しているソケット番号と関連づけて記録する
                        add_imember(&userlist_head, packet->data, sock_accepted);

                        // ユーザ名を表示する
                        printf("%s joined!(l.333)\n",
                               get_username(userlist_head, sock_accepted));
                        break;
                    case POST:
                        // メッセージを表示する
                        printf("%s\n", packet->data);

                        // パケットの作成
                        create_packet(s_buf, MESSAGE, packet->data);

                        // ユーザリストの全ユーザにパケットを送信する．ただし，送信者には送信しない
                        puts("send to all users");
                        imember p;
                        p = userlist_head;
                        while (p != NULL) {
                            if (p->sock != sock_accepted) {
                                if (send(p->sock, s_buf, strsize, 0) == -1) {
                                    exit_errmesg("send()");
                                }
                            }
                            p = p->next;
                        }
                        break;
                    case QUIT:
                        // ユーザリストから削除する
                        delete_imember(&userlist_head, sock_accepted);

                        // ユーザ名を表示する
                        printf("%s left!\n",
                               get_username(userlist_head, sock_accepted));
                        break;

                    default:
                        break;
                        // 何もしない
                }
            }
        }
    }

    // // 2.TCPポートを監視し，クライアントからの接続を待ち受ける
    // puts("sock_tcp");

    // // puts("sock_accepted");
    // // sock_accepted = accept(sock_listen, NULL, NULL);
    // // close(sock_listen);
    // // puts("debug0");

    // // 3.上記で接続したクライアントから「JOIN
    // // username」という内容のメッセージ
    // // (usernameの部分は起動時に指定した各ユーザのユーザ名）を受信したら、そのusernameを
    // // 接続しているソケット番号と関連づけて記録する(ログイン完了)。
    // // クライアントとの接続状態はそのまま保持する。
    // // パケットをクライアントから受信する
    // if ((strsize = recv(sock_accepted, r_buf, PACKET_LEN, 0)) == -1) {
    //     exit_errmesg("recv()");
    // }
    // r_buf[strsize] = '\0';
    // packet = (idobata_packet *)r_buf;

    // // パケットのヘッダを解析する
    // switch (analyze_header(packet->header)) {
    //     case JOIN:
    //         // usernameを接続しているソケット番号と関連づけて記録する
    //         add_imember(&userlist_head, packet->data, sock_accepted);

    //         // ユーザ名を表示する
    //         printf("%s joined!(l.333)\n",
    //                get_username(userlist_head, sock_accepted));
    //         break;
    //     default:
    //         break;
    //         // 何もしない
    // }

    // for (;;) {
    //     FD_ZERO(&readfds);
    //     FD_SET(sock_listen, &readfds);

    //     select(sock_accepted + 1, &readfds, NULL, NULL, NULL);

    //     // キーボードからの入力をチェック
    //     if (FD_ISSET(0, &readfds)) {
    //         fgets(k_buf, MESSAGE_LEN, stdin);
    //         k_buf[strlen(k_buf) - 1] = '\0';

    //         char str1[USERNAME_LEN + 4];
    //         snprintf(str1, USERNAME_LEN, "[%s] ", username);
    //         strcat(str1, k_buf);

    //         // パケットの作成
    //         create_packet(s_buf, MESSAGE, str1);
    //         strsize = strlen(s_buf);

    //         // ユーザリストの全ユーザにパケットを送信する
    //         puts("send to all users");
    //         imember p;
    //         p = userlist_head;
    //         while (p != NULL) {
    //             if (send(p->sock, s_buf, strsize, 0) == -1) {
    //                 exit_errmesg("send()");
    //             }
    //             p = p->next;
    //         }
    //     }

    //     // クライアントからの受信をチェック
    //     if (FD_ISSET(sock_accepted, &readfds)) {
    //         // パケットを受信する
    //         if ((strsize = recv(sock_accepted, r_buf, PACKET_LEN, 0)) == -1) {
    //             exit_errmesg("recv()");
    //         }
    //         r_buf[strsize] = '\0';
    //         packet = (idobata_packet *)r_buf;

    //         // パケットのヘッダを解析する
    //         switch (analyze_header(packet->header)) {
    //             case POST:
    //                 // メッセージを表示する
    //                 packet = (idobata_packet *)r_buf;
    //                 fprintf(stdout, "%s\n", packet->data);
    //                 break;
    //             default:
    //                 break;
    //                 // 何もしない
    //         }
    //     }
    // }
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
        printf("Start as a server.\n");
        // サーバとして起動
        idobata_server(username, port_number);
        
    }
    exit(EXIT_SUCCESS);
}