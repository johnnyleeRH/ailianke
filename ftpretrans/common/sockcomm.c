#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "sockcomm.h"


int SetNonBlock(int sockfd) {
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (-1 == flags) {
    printf("get socket flags failed %d.", errno);
    return -1;
  }
  flags |= O_NONBLOCK;
  if (-1 == fcntl(sockfd, F_SETFL, flags)) {
    printf("set socket flags failed %d.", errno);
    return -1;
  }
  return 0;
}

int CreateAndBind(uint16_t port, int* listenfd) {
  *listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == *listenfd) {
    printf("create server socket failed error %d.", errno);
    return -1;
  }
  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(port);
  if (-1 ==
      bind(*listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
    printf("server socket bind error %d.", errno);
    return -1;
  }
  return 0;
}