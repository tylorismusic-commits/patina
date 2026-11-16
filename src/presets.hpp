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
  bool multiband;
  PresetValue values[24];
};

inline constexpr Preset kPresets[] = {
    {"Valve Vocal Warmth", "Vocals", 0, false,
     {{P_DRIVE, 0.45f}, {P_CHARACTER, 0.35f}, {P_TONE, 0.55f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Valve Drum Drive", "Drums", 0, false,
     {{P_DRIVE, 0.62f}, {P_CHARACTER, 0.60f}, {P_TONE, 0.45f},
      {P_MIX, 0.80f}, {P_OUTPUT, 0.44f}, {-1, 0}}},
    {"Console Vocal Air", "Vocals", 1, false,
     {{P_DRIVE, 0.40f}, {P_CHARACTER, 0.40f}, {P_TONE, 0.66f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Console Drum Console", "Drums", 1, false,
     {{P_DRIVE, 0.55f}, {P_CHARACTER, 0.50f}, {P_TONE, 0.55f},
      {P_MIX, 0.70f}, {P_OUTPUT, 0.46f}, {-1, 0}}},
    {"Bus Mix Glue", "Mix Bus", 2, false,
     {{P_DRIVE, 0.35f}, {P_CHARACTER, 0.55f}, {P_TONE, 0.50f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Bus Drum Bus", "Drums", 2, false,
     {{P_DRIVE, 0.50f}, {P_CHARACTER, 0.62f}, {P_TONE, 0.45f},
      {P_MIX, 0.80f}, {P_OUTPUT, 0.46f}, {-1, 0}}},
    {"Tape Vocal Tape", "Vocals", 3, false,
     {{P_DRIVE, 0.50f}, {P_CHARACTER, 0.45f}, {P_TONE, 0.40f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.50f}, {-1, 0}}},
    {"Tape Drum Tape", "Drums", 3, false,
     {{P_DRIVE, 0.60f}, {P_CHARACTER, 0.50f}, {P_TONE, 0.35f},
      {P_MIX, 0.90f}, {P_OUTPUT, 0.45f}, {-1, 0}}},
    {"Transformer Bass Iron", "Bass", 4, false,
     {{P_DRIVE, 0.55f}, {P_CHARACTER, 0.65f}, {P_TONE, 0.30f},
      {P_MIX, 1.0f}, {P_OUTPUT, 0.48f}, {-1, 0}}},
    {"Transformer Mix Glue", "Mix Bus", 4, false,
     {{P_DRIVE, 0.40f}, {P_CHARACTER, 0.70f}, {P_TONE, 0.45f},
      {P_MIX, 0.80f}, {P_OUTPUT, 0.47f}, {-1, 0}}},

    // Multiband factory presets.  Per-band parameters omitted here reset to
    // their defaults first, so only the values that shape the preset appear.
    {"3-Band Iron Glue", "Multiband", 4, true,
     {{P_MB, 1.0f}, {P_XOVER1, 100.0f}, {P_XOVER2, 3200.0f},
      {P_BAND, 1.0f}, {P_OUTPUT, 0.50f},
      {bandParam(0, BF_MODEL), 4.0f}, {bandParam(0, BF_DRIVE), 0.52f},
      {bandParam(0, BF_CHARACTER), 0.65f}, {bandParam(0, BF_TONE), 0.35f},
      {bandParam(0, BF_MIX), 1.0f}, {bandParam(0, BF_OUTPUT), 0.50f},
      {bandParam(1, BF_MODEL), 0.0f}, {bandParam(1, BF_DRIVE), 0.40f},
      {bandParam(1, BF_CHARACTER), 0.45f}, {bandParam(1, BF_TONE), 0.55f},
      {bandParam(1, BF_MIX), 0.90f}, {bandParam(1, BF_OUTPUT), 0.50f},
      {bandParam(2, BF_MODEL), 1.0f}, {bandParam(2, BF_DRIVE), 0.35f},
      {bandParam(2, BF_CHARACTER), 0.40f}, {bandParam(2, BF_TONE), 0.62f},
      {bandParam(2, BF_MIX), 0.80f}, {bandParam(2, BF_OUTPUT), 0.48f}}},
    {"Tape Low / Console Air", "Multiband", 3, true,
     {{P_MB, 1.0f}, {P_XOVER1, 150.0f}, {P_XOVER2, 4000.0f},
      {P_BAND, 1.0f}, {P_OUTPUT, 0.50f},
      {bandParam(0, BF_MODEL), 3.0f}, {bandParam(0, BF_DRIVE), 0.58f},
      {bandParam(0, BF_CHARACTER), 0.50f}, {bandParam(0, BF_TONE), 0.38f},
      {bandParam(0, BF_MIX), 1.0f}, {bandParam(0, BF_OUTPUT), 0.48f},
      {bandParam(1, BF_MODEL), 2.0f}, {bandParam(1, BF_DRIVE), 0.42f},
      {bandParam(1, BF_CHARACTER), 0.55f}, {bandParam(1, BF_TONE), 0.50f},
      {bandParam(1, BF_MIX), 0.85f}, {bandParam(1, BF_OUTPUT), 0.50f},
      {bandParam(2, BF_MODEL), 1.0f}, {bandParam(2, BF_DRIVE), 0.34f},
      {bandParam(2, BF_CHARACTER), 0.38f}, {bandParam(2, BF_TONE), 0.68f},
      {bandParam(2, BF_MIX), 0.75f}, {bandParam(2, BF_OUTPUT), 0.50f}}},
    {"Valve Three-Way", "Multiband", 0, true,
     {{P_MB, 1.0f}, {P_XOVER1, 180.0f}, {P_XOVER2, 2800.0f},
      {P_BAND, 1.0f}, {P_OUTPUT, 0.50f},
      {bandParam(0, BF_MODEL), 0.0f}, {bandParam(0, BF_DRIVE), 0.48f},
      {bandParam(0, BF_CHARACTER), 0.58f}, {bandParam(0, BF_TONE), 0.40f},
      {bandParam(0, BF_MIX), 0.95f}, {bandParam(0, BF_OUTPUT), 0.50f},
      {bandParam(1, BF_MODEL), 0.0f}, {bandParam(1, BF_DRIVE), 0.42f},
      {bandParam(1, BF_CHARACTER), 0.40f}, {bandParam(1, BF_TONE), 0.55f},
      {bandParam(1, BF_MIX), 1.0f}, {bandParam(1, BF_OUTPUT), 0.50f},
      {bandParam(2, BF_MODEL), 0.0f}, {bandParam(2, BF_DRIVE), 0.30f},
      {bandParam(2, BF_CHARACTER), 0.30f}, {bandParam(2, BF_TONE), 0.64f},
      {bandParam(2, BF_MIX), 0.70f}, {bandParam(2, BF_OUTPUT), 0.50f}}},
    {"Drum Multiband", "Multiband", 4, true,
     {{P_MB, 1.0f}, {P_XOVER1, 90.0f}, {P_XOVER2, 2200.0f},
      {P_BAND, 1.0f}, {P_OUTPUT, 0.46f},
      {bandParam(0, BF_MODEL), 4.0f}, {bandParam(0, BF_DRIVE), 0.62f},
      {bandParam(0, BF_CHARACTER), 0.70f}, {bandParam(0, BF_TONE), 0.32f},
      {bandParam(0, BF_MIX), 1.0f}, {bandParam(0, BF_OUTPUT), 0.48f},
      {bandParam(1, BF_MODEL), 3.0f}, {bandParam(1, BF_DRIVE), 0.55f},
      {bandParam(1, BF_CHARACTER), 0.55f}, {bandParam(1, BF_TONE), 0.42f},
      {bandParam(1, BF_MIX), 0.85f}, {bandParam(1, BF_OUTPUT), 0.50f},
      {bandParam(2, BF_MODEL), 1.0f}, {bandParam(2, BF_DRIVE), 0.45f},
      {bandParam(2, BF_CHARACTER), 0.48f}, {bandParam(2, BF_TONE), 0.60f},
      {bandParam(2, BF_MIX), 0.70f}, {bandParam(2, BF_OUTPUT), 0.46f}}},
};

inline constexpr int kPresetCount =
    static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace patina
