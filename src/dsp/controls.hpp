// Patina — controls.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace patina {

enum class Model : int {
  Valve = 0,
  Console = 1,
  Bus = 2,
  Tape = 3,
  Transformer = 4,
  Count = 5,
};

enum class Band : int {
  Low = 0,
  LowMid = 1,
  HighMid = 2,
  High = 3,
  Count = 4,
};

constexpr int kBandCount = static_cast<int>(Band::Count);
constexpr int kSpectrumBins = 128;

// Which per-band slot is used by each position in 2/3/4-band mode.  Two
// bands keep the outer Low/High slots; three bands keep Low, Lo-Mid and High
// and merge the two middle regions into the Mid band.
constexpr int activeBandSlot(int count, int index) noexcept {
  if (count <= 2) {
    return index == 0 ? 0 : 3;
  }
  if (count == 3) {
    return index == 0 ? 0 : (index == 1 ? 1 : 3);
  }
  return index < 0 ? 0 : (index > 3 ? 3 : index);
}

constexpr int activeBandIndex(int count, int slot) noexcept {
  for (int i = 0; i < count; ++i) {
    if (activeBandSlot(count, i) == slot) {
      return i;
    }
  }
  return -1;
}

struct BandControls {
  int model = 0;
  bool enabled = true;
  float input = 0.5f;   // 0..1 maps to -12..+12 dB
  float drive = 0.45f;
  float character = 0.5f;
  float tone = 0.5f;
  float mix = 1.0f;
  float level = 0.5f;   // 0 = off, 0.5 = 0 dB, 1 = +12 dB
};

struct Controls {
  bool enabled = true;
  bool multiband = false;
  int bandCount = 4;
  int selectedBand = 1;
  int soloBand = -1;
  float xover1 = 120.0f;  // Hz
  float xover2 = 900.0f;  // Hz
  float xover3 = 5000.0f; // Hz
  float output = 0.5f;
  bool autoGain = false;
  float oversample = 0.0f; // 0 Off, 1 2x, 2 4x
  BandControls bands[kBandCount];
};

struct Meters {
  float inPeakL = -120.0f;
  float inPeakR = -120.0f;
  float outPeakL = -120.0f;
  float outPeakR = -120.0f;
  float outVu = -120.0f;
  float bandLevel[kBandCount] = {-120.0f, -120.0f, -120.0f, -120.0f};
};

} // namespace patina
