#include "tcpsvr.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "sockcomm.h"

//#define MAXEVENTS 4
#define MAXLINE 4096

static uint16_t listenport_ = -1;
static int epollfd_ = -1;
static int listenfd_ = -1;
static int running_ = 1;
static size_t epollsize_ = -1;
static struct epoll_event* epollevent_ = 0;
static pthread_t tid_ = -1;

/*xu: release the connection to the real svr*/
void Release() {
  printf("release tcpsvr.");
  if (-1 != epollfd_) close(epollfd_);
  if (-1 != listenfd_) close(listenfd_);
  for (int index = 0; index < epollsize_; ++index)
    close(epollevent_[index].data.fd);
  free(epollevent_);
}

static int CreateAndBind() {
  listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == listenfd_) {
    printf("create server socket failed error %d.", errno);
    return -1;
  }
  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(listenport_);
  if (-1 ==
      bind(listenfd_, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
    printf("server socket bind error %d.", errno);
    return -1;
  }
  return 0;
}

/*xu: recv data retrans to the destination server*/
static void RecvTcpData(int readfd) {
  static char buf[MAXLINE];
  while (running_) {
    ssize_t count;
    count = read(readfd, buf, sizeof(buf));
    if (-1 == count) {
      if (errno != EAGAIN) {
        // EAGAIN means all data been read, go back.
        printf("read failed %d.", errno);
      }
      break;
    } else if (count == 0) {
      /*graceful socket close*/
      break;
    } else {
      // handle the user data
    }
  }
}

/*xu: new socket recv, shall connect to the destination server*/
static void AcceptHandle() {
  struct sockaddr in_addr;
  socklen_t in_len = sizeof(in_addr);

  while (running_) {
    int infd = accept(listenfd_, &in_addr, &in_len);
    if (-1 == infd) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        return;
      else {
        printf("accept failed.");
        return;
      }
    }
    if (-1 == SetNonBlock(infd)) return;
    struct epoll_event event;
    event.data.fd = infd;
    event.events = EPOLLIN | EPOLLET;
    if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, infd, &event)) {
      printf("epoll control add error %d.", errno);
      return;
    }
    printf("new socket %d accepted.", infd);
  }
}

/*xu: release the connection to the real svr*/
static void HandleSockErr(const int fd) {
  if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, NULL))
    printf("epoll control delete socket [%d] error %d.", fd, errno);
  else
    printf("delete socket [%d] event success.", fd);
  close(fd);
}

void Stop() {
  running_ = 0;
  pthread_join(&tid_, NULL);
}

int IsRunning() { return running_; }

static void* IOStart(void* param) {
  struct epoll_event event;
  event.data.fd = listenfd_;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, listenfd_, &event)) {
    printf("epoll control listen error %d.", errno);
    return;
  }

  if (-1 == listen(listenfd_, SOMAXCONN)) {
    printf("server socket listen error %d.", errno);
    return;
  }

  // set wait time 1 sec
  const int kexpire = 1000;
  while (running_) {
    int readynum = epoll_wait(epollfd_, epollevent_, epollsize_, kexpire);
    if (-1 == readynum) {
      printf("epoll wait error %d.", errno);
      continue;
    } else if (0 == readynum) {
      // timer expire
      continue;
    }
    for (int index = 0; index < readynum; ++index) {
      if ((epollevent_[index].events & EPOLLERR) ||
          (epollevent_[index].events & EPOLLHUP) ||
          (epollevent_[index].events & EPOLLRDHUP) ||
          (!(epollevent_[index].events & EPOLLIN))) {
        printf("epoll error happen, events %u.", epollevent_[index].events);
        HandleSockErr(epollevent_[index].data.fd);
        continue;
      }
      if (listenfd_ == epollevent_[index].data.fd) {
        printf("listen index %d, fd %d.", index, listenfd_);
        AcceptHandle();
      } else {
        RecvTcpData(epollevent_[index].data.fd);
      }
    }
  }
}

int StartTcpSvr(uint16_t port, size_t pollsize) {
  listenport_ = port;
  epollsize_ = pollsize;
  epollevent_ = (struct epoll_event*)malloc((epollsize_ + 1) *
                                            sizeof(struct epoll_event));
  if (0 == epollevent_) return -1;
  if (-1 == CreateAndBind()) return -1;
  if (-1 == SetNonBlock(listenfd_)) return -1;
  if (-1 == (epollfd_ = epoll_create1(0))) {
    printf("epoll create error %d.", errno);
    return -1;
  }
  if (0 != pthread_create(&tid_, NULL, IOStart, NULL)) {
    printf("create iostart failed!\n");
    return -1;
  }
  return 0;
}