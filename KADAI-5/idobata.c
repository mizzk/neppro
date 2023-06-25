// idobata.c
// 21122051 MIZUTANI Kota

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>

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
#define TIMEOUT_SEC 5       // タイムアウト時間

// パケットの構造を表す構造体
typedef struct {
    char header[4];
    char sep;
    char data[];
} idobata_packet;

// オプション用文字列
extern char *optarg;
extern int optind, opterr, optopt;

// グローバル変数
char *server_ip;  // HEREパケットを受信したサーバのIPアドレス

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
                snprintf(server_ip, INET_ADDRSTRLEN, "%s",
                         inet_ntoa(from_adrs.sin_addr));
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

    // サーバに接続する
    sock = init_tcpclient(server_ip, port_number);

    // サーバに接続完了したら，JOIN usernameを送信する
    if (sock != -1) {
        // 送信するパケットを作成する
        create_packet(s_buf, JOIN, username);
        strsize = strlen(s_buf);

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
            packet = (idobata_packet *)r_buf;

            // パケットのヘッダを解析する
            switch (analyze_header(packet->header)) {
                case MESSAGE:
                    // メッセージを表示する
                    packet = (idobata_packet *)r_buf;
                    fprintf(stdout, "%s\n", packet->data);
                default:
                    // 何もしない
            }
        }
    }
    // ソケットをクローズする
    close(sock);

    exit(EXIT_SUCCESS);
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
        // 今回は実装しないので，エラーを出力して終了
        fprintf(stderr, "Server mode is not implemented.\n");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}