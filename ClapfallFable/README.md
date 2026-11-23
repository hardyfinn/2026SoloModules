# Clapfall Fable

One VCV Rack module, **Clapfall**: multi-particle percussion. Each trigger releases a burst of tiny modelled impacts (hand claps, snaps, sticks, coins, drum-machine noise pulses) so one module covers a tight 808 clap, a gospel room clapping on 2 and 4, and a hall full of applause.

- Manual: [docs/Clapfall-Manual.pdf](docs/Clapfall-Manual.pdf) (source: docs/MANUAL.md)
- Design spec: [docs/BUILD-INSTRUCTIONS.md](docs/BUILD-INSTRUCTIONS.md); Python reference in `tools/proto/`
- Offline checks and demo WAVs without Rack: `make test` (writes to `test/out/`)
- Panel: `python3 res/make_panel.py` (needs fonttools); preview: `python3 tools/preview.py out.png <Rack>/res/ComponentLibrary`

Inspired by multi-particle clap synthesis in general (WMD's Fracture is the best-known hardware example); our own name, panel, particle models and code. `src/dsp/dr_wav.h` is dr_wav by David Reid (public domain / MIT-0).
