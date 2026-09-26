// Session metadata for the flash log: one session per seq space (boot or reset).
#pragma once
#include <cstdint>

namespace speed_core {

struct SessionInfo {
  uint32_t id;
  uint32_t bytes;
  uint32_t records;   // valid only when has_records
  bool active;
  bool has_records;   // exact count is known only for the active session
};

// 1..9 decimal digits, no leading zero, value > 0.
bool parseSessionId(const char* s, uint32_t& out);

}  // namespace speed_core
