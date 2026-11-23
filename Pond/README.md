# Pond

A VCV Rack 2 module: a frog and toad chorus. Up to 16 frogs of 16 species call,
answer each other or synchronise, sing in bouts, and fall silent on **HUSH**
(a predator) before tentatively building up again. No samples.

- `docs/MANUAL.md`: the manual with patch ideas.
- `src/dsp/Pond.hpp`: the engine (no Rack includes). `src/Pond.cpp`: the module.
- `make test`: offline checks (levels of every species, HUSH silence and return, TALK answer/sync, temperature) plus demo WAVs in `test/out`.
- `res/make_panel.py` draws the panel (22 HP); `tools/preview.py` makes `panel-preview.png`.

MIT licence.
