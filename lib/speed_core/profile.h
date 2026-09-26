// A treadmill profile: what the sensor needs to know about one treadmill.
// Plain fixed-size data so it can be stored as one blob.
#pragma once
#include <cstddef>
#include <cstdint>

#include "speed_types.h"

namespace speed_core {

constexpr uint8_t PROFILE_VERSION = 3;
constexpr size_t PROFILE_V2_BLOB_SIZE = 212;  // stored layout before the slat fields
constexpr uint8_t MAX_PROFILES = 20;
constexpr uint8_t PROFILE_NAME_LEN = 48;   // including the terminating NUL
constexpr uint8_t PROFILE_NOTES_LEN = 128; // including the terminating NUL
constexpr float MIN_MARK_DISTANCE_M = 0.05f;
constexpr float MAX_MARK_DISTANCE_M = 10.0f;
constexpr uint16_t MIN_SLAT_COUNT = 2;
constexpr uint16_t MAX_SLAT_COUNT = 1000;
constexpr float MIN_SLAT_PITCH_M = 0.005f;
constexpr float MAX_SLAT_PITCH_M = 0.5f;
constexpr float SLAT_SUM_TOLERANCE = 0.005f;  // relative: sum of distances vs count x pitch

enum class TreadmillType : uint8_t { Belt = 0, Slats = 1 };

// For slats, slat_count x slat_pitch_m is the physical belt length (the chain's
// neutral line), and mark_distance_m must sum to it. mark_distance_m stays the
// engine's only source of truth so calibration keeps working for both types.
struct Profile {
  uint8_t version;                     // PROFILE_VERSION
  char name[PROFILE_NAME_LEN];         // UTF-8, NUL-terminated
  TreadmillType type;
  uint8_t marks;                       // 1..MAX_MARKS
  float mark_distance_m[MAX_MARKS];    // [k] = distance from mark k to mark k+1 (mod marks)
  char notes[PROFILE_NOTES_LEN];       // UTF-8, NUL-terminated; may contain '\n'
  uint16_t slat_count;                 // slats only; 0 for belts
  float slat_pitch_m;                  // slats only; 0 for belts
};

// Sum of mark_distance_m[0..marks).
float beltLength(const Profile& p);
// slat_count x slat_pitch_m (meaningful for slats only).
float slatBeltLength(const Profile& p);
// belt_length_m / marks into mark_distance_m[0..marks); unused slots are zeroed.
void seedEqualDistances(Profile& p, float belt_length_m);
// Decode a stored blob: the current layout as-is, or a v2 blob (212 bytes,
// version byte 2) upgraded with zeroed slat fields. False for anything else.
bool profileFromBlob(const void* data, size_t len, Profile& out);
// nullptr when valid, otherwise a static English reason.
const char* validate(const Profile& p);
EngineConfig makeEngineConfig(const Profile& p, uint32_t debounce_us, uint32_t stop_timeout_us,
                              float max_speed_mps = 0.0f);
// stopTimeoutUs() applied to the profile's longest mark distance.
uint32_t stopTimeoutFor(const Profile& p, float min_speed_mps, uint32_t min_us, uint32_t max_us);
// Equal spacing profile; name is truncated to fit.
Profile makeDefaultProfile(const char* name, float belt_length_m, uint8_t marks);

const char* typeName(TreadmillType t);
bool parseTypeName(const char* s, TreadmillType& out);

// 1..max_len_incl_nul-1 bytes, each >= 0x20, not '"' and not '\\'. UTF-8 bytes >= 0x80 pass.
bool validName(const char* s, size_t max_len_incl_nul);

// Like validName but newline (0x0A) is allowed. Empty is allowed (returns true for "").
bool validNotes(const char* s, size_t max_len_incl_nul);

// Parses a 1-2 digit slot id ("0".."19", no leading zeros except "0"); false otherwise or if >= MAX_PROFILES.
bool parseSlot(const char* s, uint8_t& out);

}  // namespace speed_core
