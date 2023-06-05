#include "mynet.h"

#define PORT 50000  /* デフォルトのポート番号 */
#define BUFSIZE 100 /* バッファサイズ */

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

int main(int argc, char *argv[]) {
    int sock_listen, sock_accepted;
    int port_number = PORT;

    if (argc == 2) {
        port_number = atoi(argv[1]);
    }

    sock_listen = init_tcpserver(port_number, 5);

    /* クライアントの接続を受け付ける */
    sock_accepted = accept(sock_listen, NULL, NULL);
    close(sock_listen);

    handle_client(sock_accepted);

    close(sock_accepted);

    exit(EXIT_SUCCESS);
}
