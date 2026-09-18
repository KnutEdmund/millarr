// CSV and JSON text formatting for records and status. snprintf-based, no heap.
#pragma once
#include <cstddef>
#include <cstdint>

#include "speed_types.h"

namespace speed_core {

extern const char* const kCsvHeader;

// Each function returns the snprintf result: the length that would have been
// written. Output is truncated if it is >= n.
int formatRecordCsv(const PulseRecord& r, char* buf, size_t n);
int formatRecordJson(const PulseRecord& r, char* buf, size_t n);
int formatStatusJson(const EngineStatus& s, const EngineConfig& c, uint32_t isr_dropped,
                     uint32_t uptime_s, char* buf, size_t n);

}  // namespace speed_core
