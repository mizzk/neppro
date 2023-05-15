/*
  myserver.c
*/
#include "mynet.h"

#define BUFSIZE 512
#define DIRNAME "~/work"

int list_files(int sock);
int send_file(int sock, char *filename);

int main(int argc, char *argv[])
{
    int port, server_sock, client_sock;
    socklen_t client_len;
    struct sockaddr_in client_adrs;
    char buf[BUFSIZE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    port = atoi(argv[1]);

    server_sock = init_tcpserver(port, 5);

    while (1) {
        client_len = sizeof(client_adrs);
        client_sock = accept(server_sock, (struct sockaddr *)&client_adrs, &client_len);
        printf("Connected from %s : %d\n", inet_ntoa(client_adrs.sin_addr), ntohs(client_adrs.sin_port));

        while (1) {
            send(client_sock, "> ", 2, 0);
            if (recv(client_sock, buf, BUFSIZE, 0) <= 0) break;
            if (strncmp(buf, "list", 4) == 0) {
                list_files(client_sock);
            } else if (strncmp(buf, "type", 4) == 0) {
                send_file(client_sock, buf + 5);
            } else if (strncmp(buf, "exit", 4) == 0) {
                break;
            }
        }
        close(client_sock);
    }
    close(server_sock);
    exit(EXIT_SUCCESS);
}

int list_files(int sock) {
    char cmd[BUFSIZE], buf[BUFSIZE];
    FILE *fp;

    sprintf(cmd, "ls %s", DIRNAME);

    fp = popen(cmd, "r");
    if (fp == NULL) {
        send(sock, "Error: Failed to execute command\n", 32, 0);
        return -1;
    }

    while (fgets(buf, BUFSIZE, fp) != NULL) {
        send(sock, buf, strlen(buf), 0);
    }

    pclose(fp);

    return 0;
}

int send_file(int sock, char *filename) {
    char filepath[BUFSIZE];
    char buf[BUFSIZE];
    FILE *fp;
    int len;

    sprintf(filepath, "%s/%s", DIRNAME, filename);
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        send(sock, "Error: File not found\n", 21, 0);
        return -1;
    }

    while ((len = fread(buf, sizeof(char), BUFSIZE, fp)) > 0) {
        send(sock, buf, len, 0);
    }

    fclose(fp);

    return 0;
}
