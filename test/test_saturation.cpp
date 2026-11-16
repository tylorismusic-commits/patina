// Patina — saturation engine tests
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <cstdio>
#include <vector>

#include "dsp/saturation.hpp"

using namespace patina;

namespace {
int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}
constexpr float kSr = 48000.0f;
} // namespace

int main() {
  for (int model = 0; model < static_cast<int>(Model::Count); ++model) {
    for (int mode = 0; mode < 3; ++mode) {
      SaturationEngine engine;
      engine.prepare(kSr);
      Controls c;
      c.model = model;
      c.drive = 0.6f;
      c.character = 0.6f;
      c.tone = 0.45f;
      c.mix = 1.0f;
      c.output = 0.5f;
      c.oversample = static_cast<float>(mode);
      engine.setControls(c);
      Meters meters;
      std::vector<float> l(256), r(256);
      double power = 0.0;
      bool finite = true;
      int count = 0;
      for (int b = 0; b < 120; ++b) {
        for (int i = 0; i < 256; ++i) {
          const float t = static_cast<float>(b * 256 + i) / kSr;
          l[i] = r[i] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * t);
        }
        engine.process(l.data(), r.data(), 256, meters);
        for (int i = 0; i < 256; ++i) {
          if (!std::isfinite(l[i]) || !std::isfinite(r[i])) finite = false;
          if (b > 80) {
            power += static_cast<double>(l[i]) * l[i] +
                     static_cast<double>(r[i]) * r[i];
            count += 2;
          }
        }
      }
      char label[128];
      std::snprintf(label, sizeof(label), "model %d mode %d is finite", model,
                    mode);
      check(finite, label);
      std::snprintf(label, sizeof(label), "model %d mode %d has output", model,
                    mode);
      check(std::sqrt(power / count) > 0.005, label);

      const int expect = mode == 0 ? 0 : (mode == 1 ? 30 : 45);
      std::snprintf(label, sizeof(label), "model %d mode %d latency", model,
                    mode);
      check(engine.latencySamples() == expect, label);
    }
  }

  // Left-only input must not appear on the right.
  {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c;
    c.model = static_cast<int>(Model::Valve);
    c.oversample = 2.0f;
    engine.setControls(c);
    Meters meters;
    std::vector<float> l(256), r(256);
    double rp = 0.0;
    for (int b = 0; b < 40; ++b) {
      for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(b * 256 + i) / kSr;
        l[i] = 0.4f * std::sin(2.0f * 3.14159265f * 440.0f * t);
        r[i] = 0.0f;
      }
      engine.process(l.data(), r.data(), 256, meters);
    }
    for (float v : r) rp += static_cast<double>(v) * v;
    check(rp < 1.0e-10, "saturation keeps L/R separate");
  }

  // Power bypass is dry.
  {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c;
    c.model = static_cast<int>(Model::Tape);
    c.enabled = false;
    c.drive = 0.8f;
    engine.setControls(c);
    Meters meters;
    std::vector<float> l(256), r(256);
    float peak = 0.0f;
    for (int b = 0; b < 20; ++b) {
      for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(b * 256 + i) / kSr;
        l[i] = r[i] = 0.2f * std::sin(2.0f * 3.14159265f * 100.0f * t);
      }
      engine.process(l.data(), r.data(), 256, meters);
      for (float v : l) peak = std::max(peak, std::fabs(v));
    }
    check(std::fabs(peak - 0.2f) < 0.005f, "power bypass is dry");
  }

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
