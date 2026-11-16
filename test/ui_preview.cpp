// Patina — panel preview
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cairo.h>
#include <hdsp/hdsp.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "params.hpp"
#include "ui/panel.hpp"

using namespace patina;

namespace {
void fillSpectrum(Shared& shared) {
  hdsp::SpectrumAnalyzer analyzer;
  analyzer.prepare(48000.0f);
  std::vector<float> l(256), r(256);
  unsigned seed = 1234567u;
  for (int b = 0; b < 220; ++b) {
    for (int i = 0; i < 256; ++i) {
      const float t = static_cast<float>(b * 256 + i) / 48000.0f;
      seed = seed * 1664525u + 1013904223u;
      const float noise =
          (static_cast<float>((seed >> 8) & 0xffff) / 32768.0f - 1.0f) * 0.006f;
      const float x =
          0.10f * std::sin(2.0f * hdsp::kPi * 90.0f * t) +
          0.06f * std::sin(2.0f * hdsp::kPi * 420.0f * t) +
          0.05f * std::sin(2.0f * hdsp::kPi * 1600.0f * t) +
          0.035f * std::sin(2.0f * hdsp::kPi * 6500.0f * t) + noise;
      l[i] = r[i] = x;
    }
    analyzer.push(l.data(), r.data(), 256);
  }
  for (int i = 0; i < kSpectrumBins; ++i) {
    shared.spectrum[i].store(analyzer.bins()[i]);
  }
}
} // namespace

int main(int argc, char** argv) {
  const std::string fontDir = argc > 2 ? argv[1] : "assets/fonts";
  const std::string outDir = argc > 2 ? argv[2] : "docs";
  static const char* names[] = {"valve", "console", "bus", "tape",
                                "transformer"};

  Panel panel;
  panel.loadFont(fontDir);
  for (int model = 0; model < static_cast<int>(Model::Count); ++model) {
    Shared shared;
    shared.set(P_MODEL, model);
    shared.set(P_DRIVE, 0.5);
    shared.set(P_CHARACTER, 0.55);
    shared.set(P_TONE, 0.5);
    shared.set(P_MIX, 1.0);
    shared.set(P_OUTPUT, 0.5);
    shared.set(P_METER, 0);
    shared.inPeakL.store(-10.0f);
    shared.inPeakR.store(-12.0f);
    shared.outPeakL.store(-16.0f);
    shared.outPeakR.store(-17.0f);
    shared.outVu.store(-18.0f);

    PanelState state;
    for (int i = 0; i < 20; ++i) panel.tick(shared, state);
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
    cairo_t* cr = cairo_create(surface);
    panel.draw(cr, shared, state);
    cairo_destroy(cr);
    const std::string path = outDir + "/patina-" + names[model] + ".png";
    const cairo_status_t st = cairo_surface_write_to_png(surface, path.c_str());
    cairo_surface_destroy(surface);
    if (st != CAIRO_STATUS_SUCCESS) {
      std::fprintf(stderr, "could not write %s\n", path.c_str());
      return 1;
    }
    std::printf("wrote %s\n", path.c_str());
  }

  // Single-band spectrum view.
  {
    Shared shared;
    shared.set(P_MODEL, 0);
    shared.set(P_DRIVE, 0.45);
    shared.set(P_CHARACTER, 0.55);
    shared.set(P_TONE, 0.5);
    shared.set(P_MIX, 1.0);
    shared.set(P_OUTPUT, 0.5);
    shared.set(P_METER, 2);
    fillSpectrum(shared);
    shared.inPeakL.store(-9.0f);
    shared.inPeakR.store(-11.0f);
    shared.outPeakL.store(-14.0f);
    shared.outPeakR.store(-15.0f);
    PanelState state;
    for (int i = 0; i < 20; ++i) panel.tick(shared, state);
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
    cairo_t* cr = cairo_create(surface);
    panel.draw(cr, shared, state);
    cairo_destroy(cr);
    const std::string path = outDir + "/patina-spectrum.png";
    if (cairo_surface_write_to_png(surface, path.c_str()) != CAIRO_STATUS_SUCCESS) {
      std::fprintf(stderr, "could not write %s\n", path.c_str());
      return 1;
    }
    cairo_surface_destroy(surface);
    std::printf("wrote %s\n", path.c_str());
  }

  // Multiband views: one per selected band.
  static const char* bandNames[] = {"low", "lomid", "himid", "high"};
  for (int band = 0; band < kBandCount; ++band) {
    Shared shared;
    shared.set(P_MODEL, 3);
    shared.set(P_DRIVE, 0.5);
    shared.set(P_CHARACTER, 0.55);
    shared.set(P_TONE, 0.5);
    shared.set(P_MIX, 1.0);
    shared.set(P_OUTPUT, 0.5);
    enterMultiband(shared);
    shared.set(P_XOVER1, 120.0);
    shared.set(P_XOVER2, 900.0);
    shared.set(P_XOVER3, 5000.0);
    shared.set(P_BAND, band);
    shared.set(P_METER, 0);
    const int models[4] = {4, 0, 2, 1};
    const double drives[4] = {0.58, 0.44, 0.38, 0.32};
    const double tones[4] = {0.36, 0.55, 0.58, 0.66};
    for (int b = 0; b < kBandCount; ++b) {
      shared.set(bandParam(b, BF_MODEL), models[b]);
      shared.set(bandParam(b, BF_DRIVE), drives[b]);
      shared.set(bandParam(b, BF_TONE), tones[b]);
      shared.set(bandParam(b, BF_CHARACTER), 0.5);
      shared.set(bandParam(b, BF_MIX), 1.0);
      shared.set(bandParam(b, BF_LEVEL), 0.5);
    }
    fillSpectrum(shared);
    shared.inPeakL.store(-9.0f);
    shared.inPeakR.store(-11.0f);
    shared.outPeakL.store(-14.0f);
    shared.outPeakR.store(-15.0f);
    shared.outVu.store(-16.0f);
    shared.bandLevel[0].store(-7.0f);
    shared.bandLevel[1].store(-12.0f);
    shared.bandLevel[2].store(-16.0f);
    shared.bandLevel[3].store(-22.0f);

    PanelState state;
    for (int i = 0; i < 20; ++i) panel.tick(shared, state);
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
    cairo_t* cr = cairo_create(surface);
    panel.draw(cr, shared, state);
    cairo_destroy(cr);
    const std::string path =
        outDir + "/patina-mb-" + bandNames[band] + ".png";
    const cairo_status_t st = cairo_surface_write_to_png(surface, path.c_str());
    cairo_surface_destroy(surface);
    if (st != CAIRO_STATUS_SUCCESS) {
      std::fprintf(stderr, "could not write %s\n", path.c_str());
      return 1;
    }
    std::printf("wrote %s\n", path.c_str());
  }

  // Scope view for the selected mid band.
  {
    Shared shared;
    shared.set(P_MODEL, 0);
    enterMultiband(shared);
    shared.set(P_XOVER1, 120.0);
    shared.set(P_XOVER2, 900.0);
    shared.set(P_XOVER3, 5000.0);
    shared.set(P_BAND, 1);
    shared.set(P_METER, 1);
    const int models[4] = {4, 0, 2, 1};
    const double drives[4] = {0.58, 0.44, 0.38, 0.32};
    for (int b = 0; b < kBandCount; ++b) {
      shared.set(bandParam(b, BF_MODEL), models[b]);
      shared.set(bandParam(b, BF_DRIVE), drives[b]);
      shared.set(bandParam(b, BF_CHARACTER), 0.55);
      shared.set(bandParam(b, BF_TONE), 0.5);
      shared.set(bandParam(b, BF_MIX), 1.0);
      shared.set(bandParam(b, BF_LEVEL), 0.5);
    }
    fillSpectrum(shared);
    shared.bandLevel[0].store(-8.0f);
    shared.bandLevel[1].store(-12.0f);
    shared.bandLevel[2].store(-16.0f);
    shared.bandLevel[3].store(-22.0f);
    PanelState state;
    for (int i = 0; i < 20; ++i) panel.tick(shared, state);
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
    cairo_t* cr = cairo_create(surface);
    panel.draw(cr, shared, state);
    cairo_destroy(cr);
    const std::string path = outDir + "/patina-mb-scope.png";
    if (cairo_surface_write_to_png(surface, path.c_str()) != CAIRO_STATUS_SUCCESS) {
      std::fprintf(stderr, "could not write %s\n", path.c_str());
      return 1;
    }
    cairo_surface_destroy(surface);
    std::printf("wrote %s\n", path.c_str());
  }

  // Dedicated spectrum view.
  {
    Shared shared;
    shared.set(P_MODEL, 0);
    enterMultiband(shared);
    shared.set(P_XOVER1, 120.0);
    shared.set(P_XOVER2, 900.0);
    shared.set(P_XOVER3, 5000.0);
    shared.set(P_BAND, 1);
    shared.set(P_METER, 2);
    const int models[4] = {4, 0, 2, 1};
    const double drives[4] = {0.58, 0.44, 0.38, 0.32};
    for (int b = 0; b < kBandCount; ++b) {
      shared.set(bandParam(b, BF_MODEL), models[b]);
      shared.set(bandParam(b, BF_DRIVE), drives[b]);
      shared.set(bandParam(b, BF_CHARACTER), 0.55);
      shared.set(bandParam(b, BF_TONE), 0.5);
      shared.set(bandParam(b, BF_MIX), 1.0);
      shared.set(bandParam(b, BF_LEVEL), 0.5);
    }
    fillSpectrum(shared);
    shared.bandLevel[0].store(-7.0f);
    shared.bandLevel[1].store(-12.0f);
    shared.bandLevel[2].store(-16.0f);
    shared.bandLevel[3].store(-22.0f);
    PanelState state;
    for (int i = 0; i < 20; ++i) panel.tick(shared, state);
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
    cairo_t* cr = cairo_create(surface);
    panel.draw(cr, shared, state);
    cairo_destroy(cr);
    const std::string path = outDir + "/patina-mb-spectrum.png";
    if (cairo_surface_write_to_png(surface, path.c_str()) != CAIRO_STATUS_SUCCESS) {
      std::fprintf(stderr, "could not write %s\n", path.c_str());
      return 1;
    }
    cairo_surface_destroy(surface);
    std::printf("wrote %s\n", path.c_str());
  }
  return 0;
}
