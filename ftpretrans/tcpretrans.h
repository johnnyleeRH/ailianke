#ifndef TCP_RETRANS_H_
#define TCP_RETRANS_H_

#include <stdint.h>
#include <stddef.h>

// 开启slb侧侦听服务
// port: 侦听端口，默认为21， pollsize： 侦听套接字数目
int StartRetrans(uint16_t port, size_t pollsize);
// 关闭slb侧侦听服务
void StopRetrans();
#endif // TCP_RETRANS_H_