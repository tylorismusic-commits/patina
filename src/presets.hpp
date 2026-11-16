// Patina — factory presets
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "params.hpp"

namespace patina {

struct PresetValue {
  int16_t param = -1;
  float value = 0.0f;
};

struct Preset {
  const char* name;
  const char* category;
  int model;
  PresetValue values[8];
};

inline constexpr Preset kPresets[] = {
    {"Valve Vocal Warmth", "Vocals", 0,
     {{P_DRIVE, 0.45f}, {P_CHARACTER, 0.35f}, {P_TONE, 0.55f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Valve Drum Drive", "Drums", 0,
     {{P_DRIVE, 0.62f}, {P_CHARACTER, 0.60f}, {P_TONE, 0.45f},
      {P_MIX, 0.80f}, {P_OUTPUT, 0.44f}, {-1, 0}}},
    {"Console Vocal Air", "Vocals", 1,
     {{P_DRIVE, 0.40f}, {P_CHARACTER, 0.40f}, {P_TONE, 0.66f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Console Drum Console", "Drums", 1,
     {{P_DRIVE, 0.55f}, {P_CHARACTER, 0.50f}, {P_TONE, 0.55f},
      {P_MIX, 0.70f}, {P_OUTPUT, 0.46f}, {-1, 0}}},
    {"Bus Mix Glue", "Mix Bus", 2,
     {{P_DRIVE, 0.35f}, {P_CHARACTER, 0.55f}, {P_TONE, 0.50f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Bus Drum Bus", "Drums", 2,
     {{P_DRIVE, 0.50f}, {P_CHARACTER, 0.62f}, {P_TONE, 0.45f},
      {P_MIX, 0.80f}, {P_OUTPUT, 0.46f}, {-1, 0}}},
    {"Tape Vocal Tape", "Vocals", 3,
     {{P_DRIVE, 0.50f}, {P_CHARACTER, 0.45f}, {P_TONE, 0.40f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Tape Drum Tape", "Drums", 3,
     {{P_DRIVE, 0.60f}, {P_CHARACTER, 0.50f}, {P_TONE, 0.35f},
      {P_MIX, 0.90f}, {P_OUTPUT, 0.45f}, {-1, 0}}},
    {"Transformer Bass Iron", "Bass", 4,
     {{P_DRIVE, 0.55f}, {P_CHARACTER, 0.65f}, {P_TONE, 0.30f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.48f}, {-1, 0}}},
    {"Transformer Mix Glue", "Mix Bus", 4,
     {{P_DRIVE, 0.40f}, {P_CHARACTER, 0.70f}, {P_TONE, 0.45f},
      {P_MIX, 0.80f}, {P_OUTPUT, 0.47f}, {-1, 0}}},
};

inline constexpr int kPresetCount =
    static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace patina
