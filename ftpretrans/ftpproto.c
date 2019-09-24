#include "ftpproto.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned char GetValFromString(char* buf, int beg, int end) {
  char tmp[4] = {0};
  if (end - beg > 4)
    return 0;
  memcpy(tmp, buf + beg + 1, end - beg -1);
  return atoi(tmp);
}

static uint16_t ReplaceAddress(const int fd, char* buf, ssize_t* cnt) {
  int firstcomma = -1;
  int forthcomma = -1;
  int fifthcomma = -1;
  int numend = -1;
  int commacnt = 0;
  // check comma cnt, total shall be 5
  for (int id = 0; id < *cnt; ++id) {
    if (*(buf + id) == ',') {
      if (-1 == firstcomma) firstcomma = id;
      commacnt++;
      if (4 == commacnt)
        forthcomma = id;
      else if (5 == commacnt)
        fifthcomma = id;
    } else if (-1 != fifthcomma && -1 == numend) {
      if (buf[id] < '0' || buf[id] > '9') numend = id;
    }
  }
  if (5 != commacnt) {
    printf("buf %s wrong or not complete, commacnt %d.\n", buf, commacnt);
    return 0;
  }
  struct sockaddr_in sin;
  int ulen = sizeof(sin);
  if (getsockname(fd, (struct sockaddr*)&sin, &ulen) == 0) {
    printf("local sock port %u , addr 0x%x.\n", ntohs(sin.sin_port),
           sin.sin_addr.s_addr);
  } else {
    printf("get local sock name failed, error %d.\n", errno);
    return 0;
  }
  // begin replace
  int beg = firstcomma - 1;
  while (buf[beg] <= '9' && buf[beg] >= '0') {
    beg--;
    if (beg < 0) break;
  }
  if (beg < 0) return 0;
  static char tmp[1024] = {0};
  int replacecnt = 0;
  memcpy(tmp, buf, beg + 1);
  replacecnt = beg + 1;
  unsigned char* addr = (unsigned char*)&sin.sin_addr.s_addr;
  int occupied = 0;
  for (int id = 0; id < 4; ++id) {
    if (*addr >= 100)
      occupied = 3;
    else if (*addr >= 10)
      occupied = 2;
    else
      occupied = 1;
    snprintf(tmp + replacecnt, occupied + 2, "%d,", *addr);
    replacecnt += occupied + 1;
    addr += 1;
  }
  memcpy(tmp + replacecnt, buf + forthcomma + 1, *cnt - forthcomma - 1);
  replacecnt += *cnt - forthcomma - 1;
  uint16_t port = 0;
  addr = (unsigned char*)&port;
  *addr = GetValFromString(buf, fifthcomma, numend);
  addr += 1;
  *addr = GetValFromString(buf, forthcomma, fifthcomma);
  memcpy(buf, tmp, replacecnt);
  *cnt = replacecnt;
  return port;
}

// client command是套接字作为客户端使用的情景
//此时收到是从真实服务端返回的包
static void ParseCliCommand(const int fd, char* buf, ssize_t* cnt) {
  DataMod mod = GetSockMod(fd, CTRLCLI);
  // have send "PASV"
  if (PASVTRIGGERED == mod) {
    if (0 != strstr(buf, "227")) {
      SetSockMod(fd, CTRLCLI, PASV);
      printf("ftp go to pasv mode.\n");
      // TODO: 接收返回的PORT信息，修改本地IP内容
      // 227 Entering Passive Mode (10,64,38,48,244,113).\r\n ==> 227 Entering
      // Passive Mode (10,10,108,147,244,113).\r\n
      ReplaceAddress(fd, buf, cnt);
    } else {
      SetSockMod(fd, CTRLCLI, DEFAULT);
    }
  } else if (PORTTRIGGERED == mod) {
    if (0 != strstr(buf, "200")) {
      SetSockMod(fd, CTRLCLI, PORT);
      printf("ftp go to port mode.\n");
    } else {
      SetSockMod(fd, CTRLCLI, DEFAULT);
    }
  }
}

// svr command是套接字作为服务端使用的情景
//此时是从真实客户端返回的包
static void ParseSvrCommand(const int fd, char* buf, ssize_t* cnt) {
  if (0 != strstr(buf, "PASV")) {
    SetSockMod(fd, CTRLSVR, PASVTRIGGERED);
    return;
  }
  // PORT模式需要修改客户端IP为本地IP
  if (0 != strstr(buf, "PORT")) {
    SetSockMod(fd, CTRLSVR, PORTTRIGGERED);
    // TODO：记录端口号，修改本地IP内容
    // PORT 10,64,38,48,236,66\r\n ==> PORT 10,10,108,147,236,66\r\n
    ReplaceAddress(fd, buf, cnt);
  }
  return;
}

//对收到的数据校验或者二次加工
void ParseFtdData(const int fd, const SockType type, char* buf, ssize_t* cnt) {
  if (CTRLCLI != type && CTRLSVR != type) return;
  if (CTRLCLI == type)
    ParseCliCommand(fd, buf, cnt);
  else
    ParseSvrCommand(fd, buf, cnt);
}