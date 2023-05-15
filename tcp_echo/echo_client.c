#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define PORT 50000         /* ポート番号 ← 実験するとき書き換える、例えば、50000 */
#define S_BUFSIZE 100   /* 送信用バッファサイズ */
#define R_BUFSIZE 100   /* 受信用バッファサイズ */

int main()
{
  struct hostent *server_host;
  struct sockaddr_in server_adrs;

  int sock;

  char servername[] = "localhost";  /* ←実験するとき書き換える */
/* char servername[] = "192.168.1.10"; */
  char s_buf[S_BUFSIZE], r_buf[R_BUFSIZE];
  int strsize;

  /* サーバ名をアドレス(hostent構造体)に変換する */
  if((server_host = gethostbyname( servername )) == NULL){
    fprintf(stderr,"gethostbyname()");
    exit(EXIT_FAILURE);
  }

  /* サーバの情報をsockaddr_in構造体に格納する */
  memset(&server_adrs, 0, sizeof(server_adrs));
  server_adrs.sin_family = AF_INET;
  server_adrs.sin_port = htons(PORT);
  memcpy(&server_adrs.sin_addr, server_host->h_addr_list[0], server_host->h_length);

  /* ソケットをSTREAMモードで作成する */
  if((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1){
    fprintf(stderr,"socket()");
    exit(EXIT_FAILURE);
  }

  /* ソケットにサーバの情報を対応づけてサーバに接続する */
  if(connect(sock, (struct sockaddr *)&server_adrs, sizeof(server_adrs))== -1){
    fprintf(stderr,"connect");
    exit(EXIT_FAILURE);
  }

  /* キーボードから文字列を入力する */
  fgets(s_buf, S_BUFSIZE, stdin);
  strsize = strlen(s_buf);

  /* 文字列をサーバに送信する */
  if( send(sock, s_buf, strsize, 0) == -1 ){
    fprintf(stderr,"send()");
    exit(EXIT_FAILURE);
  }

  /* サーバから文字列を受信する */
  if((strsize=recv(sock, r_buf, R_BUFSIZE-1, 0)) == -1){
    fprintf(stderr,"recv()");
    exit(EXIT_FAILURE);
  }
  r_buf[strsize] = '\0';

  printf("%s",r_buf);      /* 受信した文字列を画面に書く */

  close(sock);             /* ソケットを閉じる */

  exit(EXIT_SUCCESS);
}