#include "mynet.h"

int init_tcpserver(in_port_t myport, int backlog)
{
  struct sockaddr_in my_adrs;
  int sock_listen;

  /* �T�[�o(�������g)�̏���sockaddr_in�\���̂Ɋi�[���� */
  memset(&my_adrs, 0, sizeof(my_adrs));
  my_adrs.sin_family = AF_INET;
  my_adrs.sin_port = htons(myport);
  my_adrs.sin_addr.s_addr = htonl(INADDR_ANY);

  /* �҂��󂯗p�\�P�b�g��STREAM���[�h�ō쐬���� */
  if((sock_listen = socket(PF_INET, SOCK_STREAM, 0)) == -1){
    exit_errmesg("socket()");
  }

  /* �҂��󂯗p�̃\�P�b�g�Ɏ������g�̃A�h���X�������т��� */
  if(bind(sock_listen, (struct sockaddr *)&my_adrs, sizeof(my_adrs)) == -1 ){
    exit_errmesg("bind()");
  }

  /* �N���C�A���g����̐ڑ����󂯕t���鏀�������� */
  if(listen(sock_listen, backlog) == -1){
    exit_errmesg("listen()");
  }

  return(sock_listen);
}