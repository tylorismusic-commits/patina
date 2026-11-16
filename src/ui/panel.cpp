// Patina — panel.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/panel.hpp"

#include "dsp/saturation.hpp"
#include "presets.hpp"

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace patina {

namespace {

struct Rgb {
  double r, g, b;
};

struct Skin {
  const char* caption;
  Rgb bodyTop, bodyBottom;
  Rgb legend, legendDim;
  Rgb headerBg;
  Rgb tabOff, tabOn, tabTextOff, tabTextOn;
  Rgb meterFrame, meterFace, meterNeedle, meterScale;
  Rgb meterBlock;
  bool hasBlueBlock;
  Rgb knobCap, knobCapEdge, knobSkirt, knobIndicator;
  bool knurled;
  Rgb btnOff, btnOn, btnText, btnTextOn;
  Rgb screw;
};

enum Model { kValve, kConsole, kBus, kTape, kTransformer };

Skin skinFor(int model) {
  switch (model) {
    case kValve:
      return {"Valve  -  tube saturation",
              {0.84, 0.80, 0.68}, {0.68, 0.61, 0.46},
              {0.11, 0.08, 0.05}, {0.36, 0.30, 0.22},
              {0.20, 0.14, 0.09},
              {0.38, 0.30, 0.19}, {0.74, 0.55, 0.25}, {0.90, 0.86, 0.76},
              {0.13, 0.10, 0.06},
              {0.24, 0.16, 0.09}, {0.92, 0.86, 0.70}, {0.46, 0.14, 0.10},
              {0.15, 0.10, 0.06},
              {0.42, 0.20, 0.08}, false,
              {0.78, 0.72, 0.58}, {0.43, 0.34, 0.22}, {0.20, 0.14, 0.09},
              {0.88, 0.84, 0.72},
              true,
              {0.22, 0.16, 0.10}, {0.92, 0.82, 0.56}, {0.94, 0.89, 0.76},
              {0.16, 0.10, 0.06},
              {0.42, 0.36, 0.26}};
    case kConsole:
      return {"Console  -  channel saturation",
              {0.22, 0.23, 0.25}, {0.12, 0.13, 0.15},
              {0.88, 0.90, 0.92}, {0.54, 0.58, 0.62},
              {0.08, 0.09, 0.10},
              {0.20, 0.22, 0.24}, {0.36, 0.39, 0.43}, {0.62, 0.68, 0.73},
              {0.95, 0.98, 1.0},
              {0.12, 0.13, 0.15}, {0.14, 0.16, 0.18}, {0.58, 0.90, 1.0},
              {0.66, 0.73, 0.78},
              {0.34, 0.38, 0.42}, false,
              {0.82, 0.84, 0.86}, {0.42, 0.45, 0.48}, {0.11, 0.12, 0.13},
              {0.05, 0.06, 0.07},
              false,
              {0.10, 0.11, 0.13}, {0.44, 0.48, 0.53}, {0.90, 0.92, 0.94},
              {0.05, 0.06, 0.07},
              {0.50, 0.54, 0.58}};
    case kBus:
      return {"Bus  -  stereo saturation",
              {0.12, 0.19, 0.29}, {0.06, 0.10, 0.17},
              {0.82, 0.89, 0.96}, {0.43, 0.55, 0.67},
              {0.04, 0.07, 0.11},
              {0.14, 0.22, 0.33}, {0.23, 0.37, 0.50}, {0.55, 0.67, 0.78},
              {0.92, 0.97, 1.0},
              {0.09, 0.14, 0.21}, {0.06, 0.09, 0.14}, {0.35, 0.78, 1.0},
              {0.62, 0.74, 0.84},
              {0.18, 0.36, 0.55}, false,
              {0.74, 0.81, 0.87}, {0.26, 0.40, 0.54}, {0.09, 0.14, 0.20},
              {0.04, 0.07, 0.11},
              false,
              {0.08, 0.13, 0.19}, {0.32, 0.58, 0.80}, {0.86, 0.91, 0.96},
              {0.05, 0.09, 0.13},
              {0.38, 0.47, 0.56}};
    case kTape:
      return {"Tape  -  tape saturation",
              {0.67, 0.57, 0.42}, {0.47, 0.37, 0.24},
              {0.14, 0.09, 0.05}, {0.38, 0.30, 0.19},
              {0.20, 0.12, 0.07},
              {0.37, 0.27, 0.16}, {0.59, 0.30, 0.20}, {0.82, 0.74, 0.62},
              {0.13, 0.08, 0.04},
              {0.24, 0.15, 0.09}, {0.87, 0.78, 0.61}, {0.55, 0.17, 0.12},
              {0.14, 0.09, 0.05},
              {0.55, 0.24, 0.14}, false,
              {0.70, 0.60, 0.44}, {0.34, 0.24, 0.14}, {0.15, 0.09, 0.05},
              {0.84, 0.76, 0.60},
              true,
              {0.18, 0.12, 0.07}, {0.94, 0.82, 0.58}, {0.90, 0.85, 0.72},
              {0.12, 0.07, 0.04},
              {0.40, 0.32, 0.21}};
    default:
      return {"Transformer  -  iron saturation",
              {0.13, 0.27, 0.25}, {0.07, 0.16, 0.15},
              {0.82, 0.92, 0.86}, {0.48, 0.64, 0.57},
              {0.04, 0.10, 0.09},
              {0.12, 0.25, 0.23}, {0.20, 0.42, 0.37}, {0.58, 0.75, 0.68},
              {0.92, 1.0, 0.94},
              {0.08, 0.18, 0.16}, {0.10, 0.22, 0.20}, {0.76, 0.48, 0.22},
              {0.62, 0.78, 0.70},
              {0.20, 0.44, 0.34}, false,
              {0.72, 0.84, 0.76}, {0.30, 0.50, 0.42}, {0.08, 0.17, 0.15},
              {0.04, 0.09, 0.08},
              false,
              {0.08, 0.17, 0.15}, {0.38, 0.66, 0.54}, {0.84, 0.94, 0.88},
              {0.05, 0.10, 0.09},
              {0.42, 0.56, 0.49}};
  }
}

template <size_t N>
std::vector<const char*> sc(const char* const (&labels)[N]) {
  return std::vector<const char*>(labels, labels + N);
}

const char* const kScaleDrive[] = {"0", "2", "4", "6", "8", "10"};
const char* const kScaleCharacter[] = {"SOFT", "", "", "", "HARD"};
const char* const kScaleTone[] = {"DARK", "", "", "", "BRIGHT"};
const char* const kScaleMix[] = {"DRY", "", "", "", "WET"};
const char* const kScaleOutput[] = {"-12", "0", "+12"};

constexpr double kPi = 3.14159265358979323846;

constexpr double kPresetX = 72.0;
constexpr double kPresetY = 14.0;
constexpr double kPresetW = 330.0;
constexpr double kPresetH = 36.0;
constexpr double kListY = 56.0;
constexpr double kListRow = 22.0;
constexpr double kListVisible = 17.0;

double knobAngle(double f) { return (135.0 + 270.0 * f) * kPi / 180.0; }
double clamp01(double v) { return std::min(std::max(v, 0.0), 1.0); }

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi / 2);
  cairo_arc(cr, x + r, y + h - r, r, kPi / 2, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, 3 * kPi / 2);
  cairo_close_path(cr);
}

void setRgb(cairo_t* cr, Rgb c, double a = 1.0) {
  cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

Rgb mix(Rgb a, Rgb b, double t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
          a.b + (b.b - a.b) * t};
}

const std::vector<int>& presetOrder() {
  static const std::vector<int> order = [] {
    std::vector<int> v;
    for (int i = 0; i < kPresetCount; ++i) v.push_back(i);
    std::stable_sort(v.begin(), v.end(), [](int a, int b) {
      const int c = std::strcmp(kPresets[a].category, kPresets[b].category);
      if (c != 0) return c < 0;
      return std::strcmp(kPresets[a].name, kPresets[b].name) < 0;
    });
    return v;
  }();
  return order;
}

const char* modelShort(int model) {
  static const char* const kShort[5] = {"VLV", "CNS", "BUS", "TAP", "XFM"};
  return kShort[std::clamp(model, 0, 4)];
}

} // namespace

Panel::Panel() {
  grain_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
  unsigned char* data = cairo_image_surface_get_data(grain_);
  const int stride = cairo_image_surface_get_stride(grain_);
  unsigned int seed = 22222u;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      seed = seed * 1103515245u + 12345u;
      const unsigned v = (seed >> 16) & 0xff;
      const unsigned a = (v < 128) ? 10u : 0u;
      // cairo ARGB32 is premultiplied; keep the colour channels <= alpha.
      const unsigned c = (v < 128) ? 0u : 255u;
      const unsigned p = (c * a + 127u) / 255u;
      unsigned char* px = data + y * stride + x * 4;
      px[0] = px[1] = px[2] = static_cast<unsigned char>(p);
      px[3] = static_cast<unsigned char>(a);
    }
  }
  cairo_surface_mark_dirty(grain_);
  fallbackFont_ = cairo_toy_font_face_create(
      "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  font_ = fallbackFont_;
}

Panel::~Panel() {
  if (grain_) cairo_surface_destroy(grain_);
  if (font_ && font_ != fallbackFont_) cairo_font_face_destroy(font_);
  if (fallbackFont_) cairo_font_face_destroy(fallbackFont_);
  if (ftFace_) FT_Done_Face(static_cast<FT_Face>(ftFace_));
  if (ft_) FT_Done_FreeType(static_cast<FT_Library>(ft_));
}

void Panel::loadFont(const std::string& dir) {
  if (ft_) return;
  std::string path = dir;
  if (!path.empty() && path.back() != '/') path += '/';
  path += "Jost-ExtraBold.ttf";
  FT_Library lib = nullptr;
  if (FT_Init_FreeType(&lib) != 0) return;
  FT_Face face = nullptr;
  if (FT_New_Face(lib, path.c_str(), 0, &face) != 0) {
    FT_Done_FreeType(lib);
    return;
  }
  cairo_font_face_t* font = cairo_ft_font_face_create_for_ft_face(face, 0);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    cairo_font_face_destroy(font);
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
    return;
  }
  ft_ = lib;
  ftFace_ = face;
  font_ = font;
}

void Panel::text(cairo_t* cr, const char* s, double x, double y, double size,
                 int align, double r, double g, double b) const {
  cairo_set_font_face(cr, font_);
  cairo_set_font_size(cr, size);
  cairo_set_source_rgb(cr, r, g, b);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  double tx = x;
  if (align == 1) tx = x - e.x_advance * 0.5;
  else if (align == 2) tx = x - e.x_advance;
  cairo_move_to(cr, tx, y);
  cairo_show_text(cr, s);
}

void Panel::layout(const Shared& shared, const PanelState& state) {
  widgets_.clear();
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);

  auto knob = [&](uint32_t param, double x, double y, double r,
                  const char* label, std::vector<const char*> scale,
                  bool below = false) {
    Widget k{};
    k.kind = Kind::Knob;
    k.param = param;
    k.x = x;
    k.y = y;
    k.r = r;
    k.label = label;
    k.scale = std::move(scale);
    k.labelBelow = below;
    widgets_.push_back(k);
  };

  {
    Widget p{};
    p.kind = Kind::Power;
    p.param = P_ENABLED;
    p.x = 32;
    p.y = 32;
    p.r = 12;
    widgets_.push_back(p);
  }

  static const char* kTabNames[5] = {"Valve", "Console", "Bus", "Tape",
                                     "Transformer"};
  const double tabW = (980.0 - 4 * 8.0) / 5.0;
  for (int i = 0; i < 5; ++i) {
    Widget t{};
    t.kind = Kind::ModelTab;
    t.param = P_MODEL;
    t.value = i;
    t.x = 30 + i * (tabW + 8.0);
    t.y = 70;
    t.w = tabW;
    t.h = 40;
    t.label = kTabNames[i];
    widgets_.push_back(t);
  }

  {
    static const char* kModes[3] = {"SHAPE", "SCOPE", "OFF"};
    for (int i = 0; i < 3; ++i) {
      Widget s{};
      s.kind = Kind::MeterSegment;
      s.param = P_METER;
      s.value = i;
      s.x = 190 + i * 92.0;
      s.y = 178;
      s.w = 86;
      s.h = 20;
      s.label = kModes[i];
      widgets_.push_back(s);
    }
  }

  knob(P_DRIVE, 300, 430, 40, "DRIVE", sc(kScaleDrive));
  knob(P_CHARACTER, 470, 430, 34, "CHARACTER", sc(kScaleCharacter));
  knob(P_TONE, 620, 430, 34, "TONE", sc(kScaleTone));
  knob(P_MIX, 796, 508, 25, "MIX", sc(kScaleMix), true);
  knob(P_OUTPUT, 912, 500, 27, "OUTPUT", sc(kScaleOutput));

  {
    Widget p{};
    p.kind = Kind::Pill;
    p.param = P_AUTO_GAIN;
    p.x = 736;
    p.y = 20;
    p.w = 84;
    p.h = 24;
    p.label = "AUTO GAIN";
    widgets_.push_back(p);
  }
  {
    Widget c{};
    c.kind = Kind::Cycle;
    c.param = P_OVERSAMPLE;
    c.x = 828;
    c.y = 20;
    c.w = 84;
    c.h = 24;
    widgets_.push_back(c);
  }
  (void)model;
  (void)state;
}

void Panel::draw(cairo_t* cr, Shared& shared, const PanelState& state) {
  layout(shared, state);
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.06, 0.06, 0.065);
  cairo_paint(cr);

  drawHeader(cr, shared, state);
  drawBody(cr, model);
  drawDisplay(cr, shared, state, model);
  drawLeds(cr, shared);
  drawLevelMeter(cr, 20, 150, 16, 352, state.inLevelL, state.inLevelR, -60.0,
                 0.0, model, false);
  drawLevelMeter(cr, 986, 150, 16, 352, state.outLevelL, state.outLevelR,
                 -60.0, 3.0, model, true);

  for (const Widget& w : widgets_) drawWidget(cr, w, shared, state, model);
  if (state.presetOpen) drawPresetList(cr, shared, state);
  text(cr, skinFor(model).caption, 520, 622, 13, 1, 0.80, 0.82, 0.84);
  cairo_restore(cr);
}

void Panel::drawHeader(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  cairo_pattern_t* bar = cairo_pattern_create_linear(0, 0, 0, 62);
  cairo_pattern_add_color_stop_rgb(bar, 0.0, skin.headerBg.r * 1.35,
                                   skin.headerBg.g * 1.35,
                                   skin.headerBg.b * 1.35);
  cairo_pattern_add_color_stop_rgb(bar, 1.0, skin.headerBg.r * 0.75,
                                   skin.headerBg.g * 0.75,
                                   skin.headerBg.b * 0.75);
  cairo_rectangle(cr, 0, 0, kPanelWidth, 62);
  cairo_set_source(cr, bar);
  cairo_fill(cr);
  cairo_pattern_destroy(bar);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, 0, 0.5);
  cairo_line_to(cr, kPanelWidth, 0.5);
  cairo_stroke(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
  cairo_move_to(cr, 0, 61.5);
  cairo_line_to(cr, kPanelWidth, 61.5);
  cairo_stroke(cr);
  text(cr, "PATINA", 520, 38, 19, 1, 0.82, 0.85, 0.84);
  drawPresetBar(cr, shared, state);
}

void Panel::drawBody(cairo_t* cr, int model) const {
  const Skin skin = skinFor(model);
  cairo_pattern_t* pat = cairo_pattern_create_linear(0, 122, 0, 604);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, skin.bodyTop.r, skin.bodyTop.g,
                                   skin.bodyTop.b);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, skin.bodyBottom.r,
                                   skin.bodyBottom.g, skin.bodyBottom.b);
  roundRect(cr, 16, 122, 1008, 482, 6);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);

  if (grain_) {
    cairo_pattern_t* grain = cairo_pattern_create_for_surface(grain_);
    cairo_pattern_set_extend(grain, CAIRO_EXTEND_REPEAT);
    cairo_save(cr);
    cairo_rectangle(cr, 17, 123, 1006, 480);
    cairo_clip(cr);
    cairo_set_source(cr, grain);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_pattern_destroy(grain);
  }

  setRgb(cr, mix(skin.bodyTop, {1, 1, 1}, 0.10), 0.6);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, 16.5, 122.5, 1007, 481, 6);
  cairo_stroke(cr);
  setRgb(cr, mix(skin.bodyBottom, {0, 0, 0}, 0.45), 0.6);
  roundRect(cr, 15.5, 121.5, 1009, 483, 6);
  cairo_stroke(cr);
}

void Panel::drawDisplay(cairo_t* cr, const Shared& shared,
                        const PanelState& state, int model) const {
  const Skin skin = skinFor(model);
  const double fx = 182, fy = 172, fw = 546, fh = 160;
  roundRect(cr, fx, fy, fw, fh, 3);
  cairo_pattern_t* face = cairo_pattern_create_linear(0, fy, 0, fy + fh);
  cairo_pattern_add_color_stop_rgb(face, 0.0, skin.meterFace.r,
                                   skin.meterFace.g, skin.meterFace.b);
  cairo_pattern_add_color_stop_rgb(face, 1.0, skin.meterFace.r * 0.88,
                                   skin.meterFace.g * 0.88,
                                   skin.meterFace.b * 0.88);
  cairo_set_source(cr, face);
  cairo_fill(cr);
  cairo_pattern_destroy(face);

  const int mode = std::clamp(static_cast<int>(shared.get(P_METER) + 0.5), 0, 2);
  setRgb(cr, skin.meterScale, 0.22);
  cairo_set_line_width(cr, 1.0);
  for (int i = 1; i < 8; ++i) {
    const double x = fx + fw * i / 8.0;
    cairo_move_to(cr, x, fy);
    cairo_line_to(cr, x, fy + fh);
    cairo_stroke(cr);
  }
  for (int i = 1; i < 4; ++i) {
    const double y = fy + fh * i / 4.0;
    cairo_move_to(cr, fx, y);
    cairo_line_to(cr, fx + fw, y);
    cairo_stroke(cr);
  }

  if (mode == 2) {
    text(cr, "OFF", fx + fw * 0.5, fy + fh * 0.5 + 8, 20, 1, skin.meterScale.r,
         skin.meterScale.g, skin.meterScale.b);
    return;
  }

  const double drive = shared.get(P_DRIVE);
  const double character = shared.get(P_CHARACTER);
  if (mode == 0) {
    // Transfer curve: input identity (dim) and the model's static shape.
    const double x0 = fx + 28, x1 = fx + fw - 28;
    const double y0 = fy + 22, y1 = fy + fh - 22;
    setRgb(cr, skin.meterScale, 0.55);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, x0, y1);
    cairo_line_to(cr, x1, y0);
    cairo_stroke(cr);

    setRgb(cr, skin.meterNeedle, 0.95);
    cairo_set_line_width(cr, 2.0);
    bool first = true;
    for (int i = 0; i <= 200; ++i) {
      const double t = i / 200.0;
      const double in = -1.0 + 2.0 * t;
      const double out =
          std::clamp(static_cast<double>(modelShape(model, drive, character,
                                                   static_cast<float>(in))),
                     -1.0, 1.0);
      const double px = x0 + (x1 - x0) * t;
      const double py = y0 + (y1 - y0) * (1.0 - (out + 1.0) * 0.5);
      if (first) {
        cairo_move_to(cr, px, py);
        first = false;
      } else {
        cairo_line_to(cr, px, py);
      }
    }
    cairo_stroke(cr);

    const double inDb = shared.inPeakL.load();
    const double in = clamp01((inDb + 60.0) / 60.0);
    const double pointIn = -1.0 + 2.0 * in;
    const double pointOut = std::clamp(
        static_cast<double>(modelShape(model, drive, character,
                                       static_cast<float>(pointIn))),
        -1.0, 1.0);
    const double px = x0 + (x1 - x0) * in;
    const double py = y0 + (y1 - y0) * (1.0 - (pointOut + 1.0) * 0.5);
    setRgb(cr, skin.meterNeedle);
    cairo_arc(cr, px, py, 3.5, 0, 2 * kPi);
    cairo_fill(cr);
    text(cr, "TRANSFER", fx + 8, fy + fh - 8, 11, 0, skin.meterScale.r,
         skin.meterScale.g, skin.meterScale.b);
    text(cr, "IN", fx + fw - 8, fy + fh - 8, 10, 2, skin.meterScale.r,
         skin.meterScale.g, skin.meterScale.b);
    (void)state;
  } else {
    // Scope: the shaped sine against the input.
    const double x0 = fx + 10, x1 = fx + fw - 10;
    const double mid = fy + fh * 0.5;
    setRgb(cr, skin.meterScale, 0.55);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, x0, mid);
    cairo_line_to(cr, x1, mid);
    cairo_stroke(cr);

    setRgb(cr, mix(skin.meterNeedle, {1, 1, 1}, 0.35), 0.35);
    cairo_set_line_width(cr, 1.2);
    for (int i = 0; i < 500; ++i) {
      const double t = i / 499.0;
      const double s = std::sin(2 * kPi * 3.0 * t) * 0.60;
      const double px = x0 + (x1 - x0) * t;
      const double py = mid - s * (fh * 0.42);
      if (i == 0) cairo_move_to(cr, px, py);
      else cairo_line_to(cr, px, py);
    }
    cairo_stroke(cr);

    setRgb(cr, skin.meterNeedle, 0.95);
    cairo_set_line_width(cr, 1.8);
    for (int i = 0; i < 500; ++i) {
      const double t = i / 499.0;
      const double s = std::sin(2 * kPi * 3.0 * t) * 0.60;
      const double y = modelShape(model, drive, character,
                                  static_cast<float>(s));
      const double px = x0 + (x1 - x0) * t;
      const double py = mid - std::clamp(y, -1.2, 1.2) * (fh * 0.42);
      if (i == 0) cairo_move_to(cr, px, py);
      else cairo_line_to(cr, px, py);
    }
    cairo_stroke(cr);
    text(cr, "WAVEFORM", fx + 8, fy + fh - 8, 11, 0, skin.meterScale.r,
         skin.meterScale.g, skin.meterScale.b);
  }
}

void Panel::drawLeds(cairo_t* cr, const Shared& shared) const {
  const float db = std::max(shared.inPeakL.load(), shared.inPeakR.load());
  const int lit = std::clamp(static_cast<int>(std::lround((db + 60.0f) / 5.0f)),
                             0, 12);
  for (int i = 0; i < 12; ++i) {
    const double y = 146 + i * 16;
    Rgb c;
    if (i < 4) c = {0.27, 0.88, 0.42};
    else if (i < 8) c = {0.96, 0.82, 0.29};
    else c = {1.0, 0.30, 0.24};
    roundRect(cr, 754, y, 16, 12, 2);
    if (i < lit) setRgb(cr, c);
    else setRgb(cr, mix(c, {0.05, 0.05, 0.05}, 0.78));
    cairo_fill(cr);
  }
}

void Panel::drawLevelMeter(cairo_t* cr, double x, double y, double w, double h,
                           double dbL, double dbR, double minDb, double maxDb,
                           int model, bool labelsRight) const {
  const Skin skin = skinFor(model);
  roundRect(cr, x, y, w, h, 2);
  setRgb(cr, skin.meterFrame);
  cairo_fill(cr);
  const double l = clamp01((dbL - minDb) / (maxDb - minDb));
  const double r = clamp01((dbR - minDb) / (maxDb - minDb));
  const double bh = (h - 10) * 0.5;
  for (int ch = 0; ch < 2; ++ch) {
    const double lv = ch == 0 ? l : r;
    const double by = y + 5 + ch * (bh + 2);
    roundRect(cr, x + 4, by, w - 8, bh, 1);
    setRgb(cr, mix(skin.meterFace, {0, 0, 0}, 0.55));
    cairo_fill(cr);
    const double fill = bh * lv;
    const Rgb colour = lv > 0.9 ? Rgb{1.0, 0.30, 0.24}
                               : (lv > 0.7 ? Rgb{0.96, 0.82, 0.29}
                                           : Rgb{0.27, 0.88, 0.42});
    setRgb(cr, colour, 0.85);
    cairo_rectangle(cr, x + 4, by + bh - fill, w - 8, fill);
    cairo_fill(cr);
    (void)ch;
  }
  const char* labels[] = {"0", "-6", "-12", "-24", "-36", "-48", "-60"};
  const double values[] = {0, -6, -12, -24, -36, -48, -60};
  for (int i = 0; i < 7; ++i) {
    const double fy = y + 6 + (h - 12) * (values[i] - maxDb) / (minDb - maxDb);
    if (labelsRight) {
      text(cr, labels[i], x + w + 7, fy + 4, 9, 0, skin.legendDim.r,
           skin.legendDim.g, skin.legendDim.b);
    } else {
      text(cr, labels[i], x - 7, fy + 4, 9, 2, skin.legendDim.r,
           skin.legendDim.g, skin.legendDim.b);
    }
  }
}

void Panel::drawWidget(cairo_t* cr, const Widget& w, const Shared& shared,
                       const PanelState& state, int model) const {
  const Skin skin = skinFor(model);
  switch (w.kind) {
    case Kind::Power: {
      const bool on = shared.get(P_ENABLED) > 0.5;
      cairo_arc(cr, w.x, w.y, w.r, 0, 2 * kPi);
      setRgb(cr, {0.10, 0.10, 0.11});
      cairo_fill(cr);
      cairo_arc(cr, w.x, w.y, w.r - 2, 0, 2 * kPi);
      setRgb(cr, on ? Rgb{0.36, 0.86, 0.46} : Rgb{0.42, 0.42, 0.42});
      cairo_set_line_width(cr, 2.2);
      cairo_stroke(cr);
      cairo_set_line_width(cr, 2.0);
      cairo_move_to(cr, w.x, w.y - 6);
      cairo_line_to(cr, w.x, w.y + 1);
      setRgb(cr, on ? Rgb{0.80, 0.95, 0.82} : Rgb{0.62, 0.62, 0.62});
      cairo_stroke(cr);
      break;
    }
    case Kind::Knob:
      drawKnob(cr, w, shared.get(w.param), model);
      break;
    case Kind::ModelTab: {
      const bool lit = std::fabs(shared.get(P_MODEL) - w.value) < 1e-3;
      roundRect(cr, w.x, w.y, w.w, w.h, 4);
      setRgb(cr, lit ? skin.tabOn : skin.tabOff);
      cairo_fill(cr);
      setRgb(cr, mix(lit ? skin.tabOn : skin.tabOff, {0, 0, 0}, 0.35), 0.7);
      cairo_set_line_width(cr, 1.0);
      roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 4);
      cairo_stroke(cr);
      const Rgb tc = lit ? skin.tabTextOn : skin.tabTextOff;
      text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 5, 13, 1, tc.r,
           tc.g, tc.b);
      break;
    }
    case Kind::MeterSegment: {
      const bool lit = std::fabs(shared.get(P_METER) - w.value) < 1e-3;
      roundRect(cr, w.x, w.y, w.w, w.h, 3);
      setRgb(cr, lit ? skin.tabOn : skin.tabOff);
      cairo_fill(cr);
      const Rgb tc = lit ? skin.tabTextOn : skin.tabTextOff;
      text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 4, 10, 1, tc.r,
           tc.g, tc.b);
      break;
    }
    case Kind::Pill:
      drawPill(cr, w, shared.get(w.param) > 0.5, model);
      break;
    case Kind::Cycle: {
      static const char* const kNames[3] = {"OS OFF", "OS 2X", "OS 4X"};
      const int v =
          std::clamp(static_cast<int>(shared.get(w.param) + 0.5), 0, 2);
      Widget copy = w;
      copy.label = kNames[v];
      drawPill(cr, copy, v != 0, model);
      break;
    }
  }
  (void)state;
}

void Panel::drawKnob(cairo_t* cr, const Widget& w, double value,
                     int model) const {
  const Skin skin = skinFor(model);
  const double cx = w.x, cy = w.y, r = w.r;

  setRgb(cr, skin.legendDim, 0.55);
  cairo_set_line_width(cr, 1.0);
  for (int i = 0; i <= 10; ++i) {
    const double a = knobAngle(i / 10.0);
    cairo_move_to(cr, cx + std::cos(a) * r * 1.10, cy + std::sin(a) * r * 1.10);
    cairo_line_to(cr, cx + std::cos(a) * r * 1.19, cy + std::sin(a) * r * 1.19);
    cairo_stroke(cr);
  }

  const size_t n = w.scale.size();
  for (size_t i = 0; i < n; ++i) {
    const double f = n > 1 ? static_cast<double>(i) / (n - 1) : 0.0;
    const double a = knobAngle(f);
    text(cr, w.scale[i], cx + std::cos(a) * (r * w.scaleRadius),
         cy + std::sin(a) * (r * w.scaleRadius) + 4, 10.5, 1, skin.legend.r,
         skin.legend.g, skin.legend.b);
  }

  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.18);
  cairo_arc(cr, cx + 0.5, cy + 1.5, r * 1.03, 0, 2 * kPi);
  cairo_fill(cr);

  cairo_pattern_t* body = cairo_pattern_create_linear(0, cy - r, 0, cy + r);
  const Rgb top = mix(skin.knobSkirt, {1, 1, 1}, 0.16);
  const Rgb bot = mix(skin.knobSkirt, {0, 0, 0}, 0.30);
  cairo_pattern_add_color_stop_rgb(body, 0.0, top.r, top.g, top.b);
  cairo_pattern_add_color_stop_rgb(body, 1.0, bot.r, bot.g, bot.b);
  cairo_arc(cr, cx, cy, r, 0, 2 * kPi);
  cairo_set_source(cr, body);
  cairo_fill(cr);
  cairo_pattern_destroy(body);
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
  cairo_arc(cr, cx, cy, r - 0.5, 0, 2 * kPi);
  cairo_stroke(cr);

  cairo_pattern_t* cap = cairo_pattern_create_radial(
      cx - r * 0.35, cy - r * 0.40, r * 0.05, cx, cy, r * 0.98);
  const Rgb hi = mix(skin.knobCap, {1, 1, 1}, 0.24);
  cairo_pattern_add_color_stop_rgb(cap, 0.0, hi.r, hi.g, hi.b);
  cairo_pattern_add_color_stop_rgb(cap, 0.75, skin.knobCap.r, skin.knobCap.g,
                                   skin.knobCap.b);
  cairo_pattern_add_color_stop_rgb(cap, 1.0, skin.knobCapEdge.r,
                                   skin.knobCapEdge.g, skin.knobCapEdge.b);
  cairo_arc(cr, cx, cy, r * 0.82, 0, 2 * kPi);
  cairo_set_source(cr, cap);
  cairo_fill(cr);
  cairo_pattern_destroy(cap);

  if (skin.knurled) {
    cairo_set_line_width(cr, 0.8);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.22);
    for (int k = 0; k < 60; ++k) {
      const double a = k * 2 * kPi / 60.0;
      cairo_move_to(cr, cx + std::cos(a) * r * 0.86,
                    cy + std::sin(a) * r * 0.86);
      cairo_line_to(cr, cx + std::cos(a) * r * 0.99,
                    cy + std::sin(a) * r * 0.99);
      cairo_stroke(cr);
    }
  }

  const ParamSpec& spec = kParamSpecs[w.param];
  const double vmax = w.steps > 1 ? w.steps - 1 : spec.max;
  const double span = vmax - spec.min;
  const double frac = span > 0.0 ? (std::min(value, vmax) - spec.min) / span : 0.0;
  const double a = knobAngle(clamp01(frac));
  cairo_set_line_width(cr, 2.0);
  setRgb(cr, skin.knobIndicator);
  cairo_move_to(cr, cx + std::cos(a) * r * 0.26, cy + std::sin(a) * r * 0.26);
  cairo_line_to(cr, cx + std::cos(a) * r * 0.78, cy + std::sin(a) * r * 0.78);
  cairo_stroke(cr);

  const double labelY = w.labelBelow ? cy + r * 1.44 + 20 : cy - r * 1.44 - 18;
  text(cr, w.label, cx, labelY, 11, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
}

void Panel::drawPill(cairo_t* cr, const Widget& w, bool on, int model) const {
  const Skin skin = skinFor(model);
  roundRect(cr, w.x, w.y, w.w, w.h, 5);
  setRgb(cr, on ? skin.btnOn : skin.btnOff);
  cairo_fill(cr);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.35);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1.0, w.h - 1.0, 5);
  cairo_stroke(cr);
  const Rgb tc = on ? skin.btnTextOn : skin.btnText;
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 3.5, 10, 1, tc.r,
       tc.g, tc.b);
}

void Panel::tick(const Shared& shared, PanelState& state) const {
  state.inLevelL = shared.inPeakL.load();
  state.inLevelR = shared.inPeakR.load();
  state.outLevelL = shared.outPeakL.load();
  state.outLevelR = shared.outPeakR.load();
  state.inLevel = std::max(state.inLevelL, state.inLevelR);
  state.outLevel = std::max(state.outLevelL, state.outLevelR);
  state.outPeakHold = std::max(state.outPeakHold - 0.2f, state.outLevel);
}

bool Panel::hitWidget(const Widget& w, double x, double y) const {
  switch (w.kind) {
    case Kind::Knob:
    case Kind::Power: {
      const double dx = x - w.x, dy = y - w.y;
      return dx * dx + dy * dy <= w.r * w.r * 1.3;
    }
    default:
      return x >= w.x && x <= w.x + w.w && y >= w.y && y <= w.y + w.h;
  }
}

double Panel::knobValue(const Shared& shared, const Widget& w) const {
  return shared.get(w.param);
}

bool Panel::isLit(const Shared& shared, const Widget& w) const {
  return std::fabs(shared.get(w.param) - w.value) < 1e-3;
}

bool Panel::pointerDown(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  if (handlePresetClick(shared, state, x, y)) return true;
  for (const Widget& w : widgets_) {
    if (!hitWidget(w, x, y)) continue;
    switch (w.kind) {
      case Kind::Knob:
        dragParam_ = static_cast<int>(w.param);
        dragSteps_ = w.steps;
        dragStartY_ = y;
        dragStartValue_ = shared.get(w.param);
        (void)fine;
        return false;
      case Kind::Power:
        shared.set(P_ENABLED, shared.get(P_ENABLED) > 0.5 ? 0.0 : 1.0);
        state.presetDirty = true;
        return true;
      case Kind::ModelTab:
      case Kind::MeterSegment:
        shared.set(w.param, w.value);
        state.presetDirty = true;
        return true;
      case Kind::Pill:
        shared.set(w.param, shared.get(w.param) > 0.5 ? 0.0 : 1.0);
        state.presetDirty = true;
        return true;
      case Kind::Cycle: {
        const int v = static_cast<int>(shared.get(w.param) + 0.5);
        shared.set(w.param, static_cast<double>((v + 1) % 3));
        state.presetDirty = true;
        return true;
      }
    }
  }
  return false;
}

bool Panel::pointerMove(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  (void)state;
  (void)x;
  if (dragParam_ < 0) return false;
  const uint32_t param = static_cast<uint32_t>(dragParam_);
  const ParamSpec& spec = kParamSpecs[param];
  const double perPixel = (spec.max - spec.min) / (fine ? 800.0 : 200.0);
  const double v = dragStartValue_ + (dragStartY_ - y) * perPixel;
  shared.set(param, v);
  state.presetDirty = true;
  return true;
}

void Panel::pointerUp() { dragParam_ = -1; }

bool Panel::wheel(Shared& shared, PanelState& state, double x, double y,
                  double dy) {
  if (state.presetOpen && x >= kPresetX && x <= kPresetX + kPresetW &&
      y >= kListY && y <= kListY + kListVisible * kListRow + 10) {
    int rows = kPresetCount;
    const char* last = nullptr;
    for (int i : presetOrder()) {
      if (!last || std::strcmp(last, kPresets[i].category) != 0) {
        last = kPresets[i].category;
        ++rows;
      }
    }
    const int maxScroll = std::max(0, rows - static_cast<int>(kListVisible));
    state.presetScroll = std::min(
        maxScroll, std::max(0, state.presetScroll - static_cast<int>(dy)));
    return true;
  }
  for (const Widget& w : widgets_) {
    if (w.kind == Kind::Knob && hitWidget(w, x, y)) {
      const ParamSpec& spec = kParamSpecs[w.param];
      if (spec.stepped) {
        shared.set(w.param, shared.get(w.param) - (dy > 0 ? 1.0 : -1.0));
      } else {
        shared.set(w.param,
                   shared.get(w.param) - dy * (spec.max - spec.min) * 0.01);
      }
      state.presetDirty = true;
      return true;
    }
  }
  return false;
}

void Panel::drawPresetBar(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  roundRect(cr, kPresetX, kPresetY, kPresetW, kPresetH, 5);
  setRgb(cr, mix(skin.headerBg, {1, 1, 1}, 0.12));
  cairo_fill(cr);
  setRgb(cr, mix(skin.headerBg, {0, 0, 0}, 0.35));
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kPresetX + 0.5, kPresetY + 0.5, kPresetW - 1, kPresetH - 1, 5);
  cairo_stroke(cr);

  text(cr, "<", kPresetX + 18, kPresetY + 23, 14, 1, skin.tabTextOn.r,
       skin.tabTextOn.g, skin.tabTextOn.b);
  text(cr, ">", kPresetX + kPresetW - 18, kPresetY + 23, 14, 1,
       skin.tabTextOn.r, skin.tabTextOn.g, skin.tabTextOn.b);
  char label[96];
  if (state.presetIndex >= 0 && state.presetIndex < kPresetCount) {
    std::snprintf(label, sizeof(label), "%s%s", kPresets[state.presetIndex].name,
                  state.presetDirty ? " *" : "");
  } else {
    std::snprintf(label, sizeof(label), "Init%s", state.presetDirty ? " *" : "");
  }
  text(cr, label, kPresetX + kPresetW * 0.5, kPresetY + 23, 13, 1,
       skin.tabTextOn.r, skin.tabTextOn.g, skin.tabTextOn.b);
}

void Panel::drawPresetList(cairo_t* cr, const Shared& shared,
                           const PanelState& state) const {
  const int model = static_cast<int>(shared.get(P_MODEL) + 0.5);
  const Skin skin = skinFor(model);
  const double h = kListVisible * kListRow + 10;
  roundRect(cr, kPresetX, kListY, kPresetW, h, 5);
  cairo_set_source_rgba(cr, 0.05, 0.06, 0.07, 0.97);
  cairo_fill(cr);
  setRgb(cr, skin.tabOn, 0.7);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kPresetX + 0.5, kListY + 0.5, kPresetW - 1, h - 1, 5);
  cairo_stroke(cr);

  cairo_save(cr);
  roundRect(cr, kPresetX + 1, kListY + 1, kPresetW - 2, h - 2, 4);
  cairo_clip(cr);

  const char* lastCategory = nullptr;
  double y = kListY + 6 - state.presetScroll * kListRow;
  for (int i : presetOrder()) {
    const Preset& p = kPresets[i];
    if (!lastCategory || std::strcmp(lastCategory, p.category) != 0) {
      lastCategory = p.category;
      text(cr, p.category, kPresetX + 14, y + 15, 11, 0, 0.52, 0.66, 0.80);
      y += kListRow;
      if (y > kListY + h) break;
    }
    if (y + kListRow > kListY && y < kListY + h) {
      if (i == state.presetIndex) {
        roundRect(cr, kPresetX + 6, y + 1, kPresetW - 12, kListRow - 2, 3);
        setRgb(cr, skin.tabOn, 0.85);
        cairo_fill(cr);
      }
      text(cr, p.name, kPresetX + 26, y + 15, 12.5, 0,
           i == state.presetIndex ? 1.0 : 0.86,
           i == state.presetIndex ? 1.0 : 0.88,
           i == state.presetIndex ? 1.0 : 0.90);
      text(cr, modelShort(p.model), kPresetX + kPresetW - 16, y + 15, 10.5, 2,
           0.56, 0.60, 0.66);
    }
    y += kListRow;
  }
  cairo_restore(cr);
}

void Panel::applyPreset(Shared& shared, PanelState& state, int index) const {
  if (index < 0 || index >= kPresetCount) return;
  const Preset& p = kPresets[index];
  shared.set(P_MODEL, p.model);
  for (const PresetValue& v : p.values) {
    if (v.param >= 0 && v.param < static_cast<int>(P_COUNT)) {
      shared.set(static_cast<uint32_t>(v.param), v.value);
    }
  }
  state.presetIndex = index;
  state.presetDirty = false;
  state.presetOpen = 0;
}

bool Panel::handlePresetClick(Shared& shared, PanelState& state, double x,
                              double y) {
  if (y >= kPresetY && y <= kPresetY + kPresetH) {
    if (x >= kPresetX && x <= kPresetX + 36) {
      const int next = state.presetIndex <= 0 ? 0 : state.presetIndex - 1;
      applyPreset(shared, state, next);
      return true;
    }
    if (x >= kPresetX + kPresetW - 36 && x <= kPresetX + kPresetW) {
      const int next = std::min(kPresetCount - 1, state.presetIndex + 1);
      applyPreset(shared, state, next);
      return true;
    }
    if (x > kPresetX + 36 && x < kPresetX + kPresetW - 36) {
      state.presetOpen = state.presetOpen ? 0 : 1;
      if (state.presetOpen) state.presetScroll = 0;
      return true;
    }
  }
  if (!state.presetOpen) return false;
  const double h = kListVisible * kListRow + 10;
  if (x < kPresetX || x > kPresetX + kPresetW || y < kListY || y > kListY + h) {
    state.presetOpen = 0;
    return true;
  }
  double rowY = kListY + 6 - state.presetScroll * kListRow;
  const char* lastCategory = nullptr;
  for (int i : presetOrder()) {
    if (!lastCategory || std::strcmp(lastCategory, kPresets[i].category) != 0) {
      lastCategory = kPresets[i].category;
      rowY += kListRow;
    }
    if (y >= rowY && y < rowY + kListRow) {
      applyPreset(shared, state, i);
      return true;
    }
    rowY += kListRow;
  }
  return true;
}

std::vector<Panel::WidgetRect> Panel::widgetRects() const {
  std::vector<WidgetRect> out;
  out.reserve(widgets_.size());
  for (const Widget& w : widgets_) {
    WidgetRect r;
    r.param = w.param;
    r.label = w.label;
    r.knob = w.kind == Kind::Knob;
    r.toggle = w.kind == Kind::Power;
    r.r = w.r;
    if (w.kind == Kind::Knob || w.kind == Kind::Power) {
      r.x = w.x - w.r;
      r.y = w.y - w.r;
      r.w = w.r * 2;
      r.h = w.r * 2;
    } else {
      r.x = w.x;
      r.y = w.y;
      r.w = w.w;
      r.h = w.h;
    }
    out.push_back(r);
  }
  return out;
}

} // namespace patina
