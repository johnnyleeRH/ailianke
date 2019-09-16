#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "common.h"

struct SlbSvrMap{
  int slbfd_;
  int clifd_;
};

// connect to the real svr, and build fd map
int ConnectRealSvr(const int slbfd, const char* svraddr) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in servaddr;

  if (sockfd == -1) {
    printf("client socket creation failed\n");
    return -1;
  }

  if (0 != SetNonBlock(sockfd))
    return -1;

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(svraddr);
  servaddr.sin_port = htons(21);

  // connect the client socket to server socket
  if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
    printf("connection with the server %s failed\n", svraddr);
    return -1;
  }

  
}