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
  Mid = 1,
  High = 2,
  Count = 3,
};

struct BandControls {
  int model = 0;
  bool enabled = true;
  float input = 0.5f;   // 0..1 maps to -12..+12 dB
  float drive = 0.45f;
  float character = 0.5f;
  float tone = 0.5f;
  float mix = 1.0f;
  float output = 0.5f;  // 0..1 maps to -12..+12 dB
};

struct Controls {
  bool enabled = true;
  bool multiband = false;
  int selectedBand = 1;
  int soloBand = -1;
  float xover1 = 120.0f;  // Hz
  float xover2 = 2500.0f; // Hz
  float output = 0.5f;
  bool autoGain = false;
  float oversample = 0.0f; // 0 Off, 1 2x, 2 4x
  BandControls bands[3];
};

struct Meters {
  float inPeakL = -120.0f;
  float inPeakR = -120.0f;
  float outPeakL = -120.0f;
  float outPeakR = -120.0f;
  float outVu = -120.0f;
  float bandLevel[3] = {-120.0f, -120.0f, -120.0f};
};

} // namespace patina
