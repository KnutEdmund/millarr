// Collects CSV lines for the flash session log in a fixed buffer so that flash
// writes happen from loop() in batches, never from the pulse path. No heap.
#pragma once
#include <cstddef>
#include <cstdint>

#include "speed_types.h"

namespace speed_core {

class CsvBatcher {
 public:
  static constexpr size_t kCapacity = 4096;
  static constexpr size_t kMaxLine = 96;             // longest line addRecord/addLine may produce
  static constexpr uint32_t kFlushRecords = 16;      // flush when this many records are pending
  static constexpr uint32_t kFlushAgeUs = 5000000u;  // or when the oldest pending record is this old

  // Append one record as a CSV line. Returns false (and appends nothing) if the
  // line would not fit; the caller should flush first.
  bool addRecord(const PulseRecord& r, uint32_t now_us);
  // Append a raw line (header or '#' comment). Not counted as a record.
  bool addLine(const char* line);
  // pending >= kFlushRecords, or the oldest pending record is >= kFlushAgeUs old,
  // or fewer than kMaxLine bytes are free. Never true while empty.
  bool shouldFlush(uint32_t now_us) const;

  const char* data() const { return buf_; }
  size_t size() const { return used_; }
  uint32_t pending() const { return pending_; }
  void clear();

 private:
  char buf_[kCapacity];
  size_t used_ = 0;
  uint32_t pending_ = 0;
  uint32_t first_us_ = 0;  // timestamp of the oldest pending record
};

}  // namespace speed_core
