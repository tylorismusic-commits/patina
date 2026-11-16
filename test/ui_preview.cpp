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
  return 0;
}
