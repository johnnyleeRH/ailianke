#ifndef SOCK_COMM_H_
#define SOCK_COMM_H_

#define MAXLINE 4096

int SetNonBlock(int sockfd);
int CreateAndBind(uint16_t port, int* listenfd);

#endif