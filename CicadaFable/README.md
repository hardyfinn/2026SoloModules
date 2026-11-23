# Cicada Fable

A VCV Rack 2 module that models singing insects: cicadas, crickets, katydids, grasshoppers and wingbeats. No samples; every sound is a train of mechanical impulses ringing modelled body parts.

- `docs/Cicada-Manual.pdf` (and `docs/MANUAL.md`): the manual with patch recipes.
- `docs/BUILD-INSTRUCTIONS.md`: the design specification.
- `docs/RESEARCH.md`: research, measurements and the proposal.
- `src/dsp/Voice.hpp`: the engine (no Rack includes). `src/Cicada.cpp`: the module.
- `make test`: 36 offline checks, including agreement with the Python reference, plus demo WAVs in `test/out`.
- `tools/proto/`: the Python reference engine (`cicada_ref.py`), presets (`presets.py`, exported with `export_presets.py`) and reference renders.
- `res/make_panel.py` draws the panel; `tools/preview.py` makes `panel-preview.png`; `tools/make_pdf.py` makes the PDF.
