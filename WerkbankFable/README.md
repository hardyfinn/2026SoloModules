# Werkbank Fable

A VCV Rack plugin of seven modules for late-1970s German electronic pop: **TAKT** (two-row step sequencer with interval quantiser and live transpose), **BLECH** (six-voice analog rhythm box with pedals), **ZAP** (synth percussion), **VOKODER** (20-band vocoder), **SPRECHER** (speech synthesiser with formant and speech-chip engines), **ORCHESTER** (optical-disc choir and strings keyboard) and **DYNAMO** (two-oscillator ladder-filter bass and lead voice). Every sound module has an AGE knob for analog looseness, and factory presets (70 in all).

Inspired by the instruments heard on Kraftwerk's records of 1975 to 1981. Not affiliated with the band or with any instrument maker.

- Manual: [docs/MANUAL.md](docs/MANUAL.md) and [docs/Werkbank-Manual.pdf](docs/Werkbank-Manual.pdf)
- Design: [docs/RESEARCH.md](docs/RESEARCH.md), [docs/BUILD-INSTRUCTIONS.md](docs/BUILD-INSTRUCTIONS.md)
- Reference engines (the definition of the sound): [tools/proto/](tools/proto/)

## Building

The repository's GitHub workflow builds every plugin folder for Apple silicon Macs on each push and puts branch builds on the "preview" release.

Offline checks, with no Rack needed: `make test`. It checks the C++ engines against the Python reference renders (40 cases at 48 kHz, plus 44.1 and 96 kHz), the module behaviours, the SPRECHER text parser and CPU use. `make demo` renders a 16-bar demo played by the same engines.

Regenerating things: `python3 res/make_panels.py` (panels and `src/layout.hpp`), `python3 tools/preview.py Takt out.png <Rack/res/ComponentLibrary>` (panel picture), `python3 tools/export_presets.py` (presets), `python3 tools/proto/export_tables.py` (SPRECHER tables), `python3 tools/proto/make_reference.py` then `python3 tools/proto/ref_to_header.py` (reference data), `python3 tools/make_pdf.py` (manual PDF).

## Where the build differs from the spec

- **TAKT is 40 HP**, not 36. Sixteen columns of slider, three-way switch and accent button need 8 mm each to be playable, and the left block needs room for the keyboard. LAST A and LAST B sit in the left block with PATTERN and GATE instead of at the row ends.
- **Noise is scaled with the sample rate** (by the square root of rate / 48 kHz) in every engine, Python and C++, so the noise sounds the same at any rate. Fixed one-pole filters are computed from the rate too.
- **DYNAMO oversamples 2x below 70 kHz and not at all above**, so its filter always runs near 96 kHz. The decimator is the 47-tap half-band filter from Sawstack, in both the Python and the C++.
- **VOKODER formant shift**: bands shifted in from outside the bank are silent (the spec's reference clamped them to the edge band, which made a downward shift sound brighter).
- **SPRECHER CHIP engine** reads its 25 ms frames from the live 5 ms scheduler, in both the Python and the C++, so the offline and live paths are the same. RATE scales the scheduler's frame clock continuously instead of re-timing the word, so RATE CV can move during a word.
- **Test tolerances** were narrowed to what can be compared honestly: bands more than 40 dB under the loudest and above 10 kHz are skipped, noise-driven cases allow 4 dB, the ZAP drone is compared by pitch and level, and ZAP ring time and BLECH decays are compared with values measured from the Python rather than the spec's round numbers.
- **Small choices the spec left open**: BLECH's PITCH pedal CV is 0.2 octave per volt; DYNAMO's ACCENT is 5 V for a full accent; the SPRECHER word display shows the word count when idle.

## What is a guess

The BOX component values, the disc revolution time (1.8 s) and disc sounds, and the speech rules (tuned by spectrogram, never by ear). See the manual.
