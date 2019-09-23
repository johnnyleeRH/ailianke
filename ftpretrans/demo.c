#include "tcpretrans.h"
#include <unistd.h>

int main() {
  if (StartRetrans(21, 1024) < 0)
    return -1;
  while (1) {
    sleep(1);
  }
}