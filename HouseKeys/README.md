# House Keys

Polyphonic VCV Rack voice: 100 keyboard sounds of late-80s Detroit and Chicago house through models of five machines (4-operator FM with real factory voices, analog poly, phase distortion, 8-bit wavetable, 8-bit sampler). Stab chords, twin unit, chorus and tape.

- Manual: `docs/MANUAL.md` and `docs/HouseKeys-Manual.pdf`
- Design: `docs/BUILD-INSTRUCTIONS.md`; build decisions: `docs/DECISIONS.md`
- Offline tests, loudness trims and demos (no Rack needed): `make test`, `make trims`, `make demos`
- Panel: `python3 res/make_panel.py` (also writes `src/PanelLayout.hpp`)
