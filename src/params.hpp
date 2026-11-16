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
  P_MB,
  P_XOVER1,
  P_XOVER2,
  P_BAND,
  P_B0_MODEL,
  P_B0_INPUT,
  P_B0_DRIVE,
  P_B0_CHARACTER,
  P_B0_TONE,
  P_B0_MIX,
  P_B0_OUTPUT,
  P_B0_ON,
  P_B1_MODEL,
  P_B1_INPUT,
  P_B1_DRIVE,
  P_B1_CHARACTER,
  P_B1_TONE,
  P_B1_MIX,
  P_B1_OUTPUT,
  P_B1_ON,
  P_B2_MODEL,
  P_B2_INPUT,
  P_B2_DRIVE,
  P_B2_CHARACTER,
  P_B2_TONE,
  P_B2_MIX,
  P_B2_OUTPUT,
  P_B2_ON,
  P_COUNT
};

enum BandField : uint32_t {
  BF_MODEL = 0,
  BF_INPUT,
  BF_DRIVE,
  BF_CHARACTER,
  BF_TONE,
  BF_MIX,
  BF_OUTPUT,
  BF_ON,
  BF_COUNT
};

constexpr uint32_t bandParam(int band, BandField field) noexcept {
  return P_B0_MODEL + static_cast<uint32_t>(band) * BF_COUNT +
         static_cast<uint32_t>(field);
}

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
inline constexpr const char* kBandLabels[] = {"Low", "Mid", "High"};

#define PATINA_BAND_SPECS(prefix, module)                                      \
  {prefix "_model", "Model", module, 0.0, 4.0, 0.0, true, false, false,       \
   nullptr, labels(kModelLabels)},                                            \
      {prefix "_input", "Input", module, 0.0, 1.0, 0.5, false, false, false,  \
       "dB", {}},                                                             \
      {prefix "_drive", "Drive", module, 0.0, 1.0, 0.45, false, false, false, \
       nullptr, {}},                                                          \
      {prefix "_character", "Character", module, 0.0, 1.0, 0.5, false, false, \
       false, nullptr, {}},                                                   \
      {prefix "_tone", "Tone", module, 0.0, 1.0, 0.5, false, false, false,    \
       nullptr, {}},                                                          \
      {prefix "_mix", "Mix", module, 0.0, 1.0, 1.0, false, false, false,      \
       nullptr, {}},                                                          \
      {prefix "_output", "Output", module, 0.0, 1.0, 0.5, false, false, false, \
       "dB", {}},                                                             \
      {prefix "_on", "On", module, 0.0, 1.0, 1.0, true, false, true, nullptr, \
       {}}

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
    {"multiband", "Multiband", "Mode", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}},
    {"xover_low", "Low Xover", "Multiband", 40.0, 800.0, 120.0, false, false,
     false, "Hz", {}},
    {"xover_high", "High Xover", "Multiband", 800.0, 12000.0, 2500.0, false,
     false, false, "Hz", {}},
    {"band", "Band", "Multiband", 0.0, 2.0, 1.0, true, true, false, nullptr,
     labels(kBandLabels)},
    PATINA_BAND_SPECS("band0", "Low Band"),
    PATINA_BAND_SPECS("band1", "Mid Band"),
    PATINA_BAND_SPECS("band2", "High Band"),
};

#undef PATINA_BAND_SPECS

static_assert(sizeof(kParamSpecs) / sizeof(kParamSpecs[0]) == P_COUNT,
              "kParamSpecs must cover every parameter id");
static_assert(P_COUNT == 38, "parameter table and enum must stay in sync");

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
    for (int b = 0; b < 3; ++b) {
      bandLevel[b].store(-120.0f);
    }
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
  std::atomic<float> bandLevel[3];
  // UI-only solo state; not part of the parameter set or saved state.
  std::atomic<int> soloBand{-1};
  // Set once the single-band settings have been copied into the bands.
  std::atomic<bool> mbSeeded{false};

private:
  std::atomic<double> values_[P_COUNT];
  std::atomic<bool> dirty_[P_COUNT];
};

inline BandControls bandFrom(const Shared& s, int band) noexcept {
  BandControls b;
  b.model = static_cast<int>(s.get(bandParam(band, BF_MODEL)) + 0.5);
  b.enabled = s.get(bandParam(band, BF_ON)) > 0.5;
  b.input = static_cast<float>(s.get(bandParam(band, BF_INPUT)));
  b.drive = static_cast<float>(s.get(bandParam(band, BF_DRIVE)));
  b.character = static_cast<float>(s.get(bandParam(band, BF_CHARACTER)));
  b.tone = static_cast<float>(s.get(bandParam(band, BF_TONE)));
  b.mix = static_cast<float>(s.get(bandParam(band, BF_MIX)));
  b.output = static_cast<float>(s.get(bandParam(band, BF_OUTPUT)));
  return b;
}

inline Controls controlsFrom(const Shared& s) noexcept {
  Controls c;
  c.enabled = s.get(P_ENABLED) > 0.5;
  c.multiband = s.get(P_MB) > 0.5;
  c.selectedBand =
      std::min(std::max(static_cast<int>(s.get(P_BAND) + 0.5), 0), 2);
  c.soloBand = s.soloBand.load(std::memory_order_relaxed);
  c.xover1 = static_cast<float>(s.get(P_XOVER1));
  c.xover2 = static_cast<float>(s.get(P_XOVER2));
  c.output = static_cast<float>(s.get(P_OUTPUT));
  c.autoGain = s.get(P_AUTO_GAIN) > 0.5;
  c.oversample = static_cast<float>(s.get(P_OVERSAMPLE));

  if (c.multiband) {
    for (int b = 0; b < 3; ++b) {
      c.bands[b] = bandFrom(s, b);
    }
  } else {
    BandControls& b = c.bands[0];
    b.model = static_cast<int>(s.get(P_MODEL) + 0.5);
    b.enabled = true;
    b.input = 0.5f;
    b.drive = static_cast<float>(s.get(P_DRIVE));
    b.character = static_cast<float>(s.get(P_CHARACTER));
    b.tone = static_cast<float>(s.get(P_TONE));
    b.mix = static_cast<float>(s.get(P_MIX));
    b.output = 0.5f;
  }
  return c;
}

// First switch to multiband: seed all three bands from the current
// single-band sound, so the mode change is not a jump to defaults.
inline void enterMultiband(Shared& s) {
  if (!s.mbSeeded.exchange(true, std::memory_order_relaxed)) {
    const double model = s.get(P_MODEL);
    const double drive = s.get(P_DRIVE);
    const double character = s.get(P_CHARACTER);
    const double tone = s.get(P_TONE);
    const double mix = s.get(P_MIX);
    for (int b = 0; b < 3; ++b) {
      s.set(bandParam(b, BF_MODEL), model);
      s.set(bandParam(b, BF_INPUT), 0.5);
      s.set(bandParam(b, BF_DRIVE), drive);
      s.set(bandParam(b, BF_CHARACTER), character);
      s.set(bandParam(b, BF_TONE), tone);
      s.set(bandParam(b, BF_MIX), mix);
      s.set(bandParam(b, BF_OUTPUT), 0.5);
      s.set(bandParam(b, BF_ON), 1.0);
    }
  }
  s.set(P_BAND, 1.0);
  s.set(P_MB, 1.0);
  s.soloBand.store(-1, std::memory_order_relaxed);
}

// Leaving multiband: the selected band becomes the single-band sound, so
// toggling back and forth is an A/B.
inline void leaveMultiband(Shared& s) {
  const int b =
      std::min(std::max(static_cast<int>(s.get(P_BAND) + 0.5), 0), 2);
  s.set(P_MODEL, s.get(bandParam(b, BF_MODEL)));
  s.set(P_DRIVE, s.get(bandParam(b, BF_DRIVE)));
  s.set(P_CHARACTER, s.get(bandParam(b, BF_CHARACTER)));
  s.set(P_TONE, s.get(bandParam(b, BF_TONE)));
  s.set(P_MIX, s.get(bandParam(b, BF_MIX)));
  s.set(P_MB, 0.0);
  s.soloBand.store(-1, std::memory_order_relaxed);
}

} // namespace patina
