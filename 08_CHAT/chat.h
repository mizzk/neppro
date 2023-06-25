#ifndef _CHAT_H_
#define _CHAT_H_
// chat.h
// 21122051 MIZUTANI Kota
// 作成にあたって工夫・苦労した点はchat.c冒頭に記載しています。
#include "mynet.h"

// サーバのメインルーチン
void chat_server(int port_number, int n_client);

// クライアントのメインルーチン
void chat_client(char *servername, int port_number);

// クライアント情報の初期化
void init_client(int sock_listen, int n_client);

// チャットのやりとり
void handle_message();

// Accept()のラッパー関数
int Accept(int s, struct sockaddr *addr, socklen_t *addrlen);

// Recv()のラッパー関数
int Recv(int s, void *buf, size_t len, int flags);

// Send()のラッパー関数
int Send(int s, const void *buf, size_t len, int flags);

#endif // _CHAT_H_