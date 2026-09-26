#include "ble_rsc.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <atomic>

#include "config.h"
#include "rsc.h"

namespace ble_rsc {

namespace {

constexpr uint16_t kAppearanceInShoeSensor = 0x0441;
constexpr uint16_t kAdvMinInterval = 160;  // 0.625 ms units: 100 ms
constexpr uint16_t kAdvMaxInterval = 400;  // 250 ms

NimBLEServer* server = nullptr;
NimBLECharacteristic* measurement = nullptr;

// Written from the NimBLE host task, read from loop().
std::atomic<uint32_t> connects{0};
std::atomic<int> last_disconnect_reason{0};

// loop()-only state.
uint32_t notifications = 0;
float last_speed_mps = 0.0f;
uint32_t last_notify_ms = 0;
bool was_connected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { connects.fetch_add(1); }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    last_disconnect_reason.store(reason);
  }
};

ServerCallbacks callbacks;

bool connected() { return server != nullptr && server->getConnectedCount() > 0; }

}  // namespace

void begin(const char* name, const char* fw_hash) {
  const uint32_t heap_before = ESP.getFreeHeap();
  NimBLEDevice::init(name);
  server = NimBLEDevice::createServer();
  server->setCallbacks(&callbacks);
  server->advertiseOnDisconnect(true);

  NimBLEService* rsc = server->createService(NimBLEUUID((uint16_t)0x1814));
  measurement = rsc->createCharacteristic(NimBLEUUID((uint16_t)0x2A53), NIMBLE_PROPERTY::NOTIFY);
  const uint8_t feature[2] = {(uint8_t)(speed_core::RSC_FEATURE_WALK_RUN_STATUS & 0xFF),
                              (uint8_t)(speed_core::RSC_FEATURE_WALK_RUN_STATUS >> 8)};
  rsc->createCharacteristic(NimBLEUUID((uint16_t)0x2A54), NIMBLE_PROPERTY::READ)->setValue(feature, sizeof feature);
  const uint8_t location = speed_core::RSC_LOCATION_OTHER;
  rsc->createCharacteristic(NimBLEUUID((uint16_t)0x2A5D), NIMBLE_PROPERTY::READ)->setValue(&location, 1);

  NimBLEService* dis = server->createService(NimBLEUUID((uint16_t)0x180A));
  dis->createCharacteristic(NimBLEUUID((uint16_t)0x2A29), NIMBLE_PROPERTY::READ)->setValue("millarr");
  dis->createCharacteristic(NimBLEUUID((uint16_t)0x2A24), NIMBLE_PROPERTY::READ)->setValue("treadmill belt sensor");
  dis->createCharacteristic(NimBLEUUID((uint16_t)0x2A26), NIMBLE_PROPERTY::READ)->setValue(fw_hash);

  server->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(name);
  adv->setAppearance(kAppearanceInShoeSensor);
  adv->addServiceUUID(NimBLEUUID((uint16_t)0x1814));
  adv->enableScanResponse(true);
  adv->setMinInterval(kAdvMinInterval);
  adv->setMaxInterval(kAdvMaxInterval);
  const bool ok = adv->start();
  Serial.printf("ble: %s as '%s' fw=%s heap %lu -> %lu\n", ok ? "advertising" : "advertising FAILED", name,
                fw_hash, (unsigned long)heap_before, (unsigned long)ESP.getFreeHeap());
}

void service(uint32_t now_ms, float speed_mps) {
  const bool now_connected = connected();
  if (now_connected != was_connected) {
    if (now_connected) {
      Serial.printf("ble: connected (%lu since boot)\n", (unsigned long)connects.load());
    } else {
      Serial.printf("ble: disconnected reason=%d\n", last_disconnect_reason.load());
    }
    was_connected = now_connected;
    last_notify_ms = now_ms - RSC_NOTIFY_INTERVAL_MS;  // first packet right away
  }
  if (!now_connected || now_ms - last_notify_ms < RSC_NOTIFY_INTERVAL_MS) return;
  last_notify_ms = now_ms;

  uint8_t packet[speed_core::RSC_MEASUREMENT_LEN];
  const size_t n = speed_core::encodeRscMeasurement(
      speed_core::makeRscMeasurement(speed_mps, RSC_CADENCE_SPM, RSC_RUNNING_MIN_MPS), packet);
  if (measurement->notify(packet, n)) {
    ++notifications;
    last_speed_mps = speed_mps;
  }
}

speed_core::BleStatus status() {
  speed_core::BleStatus s{};
  s.advertising = NimBLEDevice::getAdvertising()->isAdvertising();
  s.connected = connected();
  s.connects = connects.load();
  s.notifications = notifications;
  s.last_speed_mps = last_speed_mps;
  return s;
}

}  // namespace ble_rsc
