#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

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