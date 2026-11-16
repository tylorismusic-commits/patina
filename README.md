# Patina

A five-model saturation plugin for Linux, in the same visual language as Comp
Suite. GPL-3.0-or-later.

The CLAP bundle is built and installable; LV2 wiring is next.

## Models

| Tab | Character |
|---|---|
| **Valve** | two-stage tube warmth, even and odd harmonics |
| **Console** | channel-strip saturation, dual-mono with small L/R differences |
| **Bus** | stereo-linked, gentle even-order glue |
| **Tape** | head bump, soft tape compression and HF loss |
| **Transformer** | iron low-end saturation, dark and thick |

## Controls

Drive · Character · Tone · Mix · Output · Auto Gain · Oversampling (Off/2×/4×).
The display switches between **Shape** (transfer curve), **Scope** (input vs
shaped sine) and **Off**.

## Shared DSP

Filters, ADAA shaping, oversampling and the meter helpers come from
[`hearth-dsp`](https://github.com/tylorismusic-commits/hearth-dsp), consumed as
a submodule. No DSP code is copied from Comp Suite.

## Build

```sh
git submodule update --init --recursive
cmake -S . -B build -DCLAP_DIR="$HOME/.clap"
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build
```

The host smoke test runs under `xvfb-run` and drives every model, the
oversampling latency handshake, state save/load and the embedded editor.

Render the panel previews:

```sh
./build/patina_preview assets/fonts docs
```

## Status

- [x] Five-model saturation engine with ADAA and Off/2×/4× oversampling
- [x] Panel in the Comp Suite visual language (shape/scope displays)
- [x] Factory presets
- [x] DSP tests
- [x] CLAP
- [ ] LV2
- [x] Host/state/latency integration

The previews and the CLAP editor render from the same Panel class.

## Licence

GPL-3.0-or-later. See `THIRD_PARTY_NOTICES.md`.
