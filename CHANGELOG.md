# Changelog

## 0.7.0 — 2026-11-19

- User preset bank: save, rename, duplicate, delete, export and drag-drop
  import, alongside the factory presets.
- Preset files live in `~/.config/Patina/presets` and export to
  `~/Documents/Patina Presets`.

## 0.5.0 — 2026-11-16

### Added

- **LV2 plugin and X11/Cairo UI.** The same five models, single/2/3/4-band
  layout, per-band controls, draggable crossovers, oversampling, Auto Gain
  and FFT spectrum as the CLAP version.
- **Generated LV2 metadata.** `lv2/patina.ttl`, `manifest.ttl` and
  `presets.ttl` come from `src/params.hpp` and `src/presets.hpp`; the test
  fails if the committed files are stale.
- **Full LV2 meter surface.** 48 parameter control ports, four band level
  outputs, input/output peak and VU meters, all 128 spectrum bins and an
  `lv2:latency` port.
- **Fourteen LV2 presets** with complete parameter values, so host preset
  changes cannot inherit stale band or crossover state.
- **LV2 host tests** using lilv, plus a direct X11 UI host test for the
  resize handshake, idle throttle and meter/spectrum port events.

## 0.4.1 — 2026-11-15

### Fixed

- **Multiband oversampling retuning.** Crossover coefficients were cached by
  frequency only; after a 2×/4× switch the filters stayed tuned for the old
  sample rate when the host reactivated the plugin. The cache is invalidated
  on every rate change, with 2/3/4-band round-trip regressions.
- **Zero-frame meter blanking.** A host process call with zero frames no
  longer clears the meters or band levels.
- **Band selection and crossover validity.** Host automation, state loads and
  presets now normalize an inactive band selection and crossed crossovers.
- **Preset/state UI state.** Loading a state clears the preset label, solo
  state and meter history, and requests a clean engine reset on the audio
  thread.
- **State migrations.** 0.1.0, 0.2.0 and 0.3.0 states load correctly:
  - 0.2.0's three-band Low/Mid/High map to Low/Lo-Mid/High, its high
    crossover becomes the 3-band Air split, and its -12..+12 dB output maps
    onto the full-range Level.
  - The old Shape/Scope/Off display list maps Off to the new fourth slot
    instead of Spectrum.
  - 0.3.0 states default to four bands; 0.1.0 starts with multiband defaults.
- **Host text round-trips.** dB, percent, Hz/kHz, Off and band-count text now
  parse back to the same parameter values, including per-band Level and
  crossover text.
- **Selected band across toggles.** Leaving and re-entering Multiband keeps
  the selected active band.
- **Crossover defaults.** The 2-band split is 900 Hz, and the Air crossover
  accepts 800 Hz so 0.2.0 three-band states keep their high split.

## 0.4.0 — 2026-11-15

### Fixed

- **Oversampling in multiband mode.** The crossover coefficients were cached
  by frequency only, so after switching to 2×/4× they stayed tuned for the
  previous sample rate once the host reactivated the plugin. That shifted the
  band splits and changed the level. The cache is now invalidated whenever the
  processing rate changes. Regression tests cover 2-, 3- and 4-band modes.

### Added

- **2/3/4-band selection.** A band-count pill switches the multiband engine:
  - **2 bands:** Low/High, split at the 900 Hz crossover.
  - **3 bands:** Low/Mid/High, using the 120 Hz and 5 kHz crossovers; the two
    middle 4-band regions merge into Mid.
  - **4 bands:** Low/Lo-Mid/Hi-Mid/High with all three crossovers.
- The active band settings are preserved when changing the count, and the
  selected band follows the active slots.

## 0.3.0 — 2026-11-15

- Added a fourth band, three draggable LR4 crossovers, per-band Level down to
  silence and up to +12 dB, and a live 2048-point FFT spectrum with 128
  log-spaced bins.

## 0.2.0 — 2026-11-15

- Added three-band multiband mode with per-band models and crossovers.

## 0.1.0 — 2026-11-15

- Five-model saturation engine, panel and loadable CLAP.
