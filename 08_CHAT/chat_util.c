// chat_util.c
// 21122051 MIZUTANI Kota
#include <stdlib.h>
#include <sys/select.h>

#include "mynet.h"
#include "chat.h"

#define NAMELENGTH 20 // ユーザ名の長さ
#define R_BUFSIZE 500 // 受信用バッファサイズ
#define S_BUFSIZE 500 // 送信用バッファサイズ

// ユーザ情報を格納する構造体
typedef struct {
    char name[NAMELENGTH]; // ユーザ名
    int sock; // ソケット
} client_info;

// プライベート変数
static int N_client; // クライアント数
static client_info *Client; // クライアント情報
static int Max_sd; //ディスクリプタの最大値
static char Buf[R_BUFSIZE]; // 受信用バッファ
static char Msg[S_BUFSIZE]; // 送信用バッファ

// プライベート関数
static int client_login(int sock_listen);
static char *chop_nl(char *s);

// クライアント情報の初期化
void init_client(int sock_listen, int n_client) {
    // 大域変数にクライアント数を保存
    N_client = n_client;

    // クライアント情報の保存用構造体のメモリ確保
    if ((Client = (client_info *)malloc(n_client * sizeof(client_info))) == NULL) {
        exit_errmesg("malloc()");
    }

    // クライアントのログイン処理
    Max_sd = client_login(sock_listen);
}

// クライアントのログイン処理
static int client_login(int sock_listen) {
    int client_id, sock_accepted;
    static char prompt[] = "Input your name: ";
    char loginname[NAMELENGTH];
    int strsize;

    for(client_id = 0; client_id<N_client; client_id++) {
        // クライアントからの接続を受け付ける
        sock_accepted = Accept(sock_listen, NULL, NULL);
        printf("Client[%d] is connected.\n", client_id);

        // ログインプロンプトの送信
        Send(sock_accepted, prompt, strlen(prompt), 0);

        // ログイン名の受信
        strsize = Recv(sock_accepted, loginname, NAMELENGTH-1, 0);
        loginname[strsize] = '\0';
        chop_nl(loginname);

        // クライアント情報の保存
        Client[client_id].sock = sock_accepted;
        strncpy(Client[client_id].name, loginname, NAMELENGTH);
    }
    return sock_accepted;
}



// チャットのやりとりをする関数
void handle_message() {
    fd_set mask, readfds;
    int client_id, strsize, msgsize;

    // ビットマスクの準備
    FD_ZERO(&mask);
    for(client_id=0; client_id<N_client; client_id++) {
        FD_SET(Client[client_id].sock, &mask);
    }

    // メインループ
    while(1) {
        // 受信データの有無をチェック
        readfds = mask;
        select(Max_sd+1, &readfds, NULL, NULL, NULL);

        // クライアントからのメッセージをチェック
        for (client_id = 0; client_id < N_client; client_id++) {
            if(FD_ISSET(Client[client_id].sock, &readfds)) {
                strsize = Recv(Client[client_id].sock, Buf, R_BUFSIZE-1, 0);
                Buf[strsize] = '\0';

                // 送信メッセージの作成
                msgsize = snprintf(Msg, S_BUFSIZE, "%s: %s", Client[client_id].name, Buf);

                // 全員にメッセージを送信する
                for (int i = 0; i < N_client; i++) {
                    Send(Client[i].sock, Msg, msgsize, 0);
                }
            }
        }
    }
}

// 文字列の末尾にある改行コードを削除する関数
static char *chop_nl(char *s) {
    int len;
    len = strlen(s);

    if (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        if (len > 1 && s[len - 2] == '\r') {
            s[len - 2] = '\0';
        }
    }

    return s;
}
