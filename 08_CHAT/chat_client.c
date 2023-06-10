// chat_client.c
// 21122051 MIZUTANI Kota
#include <stdlib.h>
#include <sys/select.h>

#include "mynet.h"
#include "chat.h"

#define S_BUFSIZE 100 // 送信用バッファサイズ
#define R_BUFSIZE 100 // 受信用バッファサイズ

void chat_client(char* servername, int port_number) {
    int sock;
    char s_buf[S_BUFSIZE], r_buf[R_BUFSIZE];
    int strsize;
    fd_set mask, readfds;

    // サーバに接続する
    sock = init_tcpclient(servername, port_number);
    printf("Connected.\n");

    // ビットマスクの準備
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(sock, &mask);

    // メインループ
    while(1) {
        // 受信データの有無をチェック
        readfds = mask;
        select(sock+1, &readfds, NULL, NULL, NULL);

        // キーボードからの入力をチェック
        if(FD_ISSET(0, &readfds)) {
            fgets(s_buf, S_BUFSIZE, stdin);
            strsize = strlen(s_buf);
            // 文字列の終端をヌル文字にする
            s_buf[strsize-1] = '\0';
            // サーバにデータを送信する
            Send(sock, s_buf, strsize, 0);
        }

        // サーバからのメッセージをチェック
        if(FD_ISSET(sock, &readfds)) {
            strsize = Recv(sock, r_buf, R_BUFSIZE-1, 0);
            r_buf[strsize] = '\0';
            printf("%s\n", r_buf);
            fflush(stdout); // 標準出力の内容を強制的に出力する
        }
    }
}