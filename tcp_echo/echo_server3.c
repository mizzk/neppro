#include <pthread.h>
#include "mynet.h"

#define PORT 50000  /* デフォルトのポート番号 */
#define BUFSIZE 100 /* バッファサイズ */

//getopt用の外部変数
extern char *optarg;
extern int optind, opterr, optopt;

// クライアントからの接続を処理する関数
void handle_client(int sock_accepted) {
    char client_message[BUFSIZE], output_text[BUFSIZE];
    int strsize;
    FILE *fp;

    do {
        // ">" をクライアントに送信
        if (send(sock_accepted, "> ", 3, 0) == -1) {
            exit_errmesg("send()");
        }

        /* 文字列をクライアントから受信する */
        if ((strsize = recv(sock_accepted, client_message, BUFSIZE, 0)) == -1) {
            exit_errmesg("recv()");
        }
        client_message[strsize - 2] = '\0';

        // listコマンドの場合
        if (strcmp(client_message, "list") == 0) {
            if ((fp = popen("ls ~/KotaFiles/Code/neppro/work", "r")) == NULL) {
                exit_errmesg("popen()");
            }

            while (fgets(output_text, BUFSIZE, fp) != NULL) {
                send(sock_accepted, output_text, strlen(output_text), 0);
            }

            pclose(fp);

        }
        // typeコマンドの場合
        else if (strncmp(client_message, "type", 4) == 0) {
            char cmd[BUFSIZE];
            char *file_name = client_message + 5;
            snprintf(cmd, BUFSIZE, "cat ~/KotaFiles/Code/neppro/work/%s",
                     file_name);

            if ((fp = popen(cmd, "r")) == NULL) {
                exit_errmesg("popen()");
            }

            while (fgets(output_text, BUFSIZE, fp) != NULL) {
                send(sock_accepted, output_text, strlen(output_text), 0);
            }

            pclose(fp);
        }
        // exitコマンドの場合
        else if (strcmp(client_message, "exit") == 0) {
            break;
        }
        // それ以外のコマンドの場合
        else {
            if (send(sock_accepted, "Invalid command\n", 17, 0) == -1) {
                exit_errmesg("send()");
            }
        }
    } while (1); /* 繰り返す */
}

// スレッドを生成する関数
void* create_thread(void* arg) {
    int sock_accepted = *(int*)arg;
    handle_client(sock_accepted);
    close(sock_accepted);
    pthread_exit(NULL);
}

// 子プロセスを生成する関数
void create_child_processes(int num_processes, int sock_listen) {
    int i;
    pid_t pid;
    int sock_accepted;

    for (i = 0; i < num_processes; i++) {
        pid = fork();

        if (pid < 0) {
            exit_errmesg("fork()");
        } else if (pid == 0) { /* child process */
            /* クライアントの接続を受け付ける */
            sock_accepted = accept(sock_listen, NULL, NULL);
            close(sock_listen);

            handle_client(sock_accepted);

            close(sock_accepted);
            exit(EXIT_SUCCESS);
        }
    }
}

// ここから実際の処理が始まる
int main(int argc, char *argv[]) {
    int sock_listen, sock_accepted; //ソケット用の変数
    int port_number = PORT; //ポート番号
    int num_processes = 0; //プロセス数
    int num_threads = 0;   //スレッド数
    int i; //ループ用変数
    int fork_thread_flag = -1; // 0:fork, 1:thread


    // オプションの解析
    opterr = 0;
    int c;
    while(1){
        c = getopt(argc, argv, "p:n:fth");
        if(c == -1) break;
        switch(c){
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'n':
                num_processes = atoi(optarg);
                num_threads = atoi(optarg);
                break;
            case 'f':
                fork_thread_flag = 0;
                break;
            case 't':
                fork_thread_flag = 1;
                break;
            case 'h':
                fprintf(stderr,"Usage: %s [-p port_number] [-n num_processes] [-f|-t]\n", argv[0]);
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "Unknown option '%c'\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    // サーバの初期化
    sock_listen = init_tcpserver(port_number, 5);

    if (fork_thread_flag == 0) {
        create_child_processes(num_processes, sock_listen);
    } else { // fork_thread_flag == 1
        pthread_t threads[num_threads];

        for (i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, create_thread, (void*)&sock_listen);
        }

        for (i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }
    }

    close(sock_listen);
    exit(EXIT_SUCCESS);
}






