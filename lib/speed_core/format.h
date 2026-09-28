// CSV and JSON text formatting for records and status. snprintf-based, no heap.
#pragma once
#include <cstddef>
#include <cstdint>

#include "speed_types.h"
#include "profile.h"
#include "session.h"

namespace speed_core {

extern const char* const kCsvHeader;

struct StatusExtras {
  uint32_t isr_dropped;
  uint32_t uptime_s;
  uint8_t profile_id;
  const char* profile_name;  // already validated: no '"' or '\\'
  FlashStatus flash;
  BleStatus ble;
};

// What goes at the top of every session file, before kCsvHeader.
struct SessionHeaderInfo {
  uint32_t session_id;
  const char* firmware;   // short git hash, or "unknown"
  uint32_t uptime_s;      // when the file was created
  const Profile* profile;
  uint8_t profile_id;
};

// Each function returns the snprintf result: the length that would have been
// written. Output is truncated if it is >= n.
int formatRecordCsv(const PulseRecord& r, char* buf, size_t n);
int formatRecordJson(const PulseRecord& r, char* buf, size_t n);
int formatStatusJson(const EngineStatus& s, const EngineConfig& c, const StatusExtras& x, char* buf,
                     size_t n);
int formatProfileJson(const Profile& p, uint8_t id, char* buf, size_t n);
// '#' comment lines only (no kCsvHeader), each ending in '\n'.
int formatSessionHeader(const SessionHeaderInfo& h, char* buf, size_t n);
// One "# profile ..." line recording a profile switch mid-session.
int formatProfileComment(const Profile& p, uint8_t id, uint32_t uptime_s, char* buf, size_t n);
int formatSessionJson(const SessionInfo& s, char* buf, size_t n);

}  // namespace speed_core
