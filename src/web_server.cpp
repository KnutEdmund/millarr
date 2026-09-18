// src/web_server.cpp
#include "web_server.h"

#include <Arduino.h>
#include <WebServer.h>
#include <uri/UriBraces.h>

#include <cstring>

#include "active_profile.h"
#include "adc_probe.h"
#include "format.h"
#include "profile.h"
#include "profile_store.h"
#include "pulse_isr.h"
#include "web_page.h"

using speed_core::EngineStatus;
using speed_core::Profile;
using speed_core::PulseRecord;
using speed_core::SpeedEngine;

namespace web_server {
namespace {

WebServer server(80);
SpeedEngine* engine_ = nullptr;

constexpr size_t kMaxPulsesPerFetch = 500;

void sendJson(int code, const char* body) { server.send(code, "application/json", body); }

void sendError(int code, const char* msg) {
  char b[160];
  snprintf(b, sizeof b, "{\"error\":\"%s\"}", msg);
  sendJson(code, b);
}

// Path argument 0 must be a single digit 0..7.
bool parseSlot(uint8_t& slot) {
  const String s = server.pathArg(0);
  if (s.length() != 1 || s[0] < '0' || s[0] > '7') return false;
  slot = static_cast<uint8_t>(s[0] - '0');
  return true;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStatus() {
  char buf[1024];
  EngineStatus s = engine_->status(micros());
  s.rejected_debounce += pulse_isr::rejected();
  speed_core::StatusExtras x{pulse_isr::dropped(), millis() / 1000, active_profile::slot(),
                             active_profile::current().name};
  int n = speed_core::formatStatusJson(s, engine_->config(), x, buf, sizeof buf);
  if (n < 0 || n >= (int)sizeof buf) {
    Serial.println("status json truncated");
    server.send(500, "text/plain", "status too large");
    return;
  }
  server.send(200, "application/json", buf);
}

void handlePulsesJson() {
  uint32_t since = server.hasArg("since") ? strtoul(server.arg("since").c_str(), nullptr, 10) : 0;
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  bool first = true;
  char buf[192];
  engine_->log().since(since, kMaxPulsesPerFetch, [&](const PulseRecord& r) {
    if (!first) server.sendContent(",");
    first = false;
    speed_core::formatRecordJson(r, buf, sizeof buf);
    server.sendContent(buf);
  });
  server.sendContent("]");
  server.sendContent("");  // terminates chunked transfer
}

void handlePulsesCsv() {
  server.sendHeader("Content-Disposition", "attachment; filename=\"pulses.csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent(speed_core::kCsvHeader);
  static char chunk[2048];
  size_t used = 0;
  char line[96];
  const auto& log = engine_->log();
  for (size_t i = 0; i < log.size(); ++i) {
    int n = speed_core::formatRecordCsv(log.at(i), line, sizeof line);
    if (n <= 0) continue;
    if ((size_t)n >= sizeof line) n = sizeof line - 1;
    if (used + n >= sizeof chunk) {
      server.sendContent(chunk, used);
      used = 0;
    }
    memcpy(chunk + used, line, n);
    used += n;
  }
  if (used) server.sendContent(chunk, used);
  server.sendContent("");
}

void handleReset() {
  engine_->reset();
  pulse_isr::resetCounters();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  server.send(404, "text/plain", "not found");
}

adc_probe::Capture capture_buf;  // 804 bytes, static to keep it off the stack

void handleSignal() {
  adc_probe::capture(capture_buf);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  char head[96];
  snprintf(head, sizeof head, "{\"sample_rate_hz\":%lu,\"min\":%u,\"max\":%u,\"margin\":%u,\"samples\":[",
           (unsigned long)adc_probe::kSampleRateHz, capture_buf.min, capture_buf.max,
           capture_buf.max - capture_buf.min);
  server.sendContent(head);
  static char chunk[1024];
  size_t used = 0;
  for (uint16_t i = 0; i < adc_probe::kSamples; ++i) {
    int n = snprintf(chunk + used, sizeof chunk - used, "%s%u", i ? "," : "", capture_buf.samples[i]);
    if (n < 0) break;
    if ((size_t)n >= sizeof chunk - used) n = sizeof chunk - used - 1;
    used += n;
    if (used > sizeof chunk - 8) {
      server.sendContent(chunk, used);
      used = 0;
    }
  }
  if (used) server.sendContent(chunk, used);
  server.sendContent("]}");
  server.sendContent("");
}

void handleProfilesList() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  char head[40];
  snprintf(head, sizeof head, "{\"active\":%u,\"profiles\":[", (unsigned)active_profile::slot());
  server.sendContent(head);
  bool first = true;
  char buf[256];
  Profile p;
  for (uint8_t s = 0; s < speed_core::MAX_PROFILES; ++s) {
    if (!profile_store::load(s, p)) continue;
    if (!first) server.sendContent(",");
    first = false;
    int n = speed_core::formatProfileJson(p, s, buf, sizeof buf);
    if (n > 0 && n < (int)sizeof buf) server.sendContent(buf);
  }
  server.sendContent("]}");
  server.sendContent("");
}

void handleProfileSave() {
  uint8_t slot;
  if (!parseSlot(slot)) { sendError(400, "bad profile id"); return; }
  Profile p{};
  p.version = speed_core::PROFILE_VERSION;
  String name = server.arg("name");
  name.trim();
  if (name.length() >= speed_core::PROFILE_NAME_LEN) { sendError(400, "name too long"); return; }
  strncpy(p.name, name.c_str(), speed_core::PROFILE_NAME_LEN - 1);
  if (!speed_core::parseTypeName(server.arg("type").c_str(), p.type)) { sendError(400, "bad type"); return; }
  long marks = server.arg("marks").toInt();
  if (marks < 1 || marks > speed_core::MAX_MARKS) { sendError(400, "marks must be 1..8"); return; }
  p.marks = static_cast<uint8_t>(marks);
  for (uint8_t k = 0; k < p.marks; ++k) {
    char key[6];
    snprintf(key, sizeof key, "d%u", (unsigned)k);
    if (!server.hasArg(key)) { sendError(400, "missing distance"); return; }
    p.mark_distance_m[k] = server.arg(key).toFloat();
  }
  if (const char* why = speed_core::validate(p)) { sendError(400, why); return; }
  if (!profile_store::save(slot, p)) { sendError(500, "save failed"); return; }
  if (slot == active_profile::slot()) active_profile::apply(*engine_);
  sendJson(200, "{\"ok\":true}");
}

void handleProfileActivate() {
  uint8_t slot;
  if (!parseSlot(slot)) { sendError(400, "bad profile id"); return; }
  if (!profile_store::setActiveSlot(slot)) { sendError(400, "empty profile"); return; }
  active_profile::apply(*engine_);
  sendJson(200, "{\"ok\":true}");
}

void handleProfileDelete() {
  uint8_t slot;
  if (!parseSlot(slot)) { sendError(400, "bad profile id"); return; }
  if (slot == active_profile::slot()) { sendError(400, "cannot delete active profile"); return; }
  Profile p;
  if (!profile_store::load(slot, p)) { sendError(400, "empty profile"); return; }
  if (profile_store::count() <= 1) { sendError(400, "at least one profile"); return; }
  if (!profile_store::erase(slot)) { sendError(500, "erase failed"); return; }
  sendJson(200, "{\"ok\":true}");
}

}  // namespace

void begin(SpeedEngine& engine) {
  engine_ = &engine;
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/pulses.json", HTTP_GET, handlePulsesJson);
  server.on("/api/pulses.csv", HTTP_GET, handlePulsesCsv);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/signal", HTTP_GET, handleSignal);
  server.on("/api/profiles", HTTP_GET, handleProfilesList);
  // Specific patterns first: UriBraces "{}" at the end of a pattern also matches '/'.
  server.on(UriBraces("/api/profiles/{}/activate"), HTTP_POST, handleProfileActivate);
  server.on(UriBraces("/api/profiles/{}/delete"), HTTP_POST, handleProfileDelete);
  server.on(UriBraces("/api/profiles/{}"), HTTP_POST, handleProfileSave);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("http: listening on port 80");
}

void handle() { server.handleClient(); }

}  // namespace web_server
