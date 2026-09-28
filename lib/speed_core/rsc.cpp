#include "rsc.h"

#include <cmath>

namespace speed_core {

RscMeasurement makeRscMeasurement(float speed_mps, uint8_t cadence_spm, float running_min_mps) {
  RscMeasurement m{};
  if (!(speed_mps > 0.0f)) speed_mps = 0.0f;  // also catches NaN
  const long q = std::lround(static_cast<double>(speed_mps) * 256.0);
  m.speed_q8 = q > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(q);
  m.flags = speed_mps >= running_min_mps ? RSC_FLAG_RUNNING : 0;
  m.cadence_spm = cadence_spm;
  return m;
}

size_t encodeRscMeasurement(const RscMeasurement& m, uint8_t out[RSC_MEASUREMENT_LEN]) {
  out[0] = m.flags;
  out[1] = static_cast<uint8_t>(m.speed_q8 & 0xFF);
  out[2] = static_cast<uint8_t>(m.speed_q8 >> 8);
  out[3] = m.cadence_spm;
  return RSC_MEASUREMENT_LEN;
}

}  // namespace speed_core
