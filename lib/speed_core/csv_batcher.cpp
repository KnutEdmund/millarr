#include "csv_batcher.h"

#include <cstring>

#include "format.h"

namespace speed_core {

bool CsvBatcher::addRecord(const PulseRecord& r, uint32_t now_us) {
  char line[kMaxLine];
  int n = formatRecordCsv(r, line, sizeof line);
  if (n <= 0) return false;
  if (static_cast<size_t>(n) >= sizeof line) n = sizeof line - 1;
  if (used_ + static_cast<size_t>(n) > kCapacity) return false;
  std::memcpy(buf_ + used_, line, static_cast<size_t>(n));
  used_ += static_cast<size_t>(n);
  if (pending_ == 0) first_us_ = now_us;
  ++pending_;
  return true;
}

bool CsvBatcher::addLine(const char* line) {
  const size_t n = std::strlen(line);
  if (used_ + n > kCapacity) return false;
  std::memcpy(buf_ + used_, line, n);
  used_ += n;
  return true;
}

bool CsvBatcher::shouldFlush(uint32_t now_us) const {
  if (used_ == 0) return false;
  if (kCapacity - used_ < kMaxLine) return true;
  if (pending_ == 0) return false;
  if (pending_ >= kFlushRecords) return true;
  return (now_us - first_us_) >= kFlushAgeUs;  // unsigned: correct across a micros() wrap
}

void CsvBatcher::clear() {
  used_ = 0;
  pending_ = 0;
  first_us_ = 0;
}

}  // namespace speed_core
