# Changelog

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
