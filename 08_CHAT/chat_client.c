// chat_client.c
// 21122051 MIZUTANI Kota
#include <curses.h>
#include <stdlib.h>
#include <sys/select.h>

#include "chat.h"
#include "mynet.h"

#define S_BUFSIZE 100  // 送信用バッファサイズ
#define R_BUFSIZE 100  // 受信用バッファサイズ

void chat_client(char *servername, int port_number) {
    int sock;
    char s_buf[S_BUFSIZE], r_buf[R_BUFSIZE];
    int strsize;
    fd_set mask, readfds;
    WINDOW *win_main, *win_sub;

    // 画面の初期化
    initscr();
    win_main = newwin(LINES - 3, COLS, 0, 0);
    win_sub = newwin(3, COLS, LINES - 3, 0);

    // スクロール可能にする
    scrollok(win_main, TRUE);
    scrollok(win_main, TRUE);

    // タイトルを表示
    wmove(win_main, 0, COLS / 2);
    wprintw(win_main, "Chat Client\n");

    // サーバに接続する
    sock = init_tcpclient(servername, port_number);
    wprintw(win_main, "Connected.\nPress Q to quit.\n");

    // ビットマスクの準備
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(sock, &mask);

    // メインループ
    while (1) {
        // 受信データの有無をチェック
        readfds = mask;
        select(sock + 1, &readfds, NULL, NULL, NULL);

        // キーボードからの入力をチェック
        if (FD_ISSET(0, &readfds)) {
            // サブウィンドウから文字列を入力する
            wmove(win_sub, 1, 0);
            wgetnstr(win_sub, s_buf, S_BUFSIZE);
            strsize = strlen(s_buf);

            // 空行は送信しない
            if (strsize == 1) {
                continue;
            }

            // 終了コマンドのチェック
            if (s_buf == "Q") {
                break;
            }

            // 文字列の終端をヌル文字にする
            s_buf[strsize] = '\0';
            // サーバにデータを送信する
            Send(sock, s_buf, strsize, 0);

            // サブウィンドウをクリアする
            werase(win_sub);
            for (int i = 0; i < COLS; i++) {
                wprintw(win_sub, "=");
            }
            wrefresh(win_sub);
            wmove(win_sub, 1, 0);
        }

        // サーバからのメッセージをチェック
        if (FD_ISSET(sock, &readfds)) {
            strsize = Recv(sock, r_buf, R_BUFSIZE - 1, 0);

            // 問題(2) サーバダウン時の処理
            if (strsize == 0) {
                wprintw(win_main, "Server is down.\n");
                break;
            }

            r_buf[strsize] = '\0';
            wprintw(win_main, "%s\n", r_buf);

            // メインウィンドウの内容を強制的に出力する
            wrefresh(win_main);
        }
    }

    // サーバから切断する
    close(sock);

    // 画面を消去
    werase(win_main);
    werase(win_sub);

    // 画面を元に戻す
    endwin();

    exit(EXIT_SUCCESS);
}