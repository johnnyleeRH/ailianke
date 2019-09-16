#include <stdio.h>
#include <stdint.h>

int main() {
  uint64_t utest = 0;
  printf("ffs is %d.\n", __builtin_ffsl(utest));
  utest |= 1 << 0;
  printf("ffs is %d.\n", __builtin_ffsl(utest));
  utest &= ~(1 << 0);
  utest |= 1 << 15;
  printf("ffs is %d.\n", __builtin_ffsl(utest));
  utest &= ~(1 << 15);
  utest |= 1 << 31;
  // 返回右起第一个位为1的位数，最多支持32位
  printf("ffs is %d.\n", __builtin_ffsl(utest));
  // 返回为1 的个数，只支持32位
  printf("ffs is %d.\n", __builtin_popcount(utest));
}