#include "format.h"

#include <cstdio>

namespace {
// Appends `s` with '\n' escaped as "\\n" (no other escaping needed: '"' and '\\' are
// rejected by validation). Returns the snprintf-style length.
int appendJsonString(const char* s, char* buf, size_t n, int len) {
  if (len < 0 || (size_t)len >= n) return len;
  for (const char* p = s; *p != '\0'; ++p) {
    if ((size_t)len >= n) return len;
    int k = *p == '\n' ? std::snprintf(buf + len, n - len, "\\n")
                        : std::snprintf(buf + len, n - len, "%c", *p);
    if (k < 0) return k;
    len += k;
  }
  return len;
}

// Appends "[d0,d1,...]" for the first `count` distances. Returns total length like snprintf.
int appendDistances(const float* d, uint8_t count, char* buf, size_t n, int len) {
  if (len < 0 || (size_t)len >= n) return len;
  int k = std::snprintf(buf + len, n - len, "[");
  if (k < 0) return k;
  len += k;
  for (uint8_t i = 0; i < count; ++i) {
    if ((size_t)len >= n) return len;
    k = std::snprintf(buf + len, n - len, "%s%.3f", i ? "," : "", (double)d[i]);
    if (k < 0) return k;
    len += k;
  }
  if ((size_t)len >= n) return len;
  k = std::snprintf(buf + len, n - len, "]");
  if (k < 0) return k;
  return len + k;
}
}  // namespace

namespace speed_core {

const char* const kCsvHeader = "seq,t_us,interval_us,mark,mark_speed_mps,rev_speed_mps\n";

int formatRecordCsv(const PulseRecord& r, char* buf, size_t n) {
  return std::snprintf(buf, n, "%lu,%lu,%lu,%u,%.3f,%.3f\n", (unsigned long)r.seq,
                       (unsigned long)r.t_us, (unsigned long)r.interval_us, (unsigned)r.mark_index,
                       (double)r.mark_speed_mps, (double)r.rev_speed_mps);
}

int formatRecordJson(const PulseRecord& r, char* buf, size_t n) {
  return std::snprintf(buf, n,
                       "{\"seq\":%lu,\"t_us\":%lu,\"interval_us\":%lu,\"mark\":%u,"
                       "\"mark_speed_mps\":%.3f,\"rev_speed_mps\":%.3f}",
                       (unsigned long)r.seq, (unsigned long)r.t_us, (unsigned long)r.interval_us,
                       (unsigned)r.mark_index, (double)r.mark_speed_mps, (double)r.rev_speed_mps);
}

int formatStatusJson(const EngineStatus& s, const EngineConfig& c, const StatusExtras& x, char* buf,
                     size_t n) {
  int len = std::snprintf(buf, n,
                          "{\"speed_mps\":%.3f,\"speed_kmh\":%.3f,\"running\":%s,\"phase_locked\":%s,"
                          "\"distance_m\":%.3f,\"pulse_count\":%lu,\"revolution_count\":%lu,"
                          "\"rejected_debounce\":%lu,\"rejected_implausible\":%lu,\"isr_dropped\":%lu,"
                          "\"mark_share\":[",
                          (double)s.speed_mps, (double)s.speed_mps * 3.6, s.running ? "true" : "false",
                          s.phase_locked ? "true" : "false", (double)s.distance_m,
                          (unsigned long)s.pulse_count, (unsigned long)s.revolution_count,
                          (unsigned long)s.rejected_debounce, (unsigned long)s.rejected_implausible,
                          (unsigned long)x.isr_dropped);
  if (len < 0 || (size_t)len >= n) return len;
  const uint8_t n_marks = c.marks_count > MAX_MARKS ? MAX_MARKS : c.marks_count;
  for (uint8_t m = 0; m < n_marks; ++m) {
    int k = std::snprintf(buf + len, n - len, "%s%.3f", m ? "," : "", (double)s.mark_share[m]);
    if (k < 0) return k;
    len += k;
    if ((size_t)len >= n) return len;
  }
  int k = std::snprintf(buf + len, n - len,
                        "],\"rev_speed_stddev\":%.4f,\"stats_revolutions\":%u,"
                        "\"log_size\":%lu,\"log_capacity\":%lu,\"log_overwritten\":%lu,"
                        "\"last_seq\":%lu,\"uptime_s\":%lu,"
                        "\"flash\":{\"ok\":%s,\"full\":%s,\"total\":%lu,\"used\":%lu,"
                        "\"session_id\":%lu,\"session_records\":%lu,\"session_bytes\":%lu,"
                        "\"write_errors\":%lu,\"sessions\":%u},"
                        "\"profile\":{\"id\":%u,\"name\":\"%s\"},"
                        "\"config\":{\"marks_count\":%u,\"mark_distance_m\":",
                        (double)s.rev_speed_stddev, (unsigned)s.stats_revolutions,
                        (unsigned long)s.log_size, (unsigned long)s.log_capacity,
                        (unsigned long)s.log_overwritten, (unsigned long)s.last_seq,
                        (unsigned long)x.uptime_s, x.flash.ok ? "true" : "false",
                        x.flash.full ? "true" : "false", (unsigned long)x.flash.total_bytes,
                        (unsigned long)x.flash.used_bytes, (unsigned long)x.flash.session_id,
                        (unsigned long)x.flash.session_records, (unsigned long)x.flash.session_bytes,
                        (unsigned long)x.flash.write_errors, (unsigned)x.flash.sessions,
                        (unsigned)x.profile_id, x.profile_name ? x.profile_name : "",
                        (unsigned)n_marks);
  if (k < 0) return k;
  len += k;
  len = appendDistances(c.mark_distance_m, n_marks, buf, n, len);
  if (len < 0 || (size_t)len >= n) return len;
  k = std::snprintf(buf + len, n - len,
                    ",\"belt_length_m\":%.3f,\"debounce_us\":%lu,\"stop_timeout_us\":%lu,"
                    "\"max_speed_mps\":%.3f}}",
                    (double)beltLength(c), (unsigned long)c.debounce_us,
                    (unsigned long)c.stop_timeout_us, (double)c.max_speed_mps);
  if (k < 0) return k;
  return len + k;
}

int formatProfileJson(const Profile& p, uint8_t id, char* buf, size_t n) {
  const uint8_t marks = p.marks > MAX_MARKS ? MAX_MARKS : p.marks;
  int len = std::snprintf(buf, n, "{\"id\":%u,\"name\":\"%s\",\"notes\":\"", (unsigned)id, p.name);
  len = appendJsonString(p.notes, buf, n, len);
  if (len < 0 || (size_t)len >= n) return len;
  int k = std::snprintf(buf + len, n - len, "\",\"type\":\"%s\",\"marks\":%u,\"mark_distance_m\":",
                        typeName(p.type), (unsigned)marks);
  if (k < 0) return k;
  len += k;
  len = appendDistances(p.mark_distance_m, marks, buf, n, len);
  if (len < 0 || (size_t)len >= n) return len;
  k = std::snprintf(buf + len, n - len, ",\"slat_count\":%u,\"slat_pitch_m\":%.5f,\"belt_length_m\":%.3f}",
                    (unsigned)p.slat_count, (double)p.slat_pitch_m, (double)beltLength(p));
  if (k < 0) return k;
  return len + k;
}

}  // namespace speed_core

namespace speed_core {
namespace {
// "# profile <id> "<name>" type=... marks=N belt_length_m=... mark_distance_m=[...]" plus
// " slat_count=N slat_pitch_m=..." for slats. No trailing newline.
int appendProfileComment(const Profile& p, uint8_t id, char* buf, size_t n, int len) {
  if (len < 0 || (size_t)len >= n) return len;
  const uint8_t marks = p.marks > MAX_MARKS ? MAX_MARKS : p.marks;
  int k = std::snprintf(buf + len, n - len, "# profile %u \"%s\" type=%s marks=%u belt_length_m=%.3f mark_distance_m=",
                        (unsigned)id, p.name, typeName(p.type), (unsigned)marks, (double)beltLength(p));
  if (k < 0) return k;
  len += k;
  len = appendDistances(p.mark_distance_m, marks, buf, n, len);
  if (len < 0 || (size_t)len >= n) return len;
  if (p.type == TreadmillType::Slats) {
    k = std::snprintf(buf + len, n - len, " slat_count=%u slat_pitch_m=%.5f", (unsigned)p.slat_count,
                      (double)p.slat_pitch_m);
    if (k < 0) return k;
    len += k;
  }
  return len;
}
}  // namespace

int formatSessionHeader(const SessionHeaderInfo& h, char* buf, size_t n) {
  int len = std::snprintf(buf, n, "# millarr session %lu\n# firmware %s boot_uptime_s %lu\n",
                          (unsigned long)h.session_id, h.firmware ? h.firmware : "unknown",
                          (unsigned long)h.uptime_s);
  if (len < 0 || (size_t)len >= n) return len;
  if (h.profile) {
    len = appendProfileComment(*h.profile, h.profile_id, buf, n, len);
    if (len < 0 || (size_t)len >= n) return len;
    int k = std::snprintf(buf + len, n - len, "\n");
    if (k < 0) return k;
    len += k;
  }
  if ((size_t)len >= n) return len;
  int k = std::snprintf(buf + len, n - len,
                        "# t_us wraps every 4294967296 us: add 2^32 whenever it decreases. "
                        "interval_us=0 marks a run restart.\n");
  if (k < 0) return k;
  return len + k;
}

int formatProfileComment(const Profile& p, uint8_t id, uint32_t uptime_s, char* buf, size_t n) {
  int len = appendProfileComment(p, id, buf, n, 0);
  if (len < 0 || (size_t)len >= n) return len;
  int k = std::snprintf(buf + len, n - len, " at_uptime_s=%lu\n", (unsigned long)uptime_s);
  if (k < 0) return k;
  return len + k;
}

int formatSessionJson(const SessionInfo& s, char* buf, size_t n) {
  if (s.has_records) {
    return std::snprintf(buf, n, "{\"id\":%lu,\"bytes\":%lu,\"active\":%s,\"records\":%lu}",
                         (unsigned long)s.id, (unsigned long)s.bytes, s.active ? "true" : "false",
                         (unsigned long)s.records);
  }
  return std::snprintf(buf, n, "{\"id\":%lu,\"bytes\":%lu,\"active\":%s}", (unsigned long)s.id,
                       (unsigned long)s.bytes, s.active ? "true" : "false");
}

}  // namespace speed_core
