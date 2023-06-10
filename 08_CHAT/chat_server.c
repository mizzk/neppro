// chat_server.c
// 21122051 MIZUTANI Kota
#include <stdlib.h>
#include <unistd.h>

#include "mynet.h"
#include "chat.h"

void chat_server(int port_number, int n_client) {
    int sock_listen;

    // サーバの初期化
    sock_listen = init_tcpserver(port_number, 5);

    // クライアントの接続
    init_client(sock_listen, n_client);

    close(sock_listen);

    // チャットのやりとり
    handle_message();
}