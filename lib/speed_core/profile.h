// A treadmill profile: what the sensor needs to know about one treadmill.
// Plain fixed-size data so it can be stored as one blob.
#pragma once
#include <cstddef>
#include <cstdint>

#include "speed_types.h"

namespace speed_core {

constexpr uint8_t PROFILE_VERSION = 1;
constexpr uint8_t MAX_PROFILES = 8;
constexpr uint8_t PROFILE_NAME_LEN = 24;  // including the terminating NUL
constexpr float MIN_MARK_DISTANCE_M = 0.05f;
constexpr float MAX_MARK_DISTANCE_M = 10.0f;

enum class TreadmillType : uint8_t { Belt = 0, Slats = 1 };

struct Profile {
  uint8_t version;                     // PROFILE_VERSION
  char name[PROFILE_NAME_LEN];         // UTF-8, NUL-terminated
  TreadmillType type;
  uint8_t marks;                       // 1..MAX_MARKS
  float mark_distance_m[MAX_MARKS];    // [k] = distance from mark k to mark k+1 (mod marks)
};

// Sum of mark_distance_m[0..marks).
float beltLength(const Profile& p);
// nullptr when valid, otherwise a static English reason.
const char* validate(const Profile& p);
EngineConfig makeEngineConfig(const Profile& p, uint32_t debounce_us, uint32_t stop_timeout_us);
// Equal spacing profile; name is truncated to fit.
Profile makeDefaultProfile(const char* name, float belt_length_m, uint8_t marks);

const char* typeName(TreadmillType t);
bool parseTypeName(const char* s, TreadmillType& out);

// 1..max_len_incl_nul-1 bytes, each >= 0x20, not '"' and not '\\'. UTF-8 bytes >= 0x80 pass.
bool validName(const char* s, size_t max_len_incl_nul);

}  // namespace speed_core
