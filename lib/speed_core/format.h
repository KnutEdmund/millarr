// CSV and JSON text formatting for records and status. snprintf-based, no heap.
#pragma once
#include <cstddef>
#include <cstdint>

#include "speed_types.h"
#include "profile.h"

namespace speed_core {

extern const char* const kCsvHeader;

struct StatusExtras {
  uint32_t isr_dropped;
  uint32_t uptime_s;
  uint8_t profile_id;
  const char* profile_name;  // already validated: no '"' or '\\'
};

// Each function returns the snprintf result: the length that would have been
// written. Output is truncated if it is >= n.
int formatRecordCsv(const PulseRecord& r, char* buf, size_t n);
int formatRecordJson(const PulseRecord& r, char* buf, size_t n);
int formatStatusJson(const EngineStatus& s, const EngineConfig& c, const StatusExtras& x, char* buf,
                     size_t n);
int formatProfileJson(const Profile& p, uint8_t id, char* buf, size_t n);

}  // namespace speed_core
