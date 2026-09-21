// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace haier_management {
struct Credentials {
  uint32_t magic{0x48435201};
  char web[65]{}, ota[65]{}, setup[64]{};
  uint8_t configured{0};
};
inline bool password(const char *s, size_t capacity, size_t minimum) {
  size_t n=strnlen(s,capacity);if(n<minimum||n>=capacity)return false;
  for(size_t i=0;i<n;++i)if(static_cast<unsigned char>(s[i])<33||static_cast<unsigned char>(s[i])>126)return false;
  return true;
}
inline bool valid(const Credentials &c) {
  return c.magic==0x48435201&&c.configured<=1&&password(c.web,sizeof(c.web),16)&&password(c.ota,sizeof(c.ota),16)&&password(c.setup,sizeof(c.setup),8);
}
}
