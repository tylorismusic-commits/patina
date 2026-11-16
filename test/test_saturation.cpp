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

Controls singleControls(int model, int mode) {
  Controls c;
  c.bands[0].model = model;
  c.bands[0].drive = 0.6f;
  c.bands[0].character = 0.6f;
  c.bands[0].tone = 0.45f;
  c.bands[0].mix = 1.0f;
  c.output = 0.5f;
  c.oversample = static_cast<float>(mode);
  return c;
}

Controls multiControls(int mode) {
  Controls c;
  c.multiband = true;
  c.xover1 = 120.0f;
  c.xover2 = 2500.0f;
  c.output = 0.5f;
  c.oversample = static_cast<float>(mode);
  const int models[3] = {4, 0, 1};
  for (int b = 0; b < 3; ++b) {
    c.bands[b].model = models[b];
    c.bands[b].drive = 0.6f;
    c.bands[b].character = 0.6f;
    c.bands[b].tone = 0.45f;
    c.bands[b].mix = 1.0f;
    c.bands[b].output = 0.5f;
  }
  return c;
}

} // namespace

int main() {
  for (int model = 0; model < static_cast<int>(Model::Count); ++model) {
    for (int mode = 0; mode < 3; ++mode) {
      SaturationEngine engine;
      engine.prepare(kSr);
      Controls c = singleControls(model, mode);
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
      std::snprintf(label, sizeof(label), "single model %d mode %d is finite",
                    model, mode);
      check(finite, label);
      std::snprintf(label, sizeof(label), "single model %d mode %d has output",
                    model, mode);
      check(std::sqrt(power / count) > 0.005, label);

      const int expect = mode == 0 ? 0 : (mode == 1 ? 30 : 45);
      std::snprintf(label, sizeof(label), "single model %d mode %d latency",
                    model, mode);
      check(engine.latencySamples() == expect, label);
    }
  }

  // All three bands, each with a different model, at every oversampling mode.
  for (int mode = 0; mode < 3; ++mode) {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c = multiControls(mode);
    engine.setControls(c);
    Meters meters;
    std::vector<float> l(256), r(256);
    double power = 0.0;
    bool finite = true;
    for (int b = 0; b < 140; ++b) {
      for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(b * 256 + i) / kSr;
        l[i] = r[i] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * t);
      }
      engine.process(l.data(), r.data(), 256, meters);
      if (b > 100) {
        for (int i = 0; i < 256; ++i) {
          if (!std::isfinite(l[i]) || !std::isfinite(r[i])) finite = false;
          power += static_cast<double>(l[i]) * l[i];
        }
      }
    }
    char label[128];
    std::snprintf(label, sizeof(label), "multiband mode %d is finite", mode);
    check(finite, label);
    std::snprintf(label, sizeof(label), "multiband mode %d has output", mode);
    check(std::sqrt(power / (40 * 256)) > 0.005, label);
    const int expect = mode == 0 ? 0 : (mode == 1 ? 30 : 45);
    std::snprintf(label, sizeof(label), "multiband mode %d latency", mode);
    check(engine.latencySamples() == expect, label);
  }

  // Clean bands (mix = 0) must reconstruct the input magnitude.
  {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c = multiControls(0);
    for (BandControls& b : c.bands) {
      b.mix = 0.0f;
    }
    engine.setControls(c);
    Meters meters;
    std::vector<float> l(256), r(256);
    double inPower = 0.0;
    double outPower = 0.0;
    for (int b = 0; b < 120; ++b) {
      for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(b * 256 + i) / kSr;
        l[i] = r[i] = 0.4f * std::sin(2.0f * 3.14159265f * 220.0f * t);
      }
      engine.process(l.data(), r.data(), 256, meters);
      if (b > 60) {
        for (int i = 0; i < 256; ++i) {
          outPower += static_cast<double>(l[i]) * l[i];
          const float t = static_cast<float>(b * 256 + i) / kSr;
          const double in =
              0.4 * std::sin(2.0 * 3.14159265 * 220.0 * t);
          inPower += in * in;
        }
      }
    }
    const double ratio = std::sqrt(outPower / inPower);
    check(std::fabs(ratio - 1.0) < 0.02,
          "clean three-band path sums flat");
  }

  // Muting the upper bands must remove high-frequency content.
  {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c = multiControls(0);
    c.bands[1].enabled = false;
    c.bands[2].enabled = false;
    engine.setControls(c);
    Meters meters;
    std::vector<float> l(256), r(256);
    double power = 0.0;
    for (int b = 0; b < 100; ++b) {
      for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(b * 256 + i) / kSr;
        l[i] = r[i] = 0.4f * std::sin(2.0f * 3.14159265f * 8000.0f * t);
      }
      engine.process(l.data(), r.data(), 256, meters);
      if (b > 70) {
        for (int i = 0; i < 256; ++i) {
          power += static_cast<double>(l[i]) * l[i];
        }
      }
    }
    check(std::sqrt(power / (29 * 256)) < 0.05,
          "muting mid and high removes 8 kHz");
  }

  // Solo must isolate one band.
  {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c = multiControls(0);
    c.soloBand = 1;
    engine.setControls(c);
    Meters meters;
    std::vector<float> l(256), r(256);
    double power = 0.0;
    for (int b = 0; b < 100; ++b) {
      for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(b * 256 + i) / kSr;
        l[i] = r[i] = 0.4f * std::sin(2.0f * 3.14159265f * 1000.0f * t);
      }
      engine.process(l.data(), r.data(), 256, meters);
      if (b > 70) {
        for (int i = 0; i < 256; ++i) {
          power += static_cast<double>(l[i]) * l[i];
        }
      }
    }
    check(std::sqrt(power / (29 * 256)) > 0.01,
          "soloing the mid band keeps 1 kHz");
  }

  // Left-only input must not appear on the right.
  {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c = multiControls(1);
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
    check(rp < 1.0e-10, "multiband keeps L/R separate");
  }

  // Power bypass is dry.
  {
    SaturationEngine engine;
    engine.prepare(kSr);
    Controls c = multiControls(0);
    c.enabled = false;
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
