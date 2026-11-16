// Patina — params.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "dsp/controls.hpp"

namespace patina {

enum ParamId : uint32_t {
  P_MODEL = 0,
  P_ENABLED,
  P_DRIVE,
  P_CHARACTER,
  P_TONE,
  P_MIX,
  P_OUTPUT,
  P_AUTO_GAIN,
  P_METER,
  P_OVERSAMPLE,
  P_COUNT
};

struct Labels {
  const char* const* names = nullptr;
  int count = 0;
};

template <size_t N>
constexpr Labels labels(const char* const (&arr)[N]) noexcept {
  return Labels{arr, static_cast<int>(N)};
}

struct ParamSpec {
  const char* symbol;
  const char* name;
  const char* module;
  double min;
  double max;
  double def;
  bool stepped;
  bool hidden;
  bool toggled;
  const char* unit;
  Labels labels;
};

inline constexpr const char* kModelLabels[] = {"Valve", "Console", "Bus", "Tape",
                                               "Transformer"};
inline constexpr const char* kMeterLabels[] = {"Shape", "Scope", "Off"};
inline constexpr const char* kOversampleLabels[] = {"Off", "2x", "4x"};

inline constexpr ParamSpec kParamSpecs[P_COUNT] = {
    {"model", "Model", "Saturation", 0.0, 4.0, 0.0, true, false, false, nullptr,
     labels(kModelLabels)},
    {"power", "Power", "Saturation", 0.0, 1.0, 1.0, true, false, true, nullptr,
     {}},
    {"drive", "Drive", "Saturation", 0.0, 1.0, 0.45, false, false, false,
     nullptr, {}},
    {"character", "Character", "Saturation", 0.0, 1.0, 0.5, false, false, false,
     nullptr, {}},
    {"tone", "Tone", "Saturation", 0.0, 1.0, 0.5, false, false, false, nullptr,
     {}},
    {"mix", "Mix", "Output", 0.0, 1.0, 1.0, false, false, false, nullptr, {}},
    {"output", "Output", "Output", 0.0, 1.0, 0.5, false, false, false, nullptr,
     {}},
    {"auto_gain", "Auto Gain", "Output", 0.0, 1.0, 0.0, true, false, true,
     nullptr, {}},
    {"meter", "Display", "Display", 0.0, 2.0, 0.0, true, true, false, nullptr,
     labels(kMeterLabels)},
    {"oversample", "Oversampling", "Oversampling", 0.0, 2.0, 0.0, true, false,
     false, nullptr, labels(kOversampleLabels)},
};

static_assert(sizeof(kParamSpecs) / sizeof(kParamSpecs[0]) == P_COUNT,
              "kParamSpecs must cover every parameter id");

class Shared {
public:
  Shared() {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      values_[p].store(kParamSpecs[p].def, std::memory_order_relaxed);
      dirty_[p].store(false, std::memory_order_relaxed);
    }
    inPeakL.store(-120.0f);
    inPeakR.store(-120.0f);
    outPeakL.store(-120.0f);
    outPeakR.store(-120.0f);
    outVu.store(-120.0f);
  }

  double get(uint32_t p) const noexcept {
    return values_[p].load(std::memory_order_relaxed);
  }

  void set(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) {
      v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    }
    values_[p].store(v, std::memory_order_relaxed);
    dirty_[p].store(true, std::memory_order_release);
  }

  void setFromHost(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) {
      v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    }
    values_[p].store(v, std::memory_order_relaxed);
  }

  bool takeDirty(uint32_t p) noexcept {
    return dirty_[p].exchange(false, std::memory_order_acquire);
  }

  bool isDirty(uint32_t p) const noexcept {
    return dirty_[p].load(std::memory_order_acquire);
  }

  bool anyDirty() const noexcept {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      if (dirty_[p].load(std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }

  std::atomic<float> inPeakL;
  std::atomic<float> inPeakR;
  std::atomic<float> outPeakL;
  std::atomic<float> outPeakR;
  std::atomic<float> outVu;

private:
  std::atomic<double> values_[P_COUNT];
  std::atomic<bool> dirty_[P_COUNT];
};

inline Controls controlsFrom(const Shared& s) {
  Controls c;
  c.model = static_cast<int>(s.get(P_MODEL) + 0.5);
  c.enabled = s.get(P_ENABLED) > 0.5;
  c.drive = static_cast<float>(s.get(P_DRIVE));
  c.character = static_cast<float>(s.get(P_CHARACTER));
  c.tone = static_cast<float>(s.get(P_TONE));
  c.mix = static_cast<float>(s.get(P_MIX));
  c.output = static_cast<float>(s.get(P_OUTPUT));
  c.autoGain = s.get(P_AUTO_GAIN) > 0.5;
  c.oversample = static_cast<float>(s.get(P_OVERSAMPLE));
  return c;
}

} // namespace patina
