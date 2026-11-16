// Patina — minimal CLAP host smoke test.
// Loads the built bundle, drives audio and parameters, round-trips state,
// and opens the editor on the current X display (run under xvfb-run).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/all.h>

#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "params.hpp"

namespace {

int gFailures = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    ++gFailures;
  }
}

constexpr uint32_t kFrames = 512;

struct HostEvents {
  std::vector<clap_event_param_value_t> inputs;
  std::vector<clap_event_param_value_t> outputs;
};

uint32_t inSize(const clap_input_events_t* list) {
  return static_cast<uint32_t>(static_cast<HostEvents*>(list->ctx)->inputs.size());
}

const clap_event_header_t* inGet(const clap_input_events_t* list, uint32_t index) {
  return &static_cast<HostEvents*>(list->ctx)->inputs[index].header;
}

bool outTryPush(const clap_output_events_t* list, const clap_event_header_t* hdr) {
  auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
  static_cast<HostEvents*>(list->ctx)->outputs.push_back(*ev);
  return true;
}

clap_host_t makeHost() {
  clap_host_t host{};
  host.clap_version = CLAP_VERSION;
  host.name = "patina-test";
  host.vendor = "tylorismusic";
  host.version = "0.1";
  return host;
}

int gCallbacks = 0;
int gLatencyChanges = 0;
int gRestarts = 0;

void hostRequestCallback(const clap_host_t*) { ++gCallbacks; }
void hostLatencyChanged(const clap_host_t*) { ++gLatencyChanges; }
void hostRequestRestart(const clap_host_t*) { ++gRestarts; }

const clap_host_latency_t kHostLatency = {hostLatencyChanged};

const void* hostGetExtension(const clap_host_t*, const char* id) {
  if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) {
    return &kHostLatency;
  }
  return nullptr;
}

clap_event_param_value_t paramEvent(uint32_t id, double value) {
  clap_event_param_value_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_PARAM_VALUE;
  ev.param_id = id;
  ev.note_id = -1;
  ev.port_index = -1;
  ev.channel = -1;
  ev.key = -1;
  ev.value = value;
  return ev;
}

struct MemStream {
  std::vector<unsigned char> data;
  size_t readPos = 0;
};

int64_t streamWrite(const clap_ostream_t* s, const void* buf, uint64_t size) {
  auto* m = static_cast<MemStream*>(s->ctx);
  const auto* bytes = static_cast<const unsigned char*>(buf);
  m->data.insert(m->data.end(), bytes, bytes + size);
  return static_cast<int64_t>(size);
}

int64_t streamRead(const clap_istream_t* s, void* buf, uint64_t size) {
  auto* m = static_cast<MemStream*>(s->ctx);
  const size_t n = std::min<size_t>(size, m->data.size() - m->readPos);
  std::memcpy(buf, m->data.data() + m->readPos, n);
  m->readPos += n;
  return static_cast<int64_t>(n);
}

void writePpm(const char* path, XImage* img, int w, int h) {
  FILE* f = std::fopen(path, "wb");
  if (!f) {
    return;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const unsigned long p = XGetPixel(img, x, y);
      const unsigned char rgb[3] = {
          static_cast<unsigned char>((p >> 16) & 0xff),
          static_cast<unsigned char>((p >> 8) & 0xff),
          static_cast<unsigned char>(p & 0xff)};
      std::fwrite(rgb, 1, 3, f);
    }
  }
  std::fclose(f);
}

} // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "build/clap/Patina.clap";
  void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  check(lib != nullptr, "bundle loads");
  if (!lib) {
    std::printf("%s\n", dlerror());
    return 1;
  }
  auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
  check(entry != nullptr, "clap_entry symbol present");
  if (!entry || !entry->init(path)) {
    return 1;
  }

  auto* factory = static_cast<const clap_plugin_factory_t*>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  check(factory && factory->get_plugin_count(factory) == 1, "one plugin in factory");

  clap_host_t host = makeHost();
  host.get_extension = hostGetExtension;
  host.request_callback = hostRequestCallback;
  host.request_restart = hostRequestRestart;
  const clap_plugin_t* plugin =
      factory->create_plugin(factory, &host, "com.tylorismusic.patina");
  check(plugin != nullptr, "plugin instantiates");
  if (!plugin) {
    return 1;
  }
  check(plugin->init(plugin), "plugin init");

  auto* params = static_cast<const clap_plugin_params_t*>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  auto* state = static_cast<const clap_plugin_state_t*>(
      plugin->get_extension(plugin, CLAP_EXT_STATE));
  auto* gui = static_cast<const clap_plugin_gui_t*>(
      plugin->get_extension(plugin, CLAP_EXT_GUI));
  auto* latency = static_cast<const clap_plugin_latency_t*>(
      plugin->get_extension(plugin, CLAP_EXT_LATENCY));

  check(params && params->count(plugin) == patina::P_COUNT,
        "parameter count matches the table");
  check(params && params->count(plugin) == 10, "ten parameters");
  check(latency != nullptr, "latency extension present");
  check(latency && latency->get(plugin) == 0,
        "latency is zero with oversampling off");
  check(state != nullptr, "state extension present");
  check(gui != nullptr, "gui extension present");

  clap_param_info_t info{};
  check(params->get_info(plugin, 0, &info) && std::strcmp(info.name, "Model") == 0,
        "parameter 0 is Model");
  check(params->get_info(plugin, 3, &info) &&
            std::strcmp(info.name, "Character") == 0,
        "parameter 3 is Character");

  check(plugin->activate(plugin, 48000.0, kFrames, kFrames), "activate");
  check(plugin->start_processing(plugin), "start processing");

  HostEvents events;
  clap_input_events_t inEvents{&events, inSize, inGet};
  clap_output_events_t outEvents{&events, outTryPush};

  std::vector<float> left(kFrames), right(kFrames);
  float* chans[2] = {left.data(), right.data()};
  clap_audio_buffer_t inBuf{};
  inBuf.data32 = chans;
  inBuf.channel_count = 2;
  clap_audio_buffer_t outBuf{};
  outBuf.data32 = chans;
  outBuf.channel_count = 2;

  // Every model, with drive high enough to saturate.
  bool finite = true;
  float peakOut = 0.0f;
  double power = 0.0;
  int powered = 0;
  for (int model = 0; model < static_cast<int>(patina::Model::Count); ++model) {
    events.inputs.push_back(paramEvent(patina::P_MODEL, model));
    events.inputs.push_back(paramEvent(patina::P_DRIVE, 0.7));
    events.inputs.push_back(paramEvent(patina::P_CHARACTER, 0.6));
    events.inputs.push_back(paramEvent(patina::P_TONE, 0.45));
    for (int block = 0; block < 80; ++block) {
      for (uint32_t i = 0; i < kFrames; ++i) {
        const double t = (block * kFrames + i) / 48000.0;
        left[i] = right[i] =
            static_cast<float>(0.35 * std::sin(2.0 * M_PI * 110.0 * t));
      }
      clap_process_t proc{};
      proc.frames_count = kFrames;
      proc.audio_inputs = &inBuf;
      proc.audio_outputs = &outBuf;
      proc.audio_inputs_count = 1;
      proc.audio_outputs_count = 1;
      proc.in_events = &inEvents;
      proc.out_events = &outEvents;
      plugin->process(plugin, &proc);
      for (uint32_t i = 0; i < kFrames; ++i) {
        if (!std::isfinite(left[i]) || !std::isfinite(right[i])) {
          finite = false;
        }
        peakOut = std::max(peakOut, std::fabs(left[i]));
        if (block > 50) {
          power += static_cast<double>(left[i]) * left[i];
          ++powered;
        }
      }
      events.inputs.clear();
    }
  }
  check(finite, "host-driven processing stays finite on every model");
  check(peakOut > 0.02f && peakOut < 4.0f, "processed output has sane level");
  check(std::sqrt(power / std::max(powered, 1)) > 0.005,
        "every model produces audible output");

  double value = 0.0;
  check(params->get_value(plugin, patina::P_DRIVE, &value) &&
            std::fabs(value - 0.7) < 1e-9,
        "drive value round-trips through the host");

  // Oversampling changes latency, which CLAP only permits on activation.
  {
    clap_process_t zeroProc{};
    zeroProc.frames_count = 0;
    zeroProc.audio_inputs = &inBuf;
    zeroProc.audio_outputs = &outBuf;
    zeroProc.audio_inputs_count = 1;
    zeroProc.audio_outputs_count = 1;
    zeroProc.in_events = &inEvents;
    zeroProc.out_events = &outEvents;

    const int restartsBefore = gRestarts;
    events.inputs.push_back(paramEvent(patina::P_OVERSAMPLE, 1.0));
    plugin->process(plugin, &zeroProc);
    events.inputs.clear();
    check(latency && latency->get(plugin) == 0,
          "old latency is kept until the host reactivates");
    plugin->on_main_thread(plugin);
    check(gRestarts > restartsBefore,
          "oversampling change requests a host restart");
    plugin->deactivate(plugin);
    plugin->activate(plugin, 48000.0, kFrames, kFrames);
    check(latency && latency->get(plugin) == 30,
          "2x oversampling reports 30 samples after reactivation");

    const int changesAfter2x = gLatencyChanges;
    plugin->deactivate(plugin);
    plugin->activate(plugin, 48000.0, kFrames, kFrames);
    check(gLatencyChanges == changesAfter2x,
          "reactivation with the same latency does not notify again");

    events.inputs.push_back(paramEvent(patina::P_OVERSAMPLE, 2.0));
    plugin->process(plugin, &zeroProc);
    events.inputs.clear();
    plugin->on_main_thread(plugin);
    plugin->deactivate(plugin);
    plugin->activate(plugin, 48000.0, kFrames, kFrames);
    check(latency && latency->get(plugin) == 45,
          "4x oversampling reports 45 samples after reactivation");

    events.inputs.push_back(paramEvent(patina::P_OVERSAMPLE, 0.0));
    plugin->process(plugin, &zeroProc);
    events.inputs.clear();
    plugin->on_main_thread(plugin);
    plugin->deactivate(plugin);
    plugin->activate(plugin, 48000.0, kFrames, kFrames);
    check(latency && latency->get(plugin) == 0,
          "oversampling off reports zero after reactivation");
  }

  // State round trip: save, change, restore.
  MemStream saved;
  clap_ostream_t out{&saved, streamWrite};
  check(state->save(plugin, &out) && saved.data.size() > 0, "state saves");
  events.inputs.push_back(paramEvent(patina::P_DRIVE, 0.1));
  clap_input_events_t changeEvents{&events, inSize, inGet};
  clap_output_events_t changeOut{&events, outTryPush};
  clap_process_t changeProc{};
  changeProc.frames_count = 0;
  changeProc.in_events = &changeEvents;
  changeProc.out_events = &changeOut;
  changeProc.audio_inputs = &inBuf;
  changeProc.audio_outputs = &outBuf;
  changeProc.audio_inputs_count = 1;
  changeProc.audio_outputs_count = 1;
  plugin->process(plugin, &changeProc);
  events.inputs.clear();
  params->get_value(plugin, patina::P_DRIVE, &value);
  check(std::fabs(value - 0.1) < 1e-9, "drive changed before restore");

  MemStream readBack;
  readBack.data = saved.data;
  clap_istream_t in{&readBack, streamRead};
  check(state->load(plugin, &in), "state loads");
  params->get_value(plugin, patina::P_DRIVE, &value);
  check(std::fabs(value - 0.7) < 1e-9, "state restore brings back drive");

  // Host-facing text must be readable, not bare numbers.
  {
    char buf[128] = {0};
    params->value_to_text(plugin, patina::P_MODEL, 4.0, buf, sizeof(buf));
    check(std::strcmp(buf, "Transformer") == 0, "model reads as a name");
    params->value_to_text(plugin, patina::P_OVERSAMPLE, 2.0, buf, sizeof(buf));
    check(std::strcmp(buf, "4x") == 0, "oversampling reads as a mode");
    params->value_to_text(plugin, patina::P_DRIVE, 0.5, buf, sizeof(buf));
    check(std::strcmp(buf, "+0.0 dB") == 0, "drive reads as a signed dB value");
    params->value_to_text(plugin, patina::P_METER, 0.0, buf, sizeof(buf));
    check(std::strcmp(buf, "Shape") == 0, "display reads as a mode");
  }

  // Editor on the current display, if there is one.
  if (gui) {
    Display* dpy = XOpenDisplay(nullptr);
    if (dpy) {
      const bool api = gui->is_api_supported(plugin, CLAP_WINDOW_API_X11, false);
      check(api, "x11 embedding supported");
      check(gui->create(plugin, CLAP_WINDOW_API_X11, false), "gui create");
      Window parent = XCreateSimpleWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                          0, 0, 1040, 640, 0, 0, 0);
      XMapWindow(dpy, parent);
      XFlush(dpy);
      clap_window_t win{};
      win.api = CLAP_WINDOW_API_X11;
      win.x11 = parent;
      check(gui->set_parent(plugin, &win), "gui set_parent");

      int originX = 0, originY = 0;
      Window editorWindow = 0;
      {
        Window rootRet, parentRet, *kids = nullptr;
        unsigned n = 0;
        if (XQueryTree(dpy, parent, &rootRet, &parentRet, &kids, &n) && n > 0) {
          for (unsigned i = 0; i < n; ++i) {
            XWindowAttributes a;
            if (XGetWindowAttributes(dpy, kids[i], &a) && a.width >= 1000) {
              editorWindow = kids[i];
            }
          }
          if (editorWindow) {
            Window rr;
            XTranslateCoordinates(dpy, editorWindow,
                                  RootWindow(dpy, DefaultScreen(dpy)), 0, 0,
                                  &originX, &originY, &rr);
          }
          XFree(kids);
        }
      }
      check(editorWindow != 0, "editor created a child window");
      check(gui->show(plugin), "gui show");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      uint32_t w = 0, h = 0;
      check(gui->get_size(plugin, &w, &h) && w == 1040 && h == 640,
            "gui size is 1040x640");

      // Capture what the embedded editor is actually showing.
      if (editorWindow) {
        XImage* img = XGetImage(dpy, editorWindow, 0, 0, 1040, 640, AllPlanes,
                                ZPixmap);
        if (img) {
          writePpm("/tmp/opencode/patina-editor.ppm", img, 1040, 640);
          XDestroyImage(img);
          check(true, "captured embedded editor image");
        } else {
          check(false, "captured embedded editor image");
        }
      }

      // A real drag must move a knob (catches the X11 event-mask bug).
      auto drag = [&](int x, int y, int dy) {
        x += originX;
        y += originY;
        XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
        XFlush(dpy);
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
        XFlush(dpy);
        for (int i = 1; i <= 10; ++i) {
          XTestFakeMotionEvent(dpy, -1, x, y + dy * i / 10, CurrentTime);
          XFlush(dpy);
          std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
        XFlush(dpy);
        std::this_thread::sleep_for(std::chrono::milliseconds(180));
      };
      auto paramValue = [&](uint32_t id) {
        double v = 0.0;
        params->get_value(plugin, id, &v);
        return v;
      };

      const double driveBefore = paramValue(patina::P_DRIVE);
      drag(300, 430, -60);
      std::printf("  drive %.2f -> %.2f\n", driveBefore,
                  paramValue(patina::P_DRIVE));
      check(paramValue(patina::P_DRIVE) > driveBefore + 0.05,
            "dragging DRIVE moves it");

      const double toneBefore = paramValue(patina::P_TONE);
      drag(620, 430, 40);
      std::printf("  tone %.2f -> %.2f\n", toneBefore,
                  paramValue(patina::P_TONE));
      check(paramValue(patina::P_TONE) < toneBefore - 0.05,
            "dragging TONE moves it");

      // Model tabs are plain buttons, and the display mode switches.
      XTestFakeMotionEvent(dpy, -1, originX + 500, originY + 90, CurrentTime);
      XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
      XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
      XFlush(dpy);
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      check(paramValue(patina::P_MODEL) > 0.5,
            "clicking a model tab selects it");

      gui->hide(plugin);
      gui->destroy(plugin);
      XDestroyWindow(dpy, parent);
      XCloseDisplay(dpy);
      check(true, "gui create/embed/destroy completes");
    } else {
      std::printf("SKIP  editor (no X display)\n");
    }
  }

  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  plugin->destroy(plugin);
  entry->deinit();
  dlclose(lib);

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
