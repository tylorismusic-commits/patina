// Patina — typed parameter values
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "params.hpp"

namespace patina {

inline std::string lowerText(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

inline std::string trimText(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) {
    return {};
  }
  const size_t b = s.find_last_not_of(" \t");
  return s.substr(a, b - a + 1);
}

inline std::string formatParamValue(const Shared& shared, uint32_t id,
                                   double value) {
  const ParamSpec& s = kParamSpecs[id];
  const int bands = bandCount(shared);
  if (id == P_BAND) {
    int index =
        activeBandIndex(bands, std::clamp(static_cast<int>(value + 0.5), 0, 3));
    if (index < 0) {
      index = 0;
    }
    return bandLabelForCount(bands, index);
  }
  if (s.labels.count > 0) {
    const int i = std::clamp(
        static_cast<int>(std::lround(value - s.min)), 0, s.labels.count - 1);
    return s.labels.names[i];
  }
  char buf[64];
  switch (id) {
    case P_ENABLED:
    case P_AUTO_GAIN:
    case P_MB:
      return value > 0.5 ? "On" : "Off";
    case P_DRIVE:
    case P_OUTPUT:
      std::snprintf(buf, sizeof(buf), "%+.1f dB", -12.0 + 24.0 * value);
      return buf;
    case P_CHARACTER:
    case P_TONE:
    case P_MIX:
      std::snprintf(buf, sizeof(buf), "%.0f%%", value * 100.0);
      return buf;
    case P_XOVER1:
    case P_XOVER2:
    case P_XOVER3:
      if (value >= 1000.0) {
        std::snprintf(buf, sizeof(buf), "%.1f kHz", value / 1000.0);
      } else {
        std::snprintf(buf, sizeof(buf), "%.0f Hz", value);
      }
      return buf;
    default:
      break;
  }
  if (id >= P_B0_MODEL && id < P_BANDS) {
    const uint32_t offset = id - P_B0_MODEL;
    const BandField field = static_cast<BandField>(offset % BF_COUNT);
    switch (field) {
      case BF_MODEL:
        std::snprintf(
            buf, sizeof(buf), "%s",
            kModelLabels[std::clamp(static_cast<int>(value + 0.5), 0,
                                    static_cast<int>(Model::Count) - 1)]);
        return buf;
      case BF_INPUT:
      case BF_DRIVE:
        std::snprintf(buf, sizeof(buf), "%+.1f dB", -12.0 + 24.0 * value);
        return buf;
      case BF_CHARACTER:
      case BF_TONE:
      case BF_MIX:
        std::snprintf(buf, sizeof(buf), "%.0f%%", value * 100.0);
        return buf;
      case BF_LEVEL:
        if (value <= 0.002) {
          return "Off";
        }
        std::snprintf(buf, sizeof(buf), "%+.1f dB",
                      value <= 0.5 ? -60.0 + 120.0 * value
                                   : 24.0 * (value - 0.5));
        return buf;
      case BF_ON:
        return value > 0.5 ? "On" : "Off";
      default:
        break;
    }
  }
  std::snprintf(buf, sizeof(buf), "%.2f", value);
  return buf;
}

inline bool parseParamValue(const Shared& shared, uint32_t id,
                            const std::string& raw, double* out) {
  const ParamSpec& s = kParamSpecs[id];
  const std::string text = lowerText(trimText(raw));
  if (text.empty()) {
    return false;
  }
  for (int i = 0; i < s.labels.count; ++i) {
    if (id != P_BAND && text == lowerText(s.labels.names[i])) {
      *out = s.min + i;
      return true;
    }
  }
  if (text == "on" || text == "true" || text == "yes") {
    *out = 1.0;
    return true;
  }
  if (text == "off" || text == "false" || text == "no") {
    *out = 0.0;
    return true;
  }

  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  const std::string suffix =
      end == text.c_str() ? std::string() : trimText(std::string(end));
  auto hz = [&]() {
    return suffix == "khz" || suffix == "k" ? value * 1000.0 : value;
  };
  auto percent = [&]() {
    return suffix == "%" || value > 1.0 ? value / 100.0 : value;
  };
  auto db = [&]() { return (value + 12.0) / 24.0; };
  auto levelDb = [&]() {
    if (value <= -60.0) {
      return 0.0;
    }
    return value <= 0.0 ? (value + 60.0) / 120.0
                        : 0.5 + value / 24.0;
  };

  switch (id) {
    case P_BAND:
      for (int i = 0; i < 4; ++i) {
        if (text == lowerText(kBandLabels[i])) {
          *out = i;
          return true;
        }
      }
      if (text == "mid") {
        *out = activeBandSlot(bandCount(shared), 1);
        return true;
      }
      return false;
    case P_XOVER1:
    case P_XOVER2:
    case P_XOVER3:
      if (end == text.c_str()) {
        return false;
      }
      *out = hz();
      return true;
    case P_DRIVE:
    case P_OUTPUT:
      if (end == text.c_str()) {
        return false;
      }
      *out = db();
      return true;
    case P_CHARACTER:
    case P_TONE:
    case P_MIX:
      if (end == text.c_str()) {
        return false;
      }
      *out = percent();
      return true;
    case P_BANDS:
      if (end == text.c_str()) {
        return false;
      }
      *out = value;
      return true;
    default:
      break;
  }

  if (id >= P_B0_MODEL && id < P_BANDS) {
    const uint32_t offset = id - P_B0_MODEL;
    const BandField field = static_cast<BandField>(offset % BF_COUNT);
    if (field == BF_MODEL) {
      for (int i = 0; i < static_cast<int>(Model::Count); ++i) {
        if (text == lowerText(kModelLabels[i])) {
          *out = i;
          return true;
        }
      }
      return false;
    }
    if (end == text.c_str()) {
      return false;
    }
    switch (field) {
      case BF_INPUT:
      case BF_DRIVE:
        *out = db();
        return true;
      case BF_CHARACTER:
      case BF_TONE:
      case BF_MIX:
        *out = percent();
        return true;
      case BF_LEVEL:
        *out = levelDb();
        return true;
      case BF_ON:
        *out = value > 0.5 ? 1.0 : 0.0;
        return true;
      default:
        break;
    }
  }

  if (end == text.c_str()) {
    return false;
  }
  *out = value;
  return true;
}

} // namespace patina
