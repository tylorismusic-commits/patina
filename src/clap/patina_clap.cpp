// Patina — CLAP plugin
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/all.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo-xlib.h>
#include <dlfcn.h>
#include <sys/time.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <cstdarg>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dsp/saturation.hpp"
#include "params.hpp"
#include "version.hpp"
#include "ui/geometry.hpp"
#include "ui/panel.hpp"

namespace patina {
namespace {

// Set PATINA_DEBUG=1 to trace the editor thread to /tmp/patina-ui.log.
bool debugOn() {
  static const bool on = std::getenv("PATINA_DEBUG") != nullptr;
  return on;
}

void dbg(const char* fmt, ...) {
  if (!debugOn()) {
    return;
  }
  FILE* f = std::fopen("/tmp/patina-ui.log", "a");
  if (!f) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);
  std::fputc('\n', f);
  std::fclose(f);
}

constexpr const char* kPluginId = "com.tylorismusic.patina";
constexpr const char* kPluginName = "Patina";
constexpr const char* kVendor = "tylorismusic";

std::string bundleResourceDir() {
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(&bundleResourceDir), &info) && info.dli_fname) {
    std::string path = info.dli_fname;
    const size_t slash = path.rfind('/');
    if (slash != std::string::npos) {
      path = path.substr(0, slash);
    }
    return path + "/Patina.resources";
  }
  return "Patina.resources";
}

// ---------------------------------------------------------------------------
// X11 editor.  Owns the window and the Cairo surface, and runs its own
// thread so the host's audio and main threads are never blocked on drawing.
// ---------------------------------------------------------------------------

class Editor {
public:
  explicit Editor(Shared& shared) : shared_(shared) {}

  // Called when the user changes a control, so the host can ask for a
  // process call and pick up the new values.
  std::function<void()> requestFlush;

  double scale() const { return scale_.load(std::memory_order_relaxed); }
  UiSize size() const { return sizeForScale(scale()); }

  // Host thread: the editor thread clears the preset label after a state load
  // without touching PanelState from outside its thread.
  void resetUiState() {
    resetUi_.store(true, std::memory_order_relaxed);
  }

  // Host thread: record the request only.  All Xlib calls happen on the
  // editor thread, because Xlib is not thread-safe and calling
  // XInitThreads() after the host has used Xlib is undefined behaviour
  // (that made the editor fail to load in REAPER).
  void resize(double scale) {
    scale_.store(clampScale(scale), std::memory_order_relaxed);
  }

  bool open(void* parent) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (thread_.joinable()) {
      return opened_;
    }
    parentWindow_ =
        parent ? static_cast<Window>(reinterpret_cast<uintptr_t>(parent)) : 0;
    opened_ = false;
    openFailed_ = false;
    quitting_ = false;
    thread_ = std::thread([this] { run(); });
    cv_.wait_for(lock, std::chrono::seconds(3),
                 [this] { return opened_ || openFailed_; });
    return opened_;
  }

  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      quitting_ = true;
    }
    if (thread_.joinable()) {
      thread_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    opened_ = false;
  }

  ~Editor() { close(); }

private:
  void run() {
    dbg("thread start");
    Display* dpy = XOpenDisplay(nullptr);
    dbg("display=%p", static_cast<void*>(dpy));
    if (!dpy) {
      std::lock_guard<std::mutex> lock(mutex_);
      openFailed_ = true;
      cv_.notify_all();
      return;
    }
    const int screen = DefaultScreen(dpy);
    const Window parent = parentWindow_ ? parentWindow_ : RootWindow(dpy, screen);
    UiSize current = size();

    XSetWindowAttributes attrs{};
    // Button1MotionMask is essential: PointerMotionMask only reports motion
    // with no button held, so dragging a knob would never deliver events.
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | Button1MotionMask | ButtonMotionMask |
                       KeyPressMask;
    const Window win =
        XCreateWindow(dpy, parent, 0, 0, current.width, current.height, 0,
                      CopyFromParent, InputOutput, CopyFromParent, CWEventMask,
                      &attrs);

    const UiSize minSize = sizeForScale(kMinUiScale);
    const UiSize maxSize = sizeForScale(kMaxUiScale);
    XSizeHints hints{};
    hints.flags = PAspect | PMinSize | PMaxSize;
    hints.min_aspect.x = hints.max_aspect.x = kPanelWidth;
    hints.min_aspect.y = hints.max_aspect.y = kPanelHeight;
    hints.min_width = static_cast<int>(minSize.width);
    hints.min_height = static_cast<int>(minSize.height);
    hints.max_width = static_cast<int>(maxSize.width);
    hints.max_height = static_cast<int>(maxSize.height);
    XSetWMNormalHints(dpy, win, &hints);
    XStoreName(dpy, win, kPluginName);
    XMapRaised(dpy, win);
    XFlush(dpy);

    panel_.loadFont(bundleResourceDir());
    {
      std::lock_guard<std::mutex> lock(mutex_);
      opened_ = true;
      cv_.notify_all();
    }

    // Use the window's own visual and depth.  Hosts often give the plugin a
    // parent with a non-default visual (ARGB), and cairo refuses to draw on a
    // surface whose visual does not match the drawable.
    XWindowAttributes wa{};
    XGetWindowAttributes(dpy, win, &wa);
    dbg("window=%lu depth=%d visual=%p size=%dx%d", win, wa.depth,
        static_cast<void*>(wa.visual), current.width, current.height);
    cairo_surface_t* windowSurface =
        cairo_xlib_surface_create(dpy, win, wa.visual, current.width, current.height);
    dbg("xlib surface status=%s",
        cairo_status_to_string(cairo_surface_status(windowSurface)));
    const cairo_format_t imageFormat =
        wa.depth == 32 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    cairo_surface_t* backing = nullptr;
    cairo_t* cr = nullptr;

    auto rebuildBacking = [&]() {
      if (cr) {
        cairo_destroy(cr);
        cr = nullptr;
      }
      if (backing) {
        cairo_surface_destroy(backing);
        backing = nullptr;
      }
      backing = cairo_image_surface_create(imageFormat, current.width,
                                           current.height);
      cr = cairo_create(backing);
    };
    rebuildBacking();

    while (true) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (quitting_) {
          break;
        }
      }
      const UiSize want = size();
      if (want.width != current.width || want.height != current.height) {
        current = want;
        XResizeWindow(dpy, win, current.width, current.height);
        cairo_xlib_surface_set_size(windowSurface, current.width, current.height);
        rebuildBacking();
      }
      while (XPending(dpy) > 0) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        handleEvent(ev);
      }
      if (resetUi_.exchange(false, std::memory_order_relaxed)) {
        state_.presetIndex = -1;
        state_.presetDirty = false;
        state_.presetOpen = 0;
        state_.presetScroll = 0;
        state_.inLevelL = state_.inLevelR = -120.0f;
        state_.outLevelL = state_.outLevelR = -120.0f;
        for (int b = 0; b < kBandCount; ++b) {
          state_.bandLevel[b] = -120.0f;
        }
        for (int i = 0; i < kSpectrumBins; ++i) {
          state_.spectrum[i] = -120.0f;
        }
      }
      panel_.tick(shared_, state_);
      cairo_save(cr);
      cairo_scale(cr, scale(), scale());
      panel_.draw(cr, shared_, state_);
      cairo_restore(cr);
      cairo_surface_flush(backing);
      cairo_t* xcr = cairo_create(windowSurface);
      cairo_set_source_surface(xcr, backing, 0, 0);
      cairo_paint(xcr);
      cairo_destroy(xcr);
      // Required: cairo's Xlib backend buffers the composite, and without
      // this flush nothing reaches the server (the window stays black).
      cairo_surface_flush(windowSurface);
      XFlush(dpy);
      static int frames = 0;
      if (debugOn() && (frames++ % 120) == 0) {
        XWindowAttributes ga;
        XGetWindowAttributes(dpy, win, &ga);
        dbg("frame %d geom=%dx%d mapped=%d", frames, ga.width, ga.height,
            ga.map_state == IsViewable);
      }
      if (requestFlush && shared_.anyDirty()) {
        requestFlush();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    if (cr) {
      cairo_destroy(cr);
    }
    if (backing) {
      cairo_surface_destroy(backing);
    }
    if (windowSurface) {
      cairo_surface_destroy(windowSurface);
    }
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
  }

  // Event coordinates are physical pixels; the panel works in logical units.
  void handleEvent(const XEvent& ev) {
    const double s = scale();
    switch (ev.type) {
      case ButtonPress: {
        const double x = ev.xbutton.x / s;
        const double y = ev.xbutton.y / s;
        if (ev.xbutton.button == Button4) {
          panel_.wheel(shared_, state_, x, y, 1.0);
        } else if (ev.xbutton.button == Button5) {
          panel_.wheel(shared_, state_, x, y, -1.0);
        } else if (ev.xbutton.button == Button1) {
          panel_.pointerDown(shared_, state_, x, y);
        }
        break;
      }
      case ButtonRelease:
        panel_.pointerUp();
        break;
      case MotionNotify: {
        const bool fine = (ev.xmotion.state & ShiftMask) != 0;
        panel_.pointerMove(shared_, state_, ev.xmotion.x / s, ev.xmotion.y / s,
                           fine);
        break;
      }
      default:
        break;
    }
  }

  Shared& shared_;
  PanelState state_;
  Panel panel_;
  std::atomic<double> scale_{1.0};
  std::atomic<bool> resetUi_{false};
  std::mutex mutex_;
  std::condition_variable cv_;
  Window parentWindow_ = 0;
  bool opened_ = false;
  bool openFailed_ = false;
  bool quitting_ = false;
  std::thread thread_;
};

// ---------------------------------------------------------------------------
// Plugin
// ---------------------------------------------------------------------------

struct Plugin {
  clap_plugin_t plugin{};
  const clap_host_t* host = nullptr;
  Shared shared;
  SaturationEngine engine;
  Editor editor{shared};
  double sampleRate = 48000.0;
  Meters meters;
  std::atomic<int> reportedOversample{-1};
  std::atomic<uint32_t> reportedLatency{0xffffffffu};
  std::atomic<bool> rescanQueued{false};
  std::atomic<bool> active{false};
  std::atomic<bool> resetEngine{false};
};

const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
                                 CLAP_PLUGIN_FEATURE_DISTORTION,
                                 CLAP_PLUGIN_FEATURE_STEREO, nullptr};

const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION,
    kPluginId,
    kPluginName,
    kVendor,
    "https://github.com/tylorismusic-commits/patina",
    "",
    "",
    kVersion,
    "Five saturation models in one plugin: Valve, Console, Bus, Tape and Transformer.",
    kFeatures,
};

void writeName(char* dst, size_t size, const char* src) {
  std::strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

Plugin* self(const clap_plugin_t* p) { return static_cast<Plugin*>(p->plugin_data); }

// --- Parameters -------------------------------------------------------------

uint32_t paramsCount(const clap_plugin_t*) { return P_COUNT; }

bool paramsInfo(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
  if (index >= P_COUNT) {
    return false;
  }
  const ParamSpec& s = kParamSpecs[index];
  std::memset(info, 0, sizeof(*info));
  info->id = index;
  info->flags = CLAP_PARAM_IS_AUTOMATABLE;
  if (s.stepped) {
    info->flags |= CLAP_PARAM_IS_STEPPED;
  }
  if (s.hidden) {
    info->flags |= CLAP_PARAM_IS_HIDDEN;
  }
  // Power is a normal parameter.  It must NOT be flagged CLAP_PARAM_IS_BYPASS:
  // hosts then treat it with inverted semantics (0 = not bypassed) and write 0
  // at load, which would switch saturation off.
  writeName(info->name, sizeof(info->name), s.name);
  writeName(info->module, sizeof(info->module), s.module);
  info->min_value = s.min;
  info->max_value = s.max;
  info->default_value = s.def;
  return true;
}

bool paramsValue(const clap_plugin_t* p, clap_id id, double* value) {
  if (id >= P_COUNT) {
    return false;
  }
  *value = self(p)->shared.get(id);
  return true;
}

bool paramsValueToText(const clap_plugin_t* p, clap_id id, double value,
                       char* out, uint32_t size) {
  const int count = bandCount(self(p)->shared);
  switch (id) {
    case P_MODEL:
      std::snprintf(
          out, size, "%s",
          kModelLabels[std::clamp(static_cast<int>(value + 0.5), 0,
                                  static_cast<int>(Model::Count) - 1)]);
      return true;
    case P_ENABLED:
    case P_AUTO_GAIN:
    case P_MB:
      std::snprintf(out, size, "%s", value > 0.5 ? "On" : "Off");
      return true;
    case P_DRIVE:
    case P_OUTPUT:
      std::snprintf(out, size, "%+.1f dB", -12.0 + 24.0 * value);
      return true;
    case P_CHARACTER:
    case P_TONE:
    case P_MIX:
      std::snprintf(out, size, "%.0f%%", value * 100.0);
      return true;
    case P_XOVER1:
    case P_XOVER2:
    case P_XOVER3:
      std::snprintf(out, size, value >= 1000.0 ? "%.1f kHz" : "%.0f Hz",
                    value >= 1000.0 ? value / 1000.0 : value);
      return true;
    case P_BANDS:
      std::snprintf(
          out, size, "%s",
          kBandCountLabels[std::clamp(static_cast<int>(value + 0.5), 2, 4) -
                           2]);
      return true;
    case P_BAND: {
      int index = activeBandIndex(
          count, std::clamp(static_cast<int>(value + 0.5), 0, 3));
      if (index < 0) {
        index = 0;
      }
      std::snprintf(out, size, "%s", bandLabelForCount(count, index));
      return true;
    }
    case P_METER:
      std::snprintf(
          out, size, "%s",
          kMeterLabels[std::clamp(static_cast<int>(value + 0.5), 0, 3)]);
      return true;
    case P_OVERSAMPLE:
      std::snprintf(
          out, size, "%s",
          kOversampleLabels[std::clamp(static_cast<int>(value + 0.5), 0, 2)]);
      return true;
    default:
      break;
  }
  if (id >= P_B0_MODEL && id < P_BANDS) {
    const uint32_t offset = id - P_B0_MODEL;
    const BandField field = static_cast<BandField>(offset % BF_COUNT);
    switch (field) {
      case BF_MODEL:
        std::snprintf(
            out, size, "%s",
            kModelLabels[std::clamp(static_cast<int>(value + 0.5), 0,
                                    static_cast<int>(Model::Count) - 1)]);
        return true;
      case BF_INPUT:
        std::snprintf(out, size, "%+.1f dB", -12.0 + 24.0 * value);
        return true;
      case BF_DRIVE:
        std::snprintf(out, size, "%+.1f dB", -12.0 + 24.0 * value);
        return true;
      case BF_CHARACTER:
      case BF_TONE:
      case BF_MIX:
        std::snprintf(out, size, "%.0f%%", value * 100.0);
        return true;
      case BF_LEVEL:
        if (value <= 0.002) {
          std::snprintf(out, size, "Off");
        } else {
          const double db = value <= 0.5 ? -60.0 + 120.0 * value
                                        : 24.0 * (value - 0.5);
          std::snprintf(out, size, "%+.1f dB", db);
        }
        return true;
      case BF_ON:
        std::snprintf(out, size, "%s", value > 0.5 ? "On" : "Off");
        return true;
      default:
        break;
    }
  }
  std::snprintf(out, size, "%.2f", value);
  return true;
}

bool paramsTextToValue(const clap_plugin_t* p, clap_id id, const char* text,
                       double* value) {
  if (!text) {
    return false;
  }
  auto matchList = [&](const char* const* list, int n, double* out) {
    for (int i = 0; i < n; ++i) {
      if (strcasecmp(text, list[i]) == 0) {
        *out = i;
        return true;
      }
    }
    return false;
  };
  auto hasDb = [&]() {
    for (const char* c = text; *c; ++c) {
      if ((c[0] == 'd' || c[0] == 'D') &&
          (c[1] == 'b' || c[1] == 'B')) {
        return true;
      }
    }
    return false;
  };
  auto parseDb = [&](double* out) {
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text) {
      return false;
    }
    *out = hasDb() ? (v + 12.0) / 24.0 : (v > 1.0 ? (v + 12.0) / 24.0 : v);
    return true;
  };
  auto parsePercent = [&](double* out) {
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text) {
      return false;
    }
    *out = (std::strchr(text, '%') != nullptr || v > 1.0) ? v / 100.0 : v;
    return true;
  };
  auto parseHz = [&](double* out) {
    char* end = nullptr;
    double v = std::strtod(text, &end);
    if (end == text) {
      return false;
    }
    while (*end == ' ' || *end == '\t') {
      ++end;
    }
    if (*end == 'k' || *end == 'K') {
      v *= 1000.0;
    }
    *out = v;
    return true;
  };
  auto parseLevel = [&](double* out) {
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text) {
      return false;
    }
    if (!hasDb() && v >= 0.0 && v <= 1.0) {
      *out = v;
    } else if (v <= -60.0) {
      *out = 0.0;
    } else if (v <= 0.0) {
      *out = (v + 60.0) / 120.0;
    } else {
      *out = 0.5 + v / 24.0;
    }
    return true;
  };

  double v = 0.0;
  switch (id) {
    case P_MODEL:
      if (matchList(kModelLabels, static_cast<int>(Model::Count), &v)) {
        *value = v;
        return true;
      }
      break;
    case P_ENABLED:
    case P_AUTO_GAIN:
    case P_MB:
      if (strcasecmp(text, "On") == 0) {
        *value = 1.0;
        return true;
      }
      if (strcasecmp(text, "Off") == 0) {
        *value = 0.0;
        return true;
      }
      break;
    case P_DRIVE:
    case P_OUTPUT:
      if (parseDb(value)) {
        return true;
      }
      break;
    case P_CHARACTER:
    case P_TONE:
    case P_MIX:
      if (parsePercent(value)) {
        return true;
      }
      break;
    case P_XOVER1:
    case P_XOVER2:
    case P_XOVER3:
      if (parseHz(value)) {
        return true;
      }
      break;
    case P_METER:
      if (matchList(kMeterLabels, 4, &v)) {
        *value = v;
        return true;
      }
      break;
    case P_OVERSAMPLE:
      if (matchList(kOversampleLabels, 3, &v)) {
        *value = v;
        return true;
      }
      break;
    case P_BAND:
      if (matchList(kBandLabels, 4, &v)) {
        *value = v;
        return true;
      }
      if (strcasecmp(text, "Mid") == 0) {
        *value = activeBandSlot(bandCount(self(p)->shared), 1);
        return true;
      }
      break;
    case P_BANDS:
      if (matchList(kBandCountLabels, 3, &v)) {
        *value = v + 2.0;
        return true;
      }
      break;
    default:
      break;
  }

  if (id >= P_B0_MODEL && id < P_BANDS) {
    const uint32_t offset = id - P_B0_MODEL;
    const BandField field = static_cast<BandField>(offset % BF_COUNT);
    if (field == BF_MODEL &&
        matchList(kModelLabels, static_cast<int>(Model::Count), &v)) {
      *value = v;
      return true;
    }
    if (field == BF_ON) {
      if (strcasecmp(text, "On") == 0) {
        *value = 1.0;
        return true;
      }
      if (strcasecmp(text, "Off") == 0) {
        *value = 0.0;
        return true;
      }
    }
    if (field == BF_INPUT || field == BF_DRIVE) {
      if (parseDb(value)) {
        return true;
      }
    }
    if (field == BF_CHARACTER || field == BF_TONE || field == BF_MIX) {
      if (parsePercent(value)) {
        return true;
      }
    }
    if (field == BF_LEVEL) {
      if (strcasecmp(text, "Off") == 0) {
        *value = 0.0;
        return true;
      }
      if (parseLevel(value)) {
        return true;
      }
    }
  }

  char* end = nullptr;
  v = std::strtod(text, &end);
  if (end == text) {
    return false;
  }
  *value = v;
  return true;
}

void applyEvents(Plugin* s, const clap_input_events_t* in) {
  const uint32_t count = in->size(in);
  for (uint32_t i = 0; i < count; ++i) {
    const clap_event_header_t* hdr = in->get(in, i);
    if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) {
      continue;
    }
    if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
      const auto* pv = reinterpret_cast<const clap_event_param_value_t*>(hdr);
      // A pending UI change wins over the host's cached value: hosts that
      // resend their whole parameter set each block would otherwise undo a
      // preset (or a knob move) before we could send it back.
      if (pv->param_id < P_COUNT && !s->shared.isDirty(pv->param_id)) {
        s->shared.setFromHost(pv->param_id, pv->value);
      }
    }
  }
  // Keep host-driven band counts, selections and crossovers in a valid
  // arrangement.  This uses setFromHost, so it does not echo back as a UI
  // change.
  normalizeBandSelection(s->shared);
  normalizeCrossovers(s->shared);
}

// Sends the values the editor changed back to the host, so they can be
// automated and recorded.
void emitDirty(Plugin* s, const clap_output_events_t* out) {
  if (!out) {
    return;
  }
  for (uint32_t p = 0; p < P_COUNT; ++p) {
    if (!s->shared.takeDirty(p)) {
      continue;
    }
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.flags = 0;
    ev.param_id = p;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = s->shared.get(p);
    out->try_push(out, &ev.header);
  }
}

// Queue a main-thread callback if the oversampling mode changed.  Safe on the
// audio thread: it only sets a flag and asks the host for a callback.
void noteHostChanges(Plugin* s) {
  const int os = static_cast<int>(s->shared.get(P_OVERSAMPLE) + 0.5);
  if (os == s->reportedOversample.load(std::memory_order_relaxed)) {
    return;
  }
  if (!s->rescanQueued.exchange(true, std::memory_order_relaxed)) {
    if (s->host && s->host->request_callback) {
      s->host->request_callback(s->host);
    }
  }
}

void paramsFlush(const clap_plugin_t* p, const clap_input_events_t* in,
                 const clap_output_events_t* out) {
  Plugin* s = self(p);
  if (in) {
    applyEvents(s, in);
  }
  emitDirty(s, out);
  noteHostChanges(s);
}

const clap_plugin_params_t kParamsExt = {
    paramsCount, paramsInfo, paramsValue, paramsValueToText, paramsTextToValue,
    paramsFlush,
};

// --- State ------------------------------------------------------------------

constexpr uint32_t kStateMagic = 0x50544E41u; // "PTNA"
constexpr uint32_t kStateVersion = 4;

bool stateSave(const clap_plugin_t* p, const clap_ostream_t* stream) {
  Plugin* s = self(p);
  const uint32_t header[4] = {kStateMagic, kStateVersion, P_COUNT, 0};
  double values[P_COUNT];
  for (uint32_t i = 0; i < P_COUNT; ++i) {
    values[i] = s->shared.get(i);
  }
  const double scale = s->editor.scale();
  const int64_t writtenHeader = stream->write(stream, header, sizeof(header));
  const int64_t written = stream->write(stream, values, sizeof(values));
  const int64_t writtenScale = stream->write(stream, &scale, sizeof(scale));
  return writtenHeader == static_cast<int64_t>(sizeof(header)) &&
         written == static_cast<int64_t>(sizeof(values)) &&
         writtenScale == static_cast<int64_t>(sizeof(scale));
}

bool stateLoad(const clap_plugin_t* p, const clap_istream_t* stream) {
  Plugin* s = self(p);
  uint32_t header[4] = {};
  if (stream->read(stream, header, sizeof(header)) !=
      static_cast<int64_t>(sizeof(header))) {
    return false;
  }

  auto migrateOldMeter = [](double v) {
    const int m = std::clamp(static_cast<int>(v + 0.5), 0, 2);
    return m == 2 ? 3.0 : static_cast<double>(m);
  };
  // 0.2.0's per-band output ran -12..+12 dB; the current Level runs
  // silence..0..+12.  Preserve the old dB value for -12..+12.
  auto migrateOldOutput = [](double v) {
    return v <= 0.5 ? 0.4 + 0.2 * v : v;
  };

  // Patina 0.2.0 had three bands with the band block one id earlier.  Map its
  // Low/Mid/High slots to the current Low/Lo-Mid/High slots and select the
  // matching 3-band layout.
  auto applyThreeBand = [&](const double* values) {
    for (uint32_t i = 0; i < 13; ++i) {
      s->shared.setFromHost(i, values[i]);
    }
    s->shared.setFromHost(P_METER, migrateOldMeter(values[P_METER]));
    const double oldHigh = values[P_XOVER2];
    const int oldBand =
        std::clamp(static_cast<int>(values[13] + 0.5), 0, 2);
    s->shared.setFromHost(P_BAND, oldBand == 0 ? 0 : (oldBand == 1 ? 1 : 3));
    for (int band = 0; band < 3; ++band) {
      const int slot = band == 2 ? 3 : band;
      for (uint32_t field = 0; field < BF_COUNT; ++field) {
        double v = values[14 + band * BF_COUNT + field];
        if (field == BF_LEVEL) {
          v = migrateOldOutput(v);
        }
        s->shared.setFromHost(
            bandParam(slot, static_cast<BandField>(field)), v);
      }
    }
    s->shared.setFromHost(P_BANDS, 3.0);
    // The old three-band split was xover1/xover2.  The current 3-band layout
    // uses xover1/xover3, so move the old high split to Air and choose a
    // valid 4-band middle split for later.
    s->shared.setFromHost(P_XOVER3, oldHigh);
    double mid = std::min(kParamSpecs[P_XOVER2].def, oldHigh - 100.0);
    mid = std::min(std::max(mid, kParamSpecs[P_XOVER2].min),
                   kParamSpecs[P_XOVER2].max);
    s->shared.setFromHost(P_XOVER2, mid);
  };

  double scale = 0.0;
  bool haveScale = false;
  bool hasBandData = true;
  if (header[0] == kStateMagic) {
    if (header[1] < 2 || header[1] > kStateVersion || header[2] > 64) {
      return false;
    }
    double values[64] = {};
    const uint32_t count = header[2];
    if (stream->read(stream, values, count * sizeof(double)) !=
        static_cast<int64_t>(count * sizeof(double))) {
      return false;
    }
    hasBandData = count > P_MB;
    if (header[1] == 2 && count == 38) {
      applyThreeBand(values);
    } else {
      for (uint32_t i = 0; i < count && i < P_COUNT; ++i) {
        s->shared.setFromHost(i, values[i]);
      }
      if (header[1] == 2 && count == 47) {
        // The old display list was Shape/Scope/Off; Spectrum was added later.
        s->shared.setFromHost(P_METER, migrateOldMeter(values[P_METER]));
      }
      if (count < P_COUNT) {
        // 0.3.0 did not have a band-count parameter.
        s->shared.setFromHost(P_BANDS, kParamSpecs[P_BANDS].def);
      }
    }
    haveScale =
        stream->read(stream, &scale, sizeof(scale)) ==
        static_cast<int64_t>(sizeof(scale));
  } else {
    // Pre-header states: 0.1.0 wrote ten doubles + scale, 0.2.0 wrote
    // thirty-eight doubles + scale.  Read to the end so the size identifies
    // the format.
    std::vector<unsigned char> rest;
    unsigned char chunk[512];
    while (true) {
      const int64_t n = stream->read(stream, chunk, sizeof(chunk));
      if (n <= 0) {
        break;
      }
      rest.insert(rest.end(), chunk, chunk + n);
    }
    const size_t total = sizeof(header) + rest.size();
    if (total == 88) {
      // 10 values + scale.
      double values[10] = {};
      std::memcpy(values, header, sizeof(header));
      std::memcpy(values + 2, rest.data(), 8 * sizeof(double));
      std::memcpy(&scale, rest.data() + 8 * sizeof(double), sizeof(scale));
      haveScale = true;
      for (uint32_t i = 0; i < 10; ++i) {
        s->shared.setFromHost(i, values[i]);
      }
      s->shared.setFromHost(P_METER, migrateOldMeter(values[P_METER]));
      // Everything added after 0.1.0 starts at its default.
      for (uint32_t i = 10; i < P_COUNT; ++i) {
        s->shared.setFromHost(i, kParamSpecs[i].def);
      }
      hasBandData = false;
    } else if (total == 312) {
      // 38 values + scale (0.2.0, three bands).
      double values[38] = {};
      std::memcpy(values, header, sizeof(header));
      std::memcpy(values + 2, rest.data(), 36 * sizeof(double));
      std::memcpy(&scale, rest.data() + 36 * sizeof(double), sizeof(scale));
      haveScale = true;
      applyThreeBand(values);
    } else {
      return false;
    }
  }

  normalizeLoadedState(s->shared, hasBandData);
  s->shared.inPeakL.store(-120.0f, std::memory_order_relaxed);
  s->shared.inPeakR.store(-120.0f, std::memory_order_relaxed);
  s->shared.outPeakL.store(-120.0f, std::memory_order_relaxed);
  s->shared.outPeakR.store(-120.0f, std::memory_order_relaxed);
  s->shared.outVu.store(-120.0f, std::memory_order_relaxed);
  for (int b = 0; b < kBandCount; ++b) {
    s->shared.bandLevel[b].store(-120.0f, std::memory_order_relaxed);
  }
  for (int i = 0; i < kSpectrumBins; ++i) {
    s->shared.spectrum[i].store(-120.0f, std::memory_order_relaxed);
  }
  s->resetEngine.store(true, std::memory_order_relaxed);
  s->editor.resetUiState();
  noteHostChanges(s);
  if (haveScale && scale > 0.0) {
    s->editor.resize(scale);
  }
  return true;
}

const clap_plugin_state_t kStateExt = {stateSave, stateLoad};

// --- Audio ports ------------------------------------------------------------

uint32_t audioPortsCount(const clap_plugin_t*, bool isInput) {
  return isInput ? 1 : 1;
}

bool audioPortsInfo(const clap_plugin_t*, uint32_t index, bool isInput,
                    clap_audio_port_info_t* info) {
  std::memset(info, 0, sizeof(*info));
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  if (index != 0) {
    return false;
  }
  info->id = isInput ? 0 : 1;
  writeName(info->name, sizeof(info->name), isInput ? "Input" : "Output");
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  return true;
}

const clap_plugin_audio_ports_t kAudioPortsExt = {audioPortsCount, audioPortsInfo};

// --- GUI --------------------------------------------------------------------

bool guiIsApiSupported(const clap_plugin_t*, const char* api, bool isFloating) {
  return !isFloating && api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
}

bool guiGetPreferredApi(const clap_plugin_t*, const char** api, bool* isFloating) {
  *api = CLAP_WINDOW_API_X11;
  *isFloating = false;
  return true;
}

bool guiCreate(const clap_plugin_t*, const char*, bool) { return true; }

void guiDestroy(const clap_plugin_t* p) { self(p)->editor.close(); }

// X11 embedding uses physical pixels, so the host does not set a scale.
bool guiSetScale(const clap_plugin_t*, double) { return false; }

bool guiGetSize(const clap_plugin_t* p, uint32_t* width, uint32_t* height) {
  const UiSize s = self(p)->editor.size();
  *width = s.width;
  *height = s.height;
  return true;
}

bool guiCanResize(const clap_plugin_t*) { return true; }

bool guiGetResizeHints(const clap_plugin_t*, clap_gui_resize_hints_t* hints) {
  hints->can_resize_horizontally = true;
  hints->can_resize_vertically = true;
  hints->preserve_aspect_ratio = true;
  hints->aspect_ratio_width = kPanelWidth;
  hints->aspect_ratio_height = kPanelHeight;
  return true;
}

// Returns the nearest valid size; the editor always keeps 1040:640.
bool guiAdjustSize(const clap_plugin_t*, uint32_t* width, uint32_t* height) {
  const UiSize s = snapSize(*width, *height);
  *width = s.width;
  *height = s.height;
  return true;
}

bool guiSetSize(const clap_plugin_t* p, uint32_t width, uint32_t height) {
  self(p)->editor.resize(scaleForRequest(width, height));
  return true;
}

bool guiSetParent(const clap_plugin_t* p, const clap_window_t* window) {
  return self(p)->editor.open(window->x11 ? reinterpret_cast<void*>(
                                                static_cast<uintptr_t>(window->x11))
                                          : nullptr);
}

bool guiSetTransient(const clap_plugin_t*, const clap_window_t*) { return false; }

void guiSuggestTitle(const clap_plugin_t*, const char*) {}

bool guiShow(const clap_plugin_t*) { return true; }

bool guiHide(const clap_plugin_t*) { return true; }

const clap_plugin_gui_t kGuiExt = {
    guiIsApiSupported, guiGetPreferredApi, guiCreate, guiDestroy, guiSetScale,
    guiGetSize, guiCanResize, guiGetResizeHints, guiAdjustSize, guiSetSize,
    guiSetParent, guiSetTransient, guiSuggestTitle, guiShow, guiHide,
};

uint32_t latencyGet(const clap_plugin_t* p) {
  return static_cast<uint32_t>(self(p)->engine.latencySamples());
}

const clap_plugin_latency_t kLatencyExt = {latencyGet};

// --- Plugin lifecycle -------------------------------------------------------

bool pluginInit(const clap_plugin_t*) { return true; }

void pluginDestroy(const clap_plugin_t* p) {
  delete self(p);
}

bool pluginActivate(const clap_plugin_t* p, double sampleRate, uint32_t,
                    uint32_t) {
  Plugin* s = self(p);
  s->sampleRate = sampleRate;
  // Activation is the one legal moment to change latency: apply the
  // oversampling mode the parameters requested while the plugin was idle.
  s->engine.prepare(static_cast<float>(sampleRate),
                    static_cast<float>(s->shared.get(P_OVERSAMPLE)));
  s->active.store(true, std::memory_order_relaxed);
  s->reportedOversample.store(
      static_cast<int>(s->shared.get(P_OVERSAMPLE) + 0.5),
      std::memory_order_relaxed);
  // REAPER only picks up a new latency when the plugin calls changed(), so do
  // that here (being-activated is the one legal moment) - but only when the
  // value actually changed, so a host that reacts by restarting cannot loop.
  const uint32_t latencyNow = s->engine.latencySamples();
  if (latencyNow != s->reportedLatency.exchange(latencyNow)) {
    if (s->host && s->host->get_extension) {
      const auto* latency = static_cast<const clap_host_latency_t*>(
          s->host->get_extension(s->host, CLAP_EXT_LATENCY));
      if (latency && latency->changed) {
        latency->changed(s->host);
      }
    }
  }
  return true;
}

void pluginDeactivate(const clap_plugin_t* p) {
  self(p)->active.store(false, std::memory_order_relaxed);
}

bool pluginStartProcessing(const clap_plugin_t*) { return true; }

void pluginStopProcessing(const clap_plugin_t*) {}

void pluginReset(const clap_plugin_t* p) {
  self(p)->engine.reset();
}

clap_process_status pluginProcess(const clap_plugin_t* p, const clap_process_t* proc) {
  Plugin* s = self(p);
  const uint32_t frames = proc->frames_count;
  if (proc->in_events) {
    applyEvents(s, proc->in_events);
  }
  emitDirty(s, proc->out_events);

  if (proc->audio_inputs_count == 0 || proc->audio_outputs_count == 0) {
    return CLAP_PROCESS_CONTINUE;
  }
  // Be tolerant of mono or partially connected buffers.
  const clap_audio_buffer_t& inBuf = proc->audio_inputs[0];
  const clap_audio_buffer_t& outBuf = proc->audio_outputs[0];
  if (!outBuf.data32 || !inBuf.data32 || !outBuf.data32[0] || !inBuf.data32[0]) {
    return CLAP_PROCESS_CONTINUE;
  }
  float* left = outBuf.data32[0];
  float* right = outBuf.channel_count > 1 && outBuf.data32[1] ? outBuf.data32[1] : left;
  const float* inL = inBuf.data32[0];
  const float* inR = inBuf.channel_count > 1 && inBuf.data32[1] ? inBuf.data32[1] : inL;
  if (left != inL) {
    std::memcpy(left, inL, frames * sizeof(float));
  }
  if (right != inR) {
    std::memcpy(right, inR, frames * sizeof(float));
  }

  if (s->resetEngine.exchange(false, std::memory_order_relaxed)) {
    s->engine.reset();
  }
  s->engine.setControls(controlsFrom(s->shared));
  if (frames > 0) {
    s->engine.process(left, right, static_cast<int>(frames), s->meters);

    s->shared.inPeakL.store(s->meters.inPeakL, std::memory_order_relaxed);
    s->shared.inPeakR.store(s->meters.inPeakR, std::memory_order_relaxed);
    s->shared.outPeakL.store(s->meters.outPeakL, std::memory_order_relaxed);
    s->shared.outPeakR.store(s->meters.outPeakR, std::memory_order_relaxed);
    s->shared.outVu.store(s->meters.outVu, std::memory_order_relaxed);
    for (int b = 0; b < kBandCount; ++b) {
      s->shared.bandLevel[b].store(s->meters.bandLevel[b],
                                   std::memory_order_relaxed);
    }
    const float* spectrum = s->engine.spectrumBins();
    for (int i = 0; i < kSpectrumBins; ++i) {
      s->shared.spectrum[i].store(spectrum[i], std::memory_order_relaxed);
    }
  }
  noteHostChanges(s);
  return CLAP_PROCESS_CONTINUE;
}

const void* pluginGetExtension(const clap_plugin_t*, const char* id) {
  if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
    return &kParamsExt;
  }
  if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
    return &kStateExt;
  }
  if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
    return &kAudioPortsExt;
  }
  if (std::strcmp(id, CLAP_EXT_GUI) == 0) {
    return &kGuiExt;
  }
  if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) {
    return &kLatencyExt;
  }
  return nullptr;
}

void pluginOnMainThread(const clap_plugin_t* p) {
  Plugin* s = self(p);
  if (!s->rescanQueued.exchange(false, std::memory_order_relaxed)) {
    return;
  }
  const int os = static_cast<int>(s->shared.get(P_OVERSAMPLE) + 0.5);
  const bool latencyChanged =
      s->reportedOversample.exchange(os, std::memory_order_relaxed) != os;
  if (latencyChanged && s->active.load(std::memory_order_relaxed) && s->host) {
    // CLAP only allows the latency to change while the plugin is being
    // activated; ask the host to restart.  The new latency is reported from
    // pluginActivate(), after the engine has actually switched rates.
    if (s->host->request_restart) {
      s->host->request_restart(s->host);
    }
  }
}

Plugin* createPlugin(const clap_host_t* host) {
  Plugin* s = new Plugin();
  s->host = host;
  s->plugin.desc = &kDescriptor;
  s->plugin.plugin_data = s;
  s->plugin.init = pluginInit;
  s->plugin.destroy = pluginDestroy;
  s->plugin.activate = pluginActivate;
  s->plugin.deactivate = pluginDeactivate;
  s->plugin.start_processing = pluginStartProcessing;
  s->plugin.stop_processing = pluginStopProcessing;
  s->plugin.reset = pluginReset;
  s->plugin.process = pluginProcess;
  s->plugin.get_extension = pluginGetExtension;
  s->plugin.on_main_thread = pluginOnMainThread;
  s->engine.prepare(48000.0f);
  // Latency may only change while the plugin is being activated, so the
  // oversampling switch is applied on the host's restart, not mid-block.
  s->engine.setDeferOversample(true);
  s->editor.requestFlush = [s] {
    if (s->host && s->host->request_process) {
      s->host->request_process(s->host);
    }
  };
  return s;
}

// --- Factory and entry ------------------------------------------------------

uint32_t factoryCount(const clap_plugin_factory_t*) { return 1; }

const clap_plugin_descriptor_t* factoryDescriptor(const clap_plugin_factory_t*, uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_t* factoryCreate(const clap_plugin_factory_t*, const clap_host_t* host,
                                   const char* id) {
  if (std::strcmp(id, kPluginId) != 0) {
    return nullptr;
  }
  return &createPlugin(host)->plugin;
}

const clap_plugin_factory_t kFactory = {factoryCount, factoryDescriptor, factoryCreate};

bool entryInit(const char*) {
  // Do not call XInitThreads() here: the host has already used Xlib by the
  // time plugins are loaded, and initialising threading afterwards is
  // undefined behaviour (it stopped the editor loading in REAPER).  The
  // editor instead confines every Xlib call to its own thread.
  return true;
}

void entryDeinit() {}

const void* entryGetFactory(const char* factoryId) {
  return std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0 ? &kFactory : nullptr;
}

} // namespace
} // namespace patina

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION,
    patina::entryInit,
    patina::entryDeinit,
    patina::entryGetFactory,
};
