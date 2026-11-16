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

// One saturating band: input trim, drive, model shaping, tone, mix and output
// trim.  The global output, auto gain and oversampling live in the engine so
// single-band and multiband share exactly the same signal path.
class BandSaturator {
public:
  void prepare(float sampleRate) noexcept {
    rate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    inputSmooth_.setTime(12.0f, rate_);
    driveSmooth_.setTime(15.0f, rate_);
    mixSmooth_.setTime(10.0f, rate_);
    outputSmooth_.setTime(15.0f, rate_);
    enableSmooth_.setTime(8.0f, rate_);
    reset();
    setFilters();
  }

  void setControls(const BandControls& c, int soloBand, int band) noexcept {
    controls_ = c;
    model_ = std::min(std::max(c.model, 0), static_cast<int>(Model::Count) - 1);
    inputSmooth_.setTarget(
        hdsp::dbToGain(-12.0f + 24.0f * clamp01(c.input)));
    driveSmooth_.setTarget(
        hdsp::dbToGain(-12.0f + 24.0f * clamp01(c.drive)));
    mixSmooth_.setTarget(clamp01(c.mix));
    outputSmooth_.setTarget(levelGain(c.level));
    const bool audible = c.enabled && (soloBand < 0 || soloBand == band);
    enableSmooth_.setTarget(audible ? 1.0f : 0.0f);
    setFilters();
  }

  void snap() noexcept {
    inputSmooth_.snap();
    driveSmooth_.snap();
    mixSmooth_.snap();
    outputSmooth_.snap();
    enableSmooth_.snap();
  }

  void reset() noexcept {
    for (int ch = 0; ch < 2; ++ch) {
      pre_[ch].reset();
      tone_[ch].reset();
      lp_[ch].reset();
      hp_[ch].reset();
      shaper_[ch].reset();
    }
  }

  inline void process(float& left, float& right) noexcept {
    const float in = inputSmooth_.next();
    const float drive = driveSmooth_.next();
    const float mix = mixSmooth_.next();
    const float out = outputSmooth_.next();
    const float gate = enableSmooth_.next();

    const float dryL = left * in;
    const float dryR = right * in;

    const float k = 1.0f + 5.0f * clamp01(controls_.drive);
    const float trim = hdsp::dbToGain(kModelTrimDb[model_]);

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
    xL *= trim;
    xR *= trim;

    const float yL = (dryL + mix * (xL - dryL)) * out * gate;
    const float yR = (dryR + mix * (xR - dryR)) * out * gate;
    left = std::isfinite(yL) ? yL : 0.0f;
    right = std::isfinite(yR) ? yR : 0.0f;
  }

private:
  static float clamp01(float x) noexcept {
    return std::min(std::max(x, 0.0f), 1.0f);
  }

  // 0 = silence, 0.5 = 0 dB, 1 = +12 dB.  The lower half covers -60..0 dB,
  // so a band can be faded all the way out with the Level knob.
  static float levelGain(float value) noexcept {
    const float v = clamp01(value);
    if (v <= 0.002f) {
      return 0.0f;
    }
    const float db =
        v <= 0.5f ? -60.0f + 120.0f * v : 24.0f * (v - 0.5f);
    return hdsp::dbToGain(db);
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
      pre_[ch].lowShelf(preHz, preDb, 0.7f, rate_);
      tone_[ch].highShelf(3500.0f + 1500.0f * tone, high, 0.7f, rate_);

      if (model_ == static_cast<int>(Model::Tape)) {
        lp_[ch].lowPass(12000.0f + 6000.0f * tone, 0.7071f, rate_);
      } else {
        lp_[ch].lowPass(21000.0f, 0.7071f, rate_);
      }
      hp_[ch].highPass(model_ == static_cast<int>(Model::Transformer) ? 22.0f
                                                                      : 10.0f,
                       0.7071f, rate_);
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

  static constexpr float kModelTrimDb[5] = {0.0f, -1.0f, -1.5f, -1.0f,
                                            -1.0f};

  float rate_ = 48000.0f;
  int model_ = 0;
  BandControls controls_;
  hdsp::Smoothed inputSmooth_;
  hdsp::Smoothed driveSmooth_;
  hdsp::Smoothed mixSmooth_;
  hdsp::Smoothed outputSmooth_;
  hdsp::Smoothed enableSmooth_;
  hdsp::Biquad pre_[2];
  hdsp::Biquad tone_[2];
  hdsp::Biquad lp_[2];
  hdsp::Biquad hp_[2];
  hdsp::AsymShaper shaper_[2];
};

class SaturationEngine {
public:
  void prepare(float sampleRate, float oversampleMode = -1.0f) noexcept {
    baseRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    if (oversampleMode >= 0.0f) {
      controls_.oversample = oversampleMode;
    }
    configure(factorFor(controls_.oversample));
    setControls(controls_);
    outputSmooth_.snap();
    for (BandSaturator& b : bands_) {
      b.snap();
    }
  }

  // Oversampling changes the reported latency, so a host only accepts them
  // while the plugin is being activated.  The CLAP wrapper defers the switch
  // to the next prepare() and asks the host to restart.
  void setDeferOversample(bool defer) noexcept { deferOversample_ = defer; }

  void setControls(const Controls& c) noexcept {
    const int factor = factorFor(c.oversample);
    if (!deferOversample_ && factor != factor_) {
      configure(factor);
    }
    const int previousBands = controls_.bandCount;
    const bool previousMultiband = controls_.multiband;
    controls_ = c;
    controls_.bandCount = std::min(std::max(c.bandCount, 2), 4);
    int selected =
        std::min(std::max(c.selectedBand, 0), 3);
    if (activeBandIndex(controls_.bandCount, selected) < 0) {
      selected = activeBandSlot(controls_.bandCount,
                                controls_.bandCount >= 3 ? 1 : 0);
    }
    controls_.selectedBand = selected;
    if (controls_.soloBand >= 0 &&
        activeBandIndex(controls_.bandCount, controls_.soloBand) < 0) {
      controls_.soloBand = -1;
    }
    if (controls_.bandCount != previousBands ||
        controls_.multiband != previousMultiband) {
      // Structural change: clear stale filter history from the previous
      // band layout without touching the parameter smoothers.
      for (BandSaturator& b : bands_) {
        b.reset();
      }
      for (hdsp::LinkwitzRiley4& xo : xover2_) {
        xo.reset();
      }
      for (hdsp::ThreeBandCrossover& xo : xover3_) {
        xo.reset();
      }
      for (hdsp::FourBandCrossover& xo : xover4_) {
        xo.reset();
      }
    }
    controls_.xover1 = std::min(std::max(c.xover1, 40.0f), 800.0f);
    controls_.xover2 = std::min(std::max(c.xover2, 200.0f), 6000.0f);
    controls_.xover3 = std::min(std::max(c.xover3, 800.0f), 16000.0f);
    if (controls_.xover2 < controls_.xover1 + 20.0f) {
      controls_.xover2 = controls_.xover1 + 20.0f;
    }
    if (controls_.xover3 < controls_.xover2 + 100.0f) {
      controls_.xover3 = controls_.xover2 + 100.0f;
    }

    for (int b = 0; b < kBandCount; ++b) {
      bands_[b].setControls(controls_.bands[b], controls_.soloBand, b);
    }
    outputSmooth_.setTarget(
        hdsp::dbToGain(-12.0f + 24.0f * clamp01(c.output)));
    if (controls_.multiband) {
      updateCrossovers(false);
    }
  }

  // Latency of the current oversampling mode, in base-rate samples.
  int latencySamples() const noexcept { return os_.latencySamples(); }

  const float* spectrumBins() const noexcept { return spectrum_.bins(); }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    if (frames <= 0) {
      return;
    }
    // Analyse the unprocessed input so the editor has something to show even
    // when the bands are faded out.
    spectrum_.push(left, right, frames);
    Stats st;
    for (int b = 0; b < kBandCount; ++b) {
      st.band[b] = 0.0f;
    }
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
    for (int b = 0; b < kBandCount; ++b) {
      meters.bandLevel[b] = hdsp::gainToDb(st.band[b]);
    }
  }

  void reset() noexcept {
    os_.reset();
    for (BandSaturator& b : bands_) {
      b.reset();
    }
    for (hdsp::LinkwitzRiley4& xo : xover2_) {
      xo.reset();
    }
    for (hdsp::ThreeBandCrossover& xo : xover3_) {
      xo.reset();
    }
    for (hdsp::FourBandCrossover& xo : xover4_) {
      xo.reset();
    }
    spectrum_.reset();
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
    float band[kBandCount] = {0.0f, 0.0f, 0.0f, 0.0f};
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
    for (BandSaturator& b : bands_) {
      b.prepare(osRate_);
    }
    for (hdsp::LinkwitzRiley4& xo : xover2_) {
      xo.prepare(osRate_);
    }
    for (hdsp::ThreeBandCrossover& xo : xover3_) {
      xo.prepare(osRate_);
    }
    for (hdsp::FourBandCrossover& xo : xover4_) {
      xo.prepare(osRate_);
    }
    spectrum_.prepare(baseRate_);
    outputSmooth_.setTime(15.0f, osRate_);
    vuCoeff_ = hdsp::timeCoeff(300.0f, osRate_);
    autoPowerCoeff_ = hdsp::timeCoeff(120.0f, osRate_);
    autoDbCoeff_ = hdsp::timeCoeff(250.0f, osRate_);
    // The coefficient cache is keyed by frequency only.  A rate change
    // prepares the filters at a new sample rate, so force a recompute even
    // when the crossover frequencies themselves did not move.
    xover1Applied_ = -1.0f;
    xover2Applied_ = -1.0f;
    xover3Applied_ = -1.0f;
    xover1Cur_ = controls_.xover1;
    xover2Cur_ = controls_.xover2;
    xover3Cur_ = controls_.xover3;
    updateCrossovers(true);
    reset();
  }

  void updateCrossovers(bool snap) noexcept {
    const float target1 = controls_.xover1;
    const float target2 = controls_.xover2;
    const float target3 = controls_.xover3;
    if (snap) {
      xover1Cur_ = target1;
      xover2Cur_ = target2;
      xover3Cur_ = target3;
    } else {
      // Block-rate smoothing: a dragged crossover moves in small steps
      // instead of rebuilding the filters with a jump every buffer.
      xover1Cur_ += (target1 - xover1Cur_) * 0.25f;
      xover2Cur_ += (target2 - xover2Cur_) * 0.25f;
      xover3Cur_ += (target3 - xover3Cur_) * 0.25f;
    }
    if (std::fabs(xover1Cur_ - xover1Applied_) < 0.05f &&
        std::fabs(xover2Cur_ - xover2Applied_) < 0.05f &&
        std::fabs(xover3Cur_ - xover3Applied_) < 0.05f) {
      return;
    }
    xover1Applied_ = xover1Cur_;
    xover2Applied_ = xover2Cur_;
    xover3Applied_ = xover3Cur_;
    for (hdsp::LinkwitzRiley4& xo : xover2_) {
      xo.setCrossover(xover2Cur_);
    }
    for (hdsp::ThreeBandCrossover& xo : xover3_) {
      xo.setCrossovers(xover1Cur_, xover3Cur_);
    }
    for (hdsp::FourBandCrossover& xo : xover4_) {
      xo.setCrossovers(xover1Cur_, xover2Cur_, xover3Cur_);
    }
  }

  void processBlock(float* left, float* right, int frames,
                    Stats& st) noexcept {
    const bool multiband = controls_.multiband;
    const bool enabled = controls_.enabled;
    float out = 0.0f;

    for (int i = 0; i < frames; ++i) {
      const float rawL = std::isfinite(left[i]) ? left[i] : 0.0f;
      const float rawR = std::isfinite(right[i]) ? right[i] : 0.0f;
      const float dryL = std::min(std::max(rawL, -32.0f), 32.0f);
      const float dryR = std::min(std::max(rawR, -32.0f), 32.0f);
      st.inL = std::max(st.inL, std::fabs(dryL));
      st.inR = std::max(st.inR, std::fabs(dryR));

      float wetL = dryL;
      float wetR = dryR;

      if (multiband) {
        float bandL[kBandCount] = {};
        float bandR[kBandCount] = {};
        if (controls_.bandCount <= 2) {
          xover2_[0].process(dryL, bandL[0], bandL[3]);
          xover2_[1].process(dryR, bandR[0], bandR[3]);
        } else if (controls_.bandCount == 3) {
          xover3_[0].process(dryL, bandL[0], bandL[1], bandL[3]);
          xover3_[1].process(dryR, bandR[0], bandR[1], bandR[3]);
        } else {
          xover4_[0].process(dryL, bandL[0], bandL[1], bandL[2], bandL[3]);
          xover4_[1].process(dryR, bandR[0], bandR[1], bandR[2], bandR[3]);
        }
        wetL = 0.0f;
        wetR = 0.0f;
        for (int index = 0; index < controls_.bandCount; ++index) {
          const int b = activeBandSlot(controls_.bandCount, index);
          bands_[b].process(bandL[b], bandR[b]);
          st.band[b] =
              std::max(st.band[b], std::max(std::fabs(bandL[b]),
                                            std::fabs(bandR[b])));
          wetL += bandL[b];
          wetR += bandR[b];
        }
      } else {
        bands_[0].process(wetL, wetR);
        st.band[0] =
            std::max(st.band[0], std::max(std::fabs(wetL), std::fabs(wetR)));
      }

      if (controls_.autoGain) {
        const float inPower = 0.5f * (dryL * dryL + dryR * dryR);
        const float outPower = 0.5f * (wetL * wetL + wetR * wetR);
        inPower_ += (inPower - inPower_) * (1.0f - autoPowerCoeff_);
        outPower_ += (outPower - outPower_) * (1.0f - autoPowerCoeff_);
        const float wanted =
            hdsp::gainToDb(std::sqrt(inPower_ + 1e-20f)) -
            hdsp::gainToDb(std::sqrt(outPower_ + 1e-20f));
        autoDb_ += (wanted - autoDb_) * (1.0f - autoDbCoeff_);
        const float make = hdsp::dbToGain(
            std::min(std::max(autoDb_, -18.0f), 12.0f));
        wetL *= make;
        wetR *= make;
      }

      out = outputSmooth_.next();
      float yL = wetL;
      float yR = wetR;
      if (!enabled) {
        yL = dryL;
        yR = dryR;
      } else {
        yL *= out;
        yR *= out;
      }

      left[i] = std::isfinite(yL) ? yL : 0.0f;
      right[i] = std::isfinite(yR) ? yR : 0.0f;
      st.outL = std::max(st.outL, std::fabs(left[i]));
      st.outR = std::max(st.outR, std::fabs(right[i]));
      const float power = 0.5f * (left[i] * left[i] + right[i] * right[i]);
      vuPower_ = vuCoeff_ * vuPower_ + (1.0f - vuCoeff_) * power;
    }
  }

  bool deferOversample_ = false;
  float baseRate_ = 48000.0f;
  float osRate_ = 48000.0f;
  int factor_ = 1;
  Controls controls_;
  hdsp::Oversampler os_;
  BandSaturator bands_[kBandCount];
  hdsp::LinkwitzRiley4 xover2_[2];
  hdsp::ThreeBandCrossover xover3_[2];
  hdsp::FourBandCrossover xover4_[2];
  hdsp::SpectrumAnalyzer spectrum_;
  hdsp::Smoothed outputSmooth_;
  float xover1Cur_ = 120.0f;
  float xover2Cur_ = 900.0f;
  float xover3Cur_ = 5000.0f;
  float xover1Applied_ = -1.0f;
  float xover2Applied_ = -1.0f;
  float xover3Applied_ = -1.0f;
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
