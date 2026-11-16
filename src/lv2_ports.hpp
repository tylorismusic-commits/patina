// Patina — LV2 port layout
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "params.hpp"

namespace patina {

// Audio ports first, then parameters, then read-only meter outputs.
constexpr uint32_t kLv2InL = 0;
constexpr uint32_t kLv2InR = 1;
constexpr uint32_t kLv2OutL = 2;
constexpr uint32_t kLv2OutR = 3;

constexpr uint32_t kLv2FirstParam = 4;
constexpr uint32_t kLv2FirstBandLevel =
    kLv2FirstParam + static_cast<uint32_t>(P_COUNT);
constexpr uint32_t kLv2BandLevelCount = static_cast<uint32_t>(kBandCount);

constexpr uint32_t kLv2FirstMeter = kLv2FirstBandLevel + kLv2BandLevelCount;
enum Lv2Meter : uint32_t {
  kLv2InPeakL = 0,
  kLv2InPeakR,
  kLv2OutPeakL,
  kLv2OutPeakR,
  kLv2OutVu,
  kLv2MeterCount
};

constexpr uint32_t kLv2FirstSpectrum =
    kLv2FirstMeter + static_cast<uint32_t>(kLv2MeterCount);
constexpr uint32_t kLv2SpectrumCount = static_cast<uint32_t>(kSpectrumBins);
constexpr uint32_t kLv2Latency = kLv2FirstSpectrum + kLv2SpectrumCount;
constexpr uint32_t kLv2PortCount = kLv2Latency + 1;

static_assert(kLv2FirstBandLevel == 4 + 48, "port layout drifted");
static_assert(kLv2Latency + 1 == 190, "port layout drifted");

} // namespace patina
