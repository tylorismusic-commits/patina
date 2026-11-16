// Patina — LV2 plugin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Five saturation models, single-band or 2/3/4-band multiband.  The control
// ports mirror the CLAP parameter ids: port = kLv2FirstParam + ParamId.

#include <lv2/core/lv2.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "dsp/saturation.hpp"
#include "lv2_ports.hpp"
#include "params.hpp"

namespace {

constexpr const char* kPluginUri =
    "https://github.com/tylorismusic-commits/patina";

struct Plugin {
  float sampleRate = 48000.0f;
  float lastPort[patina::P_COUNT] = {};
  bool portSeen[patina::P_COUNT] = {};
  patina::Shared shared;
  patina::SaturationEngine engine;
  patina::Meters meters;
  float* ports[patina::kLv2PortCount] = {};
};

LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*,
                       const LV2_Feature* const*) {
  auto* p = new Plugin();
  p->sampleRate = static_cast<float>(rate);
  p->engine.prepare(p->sampleRate);
  p->engine.setControls(patina::controlsFrom(p->shared));
  return p;
}

void connectPort(LV2_Handle handle, uint32_t port, void* data) {
  auto* p = static_cast<Plugin*>(handle);
  if (port < patina::kLv2PortCount) {
    p->ports[port] = static_cast<float*>(data);
  }
}

void activate(LV2_Handle handle) {
  auto* p = static_cast<Plugin*>(handle);
  p->engine.reset();
  if (p->ports[patina::kLv2Latency]) {
    *p->ports[patina::kLv2Latency] =
        static_cast<float>(p->engine.latencySamples());
  }
}

void deactivate(LV2_Handle) {}

void run(LV2_Handle handle, uint32_t frames) {
  auto* p = static_cast<Plugin*>(handle);
  float* inL = p->ports[patina::kLv2InL];
  float* inR = p->ports[patina::kLv2InR];
  float* outL = p->ports[patina::kLv2OutL];
  float* outR = p->ports[patina::kLv2OutR];
  if (!inL || !outL) {
    return;
  }
  // Mono or partially connected hosts: mirror the left input.
  if (!inR) {
    inR = inL;
  }

  // Pull host parameter values in.  Apply each port the first time it appears
  // and then only when it actually changes, so a host that caches a parameter
  // cannot undo a change made in the editor.
  bool changed = false;
  for (uint32_t i = 0; i < patina::P_COUNT; ++i) {
    float* v = p->ports[patina::kLv2FirstParam + i];
    if (!v) {
      continue;
    }
    if (!p->portSeen[i] || *v != p->lastPort[i]) {
      p->shared.setFromHost(i, static_cast<double>(*v));
      p->lastPort[i] = *v;
      p->portSeen[i] = true;
      changed = true;
    }
  }
  if (changed) {
    patina::normalizeBandSelection(p->shared);
    patina::normalizeCrossovers(p->shared);
  }
  p->engine.setControls(patina::controlsFrom(p->shared));

  // In-place when the host provides the same buffer, otherwise copy.
  if (outR) {
    if (outL != inL) {
      std::memcpy(outL, inL, frames * sizeof(float));
    }
    if (outR != inR) {
      std::memcpy(outR, inR, frames * sizeof(float));
    }
    p->engine.process(outL, outR, static_cast<int>(frames), p->meters);
  } else {
    // Mono host output: process in chunks, keeping the left channel.
    float scratch[512];
    for (uint32_t offset = 0; offset < frames; offset += 512) {
      const uint32_t n = std::min<uint32_t>(512, frames - offset);
      if (outL + offset != inL + offset) {
        std::memcpy(outL + offset, inL + offset, n * sizeof(float));
      }
      std::memcpy(scratch, inR + offset, n * sizeof(float));
      p->engine.process(outL + offset, scratch, static_cast<int>(n),
                        p->meters);
    }
  }

  for (uint32_t b = 0; b < patina::kLv2BandLevelCount; ++b) {
    float* v = p->ports[patina::kLv2FirstBandLevel + b];
    if (v) {
      *v = p->meters.bandLevel[b];
    }
  }
  auto writeMeter = [&](patina::Lv2Meter meter, float value) {
    float* v = p->ports[patina::kLv2FirstMeter + static_cast<uint32_t>(meter)];
    if (v) {
      *v = value;
    }
  };
  writeMeter(patina::kLv2InPeakL, p->meters.inPeakL);
  writeMeter(patina::kLv2InPeakR, p->meters.inPeakR);
  writeMeter(patina::kLv2OutPeakL, p->meters.outPeakL);
  writeMeter(patina::kLv2OutPeakR, p->meters.outPeakR);
  writeMeter(patina::kLv2OutVu, p->meters.outVu);

  const float* spectrum = p->engine.spectrumBins();
  for (uint32_t i = 0; i < patina::kLv2SpectrumCount; ++i) {
    float* v = p->ports[patina::kLv2FirstSpectrum + i];
    if (v) {
      *v = spectrum[i];
    }
  }
  if (p->ports[patina::kLv2Latency]) {
    *p->ports[patina::kLv2Latency] =
        static_cast<float>(p->engine.latencySamples());
  }
}

void cleanup(LV2_Handle handle) { delete static_cast<Plugin*>(handle); }

const LV2_Descriptor kDescriptor = {
    kPluginUri,
    instantiate,
    connectPort,
    activate,
    run,
    deactivate,
    cleanup,
    nullptr,
};

} // namespace

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}
