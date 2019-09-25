#ifndef SOCK_MAP_H_
#define SOCK_MAP_H_
#include <stdint.h>
#include <sys/socket.h>

typedef enum _SockType_ {
  SOCKDEFAULT = -1,
  CTRLSVR = 0,
  CTRLCLI = 1,
  DATALISTEN = 2,
  DATASVR = 3,
  DATACLI = 4,
} SockType;

typedef enum _DataMod_ {
  DEFAULT = -1,
  PASVTRIGGERED = 0,
  PORTTRIGGERED = 1,
  PASV = 2,
  PORT = 3
} DataMod;

int ConnBegin(const int slbfd, const int clifd, const char* svraddr);
// void ReleaseByCli(int clifd);
int MapInit();
int MapDestroy();
int FindPairSock(int fd);
void SetSockType(int fd, SockType type);
SockType GetSockType(int fd);
int SetSockMod(int fd, SockType type, DataMod mod);
DataMod GetSockMod(int fd, SockType type);
int SetDataPort(int fd, SockType type, uint16_t port);
uint16_t GetDataPort(int fd, SockType type);

#endif  // SOCK_MAP_H_