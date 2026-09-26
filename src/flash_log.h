// Session log on LittleFS: every accepted pulse is appended as a CSV line to
// /log/NNNNNN.csv, one file per session (boot or /api/reset). Writes happen
// from loop() in batches; the pulse path never touches flash.
#pragma once
#include <FS.h>

#include <cstddef>
#include <cstdint>

#include "profile.h"
#include "pulse_log.h"
#include "session.h"
#include "speed_types.h"

namespace flash_log {

// Mount (formatting on first use), allocate the session id, evict to budget.
void begin(const char* firmware_hash);
// Queue the newest log record if it has not been seen yet. O(1); call after onPulse.
void observe(const speed_core::PulseLog& log);
// Flush the batch to flash when it is due. Call from loop().
void service(uint32_t now_us);
// Flush unconditionally (before a download of the active session).
void flush();
// End the current session and start a new one (from /api/reset).
void rotate();
// Record a profile switch as a "# profile ..." line. No-op before the file exists.
void noteProfile(const speed_core::Profile& p, uint8_t id);
// Session header for the current session (also used by /api/pulses.csv).
int header(char* buf, size_t n);

speed_core::FlashStatus status();
uint32_t sessionId();
// Sessions on flash, ascending by id. Returns the number written to `out`.
size_t list(speed_core::SessionInfo* out, size_t max);
bool exists(uint32_t id);
uint32_t sessionBytes(uint32_t id);
// False for the active session or a missing file.
bool remove(uint32_t id);
fs::File openForRead(uint32_t id);

}  // namespace flash_log
