// Patina — saturation.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/hdsp.hpp>

#include "dsp/controls.hpp"

namespace patina {

// A model's static transfer, used by the display and the DSP.  It is the
// ADAA shaper's curve without the anti-aliasing state, so the panel can draw
// what the model is doing.
inline float modelShape(int model, float drive, float character, float x) {
  const float k = 1.0f + 5.0f * std::min(std::max(drive, 0.0f), 1.0f);
  float bias = 0.10f + 0.20f * character;
  switch (static_cast<Model>(model)) {
    case Model::Console:
      bias = 0.04f + 0.10f * character;
      break;
    case Model::Bus:
      bias = 0.02f + 0.08f * character;
      break;
    case Model::Tape:
      bias = 0.06f + 0.12f * character;
      break;
    case Model::Transformer:
      bias = 0.14f + 0.22f * character;
      break;
    default:
      break;
  }
  const float tb = std::tanh(bias);
  const float norm = std::max(k * (1.0f - tb * tb), 1.0e-6f);
  return (std::tanh(k * x + bias) - tb) / norm;
}

class SaturationEngine {
public:
  void prepare(float sampleRate) noexcept {
    baseRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    configure(factorFor(controls_.oversample));
    setControls(controls_);
    driveSmooth_.snap();
    mixSmooth_.snap();
    outputSmooth_.snap();
  }

  void setControls(const Controls& c) noexcept {
    const int factor = factorFor(c.oversample);
    if (factor != factor_) {
      configure(factor);
    }
    controls_ = c;
    controls_.model =
        std::min(std::max(c.model, 0), static_cast<int>(Model::Count) - 1);
    model_ = controls_.model;

    driveSmooth_.setTarget(
        hdsp::dbToGain(-12.0f + 24.0f * clamp01(c.drive)));
    mixSmooth_.setTarget(clamp01(c.mix));
    outputSmooth_.setTarget(
        hdsp::dbToGain(-12.0f + 24.0f * clamp01(c.output)));

    setFilters();
  }

  // Latency of the current oversampling mode, in base-rate samples.
  int latencySamples() const noexcept { return os_.latencySamples(); }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    Stats st;
    if (factor_ == 1) {
      processBlock(left, right, frames, st);
    } else {
      for (int start = 0; start < frames;
           start += hdsp::Oversampler::kMaxBaseFrames) {
        const int n =
            std::min(hdsp::Oversampler::kMaxBaseFrames, frames - start);
        os_.up(left + start, right + start, n, osL_, osR_);
        processBlock(osL_, osR_, n * factor_, st);
        os_.down(osL_, osR_, n, left + start, right + start);
      }
    }
    meters.inPeakL = hdsp::gainToDb(st.inL);
    meters.inPeakR = hdsp::gainToDb(st.inR);
    meters.outPeakL = hdsp::gainToDb(st.outL);
    meters.outPeakR = hdsp::gainToDb(st.outR);
    meters.outVu = hdsp::gainToDb(std::sqrt(vuPower_)) + 18.0f;
  }

  void reset() noexcept {
    os_.reset();
    for (int ch = 0; ch < 2; ++ch) {
      pre_[ch].reset();
      tone_[ch].reset();
      lp_[ch].reset();
      hp_[ch].reset();
      shaper_[ch].reset();
    }
    inPower_ = 0.0f;
    outPower_ = 0.0f;
    autoDb_ = 0.0f;
    vuPower_ = 0.0f;
  }

private:
  struct Stats {
    float inL = 0.0f;
    float inR = 0.0f;
    float outL = 0.0f;
    float outR = 0.0f;
  };

  static int factorFor(float mode) noexcept {
    const int m = std::min(std::max(static_cast<int>(mode + 0.5f), 0), 2);
    return m == 0 ? 1 : (m == 1 ? 2 : 4);
  }

  static float clamp01(float x) noexcept {
    return std::min(std::max(x, 0.0f), 1.0f);
  }

  void configure(int factor) noexcept {
    factor_ = factor;
    osRate_ = baseRate_ * static_cast<float>(factor_);
    os_.prepare(factor_);
    driveSmooth_.setTime(15.0f, osRate_);
    mixSmooth_.setTime(10.0f, osRate_);
    outputSmooth_.setTime(15.0f, osRate_);
    vuCoeff_ = hdsp::timeCoeff(300.0f, osRate_);
    autoPowerCoeff_ = hdsp::timeCoeff(120.0f, osRate_);
    autoDbCoeff_ = hdsp::timeCoeff(250.0f, osRate_);
    reset();
    setFilters();
  }

  void setFilters() noexcept {
    const float tone = clamp01(controls_.tone);
    const float character = clamp01(controls_.character);
    const float high = (tone - 0.5f) * 12.0f;
    const float low = (0.5f - tone) * 8.0f;
    for (int ch = 0; ch < 2; ++ch) {
      // Model-specific low-end shaping before the shaper.
      float preHz = 160.0f;
      float preDb = low * 0.35f;
      switch (static_cast<Model>(model_)) {
        case Model::Console:
          preHz = 150.0f;
          preDb += 1.5f * character;
          break;
        case Model::Bus:
          preHz = 120.0f;
          preDb += 1.0f * character;
          break;
        case Model::Tape:
          preHz = 90.0f;
          preDb += 4.0f * character;
          break;
        case Model::Transformer:
          preHz = 60.0f;
          preDb += 6.0f * character;
          break;
        default:
          preDb += 1.2f * character;
          break;
      }
      pre_[ch].lowShelf(preHz, preDb, 0.7f, osRate_);
      tone_[ch].highShelf(3500.0f + 1500.0f * tone, high, 0.7f, osRate_);

      if (model_ == static_cast<int>(Model::Tape)) {
        lp_[ch].lowPass(12000.0f + 6000.0f * tone, 0.7071f, osRate_);
      } else {
        lp_[ch].lowPass(21000.0f, 0.7071f, osRate_);
      }
      hp_[ch].highPass(model_ == static_cast<int>(Model::Transformer) ? 22.0f
                                                                      : 10.0f,
                       0.7071f, osRate_);
    }
  }

  float biasFor(int ch) const noexcept {
    const float character = clamp01(controls_.character);
    float bias = 0.10f + 0.20f * character;
    switch (static_cast<Model>(model_)) {
      case Model::Console:
        bias = 0.04f + 0.10f * character + (ch == 0 ? 0.02f : -0.02f) *
                                                     character;
        break;
      case Model::Bus:
        bias = 0.02f + 0.08f * character;
        break;
      case Model::Tape:
        bias = 0.06f + 0.12f * character;
        break;
      case Model::Transformer:
        bias = 0.14f + 0.22f * character;
        break;
      default:
        break;
    }
    return bias;
  }

  void processBlock(float* left, float* right, int frames,
                    Stats& st) noexcept {
    const float driveControl = clamp01(controls_.drive);
    const float k = 1.0f + 5.0f * driveControl;
    const float trim = hdsp::dbToGain(kModelTrimDb[model_]);
    const bool enabled = controls_.enabled;

    for (int i = 0; i < frames; ++i) {
      const float rawL = std::isfinite(left[i]) ? left[i] : 0.0f;
      const float rawR = std::isfinite(right[i]) ? right[i] : 0.0f;
      const float dryL = std::min(std::max(rawL, -32.0f), 32.0f);
      const float dryR = std::min(std::max(rawR, -32.0f), 32.0f);

      const float drive = driveSmooth_.next();
      float xL = pre_[0].process(dryL * drive);
      float xR = pre_[1].process(dryR * drive);

      xL = shaper_[0].process(xL, k, biasFor(0));
      xR = shaper_[1].process(xR, k, biasFor(1));
      xL = tone_[0].process(xL);
      xR = tone_[1].process(xR);
      if (model_ == static_cast<int>(Model::Tape)) {
        xL = lp_[0].process(xL);
        xR = lp_[1].process(xR);
      }
      xL = hp_[0].process(xL);
      xR = hp_[1].process(xR);

      if (controls_.autoGain) {
        const float inPower = 0.5f * (dryL * dryL + dryR * dryR);
        const float outPower = 0.5f * (xL * xL + xR * xR);
        inPower_ += (inPower - inPower_) * (1.0f - autoPowerCoeff_);
        outPower_ += (outPower - outPower_) * (1.0f - autoPowerCoeff_);
        const float wanted =
            hdsp::gainToDb(std::sqrt(inPower_ + 1e-20f)) -
            hdsp::gainToDb(std::sqrt(outPower_ + 1e-20f));
        autoDb_ += (wanted - autoDb_) * (1.0f - autoDbCoeff_);
        const float make = hdsp::dbToGain(
            std::min(std::max(autoDb_, -18.0f), 12.0f));
        xL *= make;
        xR *= make;
      }

      xL *= trim;
      xR *= trim;

      const float mix = enabled ? mixSmooth_.next() : 0.0f;
      const float out = outputSmooth_.next();
      float yL = dryL + mix * (xL - dryL);
      float yR = dryR + mix * (xR - dryR);
      if (!enabled) {
        yL = dryL;
        yR = dryR;
      } else {
        yL *= out;
        yR *= out;
      }

      left[i] = std::isfinite(yL) ? yL : 0.0f;
      right[i] = std::isfinite(yR) ? yR : 0.0f;
      st.inL = std::max(st.inL, std::fabs(dryL));
      st.inR = std::max(st.inR, std::fabs(dryR));
      st.outL = std::max(st.outL, std::fabs(left[i]));
      st.outR = std::max(st.outR, std::fabs(right[i]));
      const float power = 0.5f * (left[i] * left[i] + right[i] * right[i]);
      vuPower_ = vuCoeff_ * vuPower_ + (1.0f - vuCoeff_) * power;
    }
  }

  static constexpr float kModelTrimDb[5] = {0.0f, -1.0f, -1.5f, -1.0f,
                                            -1.0f};

  float baseRate_ = 48000.0f;
  float osRate_ = 48000.0f;
  int factor_ = 1;
  int model_ = 0;
  Controls controls_;
  hdsp::Oversampler os_;
  hdsp::Smoothed driveSmooth_;
  hdsp::Smoothed mixSmooth_;
  hdsp::Smoothed outputSmooth_;
  hdsp::Biquad pre_[2];
  hdsp::Biquad tone_[2];
  hdsp::Biquad lp_[2];
  hdsp::Biquad hp_[2];
  hdsp::AsymShaper shaper_[2];
  float inPower_ = 0.0f;
  float outPower_ = 0.0f;
  float autoDb_ = 0.0f;
  float vuPower_ = 0.0f;
  float vuCoeff_ = 0.0f;
  float autoPowerCoeff_ = 0.0f;
  float autoDbCoeff_ = 0.0f;
  float osL_[4 * hdsp::Oversampler::kMaxBaseFrames] = {};
  float osR_[4 * hdsp::Oversampler::kMaxBaseFrames] = {};
};

} // namespace patina
