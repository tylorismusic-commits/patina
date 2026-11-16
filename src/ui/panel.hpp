// Patina — panel.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cairo.h>

#include <string>
#include <vector>

#include "params.hpp"

namespace patina {

constexpr int kPanelWidth = 1040;
constexpr int kPanelHeight = 640;

struct PanelState {
  PanelState() {
    for (int i = 0; i < kSpectrumBins; ++i) {
      spectrum[i] = -120.0f;
    }
  }

  int presetIndex = -1;
  bool presetDirty = false;
  int presetOpen = 0;
  int presetScroll = 0;
  float inLevel = 0.0f;
  float outLevel = 0.0f;
  float inLevelL = 0.0f;
  float inLevelR = 0.0f;
  float outLevelL = 0.0f;
  float outLevelR = 0.0f;
  float outPeakHold = 0.0f;
  float bandLevel[kBandCount] = {-120.0f, -120.0f, -120.0f, -120.0f};
  float spectrum[kSpectrumBins] = {};
};

class Panel {
public:
  Panel();
  ~Panel();
  Panel(const Panel&) = delete;
  Panel& operator=(const Panel&) = delete;

  void loadFont(const std::string& dir);
  void tick(const Shared& shared, PanelState& state) const;
  void draw(cairo_t* cr, Shared& shared, const PanelState& state);

  bool pointerDown(Shared& shared, PanelState& state, double x, double y,
                   bool fine);
  bool pointerMove(Shared& shared, PanelState& state, double x, double y,
                   bool fine);
  void pointerUp();
  bool handlePresetClick(Shared& shared, PanelState& state, double x, double y);
  void applyPreset(Shared& shared, PanelState& state, int index) const;
  void drawPresetBar(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void drawPresetList(cairo_t* cr, const Shared& shared,
                      const PanelState& state) const;
  bool wheel(Shared& shared, PanelState& state, double x, double y, double dy);
  bool dragging() const { return dragParam_ >= 0; }
  void markEdited(PanelState& state) const { state.presetDirty = true; }

  struct WidgetRect {
    uint32_t param = 0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double r = 0.0;
    const char* label = "";
    bool knob = false;
    bool toggle = false;
  };
  std::vector<WidgetRect> widgetRects() const;

private:
  enum class Kind {
    Knob,
    ModelTab,
    MeterSegment,
    Power,
    Pill,
    Cycle,
    BandTab,
    BandToggle,
    BandSolo,
    Crossover
  };

  struct Widget {
    Kind kind = Kind::Knob;
    uint32_t param = 0;
    double value = 0.0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double r = 0.0;
    int steps = 0;
    double scaleRadius = 1.44;
    const char* label = "";
    std::vector<const char*> scale;
    bool labelBelow = false;
    std::vector<const char*> buttonText;
  };

  void layout(const Shared& shared, const PanelState& state);
  void drawHeader(cairo_t* cr, const Shared& shared,
                  const PanelState& state) const;
  void drawBody(cairo_t* cr, int model) const;
  void drawDisplay(cairo_t* cr, const Shared& shared, const PanelState& state,
                   int model) const;
  void drawMultibandDisplay(cairo_t* cr, const Shared& shared,
                            const PanelState& state, int model, double fx,
                            double fy, double fw, double fh) const;
  void drawSpectrum(cairo_t* cr, const PanelState& state, int model, double x,
                    double y, double w, double h, double alpha,
                    bool freqLabels) const;
  void drawLeds(cairo_t* cr, const Shared& shared) const;
  void drawLevelMeter(cairo_t* cr, double x, double y, double w, double h,
                      double dbL, double dbR, double minDb, double maxDb,
                      int model, bool labelsRight) const;
  void drawWidget(cairo_t* cr, const Widget& w, const Shared& shared,
                  const PanelState& state, int model) const;
  void drawKnob(cairo_t* cr, const Widget& w, double value, int model) const;
  void drawPill(cairo_t* cr, const Widget& w, bool on, int model) const;
  void text(cairo_t* cr, const char* s, double x, double y, double size,
            int align, double r, double g, double b) const;
  double knobValue(const Shared& shared, const Widget& w) const;
  bool isLit(const Shared& shared, const Widget& w) const;
  bool hitWidget(const Widget& w, double x, double y) const;

  int selectedBand(const Shared& shared) const;
  int displayModel(const Shared& shared) const;

  std::vector<Widget> widgets_;
  int dragParam_ = -1;
  bool dragCrossover_ = false;
  double dragStartY_ = 0.0;
  double dragStartX_ = 0.0;
  double dragStartValue_ = 0.0;
  int dragSteps_ = 0;
  double displayX_ = 182.0;
  double displayW_ = 546.0;

  cairo_surface_t* grain_ = nullptr;
  void* ft_ = nullptr;
  void* ftFace_ = nullptr;
  cairo_font_face_t* font_ = nullptr;
  cairo_font_face_t* fallbackFont_ = nullptr;
};

} // namespace patina
