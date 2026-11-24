# Essphor Fable

Three VCV Rack 2 modules for live, rhythmic granular processing, modelled on the granular
device and 48-band tuned resonator of a well-known four-track sculpting sampler (not
affiliated with it):

- **Essphor** (32 HP): clock-locked grains from a live 4/8/16 s buffer, 64 voices, scale-snapped
  pitch, STRETCH / POSITION / DELAY scanning, lock, feedback, V/OCT, grain trigger out, WAV load.
- **Essphor Res** (16 HP): morphing 48-band tuned filter bank and resonator with 20 scales.
- **Essphor Mod** (26 HP): WAVE, RANDOM, FOLLOW and ADSR, each with a spread B output.

**AGE** on Essphor and Essphor Res scales an older, cheaper machine's imperfections. It is 0 by default.

- Manual: `docs/MANUAL.md` and `docs/Essphor-Manual.pdf`
- Design: `docs/BUILD-INSTRUCTIONS.md`, research in `docs/RESEARCH.md`, build decisions in `docs/BUILD-DECISIONS.md`
- Panel preview: `docs/panel-preview.png`
- Factory presets: 12 Essphor, 8 Essphor Res, 4 Essphor Mod, in `presets/`

## Building

`make RACK_DIR=<path to Rack SDK> dist`. CI builds Apple silicon Mac only.

`make test` builds and runs the offline engine checks (no Rack needed): 20 checks at 44.1 and
48 kHz against the Python references in `tools/proto/`. `make demos` renders the demo WAVs
into `test/out/`.

Panels: `python3 res/make_panel.py` (writes the SVGs and `src/layout.hpp`).
Presets: `python3 tools/make_presets.py`. PDF: `python3 tools/make_pdf.py`.
Preview: `python3 tools/preview.py docs/panel-preview.png`.

## CPU

Offline at 48 kHz, Essphor with 64 grains sounding runs about 15x real time on one core,
Essphor Res about 20x (build container, not a Mac).
