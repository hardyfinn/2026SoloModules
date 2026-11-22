# Birdie Fable

One VCV Rack module, **Birdie**: a single bird's voice by physical model (a two-sided syrinx driven by pressure and tension, then windpipe, throat and beak), with a phrase generator, NOTE and PHRASE triggers, clock lock, AUTO and HOLD. Mono, 36 HP, 20 factory presets.

- Manual: `docs/Birdie-Manual.pdf` (source `docs/MANUAL.md`)
- Design: `docs/BUILD-INSTRUCTIONS.md`, research in `docs/RESEARCH.md`
- Reference engine: `tools/proto/` (C). `src/dsp/birdie_ref.*` is an unchanged copy; `make test` checks that.

Build: `make RACK_DIR=path/to/Rack-SDK dist`. Mac arm64 builds come from CI on the preview release.
Offline checks and demo WAVs, no Rack needed: `make test`. Presets: `make presets`. Panel: `python3 res/make_panel.py`.
