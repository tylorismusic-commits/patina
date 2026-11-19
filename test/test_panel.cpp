// Patina — panel/user preset interaction tests (no X server needed).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "params.hpp"
#include "presets.hpp"
#include "ui/panel.hpp"

using namespace patina;

namespace {
int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}
} // namespace

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "patina-preset-test";
  fs::remove_all(root);
  fs::create_directories(root / "config");
  fs::create_directories(root / "home");
  const std::string oldXdg =
      std::getenv("XDG_CONFIG_HOME") ? std::getenv("XDG_CONFIG_HOME") : "";
  const std::string oldHome =
      std::getenv("HOME") ? std::getenv("HOME") : "";
  setenv("XDG_CONFIG_HOME", (root / "config").string().c_str(), 1);
  setenv("HOME", (root / "home").string().c_str(), 1);

  Shared shared;
  Panel panel;
  PanelState state;

  shared.set(P_MODEL, 2);  // BUS
  shared.set(P_DRIVE, 0.61);
  const double savedDrive = shared.get(P_DRIVE);

  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 100, 450);
  check(state.savePreset, "SAVE opens the name field");
  for (char c : std::string("Test Preset")) {
    panel.textKey(shared, state, 0, c);
  }
  panel.textKey(shared, state, Panel::kTextEnter, 0);
  check(state.userPresets.size() == 1, "user preset is written");
  check(state.presetIndex == kPresetCount, "saved preset becomes active");
  check(state.userPresets[0].name == "Test Preset",
        "saved preset keeps its name");

  shared.set(P_DRIVE, 0.12);
  panel.applyPreset(shared, state, state.presetIndex);
  check(std::fabs(shared.get(P_DRIVE) - savedDrive) < 1e-9,
        "user preset recalls every parameter");

  // Rename.
  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 100, 480);
  check(state.renamePreset, "RENAME opens the name field");
  for (char c : std::string("Renamed Preset")) {
    panel.textKey(shared, state, 0, c);
  }
  panel.textKey(shared, state, Panel::kTextEnter, 0);
  check(state.userPresets.size() == 1 &&
            state.userPresets[0].name == "Renamed Preset",
        "user preset renames");

  // Duplicate.
  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 180, 450);
  check(state.userPresets.size() == 2, "DUPLICATE creates a second preset");

  // Export the active duplicate.
  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 250, 480);
  const fs::path exported = userPresetExportDir() /
                            (std::string("Renamed Preset copy") +
                             kUserPresetExt);
  check(fs::exists(exported), "EXPORT writes a preset file");

  // Delete the active duplicate.
  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 180, 480);
  check(state.deletePresetConfirm, "DELETE asks for confirmation");
  panel.handlePresetClick(shared, state, 180, 480);
  check(state.userPresets.size() == 1, "DELETE removes the user preset");

  // Import the exported file back.
  check(panel.importDroppedFile(shared, state, exported.string()),
        "IMPORT accepts a dropped preset file");
  check(state.userPresets.size() == 2, "import adds a preset");

  if (!oldXdg.empty()) setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
  else unsetenv("XDG_CONFIG_HOME");
  if (!oldHome.empty()) setenv("HOME", oldHome.c_str(), 1);
  else unsetenv("HOME");
  fs::remove_all(root);

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
