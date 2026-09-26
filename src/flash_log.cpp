#include "flash_log.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>

#include <cstdio>
#include <cstring>

#include "active_profile.h"
#include "csv_batcher.h"
#include "format.h"

using speed_core::CsvBatcher;
using speed_core::SessionInfo;

namespace flash_log {
namespace {

constexpr const char* kDir = "/log";
constexpr uint8_t kMaxSessions = 32;
constexpr uint32_t kReserveBytes = 96u * 1024u;  // headroom for littlefs metadata and compaction

CsvBatcher batcher;
Preferences prefs;
const char* firmware_ = "unknown";
bool ok_ = false;
bool full_ = false;
bool file_exists_ = false;
uint32_t session_id_ = 0;
uint32_t cursor_seq_ = 0;
uint32_t session_records_ = 0;
uint32_t session_bytes_ = 0;
uint32_t total_ = 0;
uint32_t used_ = 0;
uint32_t write_errors_ = 0;
uint8_t sessions_ = 0;

void pathFor(uint32_t id, char* out, size_t n) {
  snprintf(out, n, "%s/%06lu.csv", kDir, (unsigned long)id);
}

// "000123.csv" (with or without a leading directory) -> 123.
bool idFromName(const char* name, uint32_t& id) {
  const char* base = strrchr(name, '/');
  base = base ? base + 1 : name;
  size_t len = strlen(base);
  if (len != 10 || strcmp(base + 6, ".csv") != 0) return false;
  uint32_t v = 0;
  for (size_t i = 0; i < 6; ++i) {
    if (base[i] < '0' || base[i] > '9') return false;
    v = v * 10 + static_cast<uint32_t>(base[i] - '0');
  }
  if (v == 0) return false;
  id = v;
  return true;
}

// Unsorted directory scan; sorts ascending by id (at most kMaxSessions + a few).
size_t scan(SessionInfo* out, size_t max) {
  size_t n = 0;
  File dir = LittleFS.open(kDir);
  if (!dir || !dir.isDirectory()) return 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    uint32_t id;
    if (f.isDirectory() || !idFromName(f.name(), id)) continue;
    SessionInfo info{id, static_cast<uint32_t>(f.size()), 0, id == session_id_, false};
    if (info.active) {
      info.records = session_records_;
      info.has_records = true;
    }
    if (n < max) {
      out[n++] = info;
    } else {
      // Keep the `max` newest: replace the smallest id if this one is larger.
      size_t smallest = 0;
      for (size_t i = 1; i < n; ++i) if (out[i].id < out[smallest].id) smallest = i;
      if (out[smallest].id < id) out[smallest] = info;
    }
  }
  for (size_t i = 1; i < n; ++i) {  // insertion sort, n is tiny
    SessionInfo key = out[i];
    size_t j = i;
    while (j > 0 && out[j - 1].id > key.id) { out[j] = out[j - 1]; --j; }
    out[j] = key;
  }
  return n;
}

void refreshUsage() {
  total_ = static_cast<uint32_t>(LittleFS.totalBytes());
  used_ = static_cast<uint32_t>(LittleFS.usedBytes());
  SessionInfo tmp[kMaxSessions + 4];
  sessions_ = static_cast<uint8_t>(scan(tmp, sizeof tmp / sizeof tmp[0]));
}

// Delete the oldest closed sessions until `incoming` more bytes fit and the
// session count is within budget. False when only the active session remains
// and the budget is still exceeded.
bool evictToBudget(size_t incoming) {
  for (;;) {
    SessionInfo list[kMaxSessions + 4];
    const size_t n = scan(list, sizeof list / sizeof list[0]);
    const bool over_count = n > kMaxSessions;
    const bool over_bytes = static_cast<uint64_t>(used_) + incoming + kReserveBytes > total_;
    if (!over_count && !over_bytes) return true;
    size_t victim = n;
    for (size_t i = 0; i < n; ++i) {
      if (!list[i].active) { victim = i; break; }  // ascending: the first closed one is the oldest
    }
    if (victim == n) return false;
    char path[32];
    pathFor(list[victim].id, path, sizeof path);
    LittleFS.remove(path);
    Serial.printf("flash: evicted session %lu (%lu B)\n", (unsigned long)list[victim].id,
                  (unsigned long)list[victim].bytes);
    refreshUsage();
  }
}

uint32_t allocateId() {
  const uint32_t next = prefs.getULong("next", 1);
  prefs.putULong("next", next + 1);
  return next;
}

void flushBatch() {
  if (!ok_ || batcher.size() == 0) return;
  static char hdr[512];
  size_t hdr_len = 0;
  if (!file_exists_) {
    int n = header(hdr, sizeof hdr);
    hdr_len = n < 0 ? 0 : (static_cast<size_t>(n) >= sizeof hdr ? sizeof hdr - 1 : static_cast<size_t>(n));
  }
  if (!evictToBudget(batcher.size() + hdr_len)) {
    full_ = true;
    ++write_errors_;
    batcher.clear();
    return;
  }
  full_ = false;
  char path[32];
  pathFor(session_id_, path, sizeof path);
  File f = LittleFS.open(path, FILE_APPEND);
  if (!f) {
    ++write_errors_;
    batcher.clear();
    return;
  }
  bool good = true;
  if (!file_exists_) {
    good = f.write(reinterpret_cast<const uint8_t*>(hdr), hdr_len) == hdr_len;
    good = good && f.write(reinterpret_cast<const uint8_t*>(speed_core::kCsvHeader),
                           strlen(speed_core::kCsvHeader)) == strlen(speed_core::kCsvHeader);
  }
  good = good && f.write(reinterpret_cast<const uint8_t*>(batcher.data()), batcher.size()) == batcher.size();
  f.close();
  if (!good) ++write_errors_;
  file_exists_ = true;
  session_bytes_ = sessionBytes(session_id_);
  batcher.clear();
  refreshUsage();
}

}  // namespace

void begin(const char* firmware_hash) {
  firmware_ = firmware_hash ? firmware_hash : "unknown";
  ok_ = LittleFS.begin(true, "/littlefs", 4, "spiffs");
  if (!ok_) {
    Serial.println("flash: mount failed, session log disabled");
    return;
  }
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  prefs.begin("log", false);
  session_id_ = allocateId();
  refreshUsage();
  evictToBudget(0);
  Serial.printf("flash: ok total=%lu used=%lu sessions=%u session=%lu fw=%s\n", (unsigned long)total_,
                (unsigned long)used_, (unsigned)sessions_, (unsigned long)session_id_, firmware_);
}

void observe(const speed_core::PulseLog& log) {
  if (log.size() == 0) return;
  const speed_core::PulseRecord& r = log.newest();
  if (r.seq == cursor_seq_) return;
  cursor_seq_ = r.seq;
  if (!ok_) return;
  ++session_records_;
  if (batcher.addRecord(r, r.t_us)) return;
  flushBatch();
  if (!batcher.addRecord(r, r.t_us)) ++write_errors_;
}

void service(uint32_t now_us) {
  if (ok_ && batcher.shouldFlush(now_us)) flushBatch();
}

void flush() { flushBatch(); }

void rotate() {
  flushBatch();
  if (ok_) session_id_ = allocateId();
  cursor_seq_ = 0;
  session_records_ = 0;
  session_bytes_ = 0;
  file_exists_ = false;
  full_ = false;
  batcher.clear();
  if (ok_) refreshUsage();
}

void noteProfile(const speed_core::Profile& p, uint8_t id) {
  if (!ok_ || !file_exists_) return;  // the header carries the profile until then
  char line[256];
  int n = speed_core::formatProfileComment(p, id, millis() / 1000, line, sizeof line);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof line) return;
  if (!batcher.addLine(line)) {
    flushBatch();
    batcher.addLine(line);
  }
}

int header(char* buf, size_t n) {
  speed_core::SessionHeaderInfo h{session_id_, firmware_, millis() / 1000, &active_profile::current(),
                                  active_profile::slot()};
  return speed_core::formatSessionHeader(h, buf, n);
}

speed_core::FlashStatus status() {
  speed_core::FlashStatus s{};
  s.ok = ok_;
  s.full = full_;
  s.total_bytes = total_;
  s.used_bytes = used_;
  s.session_id = session_id_;
  s.session_records = session_records_;
  s.session_bytes = session_bytes_;
  s.write_errors = write_errors_;
  s.sessions = sessions_;
  return s;
}

uint32_t sessionId() { return session_id_; }

size_t list(SessionInfo* out, size_t max) { return ok_ ? scan(out, max) : 0; }

bool exists(uint32_t id) {
  if (!ok_) return false;
  char path[32];
  pathFor(id, path, sizeof path);
  return LittleFS.exists(path);
}

uint32_t sessionBytes(uint32_t id) {
  if (!ok_) return 0;
  char path[32];
  pathFor(id, path, sizeof path);
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return 0;
  const uint32_t size = static_cast<uint32_t>(f.size());
  f.close();
  return size;
}

bool remove(uint32_t id) {
  if (!ok_ || id == session_id_) return false;
  char path[32];
  pathFor(id, path, sizeof path);
  if (!LittleFS.exists(path)) return false;
  const bool removed = LittleFS.remove(path);
  if (removed) refreshUsage();
  return removed;
}

fs::File openForRead(uint32_t id) {
  if (!ok_) return fs::File();
  char path[32];
  pathFor(id, path, sizeof path);
  return LittleFS.open(path, FILE_READ);
}

}  // namespace flash_log
