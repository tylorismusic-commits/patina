// Patina — panel preview
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cairo.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "params.hpp"
#include "ui/panel.hpp"

using namespace patina;

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

  // Multiband views: one per selected band.
  static const char* bandNames[] = {"low", "mid", "high"};
  for (int band = 0; band < 3; ++band) {
    Shared shared;
    shared.set(P_MODEL, 3);
    shared.set(P_DRIVE, 0.5);
    shared.set(P_CHARACTER, 0.55);
    shared.set(P_TONE, 0.5);
    shared.set(P_MIX, 1.0);
    shared.set(P_OUTPUT, 0.5);
    enterMultiband(shared);
    shared.set(P_XOVER1, 120.0);
    shared.set(P_XOVER2, 2800.0);
    shared.set(P_BAND, band);
    shared.set(P_METER, 0);
    const int models[3] = {4, 0, 1};
    const double drives[3] = {0.58, 0.44, 0.36};
    const double tones[3] = {0.36, 0.55, 0.66};
    for (int b = 0; b < 3; ++b) {
      shared.set(bandParam(b, BF_MODEL), models[b]);
      shared.set(bandParam(b, BF_DRIVE), drives[b]);
      shared.set(bandParam(b, BF_TONE), tones[b]);
      shared.set(bandParam(b, BF_CHARACTER), 0.5);
      shared.set(bandParam(b, BF_MIX), 1.0);
    }
    shared.inPeakL.store(-9.0f);
    shared.inPeakR.store(-11.0f);
    shared.outPeakL.store(-14.0f);
    shared.outPeakR.store(-15.0f);
    shared.outVu.store(-16.0f);
    shared.bandLevel[0].store(-7.0f);
    shared.bandLevel[1].store(-12.0f);
    shared.bandLevel[2].store(-18.0f);

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
    shared.set(P_XOVER2, 2800.0);
    shared.set(P_BAND, 1);
    shared.set(P_METER, 1);
    const int models[3] = {4, 0, 1};
    const double drives[3] = {0.58, 0.44, 0.36};
    for (int b = 0; b < 3; ++b) {
      shared.set(bandParam(b, BF_MODEL), models[b]);
      shared.set(bandParam(b, BF_DRIVE), drives[b]);
      shared.set(bandParam(b, BF_CHARACTER), 0.55);
      shared.set(bandParam(b, BF_TONE), 0.5);
      shared.set(bandParam(b, BF_MIX), 1.0);
    }
    shared.bandLevel[0].store(-8.0f);
    shared.bandLevel[1].store(-12.0f);
    shared.bandLevel[2].store(-20.0f);
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
  return 0;
}
