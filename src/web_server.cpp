// src/web_server.cpp
#include "web_server.h"

#include <Arduino.h>
#include <WebServer.h>

#include <cstring>

#include "adc_probe.h"
#include "format.h"
#include "pulse_isr.h"
#include "web_page.h"

using speed_core::EngineStatus;
using speed_core::PulseRecord;
using speed_core::SpeedEngine;

namespace web_server {
namespace {

WebServer server(80);
SpeedEngine* engine_ = nullptr;

constexpr size_t kMaxPulsesPerFetch = 500;

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStatus() {
  char buf[768];
  EngineStatus s = engine_->status(micros());
  s.rejected_debounce += pulse_isr::rejected();
  speed_core::formatStatusJson(s, engine_->config(), pulse_isr::dropped(), millis() / 1000, buf,
                               sizeof buf);
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

adc_probe::Capture capture_buf;  // 800 bytes, static to keep it off the stack

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

}  // namespace

void begin(SpeedEngine& engine) {
  engine_ = &engine;
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/pulses.json", HTTP_GET, handlePulsesJson);
  server.on("/api/pulses.csv", HTTP_GET, handlePulsesCsv);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/signal", HTTP_GET, handleSignal);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("http: listening on port 80");
}

void handle() { server.handleClient(); }

}  // namespace web_server
