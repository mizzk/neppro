#ifndef _IDOBATA_H_
#define _IDOBATA_H_
// idobata.h
#include "mynet.h"

#define HELLO 1
#define HERE 2
#define JOIN 3
#define POST 4
#define MESSAGE 5
#define QUIT 6

char *create_packet(char *buffer, u_int32_t type, char *message);






#endif