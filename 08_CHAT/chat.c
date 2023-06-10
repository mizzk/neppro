// chat.c
// 21122051 MIZUTANI Kota
# include "chat.h"

#include <stdlib.h>
#include <stdio.h>

#include "mynet.h"

#define SEVER_LEN 256 // サーバ名格納用バッファサイズ
#define DEFAULT_PORT 50000 // ポート番号のデフォルト値
#define DEFAULT_NCLIENT 3 // クライアント数のデフォルト値
#define DEFAULT_MODE 'C' // モードのデフォルト値

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]){
    int port_number = DEFAULT_PORT; // ポート番号
    int n_client = DEFAULT_NCLIENT; // クライアント数
    char mode = DEFAULT_MODE; // モード
    char servername[SEVER_LEN] = "localhost"; // サーバ名(デフォルトはlocalhost)
    int c;

    // オプション文字列の取得
    while(1){
        c = getopt(argc, argv, "SCs:p:n:h");
        if(c == -1) break;
        switch(c){
            case 'S': // サーバモードにする
                mode = 'S';
                break;
            
            case 'C': // クライアントモードにする
                mode = 'C';
                break;
            
            case 's': // サーバ名の指定
                strncpy(servername, optarg, SEVER_LEN);
                break;
            
            case 'p': // ポート番号の指定
                port_number = atoi(optarg);
                break;
            
            case 'n': // クライアント数の指定
                n_client = atoi(optarg);
                break;

            case '?': // エラー
                fprintf(stderr, "Unknown option '%c'\n", optopt);
                exit(EXIT_FAILURE);
                break;
            
            case 'h': // ヘルプ
                fprintf(stderr, "Usage: %s [-S|-C] [-s servername] [-p port] [-n n_client]\n", argv[0]);
                exit(EXIT_SUCCESS);
                break;
        }
    }

    switch(mode) {
        case 'S': // サーバモード
            chat_server(port_number, n_client);
            break;
        
        case 'C': // クライアントモード
            chat_client(servername, port_number);
            break;
    }

    exit(EXIT_SUCCESS);
}