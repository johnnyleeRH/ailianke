#ifndef FTP_PROTO_H_
#define FTP_PROTO_H_

#include <sys/types.h>
#include "sockmap.h"

uint16_t ParseFtdData(const int fd, const SockType type, char* buf, ssize_t* cnt);

#endif // FTP_PROTO_H_