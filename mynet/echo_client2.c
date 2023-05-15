#include "mynet.h"

#define PORT 50000         /* ポート番号 */
#define S_BUFSIZE 100   /* 送信用バッファサイズ */
#define R_BUFSIZE 100   /* 受信用バッファサイズ */

int main()
{
  int sock;
  char servername[] = "localhost";
  char k_buf[S_BUFSIZE], s_buf[S_BUFSIZE], r_buf[R_BUFSIZE];
  int strsize;

  /* サーバに接続する */
  sock = init_tcpclient(servername, PORT); /* ←ライブラリの関数を使った */

  /* キーボードから文字列を入力する */
  while(fgets(k_buf, BUFSIZE, stdin) != '\n' ){ /* 空行が入力されるまで繰り返し */
    strsize = strlen(k_buf);
    k_buf[strsize-1] = 0;   /* 末尾の改行コードを消す */
    snprintf(s_buf, BUFSIZE, "%s\r\n",k_buf); /* HTTPの改行コードは \r\n */
  }

    /* 文字列をサーバに送信する */
    if(send(tcpsock,s_buf,strsize,0)== -1 ){
      fprintf(stderr,"send()");
      exit(EXIT_FAILURE);
    }
  }
                                        ;
  strsize = strlen(s_buf);

  /* 文字列をサーバに送信する */
  if( send(sock, s_buf, strsize, 0) == -1 ){
    exit_errmesg("send()");
  }

  /* サーバから文字列を受信する */
  do{
    if((strsize= recv(sock,r_buf,R_BUFSIZE-1,0)) == -1){
      exit_errmesg("recv()");
    }
    r_buf[strsize] = '\0';
    printf("%s",r_buf);      
  }while( r_buf[strsize-1] != '\n' );

  close(sock);             /* ソケットを閉じる */

  exit(EXIT_SUCCESS);
}