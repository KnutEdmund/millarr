#include "session.h"

namespace speed_core {

bool parseSessionId(const char* s, uint32_t& out) {
  unsigned len = 0;
  while (s[len] != '\0') ++len;
  if (len == 0 || len > 9) return false;
  if (s[0] == '0') return false;
  uint32_t v = 0;
  for (unsigned i = 0; i < len; ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + static_cast<uint32_t>(s[i] - '0');
  }
  out = v;
  return true;
}

}  // namespace speed_core
