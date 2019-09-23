#ifndef TCP_SVR_H_
#define TCP_SVR_H_

#include <stdint.h>
#include <stddef.h>

int StartTcpSvr(uint16_t port, size_t pollsize);
void Stop();
void Release();
int IsRunning();
int ConnectPairClient(const int slbfd, const char* svraddr);

#endif //TCP_SVR_H_