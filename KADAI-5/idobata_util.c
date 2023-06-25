// idobata_util.c

#include <stdlib.h>

#include "idobata.h"
#include "mynet.h"

#define MSGBUF_SIZE 512

// パケットの構造を表す構造体
typedef struct {
    char header[4];
    char sep;
    char data[];
} idobata_packet;

char *create_packet(char *buffer, u_int32_t type, char *message) {
    switch (type) {
        case HELLO:
            snprintf(buffer, MSGBUF_SIZE, "HELO");
            break;
        case HERE:
            snprintf(buffer, MSGBUF_SIZE, "HERE");
            break;
        case JOIN:
            snprintf(buffer, MSGBUF_SIZE, "JOIN %s", message);
            break;
        case POST:
            snprintf(buffer, MSGBUF_SIZE, "POST %s", message);
            break;
        case MESSAGE:
            snprintf(buffer, MSGBUF_SIZE, "MESG %s", message);
            break;
        case QUIT:
            snprintf(buffer, MSGBUF_SIZE, "QUIT");
            break;
        default:
            /* Undefined packet type */
            return (NULL);
            break;
    }
    return (buffer);
}

// パケットのヘッダを解析する関数
u_int32_t analyze_header( char *header )
{
  if( strncmp( header, "HELO", 4 )==0 ) return(HELLO);
  if( strncmp( header, "HERE", 4 )==0 ) return(HERE);
  if( strncmp( header, "JOIN", 4 )==0 ) return(JOIN);
  if( strncmp( header, "POST", 4 )==0 ) return(POST);
  if( strncmp( header, "MESG", 4 )==0 ) return(MESSAGE);
  if( strncmp( header, "QUIT", 4 )==0 ) return(QUIT);
  return 0;
}