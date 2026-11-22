# Birdie: build instructions

Design by Fable, 2026-11-22, for an Opus build. One VCV Rack 2 module, **Birdie**: a physical model of a single bird's voice with a built-in phrase generator. It sings whistles, trills, chirps, buzzes, two-note chords, coos and harsh calls, and every control can be pushed past what a bird can do, down to drones and bass. No samples. Mono.

The reference for every number is the C engine in `tools/proto/`: `birdie_ref.h`, `birdie_ref.c` (engine and phrase generator), `presets.h` (the 16 factory presets in engine units), `render_ref.c` (renders every preset) and `probe.c` (steady-tone measurements). It is plain C99 that also compiles as C++. **Use it as the engine: copy it into `src/dsp/` and wrap it. Do not rewrite the arithmetic or change the order of random draws.** Where this document and the reference disagree, the reference wins. `first_sketch.c` is the earlier sketch and is background only.

Background and research: `docs/RESEARCH.md`. Note that the tension scale changed after that document was written (section 0, point 2).

Hardy's decisions: name Birdie; mono; one large module; no V/oct requirement; two triggers, one for a single note and one for a phrase; phrases run at their own rate by default with a switch to lock to a clock. Hardy liked every early sketch except the quack and found the coo weak; both ends were reworked in the reference (the rich edge in point 2 below, and the Dove, Owl, Crow and Honker presets).

## 0. What makes it sound real (do not simplify these away)

1. **The source is a differential equation, not an oscillator with a waveshape.** Each side of the syrinx is the two-variable labial model `bd_deriv`, integrated with RK4 at 16 times the sample rate (8 times at 88.2 and 96 kHz, 4 times above). Pressure (alpha) starts and stops the sound; tension sets pitch. Do not swap in a wavetable, a phase accumulator or a cheaper integrator.
2. **Tension is measured upward from the rich edge.** The model stops singing below a boundary in tension that moves with pressure (`bd_edge`, a table built in `bd_init`). TENSION is the distance above that boundary. That makes pitch almost independent of pressure, and it means low TENSION always reaches the spiky, harmonic-rich pulses of crows, ducks and the bass voice. High TENSION is a pure whistle. This one axis is the main timbre control.
3. **SIZE rescales time inside the model** (`G = 24000 / size`). Pitch is exactly proportional to `1 / size`. Trachea length scales with size; the beak and the fixed throat formant scale with `1 / sqrt(size)`.
4. **Two independent sides.** The second oscillator has its own tension path (interval and its own slide direction). Mixed, they beat and clash like a thrush. It only runs when its mix is above 0.001.
5. **The throat follows the pitch.** A band-pass tuned to a multiple of the estimated pitch is what turns the source into a clean whistle. With FOLLOW off it is a fixed formant, which is what the harsh calls need.
6. **The note is a gesture.** A raised-cosine pressure envelope and a shaped tension slide, with BUZZ and CHOP as fast modulation of tension and pressure, and ROUGH as noise on both. The output also passes a square-root envelope gain so onsets are soft even though the model starts abruptly.
7. **The pluck at note-on** in `bd_start`. Without it a large, slow bird takes tens of milliseconds to start.

## 1. Files and conventions

Folder `BirdieFable/` in hardyfinn/2026SoloModules, same shape as `Heathrow/` on this branch. Copy Heathrow's Makefile, .gitignore, LICENSE.txt, `res/haer_logo.py`, `tools/pdf.js`, `tools/preview.py` and adapt. Other built siblings for reference: ClapfallFable on branch `claude/project-thread-p0eipc`. CI already builds any folder with a plugin.json for Mac arm64, which is the only build needed. Work on this thread's branch, not main.

```
BirdieFable/
  plugin.json            slug BirdieFable, name "Birdie Fable", brand Haer, version 2.0.0, GPL-3.0-or-later
  Makefile               as Heathrow, plus `make test`
  src/plugin.cpp/.hpp
  src/Birdie.cpp         Module + ModuleWidget only
  src/dsp/birdie_ref.h, birdie_ref.c   the engine, copied from tools/proto (no Rack includes)
  res/Birdie.svg, res/make_panel.py
  presets/Birdie/*.vcvm  the 16 factory presets
  test/render.cpp        offline checks and demo WAVs, no Rack needed
  docs/MANUAL.md, docs/Birdie-Manual.pdf, docs/BUILD-INSTRUCTIONS.md, docs/RESEARCH.md
  tools/proto/           the C reference (already here)
```

Module slug `Birdie`, name "Birdie", tags Synth voice, Physical modeling, Sequencer, Random, Drone. Description along the lines of: "One bird by physical model: whistles, trills, buzzes, two-voice song and harsh calls, with a phrase generator, stretchable into drones and bass". Keep `src/dsp` free of Rack includes so `test/render.cpp` can use it (vcvrack.com is blocked in the cloud container, so the Rack build is only checked by CI). If `birdie_ref.c` is compiled as C++, rename the copy to `.cpp`; keep the `tools/proto` originals untouched and have `make test` fail if the two copies differ in anything but the file name.

## 2. Panel (36 hp, 182.88 mm)

Text labels, light panel in the Heathrow style, Hær logo on the bottom rail, control labels 3.2 mm, jack labels 2.9 mm. Generate with `res/make_panel.py` and check with the preview tool before committing. Layout is guidance; keep the grouping and make sure nothing overlaps. Widen to 38 hp if labels do not fit.

- **Top:** title BIRDIE. To its right a small dark display (about 60 x 9 mm, NanoVG): pattern name and seed while idle (for example `MOTIF 0421`), and note index out of total while a phrase plays (`MOTIF 3/6`). A light that follows the PRESS output.
- **Row 1, heading VOICE:** SIZE (large), TENSION (large), PRESSURE, BREATH, ROUGH, then a sub-group 2ND VOICE: MIX, INTERVAL, SLIDE.
- **Row 2, heading NOTE:** LENGTH, SLIDE, SHAPE (snapped, 5 positions, labels LINE / FALL / SWOOP / ARCH / DOUBLE), ATTACK, RELEASE, then a sub-group BUZZ: RATE, DEPTH, CHOP.
- **Row 3, heading BODY:** TRACHEA, TUBE, THROAT, TUNE, switch FOLLOW (on / off), BEAK. To the right, heading STRETCH: TIME, and a latching lit button HOLD.
- **Row 4, heading PHRASE:** NOTES (snapped), RATE, PATTERN (snapped, 7 positions), VARY, DRIFT, ACCEL, SWELL, GAP, switch CLOCK (FREE / LOCK), switch AUTO (off / on), button NEW, and two momentary buttons NOTE and PHRASE.
- **Jack row A, CV inputs:** SIZE, TENSION, PRESSURE, ROUGH, 2ND MIX, LENGTH, SLIDE, BUZZ RATE, BEAK, TIME, RATE, VARY.
- **Jack row B:** NOTE, PHRASE, NEW, CLOCK, HOLD, then on a dark plate the outputs END, GATE, PRESS, PITCH, OUT.

Tooltips on every control must say what it does in a plain sentence and show real units (see the table). RATE reads notes per second in FREE and "notes per clock" in LOCK.

## 3. Controls

`k` is the knob position 0 to 1. Defaults are the reference's `bd_default()`.

| Control | Engine field | Mapping | Default | Tooltip units |
|---|---|---|---|---|
| SIZE | size | 0.5 x 128^k (0.5 to 64) | 1 | "x", and the reference pitch 4250 / size in Hz |
| TENSION | tension | -36 to +7, linear | 0 | semitones |
| PRESSURE | pressure | 0.05 to 0.6, linear | 0.22 | % |
| BREATH | breath | 0 to 1 | 0.25 | % |
| ROUGH | rough | 0 to 1 | 0 | % |
| 2ND MIX | v2mix | 0 to 1 | 0 | % |
| 2ND INTERVAL | v2int | -12 to +12 | 5 | semitones |
| 2ND SLIDE | v2slide | -1 to +1 (-1 mirrors the first voice's slide) | -1 | % |
| LENGTH | length | 0.01 x 200^k s | 0.2 | ms or s |
| SLIDE | slide | -24 to +24 | -6 | semitones |
| SHAPE | shape | 0 to 4 snapped | FALL | name |
| ATTACK | attack | 0.02 to 0.6 | 0.15 | % of note |
| RELEASE | release | 0.02 to 0.6 | 0.30 | % of note |
| BUZZ RATE | buzzRate | 5 x 80^k Hz | 120 | Hz |
| BUZZ DEPTH | buzz | 0 to 0.5 | 0 | % |
| CHOP | chop | 0 to 1 | 0 | % |
| TRACHEA | trachea | 0.005 x 20^k m | 0.025 | cm |
| TUBE | tube | 0 to 0.9 | 0.6 | % |
| THROAT | throat | 0 to 1 | 0.6 | % |
| TUNE | tune | 0 to 1 | 1/3 | FOLLOW on: ratio 2^(3k-1) as "x pitch". Off: 300 x 20^k / sqrt(size) in Hz |
| FOLLOW | follow | switch | on | |
| BEAK | beak | 500 x 32^k Hz | 10000 | Hz after the 1/sqrt(size) scaling |
| TIME | time | 16^k (1 to 16) | 1 | "x slower" |
| NOTES | notes | 1 to 32 snapped | 6 | |
| RATE | rate | 40^k notes per second | 5 | see below for LOCK |
| PATTERN | pattern | 0 to 6 snapped: REPEAT, PAIR, MOTIF, TAIL, RUN, CALL, SHUFFLE | MOTIF | name |
| VARY | vary | 0 to 1 | 0.5 | % |
| DRIFT | drift | -12 to +12 | 0 | semitones over the phrase |
| ACCEL | accel | -1.5 to +1.5 | 0 | octaves of speed over the phrase |
| SWELL | swell | -1 to +1 (positive grows, negative fades) | 0 | % |
| GAP | module only | 0.1 x 80^k s | 1 | s |

**CV inputs** add to the knob, then clamp to the knob's range. SIZE: +1 V halves the size (so it tracks 1 V/oct upward, exactly). TENSION: 12 semitones per volt. BUZZ RATE and RATE: one octave per volt. TIME: +1 V doubles. LENGTH: +1 V doubles. SLIDE: 4.8 semitones per volt. PRESSURE, ROUGH, 2ND MIX, VARY: 10% of range per volt. BEAK: one octave per volt.

**What is read when.** Everything in the voice, note-shape and body groups is read every sample, so knobs and CV act on a sounding note. LENGTH is read at each note start. NOTES, PATTERN, VARY, DRIFT, ACCEL, SWELL and the note period are read at each phrase start (as `bd_phrase` does). Smooth SIZE, TENSION, PRESSURE, BEAK and TUNE with a 5 ms one-pole before the engine to avoid zipper noise; the engine itself smooths the trachea delay.

## 4. Triggers, phrases and the module's own logic

The engine provides `bd_note`, `bd_phrase` and `bd_process`. The module adds the rest.

- **NOTE** (jack or button): `bd_note`. One note from the panel settings. It cancels a running phrase.
- **PHRASE** (jack or button): `bd_phrase(seed)`. Restarts if one is running.
- **Seed.** A 32-bit seed stored in the patch. NEW (button or jack) picks a fresh one from Rack's random source; it takes effect at the next phrase start. Menu item "New seed every phrase" (default off): when on, each phrase start first picks a new seed. Menu item "Seed" shows the number and lets it be typed.
- **What a phrase is** (already in the reference): type A is always the note on the panel. Types B, C and D are relatives of it whose differences scale with VARY (tension, slide, shape, length, pressure, attack, release, and at higher VARY a chance of their own buzz or second voice). PATTERN arranges them: REPEAT is A A A A; PAIR is A B A B; MOTIF is A A B; TAIL is a run of A ending in B for the last quarter; RUN steps A, B, C, D in blocks; CALL cycles A B C D; SHUFFLE picks by seed. Each note lasts LENGTH (times its type's factor and TIME) but never more than 92% of its slot.
- **CLOCK switch.** FREE: slot length is TIME / RATE. LOCK: measure the period between the last two CLOCK edges; RATE then picks notes per clock from nine equal zones: 1/4, 1/3, 1/2, 1, 2, 3, 4, 6, 8, and `clockPeriod = period / ratio`. If no two clock edges have been seen, behave as FREE. TIME does not change slot length in LOCK (it still stretches note length and buzz).
- **AUTO switch.** When on, a phrase starts at once, and after each phrase ends the module waits GAP x TIME seconds and starts the next. PHRASE and NOTE triggers still work and restart the wait.
- **HOLD** (latching button, or gate at the jack, either one): sets `hold`. The voice sounds continuously at the panel settings, as an oscillator; notes and phrases still play their tension slides on top.
- **Outputs.** OUT: audio, about +-5 V, soft-limited at 8 V by the engine. PRESS: `outPress`, 0 to 10 V, the pressure envelope including chop. PITCH: `outPitch`, V/oct estimate of the first voice (0 V = C4), held when the voice is silent; it is an estimate, within about a semitone except at the very bottom of TENSION. GATE: 10 V while a note sounds. END: 1 ms trigger when a phrase finishes.

## 5. Module details

- `onReset` and `onSampleRateChange` call `bd_init` with the new rate (the oversampling factor and decimator are chosen there).
- Patch JSON: seed, HOLD latch, the two menu options.
- The engine uses doubles; keep them. CPU target: under 6% of a core at 48 kHz (the reference renders 108 s in 4.5 s on a cloud core). If it needs trimming, the first things to try are computing the `pow` and `tan` calls every 8 samples and replacing the decimator's modulo indexing with a doubled buffer, checking the tests still pass.
- The decimator FIR in the reference is the one to use, not Rack's.
- Flush denormals in the throat and beak filters and the trachea line.

## 6. Factory presets (`presets/Birdie/`)

Sixteen, from `presets.h`, converted back to knob positions with the table in section 3 and written as `.vcvm` files by a small script: Whistler, Triller, Chirper, Buzzer, Thrush, Dove, Crow, Honker, Songster, Warbler, Giant, Drone, Bass Bird, Robot, Tiny, Owl. Store each preset's seed. Bass Bird expects CLOCK at LOCK (the reference fixes its slot at a 16th note at 124 bpm); Drone has HOLD on.

Reference renders of all sixteen are in `/mnt/project-files/birdsong-vcv/reference/` with `spectrograms.png`.

## 7. Tests (`make test`, no Rack)

`test/render.cpp` must check, and fail loudly on:

1. **Same engine.** Each preset rendered through the `src/dsp` copy, with the trigger script of `render_ref.c`, matches the `tools/proto` build sample for sample (difference under 1e-9 V).
2. **Steady tones** (HOLD on, BREATH, THROAT and TUBE at 0, BEAK at maximum, measured by zero crossings after 1 s), within 0.5%: SIZE 1, TENSION -1, PRESSURE 0.22 gives 4026.8 Hz; SIZE 4 gives 1006.7 Hz; SIZE 16 gives 251.7 Hz. TENSION +6 gives 6141.5 Hz. TENSION -12 gives 2208.2 Hz. TENSION -24 with PRESSURE 0.30 gives 991.4 Hz. TENSION -36 with PRESSURE 0.40 gives 410.3 Hz.
3. **Pressure does not move pitch much.** At TENSION -1, PRESSURE 0.08 and 0.50 are both within 4% of the 0.22 pitch.
4. **Rich edge.** Brightness (`probe.c`'s measure) is under 1.15 at TENSION -1 and above 8 at TENSION -36 with PRESSURE 0.40.
5. **Phrases.** Same seed gives an identical render twice; a different seed with VARY 1 does not. With VARY 0 and PATTERN anything, every note of a phrase is identical to the first. NOTES notes are produced, GATE rises that many times, END fires once. REPEAT at RATE 25 gives onsets 40 ms apart within one sample; ACCEL +1 halves the last slot against the first.
6. **Clock lock.** With a 0.5 s clock and the ratio at 4, onsets are 125 ms apart within one sample.
7. **NOTE, HOLD, AUTO.** A NOTE trigger gives one note of LENGTH and silence after. HOLD gives continuous sound within 30 ms and silence within 30 ms of release. AUTO repeats with the GAP wait.
8. **Sample rates.** Octave-band levels of Whistler, Crow and Bass Bird at 44.1 and 96 kHz match 48 kHz within 1.5 dB in every band within 30 dB of the peak.
9. **Safety.** No NaN, no denormal slowdown and no output above 8 V after 60 s of random knob, CV and trigger fuzzing at 44.1, 48 and 96 kHz, including SIZE swept end to end at audio rate.
10. **Demo WAVs** for Hardy into `/mnt/project-files/birdsong-vcv/demos/`: every preset (10 to 20 s each, several seeds), a gallery, and these patched demos: one phrase with VARY swept 0 to 1 over eight repeats; the Songster seed with SIZE stepped 0.5, 1, 2, 4, 8, 16, 32; TENSION swept top to bottom under HOLD at SIZE 4; Bass Bird locked to a 124 bpm clock for eight bars with a new seed every two bars; TIME swept 1 to 16 on Warbler; an LFO on 2ND INTERVAL under HOLD.

## 8. Manual

`docs/MANUAL.md` and a PDF made with `tools/pdf.js` as in the other plugins: what it is in two paragraphs; the idea in one small diagram (two-sided syrinx driven by pressure and tension, then trachea, throat, beak; a note is a gesture; a phrase is a family of notes); a labelled panel picture; each control in plain words; the seven patterns with one line each; the presets; patch recipes. Written fresh. Hardy does not write C++ and is at home in Rack.

Recipes to include, aimed at house, techno, acid, hip-hop and reggae as well as at nature:

- A garden bird: AUTO on, Songster, "New seed every phrase", slow random CV into TENSION.
- Call and answer: two Birdies, the first one's END into the second one's PHRASE.
- Trill fills: Triller on PHRASE from a sequencer's last step, ACCEL and DRIFT for rises and falls.
- Bass Bird: CLOCK at LOCK, PHRASE from the bar, sequencer CV into SIZE for the notes (1 V/oct), envelope into TENSION as the "filter".
- Acid bird: HOLD on, SIZE from a sequencer, slide CV through a slew, PRESS out ducking a pad.
- Dub siren: Whistler, LENGTH long, an LFO into TENSION, into a delay.
- Hip-hop scratch birds: Chirper with NOTE from a drum pattern, SLIDE by CV per step.
- The bird as a modulator: PRESS and PITCH outs driving a filter or another oscillator; GATE as a rhythm.
- Monsters: any preset with SIZE up and TIME up together.
- Machines: Robot, ROUGH and CHOP by CV, FOLLOW off with TUNE swept for vowel sounds.

## 9. Honest limits to state in the manual and hand-back

The source model is the labial normal form used in the birdsong physics literature (Mindlin and colleagues), written from memory and checked by running it; its published behaviour (pressure threshold, tension sets pitch, rich spectrum at the low-tension edge) was confirmed numerically. The body filters are simplified. No preset is fitted to a recording, so presets are named by type of sound, not as species. Everything was judged by spectrogram and measurement, not by ear, until Hardy listens: Hardy liked the early sketches except the quack and the coo, and the reworked Dove, Owl, Crow and Honker have not been heard yet.
