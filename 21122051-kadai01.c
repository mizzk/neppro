#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define PORTNUM 80      /* ポート番号 */
#define BUFSIZE 1024 /* バッファサイズ */

int main(int argc, char *argv[]){
    struct hostent *server_host;
    struct sockaddr_in server_adrs;

    int tcpsock;
    char servername[BUFSIZE];
    char url[BUFSIZE];
    int port = PORTNUM;

    if(argc == 1) {
        printf("Please input server name.\n");
        exit(EXIT_FAILURE);
    }else if (argc == 2) {
        strcpy(url, argv[1]);
        strcpy(servername, url);
    } else if(argc == 3){
        strcpy(url, argv[1]);
        strcpy(servername, argv[2]);
    } else if(argc ==4){
        strcpy(url, argv[1]);
        strcpy(servername, argv[2]);
        port = atoi(argv[3]);
    }else {
        printf("Too many arguments.\n");
        exit(EXIT_FAILURE);
    }

    char response[BUFSIZE];

    char str_send[BUFSIZE] = "GET https://";
    strcat(str_send, servername);
    char str_1[] = " HTTP/1.1\r\nHost:";
    char str_2[] = "\r\n";
    strcat(str_send, str_1);
    strcat(str_send, servername);
    strcat(str_send, str_2);

    int strsize = strlen(str_send);

    /* サーバ名をアドレス(hostent構造体)に変換する */
    if ((server_host = gethostbyname(url)) == NULL){
        fprintf(stderr, "gethostbyname()");
        printf("(%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* サーバの情報をsockaddr_in構造体に格納する */
    memset(&server_adrs, 0, sizeof(server_adrs));
    server_adrs.sin_family = AF_INET;
    server_adrs.sin_port = htons(port);
    memcpy(&server_adrs.sin_addr, server_host->h_addr_list[0], server_host->h_length);

    /* ソケットをSTREAMモードで作成する */
    if ((tcpsock = socket(PF_INET, SOCK_STREAM, 0)) == -1){
        fprintf(stderr, "socket()");
        exit(EXIT_FAILURE);
    }

    /* ソケットにサーバの情報を対応づけてサーバに接続する */
    if (connect(tcpsock, (struct sockaddr *)&server_adrs, sizeof(server_adrs)) == -1){
        fprintf(stderr, "connect");
        printf("(%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if ((strsize = send(tcpsock, str_send, strsize, 0)) == -1){
        fprintf(stderr, "send()");
        exit(EXIT_FAILURE);
    }

    send(tcpsock, "\r\n", 2, 0); /* HTTPのメソッド（コマンド）の終わりは空行 */

    /* サーバから文字列を受信する */
    if (recv(tcpsock, response, BUFSIZE - 1, 0) == -1){
        fprintf(stderr, "recv()");
        exit(EXIT_FAILURE);
    }
    char *status_code_ptr = strstr(response, "HTTP/1.1 ");
    int status_code = -1;
    sscanf(status_code_ptr, "HTTP/1.1 %d", &status_code);
    if (status_code == 200 || status_code == 301) {
        char *content_length_ptr = strstr(response, "Content-Length: ");
        char *server_ptr = strstr(response, "Server: ");
        if (content_length_ptr != NULL) {
            int r_length = -1;
            sscanf(content_length_ptr, "Content-Length: %d", &r_length);
            printf("Content-Length: %d\n", r_length);
        } else {
            printf("Content-Length not found.\n");
        }
        if (server_ptr != NULL) {
            char server[BUFSIZE];
            sscanf(server_ptr, "Server: %s", server);
            printf("Server: %s\n", server);
        } else {
            printf("Server name not found.\n");
        }
    } else {
        printf("Error!!\nStatus code: %d\n", status_code);
    }

    close(tcpsock); /* ソケットを閉じる */
    exit(EXIT_SUCCESS);
}