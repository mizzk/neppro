# include "mynet.h"

#define PORT 50000         /* ポート番号 ← 実験するとき書き換える、例えば、50000 */
#define S_BUFSIZE 100   /* 送信用バッファサイズ */
#define R_BUFSIZE 100   /* 受信用バッファサイズ */

int main()
{
  int sock;
  char servername[] = "localhost";  /* ←実験するとき書き換える */
  char s_buf[S_BUFSIZE], r_buf[R_BUFSIZE];
  int strsize;

  sock= init_tcpclient(servername, PORT);

  /* キーボードから文字列を入力する */
  fgets(s_buf, S_BUFSIZE, stdin);
  strsize = strlen(s_buf);

  /* 文字列をサーバに送信する */
  if( send(sock, s_buf, strsize, 0) == -1 ){
    fprintf(stderr,"send()");
    exit(EXIT_FAILURE);
  }

  /* サーバから文字列を受信する */
  do{
    if((strsize=recv(sock, r_buf, R_BUFSIZE-1, 0)) == -1){
    fprintf(stderr,"recv()");
    exit(EXIT_FAILURE);
    }
    r_buf[strsize] = '\0';
    printf("%s",r_buf);      /* 受信した文字列を画面に書く */
  }while(r_buf[strsize-1] != '\n');
  
  close(sock);             /* ソケットを閉じる */

  exit(EXIT_SUCCESS);
}