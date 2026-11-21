# Cymbolism Fable

A physical cymbal for VCV Rack: ten types from splash to china, computed from their
geometry (shell-of-revolution finite elements), with a nonlinear core for the crash
bloom, contact beaters (tip, shoulder, mallet, rods, brush, bow), rivets, choke, tape,
a swinging mount and a modelled mic. Nothing is sampled. 40 HP, mono.

![panel](panel-preview.png)

- Manual: [docs/Cymbolism-Fable-Manual.pdf](docs/Cymbolism-Fable-Manual.pdf) (source `docs/guide.html`)
- Design: [docs/BUILD-INSTRUCTIONS.md](docs/BUILD-INSTRUCTIONS.md), review in `docs/FABLE-REVIEW.md`
- Offline checks and demo WAVs (no Rack needed): `make test` (writes `test/out/`)
- Panel: `python3 res/make_panel.py`; preview: `python3 tools/preview.py panel-preview.png <Rack/res/ComponentLibrary>`

Source: `src/CyGeometry.hpp` (types, materials, profile), `src/CyModes.*` (mode solver,
coupling tensors, pickup table), `src/CyEngine.hpp` (nonlinear modal engine, beaters,
rivets, cascade), `src/CyRunner.hpp` (engine thread, background rebuilds),
`src/Cymbolism.cpp` (module, panel, screen).
