#include "toy.h"
size_t toy_upper(char *buf, size_t len) {
  size_t n = 0;
  for (size_t i = 0; i < len; i++)
    if (buf[i] >= 'a' && buf[i] <= 'z') { buf[i] -= 32; n++; }
  return n;
}
