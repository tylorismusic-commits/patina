// Patina — LV2 host smoke test using lilv.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lv2/core/lv2.h>
#include <lilv/lilv.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "lv2_ports.hpp"
#include "params.hpp"
#include "presets.hpp"

namespace {

constexpr const char* kPluginUri =
    "https://github.com/tylorismusic-commits/patina";

int gFailures = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    ++gFailures;
  }
}

LilvNode* uri(LilvWorld* w, const char* s) { return lilv_new_uri(w, s); }

} // namespace

int main(int argc, char** argv) {
  std::string bundle = argc > 1 ? argv[1] : "build/lv2/patina.lv2";
  if (bundle.empty() || bundle.back() != '/') {
    bundle += '/';
  }
  LilvWorld* world = lilv_world_new();
  LilvNode* pluginUri = uri(world, kPluginUri);
  lilv_world_load_bundle(
      world, lilv_new_file_uri(world, nullptr, bundle.c_str()));
  const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
  const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, pluginUri);
  check(plugin != nullptr, "bundle loads and plugin is found");
  if (!plugin) {
    return 1;
  }

  {
    LilvNode* name = lilv_plugin_get_name(plugin);
    const char* n = name ? lilv_node_as_string(name) : "";
    char label[96];
    std::snprintf(label, sizeof(label), "plugin is named 'Patina' (got '%s')",
                  n);
    check(std::strcmp(n, "Patina") == 0, label);
    if (name) lilv_node_free(name);
  }

  const LilvUIs* uis = lilv_plugin_get_uis(plugin);
  check(lilv_uis_size(uis) == 1, "one X11 UI declared");

  const uint32_t nPorts = lilv_plugin_get_num_ports(plugin);
  char portCountLabel[160];
  std::snprintf(portCountLabel, sizeof(portCountLabel),
                "%u ports declared (%u expected: 4 audio + %u params + %u "
                "band levels + 5 meters + %u spectrum + latency), got %u",
                patina::kLv2PortCount, patina::kLv2PortCount,
                static_cast<unsigned>(patina::P_COUNT),
                patina::kLv2BandLevelCount, patina::kLv2SpectrumCount, nPorts);
  check(nPorts == patina::kLv2PortCount, portCountLabel);

  // Every parameter symbol must survive into the TTL, in order, because the
  // port index is part of the saved-session format.
  {
    bool symbolsOk = true;
    for (uint32_t id = 0; id < patina::P_COUNT; ++id) {
      const LilvPort* p =
          lilv_plugin_get_port_by_index(plugin, patina::kLv2FirstParam + id);
      const LilvNode* sym = p ? lilv_port_get_symbol(plugin, p) : nullptr;
      const char* got = sym ? lilv_node_as_string(sym) : "(missing)";
      if (std::strcmp(got, patina::kParamSpecs[id].symbol) != 0) {
        std::printf("  port %u symbol '%s', expected '%s'\n",
                    patina::kLv2FirstParam + id, got,
                    patina::kParamSpecs[id].symbol);
        symbolsOk = false;
      }
    }
    check(symbolsOk, "parameter symbols match the port order");
  }

  // Every control port's default must sit inside its declared range.
  {
    std::vector<float> mins(nPorts), maxs(nPorts), defs(nPorts);
    lilv_plugin_get_port_ranges_float(plugin, mins.data(), maxs.data(),
                                      defs.data());
    LilvNode* audioPort = uri(world, LV2_CORE__AudioPort);
    bool rangesOk = true;
    for (uint32_t i = 0; i < nPorts; ++i) {
      const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
      if (lilv_port_is_a(plugin, port, audioPort)) {
        continue;
      }
      if (!(mins[i] <= maxs[i]) || defs[i] < mins[i] || defs[i] > maxs[i]) {
        std::printf("  port %u range broken: min %g max %g default %g\n", i,
                    mins[i], maxs[i], defs[i]);
        rangesOk = false;
      }
    }
    lilv_node_free(audioPort);
    check(rangesOk, "port ranges and defaults are consistent");
  }

  // ---------------------------------------------------------------------
  // Presets: present, attached, complete and every symbol resolves.
  // ---------------------------------------------------------------------
  {
    LilvNode* rdfType =
        uri(world, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
    LilvNode* presetClass = uri(world, "http://lv2plug.in/ns/ext/presets#Preset");
    LilvNode* bankClass = uri(world, "http://lv2plug.in/ns/ext/presets#Bank");
    LilvNode* appliesTo = uri(world, "http://lv2plug.in/ns/lv2core#appliesTo");
    LilvNode* lv2Port = uri(world, "http://lv2plug.in/ns/lv2core#port");
    LilvNode* lv2Symbol = uri(world, "http://lv2plug.in/ns/lv2core#symbol");
    LilvNode* psetValue = uri(world, "http://lv2plug.in/ns/ext/presets#value");

    LilvNodes* presets =
        lilv_world_find_nodes(world, nullptr, rdfType, presetClass);
    LilvNodes* banks = lilv_world_find_nodes(world, nullptr, rdfType, bankClass);

    char countLabel[128];
    const unsigned found =
        presets ? static_cast<unsigned>(lilv_nodes_size(presets)) : 0u;
    std::snprintf(countLabel, sizeof(countLabel),
                  "%d presets declared (got %u)", patina::kPresetCount, found);
    check(static_cast<int>(found) == patina::kPresetCount, countLabel);

    std::set<std::string> validSymbols;
    for (uint32_t id = 0; id < patina::P_COUNT; ++id) {
      validSymbols.insert(patina::kParamSpecs[id].symbol);
    }

    bool attached = true;
    bool symbolsResolve = true;
    bool complete = true;
    int index = 0;
    LILV_FOREACH(nodes, it, presets) {
      const LilvNode* preset = lilv_nodes_get(presets, it);
      LilvNode* target = lilv_world_get(world, preset, appliesTo, nullptr);
      if (!target || !lilv_node_equals(target, pluginUri)) {
        std::printf("  preset %d does not apply to this plugin\n", index);
        attached = false;
      }
      if (target) lilv_node_free(target);

      LilvNodes* ports =
          lilv_world_find_nodes(world, preset, lv2Port, nullptr);
      const unsigned values =
          ports ? static_cast<unsigned>(lilv_nodes_size(ports)) : 0u;
      if (values != patina::P_COUNT) {
        std::printf("  preset %d has %u values, expected %u\n", index, values,
                    static_cast<unsigned>(patina::P_COUNT));
        complete = false;
      }
      LILV_FOREACH(nodes, pit, ports) {
        const LilvNode* port = lilv_nodes_get(ports, pit);
        LilvNode* sym = lilv_world_get(world, port, lv2Symbol, nullptr);
        LilvNode* val = lilv_world_get(world, port, psetValue, nullptr);
        const char* s = sym ? lilv_node_as_string(sym) : "";
        if (validSymbols.find(s) == validSymbols.end()) {
          std::printf("  preset %d uses unknown symbol '%s'\n", index, s);
          symbolsResolve = false;
        }
        if (!val) {
          std::printf("  preset %d symbol '%s' has no value\n", index, s);
          symbolsResolve = false;
        }
        if (sym) lilv_node_free(sym);
        if (val) lilv_node_free(val);
      }
      lilv_nodes_free(ports);
      ++index;
    }
    check(attached, "every preset applies to this plugin");
    check(symbolsResolve, "every preset port symbol resolves to a parameter");
    check(complete, "every preset carries the full parameter state");
    check(banks && lilv_nodes_size(banks) >= 1, "preset banks are declared");

    lilv_nodes_free(presets);
    lilv_nodes_free(banks);
    lilv_node_free(rdfType);
    lilv_node_free(presetClass);
    lilv_node_free(bankClass);
    lilv_node_free(appliesTo);
    lilv_node_free(lv2Port);
    lilv_node_free(lv2Symbol);
    lilv_node_free(psetValue);
  }

  std::vector<float> values(nPorts, 0.0f);
  for (uint32_t i = 0; i < nPorts; ++i) {
    const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
    LilvNode* def = lilv_port_get(plugin, port, uri(world, LV2_CORE__default));
    values[i] = def ? lilv_node_as_float(def) : 0.0f;
    if (def) lilv_node_free(def);
  }

  LilvInstance* inst = lilv_plugin_instantiate(plugin, 48000.0, nullptr);
  check(inst != nullptr, "plugin instantiates");
  if (!inst) {
    return 1;
  }

  const uint32_t frames = 256;
  std::vector<float> inL(frames), inR(frames), outL(frames), outR(frames);
  std::vector<float*> audio = {inL.data(), inR.data(), outL.data(),
                               outR.data()};
  for (uint32_t i = 0; i < 4; ++i) {
    lilv_instance_connect_port(inst, i, audio[i]);
  }
  for (uint32_t i = 4; i < nPorts; ++i) {
    lilv_instance_connect_port(inst, i, &values[i]);
  }

  auto setParam = [&](uint32_t id, float v) {
    values[patina::kLv2FirstParam + id] = v;
  };

  // Four bands, distinct models, fully wet.
  setParam(patina::P_MB, 1.0f);
  setParam(patina::P_BANDS, 4.0f);
  setParam(patina::P_BAND, 1.0f);
  setParam(patina::P_XOVER1, 120.0f);
  setParam(patina::P_XOVER2, 900.0f);
  setParam(patina::P_XOVER3, 5000.0f);
  const int models[4] = {4, 0, 2, 1};
  for (int b = 0; b < patina::kBandCount; ++b) {
    setParam(patina::bandParam(b, patina::BF_MODEL),
             static_cast<float>(models[b]));
    setParam(patina::bandParam(b, patina::BF_DRIVE), 0.6f);
    setParam(patina::bandParam(b, patina::BF_CHARACTER), 0.55f);
    setParam(patina::bandParam(b, patina::BF_TONE), 0.45f);
    setParam(patina::bandParam(b, patina::BF_MIX), 1.0f);
    setParam(patina::bandParam(b, patina::BF_LEVEL), 0.5f);
  }

  lilv_instance_activate(inst);
  float peak = 0.0f;
  bool finite = true;
  for (int block = 0; block < 160; ++block) {
    for (uint32_t i = 0; i < frames; ++i) {
      const float t = static_cast<float>(block * frames + i) / 48000.0f;
      inL[i] = inR[i] = 0.3f * std::sin(2.0f * 3.14159265f * 220.0f * t);
    }
    lilv_instance_run(inst, frames);
    for (uint32_t i = 0; i < frames; ++i) {
      if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) {
        finite = false;
      }
      peak = std::max(peak, std::fabs(outL[i]));
    }
  }
  check(finite, "LV2 processing stays finite");
  check(peak > 0.01f && peak < 2.0f, "LV2 output has a sane level");

  bool bandMeters = false;
  for (uint32_t b = 0; b < patina::kLv2BandLevelCount; ++b) {
    if (values[patina::kLv2FirstBandLevel + b] > -110.0f) {
      bandMeters = true;
    }
  }
  check(bandMeters, "band level ports report activity");
  bool spectrum = false;
  for (uint32_t i = 0; i < patina::kLv2SpectrumCount; ++i) {
    if (values[patina::kLv2FirstSpectrum + i] > -110.0f) {
      spectrum = true;
    }
  }
  check(spectrum, "spectrum ports report activity");
  check(values[patina::kLv2FirstMeter + patina::kLv2OutPeakL] > -60.0f,
        "output peak meter reports activity");

  check(values[patina::kLv2Latency] == 0.0f,
        "latency port is zero with oversampling off");
  setParam(patina::P_OVERSAMPLE, 1.0f);
  lilv_instance_run(inst, frames);
  check(values[patina::kLv2Latency] == 30.0f,
        "latency port reports 2x (30 samples)");
  setParam(patina::P_OVERSAMPLE, 2.0f);
  lilv_instance_run(inst, frames);
  check(values[patina::kLv2Latency] == 45.0f,
        "latency port reports 4x (45 samples)");
  setParam(patina::P_OVERSAMPLE, 0.0f);
  lilv_instance_run(inst, frames);
  check(values[patina::kLv2Latency] == 0.0f,
        "latency port returns to zero");

  // Two-band mode: the inactive slots should fall to the meter floor.
  setParam(patina::P_MB, 1.0f);
  setParam(patina::P_BANDS, 2.0f);
  for (int block = 0; block < 12; ++block) {
    lilv_instance_run(inst, frames);
  }
  check(values[patina::kLv2FirstBandLevel + 1] <= -110.0f &&
            values[patina::kLv2FirstBandLevel + 2] <= -110.0f,
        "two-band mode leaves the unused band meters idle");

  // Three-band mode uses Low/Lo-Mid/High.
  setParam(patina::P_BANDS, 3.0f);
  for (int block = 0; block < 12; ++block) {
    lilv_instance_run(inst, frames);
  }
  check(values[patina::kLv2FirstBandLevel + 2] <= -110.0f,
        "three-band mode leaves the unused band meter idle");

  lilv_instance_deactivate(inst);
  lilv_instance_free(inst);
  lilv_node_free(pluginUri);
  lilv_world_free(world);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
