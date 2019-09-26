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
  uint16_t port_;
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

DataMod GetSockMod(int fd, SockType type) {
  DataMod mod = DEFAULT;
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == CTRLCLI && tmp->ctrlcli_ == fd) {
      mod = tmp->datamod_;
      break;
    }
    if (type == CTRLSVR && tmp->ctrlsvr_ == fd) {
      mod = tmp->datamod_;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return mod;
}

int NoDataPair(int fd) {
  int ret = -1;
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (tmp->listenfd_ == fd) {
      if (-1 == tmp->datacli_ && -1 == tmp->datasvr_) {
        ret = 0;
      } else {
        ret = -1;
      }
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return ret;
}

void SetDataListenSock(int fd, SockType type, int listenfd) {
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == CTRLCLI && tmp->ctrlcli_ == fd) {
      tmp->listenfd_ = listenfd;
      break;
    }
    if (type == CTRLSVR && tmp->ctrlsvr_ == fd) {
      tmp->listenfd_ = listenfd;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return;
}

int SetSockMod(int fd, SockType type, DataMod mod) {
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == CTRLCLI && tmp->ctrlcli_ == fd) {
      tmp->datamod_ = mod;
      break;
    }
    if (type == CTRLSVR && tmp->ctrlsvr_ == fd) {
      tmp->datamod_ = mod;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return 0;
}

int SetDataPort(int fd, SockType type, uint16_t port) {
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == CTRLCLI && tmp->ctrlcli_ == fd) {
      tmp->port_ = port;
      break;
    }
    if (type == CTRLSVR && tmp->ctrlsvr_ == fd) {
      tmp->port_ = port;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return 0;
}

uint16_t GetDataPort(int fd, SockType type) {
  uint16_t port = 0;
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == CTRLCLI && tmp->ctrlcli_ == fd) {
      port = tmp->port_;
      break;
    }
    if (type == CTRLSVR && tmp->ctrlsvr_ == fd) {
      port = tmp->port_;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return port;
}

void SetSockType(int fd, SockType type) {
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

static void RemoveSockType(int fd) {
  SockTypeMap* tmp = socktypehead_;
  SockTypeMap* pre = socktypehead_;
  if (socktypehead_ == 0)
    return;
  if (tmp->fd_ == fd) {
    socktypehead_ = socktypehead_->next_;
    free(tmp);
    return;
  }
  while (tmp) {
    if (tmp->fd_ == fd) {
      pre->next_ = tmp->next_;
      free(tmp);
      break;
    }
    pre = tmp;
    tmp = tmp->next_;
  }
}

static void RemoveSlbSvrMap(int fd, SockType type) {
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  SlbSvrMap* pre = head_;
  if (head_ == 0) {
    pthread_mutex_unlock(&mtx_);
    return;
  }
  if ((type == CTRLCLI && fd == tmp->ctrlcli_) ||
      (type == CTRLSVR && fd == tmp->ctrlsvr_)) {
    head_ = head_->next_;
    free(tmp);
    pthread_mutex_unlock(&mtx_);
    return;
  }
  while (tmp) {
    if ((type == CTRLCLI && fd == tmp->ctrlcli_) ||
        (type == CTRLSVR && fd == tmp->ctrlsvr_)) {
      pre->next_ = tmp->next_;
      free(tmp);
      break;
    }
    pre = tmp;
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
}

static void ResetSlbSvrMap(int fd, SockType type) {
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (tmp) {
    if (type == DATACLI && fd == tmp->datacli_) {
      tmp->datacli_ = -1;
      break;
    }

    if (type == DATASVR && fd == tmp->datasvr_) {
      tmp->datasvr_ = -1;
      break;
    }
    if (type == DATALISTEN && fd == tmp->listenfd_) {
      tmp->listenfd_ = -1;
      break;
    }
    tmp = tmp->next_;
  }

  pthread_mutex_unlock(&mtx_);
}

void RemoveSock(int fd) {
  SockType type = GetSockType(fd);
  RemoveSockType(fd);
  if (type == CTRLCLI || type == CTRLSVR) {
    RemoveSlbSvrMap(fd, type);
  } else {
    ResetSlbSvrMap(fd, type);
  }
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

int FindPeerAddr(int fd, struct sockaddr_in* addr) {
  SockType type = GetSockType(fd);
  if (CTRLCLI != type && DATACLI != type && DATASVR != type) return -1;
  int ret = -1;
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == CTRLCLI && tmp->ctrlcli_ == fd) {
      memcpy((char*)addr, (char*)&(tmp->realsvr_), sizeof(struct sockaddr_in));
      ret = 0;
      break;
    }
    if (type == DATACLI && tmp->datacli_ == fd) {
      if (tmp->datamod_ != PASV) break;
      memcpy((char*)addr, (char*)&(tmp->realsvr_), sizeof(struct sockaddr_in));
      addr->sin_port = htons(tmp->port_);
      ret = 0;
      break;
    }
    if (type == DATASVR && tmp->datasvr_ == fd) {
      if (tmp->datamod_ != PORT) break;
      memcpy((char*)addr, (char*)&(tmp->realcli_), sizeof(struct sockaddr_in));
      addr->sin_port = htons(tmp->port_);
      ret = 0;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return ret;
}

int FineDataConnAddr(int listenfd, struct sockaddr_in* addr) {
  int ret = -1;
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (tmp->listenfd_ == listenfd) {
      // pasv connect to real svr
      if (PASV == tmp->datamod_) {
        memcpy((char*)addr, (char*)&(tmp->realsvr_),
               sizeof(struct sockaddr_in));
        addr->sin_port = htons(tmp->port_);
        ret = 0;
      } else if (PORT == tmp->datamod_) {
        // port connect to real client
        memcpy((char*)addr, (char*)&(tmp->realcli_),
               sizeof(struct sockaddr_in));
        addr->sin_port = htons(tmp->port_);
        ret = 0;
      }
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return ret;
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

int FindListenSock(int fd, SockType type) {
  int listen = -1;
  if (DATACLI != type && DATASVR != type) return listen;
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (type == DATACLI && tmp->datacli_ == fd) {
      listen = tmp->listenfd_;
      break;
    }
    if (type == DATASVR && tmp->datasvr_ == fd) {
      listen = tmp->listenfd_;
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return listen;
}

int FindPairSock(int fd) {
  int pair = -1;
  SockType type = GetSockType(fd);
  if (SOCKDEFAULT == type || DATALISTEN == type) return pair;
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
    if (type == DATASVR && tmp->datasvr_ == fd) {
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

void MakeDataPair(int listenfd, int acceptfd, int connfd) {
  pthread_mutex_lock(&mtx_);
  SlbSvrMap* tmp = head_;
  while (0 != tmp) {
    if (listenfd == tmp->listenfd_) {
      //与 real svr 直接交互的为datacli，与real client直接交互的为datasvr
      if (PASV == tmp->datamod_) {
        tmp->datacli_ = connfd;
        tmp->datasvr_ = acceptfd;
        SetSockType(tmp->datacli_, DATACLI);
        SetSockType(tmp->datasvr_, DATASVR);
      } else if (PORT == tmp->datamod_) {
        tmp->datasvr_ = connfd;
        tmp->datacli_ = acceptfd;
        SetSockType(tmp->datacli_, DATACLI);
        SetSockType(tmp->datasvr_, DATASVR);
      }
      break;
    }
    tmp = tmp->next_;
  }
  pthread_mutex_unlock(&mtx_);
  return;
}