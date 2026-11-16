# Third-party notices

Patina is GPL-3.0-or-later.

## hearth-dsp

Filters, ADAA shaping, oversampling and other shared DSP are consumed from
`hearth-dsp` (GPL-3.0-or-later), extracted from Comp Suite. See
`third_party/hearth-dsp/THIRD_PARTY_NOTICES.md`.

## Planned model references

The model designs are inspired by the MIT-licensed JClones saturators
(DuValve, SatChannel, SatBuss, AC2, Phoenix). If their code is ported, the MIT
notice and attribution will be added here with the implementation.

The shared dynamics/ratio law used by Comp Suite was matched against Tukan's
NC76 reference; no JSFX source is copied.

Algorithm provenance: RBJ biquad cookbook, first-order ADAA (Parker,
Zavalishin, Le Bivic), Kaiser-windowed half-band filtering, Catmull-Rom
interpolation.
