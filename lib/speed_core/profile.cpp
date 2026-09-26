#include "profile.h"

#include <cstring>

namespace speed_core {

float beltLength(const Profile& p) {
  const uint8_t n = p.marks > MAX_MARKS ? MAX_MARKS : p.marks;
  float sum = 0.0f;
  for (uint8_t k = 0; k < n; ++k) sum += p.mark_distance_m[k];
  return sum;
}

float slatBeltLength(const Profile& p) { return p.slat_count * p.slat_pitch_m; }

void seedEqualDistances(Profile& p, float belt_length_m) {
  const uint8_t n = p.marks < 1 ? 1 : (p.marks > MAX_MARKS ? MAX_MARKS : p.marks);
  for (uint8_t k = 0; k < MAX_MARKS; ++k) p.mark_distance_m[k] = k < n ? belt_length_m / n : 0.0f;
}

bool profileFromBlob(const void* data, size_t len, Profile& out) {
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  if (len == sizeof(Profile)) {
    std::memcpy(&out, data, sizeof out);
    return true;
  }
  if (len == PROFILE_V2_BLOB_SIZE && bytes[0] == 2) {
    Profile p{};
    std::memcpy(&p, data, PROFILE_V2_BLOB_SIZE);  // v3 is v2 plus trailing fields
    p.version = PROFILE_VERSION;
    p.slat_count = 0;
    p.slat_pitch_m = 0.0f;
    out = p;
    return true;
  }
  return false;
}

bool validName(const char* s, size_t max_len_incl_nul) {
  size_t len = 0;
  while (len < max_len_incl_nul && s[len] != '\0') ++len;
  if (len == 0 || len >= max_len_incl_nul) return false;
  for (size_t i = 0; i < len; ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x20 || c == '"' || c == '\\') return false;
  }
  return true;
}

bool validNotes(const char* s, size_t max_len_incl_nul) {
  size_t len = 0;
  while (len < max_len_incl_nul && s[len] != '\0') ++len;
  if (len >= max_len_incl_nul) return false;
  if (len == 0) return true;
  for (size_t i = 0; i < len; ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == '\n') continue;
    if (c < 0x20 || c == '"' || c == '\\') return false;
  }
  return true;
}

bool parseSlot(const char* s, uint8_t& out) {
  size_t len = 0;
  while (s[len] != '\0') ++len;
  if (len == 0 || len > 2) return false;
  if (len == 2 && s[0] == '0') return false;  // no leading zeros except "0" itself
  for (size_t i = 0; i < len; ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
  }
  unsigned v = 0;
  for (size_t i = 0; i < len; ++i) v = v * 10 + (s[i] - '0');
  if (v >= MAX_PROFILES) return false;
  out = static_cast<uint8_t>(v);
  return true;
}

const char* validate(const Profile& p) {
  if (p.version != PROFILE_VERSION) return "bad version";
  if (p.marks < 1 || p.marks > MAX_MARKS) return "marks must be 1..8";
  size_t len = 0;
  while (len < PROFILE_NAME_LEN && p.name[len] != '\0') ++len;
  if (len == 0) return "name empty";
  if (len >= PROFILE_NAME_LEN) return "name too long";
  if (!validName(p.name, PROFILE_NAME_LEN)) return "name has invalid characters";
  size_t notes_len = 0;
  while (notes_len < PROFILE_NOTES_LEN && p.notes[notes_len] != '\0') ++notes_len;
  if (notes_len >= PROFILE_NOTES_LEN) return "notes too long";
  if (notes_len > 0 && !validNotes(p.notes, PROFILE_NOTES_LEN)) return "notes has invalid characters";
  if (p.type != TreadmillType::Belt && p.type != TreadmillType::Slats) return "bad type";
  const bool slats = p.type == TreadmillType::Slats;
  if (slats) {
    if (p.slat_count < MIN_SLAT_COUNT || p.slat_count > MAX_SLAT_COUNT) return "slat count must be 2..1000";
    if (!(p.slat_pitch_m >= MIN_SLAT_PITCH_M && p.slat_pitch_m <= MAX_SLAT_PITCH_M)) return "slat pitch out of range";
  }
  for (uint8_t k = 0; k < p.marks; ++k) {
    const float d = p.mark_distance_m[k];
    if (!(d >= MIN_MARK_DISTANCE_M && d <= MAX_MARK_DISTANCE_M)) return "distance out of range";  // NaN fails too
  }
  if (slats) {
    const float expected = slatBeltLength(p);
    const float diff = beltLength(p) - expected;
    if (!((diff < 0 ? -diff : diff) <= expected * SLAT_SUM_TOLERANCE)) return "distances do not match slats";
  }
  return nullptr;
}

EngineConfig makeEngineConfig(const Profile& p, uint32_t debounce_us, uint32_t stop_timeout_us,
                              float max_speed_mps) {
  EngineConfig c{};
  c.marks_count = p.marks < 1 ? 1 : (p.marks > MAX_MARKS ? MAX_MARKS : p.marks);
  for (uint8_t k = 0; k < c.marks_count; ++k) c.mark_distance_m[k] = p.mark_distance_m[k];
  c.debounce_us = debounce_us;
  c.stop_timeout_us = stop_timeout_us;
  c.max_speed_mps = max_speed_mps;
  return c;
}

uint32_t stopTimeoutFor(const Profile& p, float min_speed_mps, uint32_t min_us, uint32_t max_us) {
  const uint8_t n = p.marks < 1 ? 1 : (p.marks > MAX_MARKS ? MAX_MARKS : p.marks);
  float longest = 0.0f;
  for (uint8_t k = 0; k < n; ++k) {
    if (p.mark_distance_m[k] > longest) longest = p.mark_distance_m[k];
  }
  return stopTimeoutUs(longest, min_speed_mps, min_us, max_us);
}

Profile makeDefaultProfile(const char* name, float belt_length_m, uint8_t marks) {
  Profile p{};
  p.version = PROFILE_VERSION;
  std::strncpy(p.name, name, PROFILE_NAME_LEN - 1);
  p.name[PROFILE_NAME_LEN - 1] = '\0';
  p.type = TreadmillType::Belt;
  p.marks = marks < 1 ? 1 : (marks > MAX_MARKS ? MAX_MARKS : marks);
  for (uint8_t k = 0; k < p.marks; ++k) p.mark_distance_m[k] = belt_length_m / p.marks;
  return p;
}

const char* typeName(TreadmillType t) { return t == TreadmillType::Slats ? "slats" : "belt"; }

bool parseTypeName(const char* s, TreadmillType& out) {
  if (std::strcmp(s, "belt") == 0) { out = TreadmillType::Belt; return true; }
  if (std::strcmp(s, "slats") == 0) { out = TreadmillType::Slats; return true; }
  return false;
}

}  // namespace speed_core
