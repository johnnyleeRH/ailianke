#ifndef SOCK_MAP_H_
#define SOCK_MAP_H_
#include <sys/socket.h>
#include <stdint.h>

typedef enum _SockType_ {
  SOCKDEFAULT = -1,
  CTRLSVR = 0,
  CTRLCLI = 1,
  DATALISTEN = 2,
  DATASVR = 3,
  DATACLI = 4,
} SockType;
// int Connected(const int clifd);
int ConnBegin(const int slbfd, const int clifd, const char* svraddr);
// int FindPeerInBack(int clifd, struct sockaddr_in* addr);
// void ReleaseByCli(int clifd);
int MapInit();
int MapDestroy();
int FindPairSock(int fd);
SockType GetSockType(int fd);
#endif // SOCK_MAP_H_