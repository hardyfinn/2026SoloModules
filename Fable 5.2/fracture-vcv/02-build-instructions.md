# Clapfall: build instructions

Design by Fable, 2026-11-21, for an Opus build. One VCV Rack 2 module, **Clapfall**, a multi-particle percussion voice: a burst of tiny modelled impacts (hand claps, snaps, sticks, coins, drum-machine noise pulses) that covers everything from a tight 808 clap to a hall full of applause. Behaviour is in the family of WMD's Fracture (documented at docs.wmdevices.com/fracture), under our own name, panel, wording and sounds. Do not use the words "Fracture" or "WMD" on the panel, in plugin.json or in the manual, and do not copy their manual text.

The reference for every number below is the Python prototype in `tools/proto/clapfall_proto.py`. Where this document and the prototype disagree, the prototype wins, except where this document says "C++ differs". Read the prototype first; it is short.

## 0. What makes it sound real (do not simplify these away)

1. **Particles are modelled, 24 different ones per surface.** Each is a short noise burst through a few body resonances, with scatter in hand size, resonance and timing. The hand recipes were fitted to third-octave spectra of real clap recordings (main resonance 1.5 to 1.9 kHz, second near 600 Hz, steep roll-off above 3 kHz). The `exc_lp` low-pass on the noise burst is what stops them sounding like white-noise clicks; keep it.
2. **Never the same variant twice running**, plus per-particle pitch, pan and level scatter.
3. **LOOSE runs from machine to human.** At LOOSE 0 nothing is random: timing is a strict clock, level is fixed, and the particle count of a burst is exact, so a drum-machine clap is identical on every hit. Turned up, timing becomes Poisson and levels scatter, which is what a crowd does.
4. **Choke on the drum-machine surfaces** (808, 909, NOISE): a new particle cuts the previous one off in 1.5 ms. With the two-stage particle (4 ms spike, then a 30 ms tail arriving 9 ms later) this reproduces the classic three sawtooth pulses followed by a longer fourth, measured against a real 808 clap (20 ms RMS frames, real 25 23 19 9 4 3, model 18 23 16 8 4 3).

## 1. Files and conventions

Folder `ClapfallFable/` in hardyfinn/2026SoloModules, same shape as `Heathrow/` (copy its Makefile, .gitignore, LICENSE.txt, `res/haer_logo.py`, `tools/pdf.js`, `tools/preview.py` and adapt). CI already builds any folder with a plugin.json for Mac arm64.

```
ClapfallFable/
  plugin.json            slug ClapfallFable, name "Clapfall Fable", brand Haer, version 2.0.0, GPL-3.0-or-later
  Makefile               as Heathrow, plus `make test`
  src/plugin.cpp/.hpp
  src/Clapfall.cpp       Module + ModuleWidget only
  src/dsp/Particles.hpp  recipes and table builder (no Rack includes)
  src/dsp/Engine.hpp     clock, voices, filter, reverb (no Rack includes)
  src/dsp/dr_wav.h       vendored, public domain, for the user folder
  res/Clapfall.svg, res/make_panel.py
  test/render.cpp        offline checks and demo WAVs, no Rack needed
  docs/MANUAL.md, docs/Clapfall-Manual.pdf, docs/BUILD-INSTRUCTIONS.md
  tools/proto/           the Python reference (already here)
```

Module slug `Clapfall`, name "Clapfall", tags Drum, Percussion, Granular, Synth voice. Description along the lines of: "Multi-particle percussion: bursts of modelled hand claps, snaps, sticks and drum-machine noise pulses, from a tight 808 clap to a hall of applause". The DSP headers must compile without Rack so `test/render.cpp` can use them (vcvrack.com is blocked in the cloud container, so the Rack build is only checked by CI; fetch Rack headers from github.com/VCVRack/Rack for a syntax check if useful).

## 2. Panel (16 hp, 81.28 mm)

Text labels, light panel in the Heathrow style, Hær logo on the bottom rail, control labels 3.2 mm, jack labels 2.9 mm. Generate with `res/make_panel.py` and check with the preview tool before committing. Layout is guidance; keep the grouping and make sure nothing overlaps.

- **Top:** title CLAPFALL. Under it a small dark display (about 44 x 9 mm, custom widget drawn with NanoVG) showing the surface name in capitals, and to its right two lit buttons: **HIT** (momentary) and **FREE** (latching).
- **Knob block, three columns by three rows:**
  - row 1: SURFACE (large, snaps), DENSITY, DECAY
  - row 2: LOOSE, TONE, SPREAD
  - row 3: REVERB, SIZE, and in the third cell two three-position switches side by side, **ENV** (AMP / BOTH / CHANCE, top to bottom) and **FILTER** (BAND / PITCH / PUNCH, top to bottom)
- **Jacks, three rows of six:**
  - row A (CV): SURF, DENS, DECAY, LOOSE, TONE, SPREAD
  - row B: REVERB, SIZE, V/OCT, FREE, ACCENT, ONE
  - row C: TRIG, then space, then OUT L, OUT R on the right (outputs on a dark plate as in Heathrow)
- A particle light near the display blinks for 20 ms on every particle (brightness by level).

Tooltips on every control must say what it does in a plain sentence, since the labels are short.

## 3. Controls

All CV inputs add to the knob, 5 V = full sweep (0.2 per volt), clamped to 0..1 after summing. Knob value k in 0..1.

| Control | Default | Mapping |
|---|---|---|
| SURFACE | CLAP | Snapped, 22 built-in positions in the order of `ORDER` in the prototype, plus USER when a folder is loaded. CV 5 V sweeps the whole list. A change applies to the next particle; voices already sounding finish. |
| DENSITY | 0.60 | Particles per second = 1.5 x 400^k (1.5 to 600). "How many people." |
| DECAY | 0.35 | Burst length T = 0.012 x 500^k seconds (12 ms to 6 s). The envelope is e = exp(-age / (T/3)), so the burst is about 26 dB down at T. |
| LOOSE | 0.50 | See section 5. `le = min(1, 4k)`. |
| TONE | 0.50 | Filter centre fc = 150 x (10000/150)^k Hz. In PITCH and PUNCH it also sets particle pitch: ratio = 2^((k - 0.5) x 4), two octaves either way. |
| SPREAD | 0.30 | Pitch scatter of +-(1.25k^2 + 0.25k) octaves, uniform; pan scatter of +-min(1, 1.6k), uniform, equal-power. |
| REVERB | 0.00 | Send level into the reverb (the dry path is untouched), so it can be opened by CV after the hit for late tails. |
| SIZE | 0.30 | Small room to hall: RT60 = 0.25 x 12.8^k seconds (0.25 to 3.2 s), pre-delay 4 to 24 ms, damping more at larger sizes. |
| ENV switch | BOTH | Where the burst envelope goes, section 5. |
| FILTER switch | PUNCH | BAND: band-pass only, TONE does not change pitch. PITCH: band-pass, TONE also sets pitch. PUNCH: dry plus 1.5 x band-pass in parallel, TONE also sets pitch. |
| HIT button | | Same as a trigger at TRIG. |
| FREE button / gate | off | Free-running cloud, section 5. Active if the button is latched or the FREE jack is above 2 V. |

Jacks: TRIG, ONE, ACCENT, FREE use a Schmitt trigger (low 0.1 V, high 2 V). **ONE** plays a single particle at full level per rising edge, independent of the burst. **ACCENT**: if high (above 2 V) at the moment of a trigger, that burst is 1.5 x louder and its envelope 1.6 x longer. **V/OCT**: when patched it replaces TONE's pitch control in every filter mode, ratio = 2^V, 0 V = natural pitch; clamp the total playback ratio to 1/16..8. Outputs: 5 V peak for a typical single particle; final soft clip `10 V x tanh(x / 10 V)`. If only OUT L is patched it carries the mono sum (L+R) x 0.707.

Polyphony: monophonic module.

## 4. Particles (`Particles.hpp`)

Port `render_variant`, `R`, `HANDS`, `SURF`, `ORDER`, `CHOKE` and `build_tables` exactly: same recipe numbers, same signal flow (contacts, optional `exc_lp` as two one-poles, modes as RBJ band-passes scaled by sqrt(Q), `air`, `click`, `chirp`, 3 ms fade, 30 Hz high-pass, normalise to peak 1.0). GROUP draws each variant from CLAP, PALMS, CUPPED, FINGERS, CLAP, CUPPED in rotation.

- Tables are built once at 48 kHz, 24 variants per surface, float, each the recipe's length in ms. Total is about 2.5 MB. Build them once per process (shared static, guarded by `std::call_once`), in a worker thread started from the first module's constructor; until ready the module outputs silence. Build time must be under about 300 ms; measure it in the test.
- Use a fixed-seed PRNG of your own (xoroshiro or PCG) so the tables are the same on every launch. They do not need to match numpy's numbers, only the statistics (test in section 8).
- Display names: CLAP, PALMS, CUPPED, FINGERS, GROUP, SNAP, SLAP, STOMP, STICKS, BLOCK, RIM, PING, SWITCH, COINS, GLASS, TIN, PEBBLE, DROP, CRACKLE, 808, 909, NOISE, USER.

**USER surface.** Context menu "Load sample folder..." (osdialog directory chooser) loads up to 64 WAV files from a folder with dr_wav, mono-summed, trimmed to 2 s, peak-normalised, 3 ms fade-out, each keeping its own sample rate for playback. Load on a worker thread and swap in atomically (shared_ptr swap; never free on the audio thread). Persist the path in the patch JSON and reload on patch load. "Clear sample folder" removes it. USER has no choke.

## 5. Engine (`Engine.hpp`)

Runs at the host sample rate. 32 voices. Port `burst()` from the prototype to a per-sample process; the prototype's logic is the specification.

**Voice.** Plays one table at `ratio x tableRate / hostRate` samples per sample with 4-point Hermite interpolation, constant-power pan, fixed gain, ends at the table end. Allocation: a free voice, else the voice furthest through its table (2 ms fade when stolen).

**Firing a particle** (`fire` in the prototype): variant = random, never equal to the previous one; pitch ratio = centre x 2^(U(-1,1) x spreadOct); pan = U(-1,1) x panWidth; gain = amp x comp x 10^(le x U(-2.5, 2.5)/20), where comp = (1 + rate/80)^-0.5 for normal surfaces and 1 for choke surfaces. On a choke surface, all sounding voices get a 1.5 ms linear fade-out first.

**Clock.** One particle clock. After each tick the next interval is `T x ((1 - loose) + loose x Exp(1))` with T = 1/rate, so LOOSE 0 is a strict clock and LOOSE 1 is a Poisson process. Read rate, envelope and knobs at tick time (control-rate smoothing is not needed for the clock; smooth the filter frequency and reverb send over about 5 ms).

**Burst (FREE off).** A trigger sets age = 0, latches accent, restarts the clock and fires a particle immediately, on the same sample, at full envelope. That first particle always fires. At each later tick: e = exp(-age / (T/3 x accentLen)); then by ENV:
- AMP: amp = e, prob = 1
- BOTH: amp = sqrt(e), prob = sqrt(e)
- CHANCE: amp = 1 - le x U(0, 0.65), prob = e

Fire if `u < prob`, with `u = 0.5 + (U(0,1) - 0.5) x le`. At LOOSE 0 that means "fire exactly while prob > 0.5", which gives a repeatable particle count set by DECAY. Stop ticking when e < 0.001. Accent multiplies amp by 1.5.

**FREE on.** No envelope. e = the DECAY value (knob plus CV, 0..1) directly, used by ENV exactly as above; at 0 nothing sounds. A trigger or HIT adds a density boost: rate x (1 + 3 x exp(-t/0.12 s)) since the trigger, summed over recent triggers and capped at x8. ONE still works.

**Menu option "Same every hit"** (default off): reseed the engine PRNG from a fixed seed on each trigger, so a burst with LOOSE above 0 is still identical each time.

**Filter.** After the voice sum, per channel: a state-variable band-pass (TPT/Zavalishin form, stable under fast modulation), Q = 1.8, output scaled x2. BAND and PITCH output the band-pass; PUNCH outputs dry + 1.5 x band-pass. Then gain so a single full-level particle peaks near 5 V.

**Reverb. C++ differs:** the prototype convolves with a synthetic impulse response as a stand-in. Build a real one: stereo 8-line feedback delay network (Householder or Hadamard matrix, mutually prime delays of about 11 to 47 ms scaled by 0.45 + 0.75 x SIZE, slow 0.3 to 1 Hz modulation of two lines by a few samples), per-line one-pole damping (cutoff 9 kHz falling to 4 kHz with SIZE), 180 Hz high-pass in the loop, feedback gains set per line from RT60 in section 3, pre-delay 4 to 24 ms, plus six early reflection taps as in `reverb_ir`. Input is `REVERB x (filtered dry)`; output is added to the dry signal. Decorrelated L and R. The reverb keeps ringing after the send closes. Wet level: at REVERB 1 a single clap's tail should start about 9 dB below the dry peak.

Denormals: flush or add a tiny DC offset in the filter and reverb loops.

## 6. Module details

- `onReset` clears voices and reverb. `onSampleRateChange` recomputes coefficients and reverb delay lengths.
- Lights: HIT flashes on trigger; FREE lit while free-running (button or gate); particle light as in section 2.
- Patch JSON: FREE latch, "Same every hit", user folder path.
- CPU target: under 2% of a core at 48 kHz in a dense applause patch.

## 7. Factory starting points (put these in the manual as recipes, with knob positions as clock-face or percent)

Values are k (0..1); `k_density`, `k_decay`, `k_freq` in `tools/proto/render2.py` convert from Hz and seconds.

| Recipe | Surface | Density | Decay | Loose | Tone | Spread | Env | Filter | Reverb / Size |
|---|---|---|---|---|---|---|---|---|---|
| 808 clap | 808 | 95/s | 90 ms | 0 | 1.1 kHz | 0 | CHANCE | PUNCH | 0.12 / 0.2 |
| 909 clap | 909 | 85/s | 130 ms | 0 | 1.4 kHz | 0 | CHANCE | PUNCH | 0.2 / 0.3 |
| One person | CLAP | 20/s | 12 ms | 0.5 | 1.6 kHz | 0.05 | BOTH | PUNCH | 0.25 / 0.2 |
| Five people | GROUP | 170/s | 60 ms | 1 | 1.5 kHz | 0.22 | CHANCE | PUNCH | 0.3 / 0.3 |
| Gospel room | GROUP | 420/s | 110 ms | 1 | 1.4 kHz | 0.3 | CHANCE | PUNCH | 0.5 / 0.55 |
| Applause | GROUP | 0.3 to 0.95 | FREE, swell with DECAY | 1 | 1.7 kHz | 0.4 | CHANCE | PUNCH | 0.35 / 0.7 |
| Snaps | SNAP | 60/s | 30 ms | 0.9 | 3 kHz | 0.12 | BOTH | PUNCH | 0.3 / 0.3 |
| Stick roll | STICKS | 28/s | 500 ms | 0.15 | 2.5 kHz | 0.15 | AMP | PUNCH | 0.2 / 0.3 |
| Rain | DROP | 40/s | 3 s | 1 | centre | 0.6 | CHANCE | PITCH | 0.4 / 0.6 |
| Dropped coins | COINS | 70/s | 500 ms | 1 | 0.55 | 0.35 | CHANCE | PUNCH | 0.25 / 0.4 |
| Ping-pong | PING | 9/s | 2.5 s | 0.3 | centre | 0.1 | AMP | PUNCH | 0.3 / 0.3 |

Add recipes aimed at house, techno, hip-hop and reggae use: layered clap and snare, clap with late reverb opened by an envelope into REVERB CV, a rimshot from RIM with ONE, sequencing SURFACE by CV for a percussion line, V/OCT melodies from GLASS and TIN, vinyl crackle from CRACKLE in FREE, and a crowd that swells into a drop.

## 8. Tests (`make test`, no Rack)

`test/render.cpp` must check, and fail loudly on:
1. Table build time, and that every table is finite, peak 1.0, DC-free.
2. Hand surfaces: averaged third-octave spectrum of CLAP variants peaks in the 1250 to 2000 Hz bands, is at least 8 dB down by 6.3 kHz, and CUPPED peaks at or below 800 Hz. (Same measure as `tools/proto/bands.py`.)
3. 808 recipe at the settings above: 20 ms RMS frames from the hit follow the shape "rise or hold for the first 40 ms, then fall below a quarter of the maximum by 100 ms", and two consecutive hits are bit-identical at LOOSE 0.
4. Particle count: at LOOSE 0, CHANCE, density 100/s the number of particles per burst equals round-ish(100 x (T/3) x ln 2) + 1, within one.
5. LOOSE 1 inter-particle intervals have a coefficient of variation between 0.85 and 1.15; LOOSE 0 below 0.01.
6. FREE with DECAY at 0 is silent; no NaN or denormal slowdowns after 60 s of random knob and trigger fuzzing at 44.1, 48 and 96 kHz; output never exceeds 10 V.
7. Reverb RT60 measured from an impulse is within 20% of the target at SIZE 0, 0.5 and 1.
8. Renders demo WAVs for every recipe in section 7 plus a gallery that plays every surface, for Hardy to hear.

## 9. Manual

`docs/MANUAL.md` and a PDF made with `tools/pdf.js` as in the other plugins: what it is in two paragraphs, a labelled panel picture, each control in plain words, the 23 surfaces with one line each, the LOOSE idea (machine to human), the recipes of section 7 with knob positions, and loading your own folder. Written fresh, no borrowed text. Hardy does not write C++ and is at home in Rack.

## 10. Honest limits to state in the manual and hand-back

Built-in particles other than the hands and the 808 are modelled from general acoustics and a handful of measured recordings, not from a matched set, so some (PING, SWITCH, COINS) are plausible more than exact. Nothing has been heard in Rack until Hardy tries it.
