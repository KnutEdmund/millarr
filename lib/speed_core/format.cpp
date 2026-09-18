#include "format.h"

#include <cstdio>

namespace {
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
                          "\"rejected_debounce\":%lu,\"isr_dropped\":%lu,\"mark_share\":[",
                          (double)s.speed_mps, (double)s.speed_mps * 3.6, s.running ? "true" : "false",
                          s.phase_locked ? "true" : "false", (double)s.distance_m,
                          (unsigned long)s.pulse_count, (unsigned long)s.revolution_count,
                          (unsigned long)s.rejected_debounce, (unsigned long)x.isr_dropped);
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
                        "\"profile\":{\"id\":%u,\"name\":\"%s\"},"
                        "\"config\":{\"marks_count\":%u,\"mark_distance_m\":",
                        (double)s.rev_speed_stddev, (unsigned)s.stats_revolutions,
                        (unsigned long)s.log_size, (unsigned long)s.log_capacity,
                        (unsigned long)s.log_overwritten, (unsigned long)s.last_seq,
                        (unsigned long)x.uptime_s, (unsigned)x.profile_id,
                        x.profile_name ? x.profile_name : "", (unsigned)n_marks);
  if (k < 0) return k;
  len += k;
  len = appendDistances(c.mark_distance_m, n_marks, buf, n, len);
  if (len < 0 || (size_t)len >= n) return len;
  k = std::snprintf(buf + len, n - len,
                    ",\"belt_length_m\":%.3f,\"debounce_us\":%lu,\"stop_timeout_us\":%lu}}",
                    (double)beltLength(c), (unsigned long)c.debounce_us,
                    (unsigned long)c.stop_timeout_us);
  if (k < 0) return k;
  return len + k;
}

int formatProfileJson(const Profile& p, uint8_t id, char* buf, size_t n) {
  const uint8_t marks = p.marks > MAX_MARKS ? MAX_MARKS : p.marks;
  int len = std::snprintf(buf, n, "{\"id\":%u,\"name\":\"%s\",\"type\":\"%s\",\"marks\":%u,\"mark_distance_m\":",
                          (unsigned)id, p.name, typeName(p.type), (unsigned)marks);
  len = appendDistances(p.mark_distance_m, marks, buf, n, len);
  if (len < 0 || (size_t)len >= n) return len;
  int k = std::snprintf(buf + len, n - len, ",\"belt_length_m\":%.3f}", (double)beltLength(p));
  if (k < 0) return k;
  return len + k;
}

}  // namespace speed_core
