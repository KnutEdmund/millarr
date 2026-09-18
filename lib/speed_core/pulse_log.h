// Fixed-capacity ring buffer of PulseRecord. Oldest records are overwritten
// when full; `seq` keeps increasing so clients can fetch incrementally.
#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>

#include "speed_types.h"

namespace speed_core {

class PulseLog {
 public:
  static constexpr size_t kCapacity = LOG_CAPACITY;

  void push(const PulseRecord& r) {
    buf_[head_] = r;
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity) {
      ++count_;
    } else {
      ++overwritten_;
    }
  }

  size_t size() const { return count_; }
  size_t capacity() const { return kCapacity; }
  uint32_t overwritten() const { return overwritten_; }
  uint32_t lastSeq() const { return count_ ? at(count_ - 1).seq : 0; }

  // i = 0 is the oldest retained record. Caller must ensure i < size().
  const PulseRecord& at(size_t i) const {
    size_t start = (head_ + kCapacity - count_) % kCapacity;
    return buf_[(start + i) % kCapacity];
  }

  // Visit records with seq > since_seq, oldest first, at most max_count.
  // Returns the number visited.
  template <typename F>
  size_t since(uint32_t since_seq, size_t max_count, F&& visit) const {
    size_t n = 0;
    for (size_t i = 0; i < count_ && n < max_count; ++i) {
      const PulseRecord& r = at(i);
      if (r.seq > since_seq) {
        visit(r);
        ++n;
      }
    }
    return n;
  }

  void clear() {
    head_ = 0;
    count_ = 0;
    overwritten_ = 0;
  }

 private:
  PulseRecord buf_[kCapacity];
  size_t head_ = 0;   // next write position
  size_t count_ = 0;  // retained records
  uint32_t overwritten_ = 0;
};

}  // namespace speed_core
