#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include "sockcomm.h"
#include "sockmap.h"

static int epollfd_ = -1;
static int listenfd_ = -1;
static int running_ = 1;
static size_t epollsize_ = -1;
static struct epoll_event* epollevent_ = 0;
static pthread_t tid_ = -1;

static int AddConnEvent(int fd) {
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLOUT;
  if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event)) {
    printf("epoll connect event add %d.", errno);
    return -1;
  }
  return 0;
}

static void ReConnect(int fd) {
  struct sockaddr_in servaddr;
  if (0 != FindPeerAddr(fd, &servaddr)) {
    printf("client peer not found.\n");
    return;
  }
  if (connect(fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
    printf("connect failed, errno %d\n", errno);
  }
}

static int CheckSockConnect(int fd) {
  int result;
  socklen_t result_len = sizeof(result);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
    printf("get sock %d opt failed %d.\n", fd, errno);
    return -1;
  }

  if (result != 0) {
    ReConnect(fd);
    return -1;
  }
  struct sockaddr addr;
  socklen_t len = sizeof(addr);
  if (0 != getpeername(fd, &addr, &len)) {
    printf("socket not connect %d.\n", errno);
    ReConnect(fd);
    return -1;
  }
  printf("fd %d connect success.\n", fd);
  return 0;
}

int ConnectDataPeer(int listenfd, int infd) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in servaddr;

  if (sockfd == -1) {
    printf("client socket creation failed\n");
    return -1;
  }

  if (0 != SetNonBlock(sockfd)) return -1;
  AddConnEvent(sockfd);
  if (0 != FineDataConnAddr(listenfd, &servaddr)) {
    printf("not found data client addr.\n");
    return -1;
  }
  MakeDataPair(listenfd, infd, sockfd);
  if (connect(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) <
      0) {
    if (errno != EINPROGRESS) {
      printf("connection with the server 0x%x failed\n",
             servaddr.sin_addr.s_addr);
      return -1;
    }
  }
  return 0;
}

// TODO：需要提供真实的svr IP，command消息发送
int ConnectRealSvr(const int slbfd, const char* svraddr) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in servaddr;

  if (sockfd == -1) {
    printf("client socket creation failed\n");
    return -1;
  }

  if (0 != SetNonBlock(sockfd)) return -1;
  AddConnEvent(sockfd);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(svraddr);
  servaddr.sin_port = htons(21);
  ConnBegin(slbfd, sockfd, svraddr);
  // connect the client socket to server socket
  if (connect(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) <
      0) {
    if (errno != EINPROGRESS) {
      printf("connection with the server %s failed\n", svraddr);
      return -1;
    }
  }
  return 0;
}

static void CloseSock(const int fd) {
  if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, 0))
    printf("epoll control delete socket [%d] error %d.\n", fd, errno);
  else
    printf("delete socket [%d] event success.\n", fd);
  close(fd);
  RemoveSock(fd);
}

// 需要关闭对应的pair套接字
static void HandleSockErr(const int fd) {
  SockType type = GetSockType(fd);
  if (DATALISTEN == type) {
    CloseSock(fd);
  } else if (DATACLI == type || DATASVR == type) {
    int pair = FindPairSock(fd);
    int listen = FindListenSock(fd, type);
    CloseSock(fd);
    if (pair > 0) {
      CloseSock(pair);
    }
    if (listen > 0) {
      CloseSock(listen);
    }
    // remove listen
  } else if (CTRLCLI == type || CTRLSVR == type) {
    int pair = FindPairSock(fd);
    CloseSock(fd);
    if (pair > 0) {
      CloseSock(pair);
    }
  }
}

// 等待与真实服务端建立连接后，再侦听客户端的数据
static int SessionConnected(int fd) {
  if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, 0))
    printf("epoll control delete socket [%d] error %d.", fd, errno);
  else {
    // remove EPOLLOUT
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event)) {
      printf("epoll connect event add %d.", errno);
      return -1;
    }
  }
  int pair = FindPairSock(fd);
  if (-1 != pair) {
    struct epoll_event event;
    event.data.fd = pair;
    event.events = EPOLLIN | EPOLLET;
    if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, pair, &event)) {
      printf("epoll control add error %d.\n", errno);
      return -1;
    }
    printf("pair <%d : %d>.\n", pair, fd);
    return 0;
  }
  return -1;
}

static void HandleDataListen(int listenfd) {
  struct sockaddr in_addr;
  socklen_t in_len = sizeof(in_addr);

  while (running_) {
    int infd = accept(listenfd, &in_addr, &in_len);
    if (-1 == infd) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        return;
      else {
        printf("data accept failed.");
        return;
      }
    }
    if (0 != NoDataPair(listenfd)) {
      printf("error happen, data sock pair already exsit.\n");
      return;
    }
    if (-1 == SetNonBlock(infd)) return;
    ConnectDataPeer(listenfd, infd);
    printf("new data socket %d accepted.\n", infd);
  }
}

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
    ConnectRealSvr(infd, "10.64.35.117");
    printf("new socket %d accepted.\n", infd);
  }
}

static int AddListenEvent(int fd) {
  struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  if (-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event)) {
    printf("epoll control listen error %d.", errno);
    return -1;
  }

  if (-1 == listen(fd, SOMAXCONN)) {
    printf("server socket listen error %d.", errno);
    return -1;
  }
  return 0;
}

static void RecvTcpData(int readfd, SockType type) {
  static char buf[MAXLINE];
  int pair = FindPairSock(readfd);
  while (running_) {
    ssize_t count;
    //此处清0，以免字符串处理时有影响
    memset(buf, 0, MAXLINE);
    count = read(readfd, buf, sizeof(buf));
    if (-1 == count) {
      if (errno != EAGAIN) {
        // EAGAIN means all data been read, go back.
        printf("read failed %d.", errno);
        HandleSockErr(readfd);
      }
      break;
    } else if (count == 0) {
      HandleSockErr(readfd);
      break;
    } else {
      uint16_t port = ParseFtdData(readfd, type, buf, &count);
      if (-1 != pair) {
        write(pair, buf, count);
      }
      if (port > 0) {
        int fd;
        if (0 == CreateAndBind(port, &fd)) {
          if (0 == SetNonBlock(fd)) {
            AddListenEvent(fd);
            SetSockType(fd, DATALISTEN);
            SetDataListenSock(readfd, type, fd);
          }
        }
      }
    }
  }
}

static void HandleEpollIn(int readfd) {
  SockType type = GetSockType(readfd);
  if (type == SOCKDEFAULT) {
    printf("sock %d type not found.\n", readfd);
    return;
  }
  if (DATALISTEN == readfd) {
  } else {
    RecvTcpData(readfd, type);
  }
}

static void* RetransThread(void* param) {
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
      printf("fd %d recv msg.\n", epollevent_[index].data.fd);
      if (epollevent_[index].events & EPOLLOUT) {
        // after connect, bind the connection
        if (0 == CheckSockConnect(epollevent_[index].data.fd)) {
          SessionConnected(epollevent_[index].data.fd);
        }
      }
      if ((epollevent_[index].events & EPOLLERR) ||
          (epollevent_[index].events & EPOLLHUP) ||
          (epollevent_[index].events & EPOLLRDHUP)) {
        printf("epoll error happen, events %u.\n", epollevent_[index].events);
        if ((epollevent_[index].events & EPOLLRDHUP) &&
            (epollevent_[index].events & EPOLLIN)) {
          RecvTcpData(epollevent_[index].data.fd,
                      GetSockType(epollevent_[index].data.fd));
        }
        HandleSockErr(epollevent_[index].data.fd);
        continue;
      }
      if (epollevent_[index].events & EPOLLIN) {
        if (listenfd_ == epollevent_[index].data.fd) {
          printf("listen index %d, fd %d.\n", index, listenfd_);
          AcceptHandle();
        } else {
          SockType type = GetSockType(epollevent_[index].data.fd);
          if (SOCKDEFAULT == type) {
            printf("error happen, no sock type found.\n");
            continue;
          }
          if (DATALISTEN == type) {
            HandleDataListen(epollevent_[index].data.fd);
          } else
            HandleEpollIn(epollevent_[index].data.fd);
        }
      }
    }
  }
}

int StartRetrans(uint16_t port, size_t pollsize) {
  epollsize_ = pollsize;
  epollevent_ = (struct epoll_event*)malloc((epollsize_ + 1) *
                                            sizeof(struct epoll_event));
  if (0 == epollevent_) return -1;
  if (-1 == CreateAndBind(port, &listenfd_)) return -1;
  if (-1 == SetNonBlock(listenfd_)) return -1;
  if (-1 == (epollfd_ = epoll_create1(0))) {
    printf("epoll create error %d.", errno);
    return -1;
  }
  if (-1 == AddListenEvent(listenfd_)) return -1;
  if (0 != pthread_create(&tid_, 0, RetransThread, 0)) {
    printf("create retransthread failed!\n");
    return -1;
  }
  MapInit();
  return 0;
}

void StopRetrans() {
  running_ = 0;
  pthread_join(&tid_, 0);
}