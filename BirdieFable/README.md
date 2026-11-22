# Birdie Fable

One VCV Rack module, **Birdie**: a single bird's voice by physical model, with a phrase generator, stretchable into drones and bass.

Status: designed, not built. The build instructions are in `docs/BUILD-INSTRUCTIONS.md` and the reference engine is in `tools/proto/`.

To hear the reference: `cd tools/proto && cc -O2 -o render_ref render_ref.c birdie_ref.c -lm && mkdir -p out && ./render_ref out`
