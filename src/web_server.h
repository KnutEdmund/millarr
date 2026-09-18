// src/web_server.h
#pragma once
#include "speed_engine.h"

namespace web_server {
void begin(speed_core::SpeedEngine& engine);
void handle();  // call from loop()
}  // namespace web_server
