#include "sockmap.h"
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>

//以下部分可以使用hash map来实现
typedef struct _SlbSvrMap_ {
  int ctrlsvr_;
  int ctrlcli_;
  int datasvr_;
  int datacli_;
  int listenfd_;
  int datamod_;
  struct sockaddr_in realcli_;
  struct sockaddr_in realsvr_;
  struct _SlbSvrMap_* next_;
} SlbSvrMap;
//以下部分可以使用hash map来实现
typedef struct _SockTypeMap_ {
  int fd_;
  SockType type_;
  struct _SockTypeMap_* next_;
} SockTypeMap;

static SlbSvrMap* head_;
static pthread_mutex_t mtx_;
static SockTypeMap* socktypehead_;

enum DataMod { DEFAULT = -1, PASV = 0, PORT = 1 };

SockType GetSockType(int fd) {
  SockTypeMap* tmp = socktypehead_;
  while (tmp) {
    if (tmp->fd_ == fd) {
      return tmp->type_;
    }
    tmp = tmp->next_;
  }
  return SOCKDEFAULT;
}

static void SetSockType(int fd, SockType type) {
  if (0 == socktypehead_) {
    socktypehead_ = (SockTypeMap*)malloc(sizeof(SockTypeMap));
    if (0 == socktypehead_) {
      printf("create SockTypeMap failed.\n");
      return;
    }
    socktypehead_->fd_ = fd;
    socktypehead_->type_ = type;
    socktypehead_->next_ = 0;
    return;
  }
  SockTypeMap* tmp = socktypehead_;
  while (tmp) {
    if (tmp->fd_ == fd) {
      if (tmp->type_ != type)
        printf("error happen, fd %d new type %d, exist type %d.\n", fd, type,
               tmp->type_);
      return;
    }
    if (0 == tmp->next_) break;
    tmp = tmp->next_;
  }
  SockTypeMap* newmap = (SockTypeMap*)malloc(sizeof(SockTypeMap));
  if (0 == newmap) {
    printf("create SockTypeMap failed.\n");
    return;
  }
  newmap->fd_ = fd;
  newmap->type_ = type;
  newmap->next_ = 0;
  tmp->next_ = newmap;
}

int MapInit() {
  head_ = 0;
  socktypehead_ = 0;
  pthread_mutex_init(&mtx_, 0);
  return 0;
}

int MapRelease() {
  pthread_mutex_destroy(&mtx_);
  return 0;
}

static int AppendSockMap(SlbSvrMap* map) {
  pthread_mutex_lock(&mtx_);
  if (0 == head_) {
    head_ = map;
    pthread_mutex_unlock(&mtx_);
    return 0;
  }
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (tmp->ctrlsvr_ == map->ctrlsvr_ || tmp->ctrlcli_ == map->ctrlcli_) {
      printf("error happen in sock map.\n");
      free(map);
      pthread_mutex_unlock(&mtx_);
      return -1;
    }
    if (0 == tmp->next_)
      break;
    else
      tmp = tmp->next_;
  }
  tmp->next_ = map;
  pthread_mutex_unlock(&mtx_);
  return 0;
}

int FindPairSock(int fd) {
  int pair = -1;
  SockType type = GetSockType(fd);
  if (SOCKDEFAULT == type || DATALISTEN == type)
    return pair;
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == CTRLCLI && tmp->ctrlcli_ == fd) {
      pair = tmp->ctrlsvr_;
      break;
    }
    if (type == CTRLSVR && tmp->ctrlsvr_ == fd) {
      pair = tmp->ctrlcli_;
      break;
    }
    if (type == DATACLI && tmp->datacli_ == fd) {
      pair = tmp->datasvr_;
      break;
    }
    if (type == CTRLSVR && tmp->datasvr_ == fd) {
      pair = tmp->datacli_;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return pair;
}

int ConnBegin(const int slbfd, const int clifd, const char* svraddr) {
  SlbSvrMap* tmp = (SlbSvrMap*)malloc(sizeof(SlbSvrMap));
  if (0 == tmp) {
    printf("SlbSvrMap malloc failed.\n");
    return -1;
  }
  tmp->ctrlsvr_ = slbfd;
  tmp->ctrlcli_ = clifd;
  bzero(&tmp->realsvr_, sizeof(struct sockaddr_in));
  tmp->realsvr_.sin_family = AF_INET;
  tmp->realsvr_.sin_addr.s_addr = inet_addr(svraddr);
  tmp->realsvr_.sin_port = htons(21);
  tmp->next_ = 0;

  tmp->datacli_ = -1;
  tmp->datasvr_ = -1;
  tmp->datamod_ = DEFAULT;
  tmp->listenfd_ = -1;

  struct sockaddr addr;
  socklen_t len = sizeof(addr);
  if (0 != getpeername(slbfd, &addr, &len)) {
    printf("slb svr fd %d not connect %d.\n", slbfd, errno);
    return -1;
  }
  memcpy((char*)&tmp->realcli_, (char*)&addr, sizeof(addr));
  if (0 == AppendSockMap(tmp)) {
    SetSockType(slbfd, CTRLSVR);
    SetSockType(clifd, CTRLCLI);
    return 0;
  }
  return -1;
}