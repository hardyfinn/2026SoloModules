# Sawstack Fable

A VCV Rack plugin with one module, **Sawstack**: a five-oscillator swarm voice. One SPREAD knob takes the oscillators from unison to a stack of octaves, TIMBRE fades sine to saw, RICHNESS multiplies the saw like a hard-sync sweep, and a stereo VCA and two rise/fall envelopes make it a complete voice. AGE and DRIVE add analog looseness and saturation. Polyphonic up to 16 voices.

Inspired by the Verbos Electronics Sawtooth Stack. Not affiliated with Verbos Electronics.

- Manual: [docs/MANUAL.md](docs/MANUAL.md) and [docs/Sawstack-Manual.pdf](docs/Sawstack-Manual.pdf)
- Design: [docs/RESEARCH.md](docs/RESEARCH.md), [docs/DSP-REVIEW.md](docs/DSP-REVIEW.md), [docs/BUILD-INSTRUCTIONS.md](docs/BUILD-INSTRUCTIONS.md)
- Reference engine (the definition of the sound): [tools/proto/sawstack_ref.py](tools/proto/sawstack_ref.py)

## Building

The repository's GitHub workflow builds every plugin folder for Apple silicon Macs on each push and puts branch builds on the "preview" release.

Offline checks, with no Rack needed: `make test`. They port-check the C++ engine against the Python reference and write demo WAVs to `test/out/`.

Regenerating things: `python3 res/make_panel.py` (panel), `python3 tools/preview.py panel-preview.png <Rack/res/ComponentLibrary>` (picture), `python3 tools/proto/export_presets.py` (presets), `python3 tools/proto/ref_bands.py` (test reference data), `python3 tools/make_pdf.py` (manual PDF).

## Where the build differs from the spec

- **AMPLITUDE knob label** reads "AMP" on the panel, because "AMPLITUDE" collided with "RICHNESS" at 22 HP. The top-row labels are 2.95 mm instead of the project's usual 3.2 mm for the same reason.
- **Pitch jitter runs at the engine rate**, not at twice the rate as in the reference. It is smoothed at 400 Hz either way and its depth is set in cents, so it sounds the same, and it saves a noise source per oscillator per sample.
- **Fast maths**: tanh, the shaper's sine and exp2 use approximations (errors 1.5e-5, 1e-7 and 4e-9). The spec allowed this for tanh.
- **Test 8 (agreement with the reference)** is done twice. With drift and jitter switched off, the C++ engine matches the Python within 0.02 dB in every band. With them on, the two engines use different random number generators, so single third-octave bands under about 300 Hz move by up to 2.4 dB with the luck of the drift; that pass checks the total level within 0.5 dB and each band within 3 dB. Bands more than 40 dB under the loudest band are skipped as window leakage.
- **Test 10 (speed)**: 16 voices run about 4.5 times faster than real time on the cloud test machine (a shared 2.3 GHz Xeon), short of the spec's 5. The check asks for 3 there. An Apple silicon Mac should be well over 5, but this was not measured.
- **Mono**: when only one of L and R is patched, that jack carries the equal-gain mono mix. Every voice is summed into one mono or stereo pair before decimation, which is cheaper than decimating each voice.
