// chat.c
// 21122051 MIZUTANI Kota
// 作成にあたって工夫した点は以下の通りです。
// 1. quiz.cを参考に、ソースコードののファイルを分割することで、可読性を高めました。
// 2. chat_client.cにおいて、ncursesライブラリを使用することで、テキストの入力部と出力部を分けて使いやすいインターフェースを実現しました。
// また、作成にあたって苦労した点は以下の通りです。
// 1. ncursesライブラリを使用してクライアント側の表示部分を作成しましたが、端末上のインターフェース作成に慣れていなかったため、思ったとおりに表示されるまでに修正を繰り返し、時間がかかりました。
// 2. ncursesを使用することで、テキスト入力部の画面の更新がうまくいかないことがありました。これは、テキスト送信後に画面をクリアすることによって解決しました。
// 3. ファイルの異なる複数の関数を用いているため、それらの関数間で変数を共有する部分が考えにくかったです。グローバル変数の実践的な使用法について復習の必要があると感じました。
#include "chat.h"

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