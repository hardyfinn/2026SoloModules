# Walker: build instructions

Design by Fable, 2026-11-23. For the model that builds it. Walker is the bass player for Comper: it reads Comper's chart and bar position, decides its own bass line, and plays it on a modelled upright bass. It also sends the notes out. `docs/WALKER-RESEARCH.md` says why and what was measured. Where the two differ, this document wins.

There are **two reference files, and they are the definition**:

- `tools/proto/walker_ref.py`: the line (which note, when) and the hands (timing, weight, intonation, string choice, the finger that stops the old note). The C++ must reproduce it note for note.
- `tools/proto/upright_ref.py`: the sound. The C++ must match it within the tolerance of `tools/check_upright.py`. Hardy approved this sound by ear on 2026-11-23 after four rounds on the pluck; do not "improve" it. Every constant in that file is fitted to recordings of a real bass.

`tools/proto/make_walker_reference.py` writes `test/walker_golden.json` (97 lines, 4,833 notes) and `test/upright_cases.json` (62 sound cases with reference measurements). `tools/proto/walker_presets.py` holds the 12 presets. `tools/proto/body_modes.json` is the measured body (40 resonances).

Hardy's decisions (2026-11-23): a second module inside the Comper Fable plugin; follows Comper with no cables when the two sit side by side; cable inputs as well; Comper stops playing roots when Walker is attached; pizzicato only, no bowing; the name is Walker.

## 1. What to build

- A second module in `ComperFable/` on branch `claude/project-thread-7vm3pn` (not main). Bump the plugin to version `2.1.0`. Module slug `Walker`, name `Walker`, tags Sequencer, Synth voice, Physical modeling, Random. Description: "Bass player for Comper: walking, two-feel, ballad, bossa, soul and house lines that lean into each chord, played on a modelled upright bass with a real pluck, a ringing body and human hands. Follows Comper with no cables. Audio and notes out". WALK and BASS must be findable in Rack's browser (add "bass" through the description).
- Panel 28 HP (142.24 mm), text labels, Haer logo, same visual language as Comper (`res/make_panel.py`; add a second panel to the script or a sibling script).
- Deliverables: C++ module, panel SVG from a script, 12 factory presets, offline tests, a Walker chapter in `docs/MANUAL.md` and the PDF, README row, demo WAVs, and the small Comper change in section 9. Apple silicon only; the repository workflow publishes to the "preview" release.

## 2. Architecture

Headers under `src/dsp/`, no Rack includes in the first three:

1. **`WalkerEngine.hpp`**: a line-by-line port of `walker_ref.py` (`BP`, `Player`, `note_string`, the tables). It uses Comper's `Rng`, `SCALES`, `auto_swing` and the quality constants from `Engine.hpp`; do not copy them. Doubles, same order of operations and random draws, candidates iterated in the same order, strict `<` for minima. Two random streams: `r = Rng(seed)` for the line, `h = Rng(seed * 7919 + 17)` for the hands.
2. **`Upright.hpp`**: the sound engine, the real-time form of `upright_ref.py` (section 5).
3. **`WalkerHost.hpp`**: clock following, the chord source, planning ahead, the note queue, the outputs. Sample-driven `process()`. No Rack types.
4. **`Link.hpp`**: the message Comper sends to Walker (section 4.1). Included by both modules.
5. **`Walker.cpp`**: the Rack module.

### 2.1 Planning steps

The reference builds a line with a loop per feel. Each **loop iteration is one planning step** and must be a function in C++ with its state on the object, so the host can call it just ahead of time:

| Feel | Planning step | State carried |
|---|---|---|
| WALK | one segment (from a chord arrival or bar line to the next, at most 4 beats): the body of the `for si` loop in `walk()` | `t1` (the note already chosen for the segment start) |
| TWO, BALLAD | one bar: the body of the loop in `two()` | `last` |
| BOSSA | one bar: the body of the loop in `bossa()` | `last` |
| SOUL, HOUSE | one bar: the body of the loop in `cells()` | `cell`, `kept` |

Each feel also has lines before its loop (the first `target()` call, the first cell draw). Those are that feel's start-up and draw random numbers; they run once at reset for the feel in use. A step returns events; sort them by beat (stable), then pass them to `perform()`, which returns notes and may change `dur` and `stopped` of the last note it returned earlier (`self.last`). `perform()` already keeps all its state on the object, so calling it per step gives exactly what the offline call gives. The test in section 10 proves that.

## 3. What the engine does (summary; the Python is the definition)

- **Arrive on the root** of each chord (`target`). If the same chord continues into a new bar, sometimes the 3rd or 5th instead. The octave is chosen near a centre that drifts slowly up and down the neck (`centre`, scaled by ROAM).
- **Lean into the arrival** (`approach`): the note before a chord arrives is a half step, a scale step or a fifth away from the coming root, weighted by OUT and by the direction the line is moving. This needs the next chord, so planning looks one chord ahead in the chart.
- **Fill the middle by the cheapest path** (`passing`): every one-note or two-note route between the arrival note and the approach note is scored (chord tone, scale tone or chromatic; step size from `STEP_COST`; change of direction; distance from the centre; a random term scaled by ROAM) and the cheapest wins.
- **Decorate** (in `walk`, by ACTIVITY): a skip on the swung upbeat (dead note, repeated note or a half step leaning into the next note) or a triplet run filling a third.
- **Other feels**: `two` (half notes, root then fifth, pickups by ACTIVITY; BALLAD adds whole notes and vibrato), `bossa` (root, pickup, fifth, pickup), `cells` (sixteenth-note bar figures from `SOUL_CELLS` or `HOUSE_CELLS`, thinned by ACTIVITY, kept by REPEAT).
- **The hands** (`perform`): the bass plays ahead of the beat (AHEAD), with slow drift and scatter (TIMING), weight (2 and 4 heavier, phrase drift, scatter), a note that fails to speak now and then (SLOP), intonation (scatter, high notes sharp, a scoop from below after a leap, vibrato on long notes), string and position (`note_string`), a note cut short when the hand must travel, and the plucking finger landing `TOUCH_S` before the pluck, which stops the old note dead if it is on the same string.
- **`blind_beat`**: one quarter note decided on the beat with no knowledge of the next chord. Used in cable mode until the tune is learned (section 4.3).

## 4. The host

### 4.1 The link from Comper

Rack runs modules on several threads, so do not read Comper's fields through a pointer. Use Rack's expander messages. Walker allocates a pair of `CompLink` buffers for its left side and a pair for its right side in its constructor. Every sample, Comper looks at its left and right neighbours; for each one whose model is Walker, it fills that Walker's facing `producerMessage` and requests a flip. Walker reads the facing `consumerMessage`; if Compers sit on both sides, the left one wins. A message older than 2 samples means no link. The link is one sample late; that is fine.

```
struct CompLink {
    uint32_t magic = 0x434F4D50;   // "COMP"
    uint32_t resetCount;           // goes up by one at every reset (RESET jack, first start, seed change taking effect)
    bool running;
    double phi;                    // beats since reset, as Comper's host has it
    double spb;                    // seconds per beat now
    int steps, beatsPerStep;       // the chart
    int root[16], qual[16];        // as the engine sees them: after KEY transposition
    bool external;                 // Comper's chords come from CV or POLY, not the chart
    int curRoot, curQual;          // the chord Comper is on now
    int groove;                    // 0 SWING 1 BOSSA 2 BALLAD 3 SOUL 4 HOUSE
    float swing;                   // Comper's SWING parameter, -1..1
};
```

Chord at a beat, as in Comper's host: step `floor(beat / beatsPerStep) mod steps`. The reference's `chord_at(beat)` and its `chart`/`total` are that lookup with `total = steps * beatsPerStep`.

### 4.2 Which source is in charge

1. **PLAY**: if GATE IN is patched, the player is silent and the bass is an instrument (section 4.6).
2. **LINK**: a live link and `external` false. Chart, position, tempo, swing and (when FEEL is FOLLOW) the feel come from Comper. The CLOCK and RESET jacks are ignored.
3. **CABLE**: CLOCK patched (or a link with `external` true, which supplies position and the current chord but no chart). Section 4.3.
4. Otherwise idle: silent, the display says so.

FEEL at FOLLOW maps Comper's groove: SWING to WALK, BOSSA to BOSSA, BALLAD to BALLAD, SOUL to SOUL, HOUSE to HOUSE. In CABLE mode FOLLOW means WALK. `BP.swing` is Comper's SWING through the link and 0 otherwise.

### 4.3 Cable mode

- Clock exactly as Comper's host does it (pulses per beat from the menu, median of the last three periods, snap `phi` at each pulse, hold if pulses stop). Take that code into a shared helper rather than copying it.
- The chord now: ROOT (V/OCT, pitch class only) and QUAL (1 V per step, the order of Comper's `Q_NAMES`, same rounding as Comper's QUAL input). If CHORD (poly V/OCT) is patched, recognise the chord from its channels with Comper's `detect_chord`; a patched ROOT then overrides the root. ROOT patched with neither QUAL nor CHORD: quality 9 (7sus), which has neither a major nor a minor third. Nothing patched but CLOCK: C 7sus.
- **Learning.** Keep a table of the chord heard on each beat since the last reset, up to 256 beats. The tune's length comes from the menu "Tune length": "From RESET" (default: the number of beats between the last two RESET pulses, rounded to whole bars; unknown until a second RESET arrives) or 4, 8, 12, 16, 24, 32 bars (position in the tune is then `phi mod length`).
- **Until the tune is known** (first time through, or length unknown): play `blind_beat` on every beat, whatever FEEL says. Decide each beat `EXT_LATE_S` (0.004 s, from `comper_ref.py`) after it arrives so the chord cables have settled; the note sounds at `max(now, its planned time)`. `changed` is true when the chord differs from the previous beat's.
- **Once every beat of the tune has been heard once**, switch at the next pass to the normal planning steps with the table as the chart (`chord_at` reads the table, `total` is the length). Keep updating the table as chords arrive; a change takes effect on the next pass.
- The display shows LEARNING or CABLE.

### 4.4 Planning ahead and the note queue

- A planning step for the stretch starting at beat `b0` runs when `phi >= b0 - LEAD`, with `LEAD = max(0.5 beat, 0.2 s)` expressed in beats at the current tempo. At reset or first start, run the feel's start-up and the first step at once.
- Each note from `perform()` has `t` (seconds from beat 0 at the tempo given to the engine) and `beat`. Schedule its onset at `now + (beat - phi) * spb + (t - beat * spb)`; durations are in seconds as returned. If the onset is already past, play it now. This matches the reference to the sample at a steady tempo and bends sensibly when tempo moves.
- When `perform()` shortens the previous note or marks it stopped, update that note's pending release. LEAD is long enough that the release has not happened yet; if it somehow has, ignore the change.
- The engine is told the tempo (`bpm`) at each step; `spb` inside the engine is `60 / bpm` as now.
- Parameters are read at each planning step. A new FEEL takes effect at the next bar line: run the new feel's start-up there, but start it from the last pitch played (`self.last`) instead of drawing a new first note when a note has already been played. This path is not covered by the golden test; keep it simple.
- **RESET** (Comper's `resetCount` changing, or the RESET jack in cable mode): all notes released (as stopped notes), queue cleared, both random streams re-seeded from SEED, the per-note counter back to 0, the learned table kept. The same seed, chart and settings then give the same line again. NEW and a changed SEED act at the next bar line, as on Comper.
- Not running: no new notes; sounding notes ring out naturally.
- **LAY OUT** gate high: planning carries on (so the random streams stay in step) but notes are not played. The bass comes back on the next planned note after the gate falls.

### 4.5 Outputs

- **OUT**: audio. Volts = reference output times 0.5 times LEVEL (LEVEL 0 to 2, default 1). A loud note peaks near 5 V.
- **V/OCT**: 0 V = middle C (MIDI 60), so bass notes are negative; `(pitch - 60) / 12`. Menu option "V/OCT carries slides" (on by default) adds the note's `cents`, its scoop (`-scoop * exp(-t / 0.035)` cents) and its vibrato, but not the first-cycle bend.
- **GATE**: 10 V from onset to release. If a new note starts while the gate is high, drop the gate 1 ms before it.
- **VEL**: `vel * 10` V, held from each onset. Dead notes send their notes too; that is how a player would hear them.

### 4.6 PLAY mode

GATE IN patched: a rising gate plucks the pitch at V/OCT IN (rounded to the nearest semitone for string choice, the remainder kept as `cents`; clamp to MIDI 28 to 67) at velocity VEL IN / 10 (0.75 if unpatched). String and fret from `note_string` with the same hand state. The falling gate releases the note. A new pluck on the string of a note still sounding stops that note at once (`stopped`). No scoop, no vibrato, no dead notes. This makes Walker a playable upright bass voice for any sequencer.

## 5. The sound engine in real time

`upright_ref.py` renders each note as whole arrays and then filters the sum. In C++ the same thing is a small polyphonic voice bank followed by one shared body. Structure:

**Voices.** Six; steal the oldest. A voice starts at note-on with `(pitch, vel, string, fret, dead, scoop, cents, vib)` and its own `Rng(seed * 7919 + 31 * idx + 5)`, where `idx` counts notes since reset. Draw order at note-on is fixed: `K` draws (level scatter), `K` draws (`wob`), `KB` draws (burst scatter), then the finger noise (`int(0.03 * SR)` draws). At release, if the note was stopped, `int(0.012 * SR)` more draws for the tick. Tone controls that shape the partials (PLUCK, BRIGHT, SUSTAIN, AGE, ATTACK, THUMP, BODY's effect on decay and beating) are read at note-on and frozen for that note. A voice ends 0.6 s after release.

Each voice produces two signals: the **string** (`s`) and the **thump** (`th`, before its low-pass).

- **Pitch.** `f(t) = f0 * 2^(pc(t) / 1200)` with the scoop, the first-cycle bend (`BEND_CENTS`, `BEND_CYCLES`) and vibrato exactly as written. Phase is the running sum of `2 pi f / SR`.
- **Sustained partials**, `K = min(40, floor(5000 / f0))`. Each has two components: slow (`env`) and fast (`envf`), the fast one offset in frequency by `dk` hertz. Partial `k` runs at `k * sqrt(1 + Bs k^2)` times the phase, plus the constant `0.5 * (k mod 2)`.
- **Burst partials**, `KB = min(220, floor(11000 / f0))`, for the first 0.6 s only, three-stage envelope (snap, fast, keep), skipped for dead notes. Drop any partial at or above 0.45 times the sample rate.
- **Envelopes** are closed-form functions of time since note-on and since release (`relenv`). Evaluate them at a control rate of 8 samples and interpolate; evaluate pitch per sample or per 8 samples while it is moving (first 0.1 s, during a scoop, during vibrato).
- **Oscillators.** Up to about 300 sines per voice in the first 0.6 s. Use rotating phasors or a fast vector sine; plain `sin()` per partial per sample will be too slow. Stop computing a burst partial once its envelope is below 1e-5 of its start.
- **Thump**: the noise draws shaped by the two exponentials; the stop tick added at release.

**Shared stages**, in this order, all linear except the first:

1. Sum the strings of all voices, then `fingerboard()` (the one-sided clip; GROWL and AGE read continuously).
2. Sum the thumps and low-pass them once (`biquad_lp(900 + 1500 * BRIGHT, 0.7)`), with each voice's gain (`NOISE_LVL * attack * thump * vel`) applied before the sum.
3. **Body path**: `(string + thump)` through three high-passes, the eight `BODY` peaks and three `SHAPE` peaks (gains follow BODY) and the low-pass.
4. **Ring**: `(string + 2 * thump)` into a parallel bank of two-pole resonators equal to `body_ir()`: 40 body modes (frequency, level and decay from `body_modes.json`, decay `RING_DECAY * decay + RING_ADD`, phase `1.7 * i`) and 32 sympathetic-string modes (the four open strings times harmonics 1 to 8, each `(1 - exp(-t / 0.02))` times a decaying sine, which is the difference of two resonators). Each resonator's impulse response must equal its term in `body_ir()`: a sine of the given phase needs both `b0` and `b1`. Normalise as the Python does (the modes by the energy of their summed 0.45 s response, the sympathetic set by the energy of its 1.6 s response); compute those sums once at start-up, at the running sample rate. Ring gain `KNOCK * (0.4 + 0.6 * ATTACK) * BODY / 0.6`.
5. **Pickup path**: `(string + 0.3 * thump)` through its high-pass and low-pass.
6. Mix by MIC/PICKUP with the make-up gain.

Run everything at the engine sample rate; all constants are in hertz, seconds and dB per second. The sound check is at 48 kHz.

**CPU.** Report the cost of a 208 bpm walking line at 48 kHz. Aim for under 6% of one core. If it is over, the allowed economies, in this order, are: a coarser control rate for envelopes (16 samples), an earlier cut-off for dying burst partials (1e-4), and `KB` capped at 160. Each must still pass the sound check. Record what you did in DECISIONS.

## 6. Panel

28 HP. Same style as Comper. Render a preview PNG and look at it.

- **Display** (top, about 60 by 12 mm): the note playing ("F2"), the feel, the source (LINK, CABLE, LEARNING, PLAY or a dash), the chord it is on and the seed.
- **Knob band**, three captioned groups:

| Group | Controls |
|---|---|
| PLAYER | FEEL (7 positions: FOLLOW, WALK, TWO, BALLAD, BOSSA, SOUL, HOUSE), ACTIVITY, REGISTER, ROAM, OUT, REPEAT, SEED, NEW (button) |
| HANDS | LOOSE (large knob), AHEAD, and trimpots TIMING, WEIGHT, SLOP, PITCH |
| SOUND | ATTACK (large knob), PLUCK, BRIGHT, SUSTAIN, GROWL, BODY, MIC/PICKUP, THUMP, AGE, LEVEL |

- Lights: a PLUCK light on every note, a LINK light when Comper is attached.
- **Jacks.** Inputs: CLOCK, RESET, ROOT, QUAL, CHORD (poly), LAY OUT, V/OCT IN, GATE IN, VEL IN, and CV inputs for ACTIVITY, REGISTER, LOOSE, ATTACK. Outputs on a darker plate: OUT, V/OCT, GATE, VEL.

## 7. Parameters

| Control | Range, default | Field |
|---|---|---|
| FEEL | 0 to 6 snapped, 0 (FOLLOW) | `BP.feel` = position minus 1 |
| ACTIVITY | 0 to 1, 0.5 | `activity` |
| REGISTER | 34 to 46 (shown as a note name), 40 | `register` |
| ROAM | 0 to 1, 0.5 | `roam` |
| OUT | 0 to 1, 0.2 | `out` |
| REPEAT | 0 to 1, 0.7 | `repeat` |
| SEED | 0 to 999 snapped, 1 | seed |
| LOOSE | 0 to 2.5, 1 | `loose` |
| AHEAD | 0 to 1, 0.5 | `ahead` |
| TIMING, WEIGHT, SLOP, PITCH | 0 to 2, 1 | `timing`, `weight`, `slop`, `pitch` |
| ATTACK | 0 to 2.5, 1 | `Tone.attack` |
| PLUCK | 0.08 to 0.30, 0.17 | `pluck` |
| BRIGHT | 0 to 1, 0.5 | `bright` |
| SUSTAIN | 0 to 1, 0.5 | `sustain` |
| GROWL | 0 to 1, 0.35 | `growl` |
| BODY | 0 to 1, 0.6 | `body` |
| MIC/PICKUP | 0 to 1, 0.25 | `pickup` |
| THUMP | 0 to 1, 0.5 | `thump` |
| AGE | 0 to 1, 0.3 | `age` |
| LEVEL | 0 to 2, 1 | output gain |

CV inputs add 0.1 per volt to ACTIVITY, LOOSE (0.25 per volt) and ATTACK (0.25 per volt), and 1 semitone per volt times 1.2 to REGISTER; clamp to the knob's range. Tooltips in plain words (ATTACK: "The pluck: finger burst, finger noise and the knock of the body. 1 is as measured on a real bass").

## 8. Right-click menu

- Clock pulses per beat: 1, 2, 4, 24 (cable mode).
- Tune length: From RESET, 4, 8, 12, 16, 24, 32 bars (cable mode).
- V/OCT carries slides (on).
- Forget the learned tune.
- The 12 factory presets appear in Rack's preset menu (`presets/Walker/`).

Store menu options in the patch JSON.

## 9. The change to Comper

- Comper sends `CompLink` to Walker neighbours (section 4.1).
- When a Walker sits next to Comper, Comper treats ROOTS as off (a pianist stops playing roots when a bassist arrives). Menu option on Comper: "Keep roots with Walker attached" (off). Decide this from the neighbour's model only.
- Nothing else in Comper changes. Its golden tests must still pass untouched.

## 10. Tests (`make test`, no Rack needed)

Add to the existing harness (a second program, `test/render_walker.cpp`, is fine):

1. **Golden line, engine level.** For every case in `test/walker_golden.json`: build the chart, call the feel's start-up and its planning steps for `nbars` (cases with `blind` true: `blind_beat` per beat as `play_blind` does), pass each step's events through `perform()`, and compare the final notes with the file: same count; pitch, string, fret, dead and stopped equal; `t`, `dur`, `vel`, `scoop`, `cents`, `vib` within 1e-6. The fields are listed in each case. If a case differs the port is wrong; find the first differing random draw. Do not loosen the tolerance.
2. **Host against golden.** Run `WalkerHost` sample by sample at 48 kHz from a synthetic `CompLink` (steady tempo) on every non-blind case; the notes it starts must match in pitch and velocity with onsets within 1.5 samples and releases within 1.5 samples.
3. **Sound check.** Render every case of `test/upright_cases.json` through `Upright.hpp` (notes as given, the case's tone and seed, 48 kHz, the case's `total` seconds) to `NNN.wav` as 32-bit float mono, with no output scaling, then run `python3 tools/check_upright.py DIR`. It must report 0 failed. The allowed difference is 0.75 dB mean and 3 dB worst over 56 band-by-time cells, and 0.5 dB overall. `python3 tools/check_upright.py --self` shows the tool passing on the reference itself. If the check fails, the port is wrong; do not move the tolerances or the constants.
4. **Host behaviour.** RESET twice gives the same line twice. A tempo ramp from 100 to 160 bpm leaves no stuck notes. Not running: no new notes. LAY OUT silences and returns. Cable mode: a 12-bar tune fed as ROOT and QUAL steps plays blind for one pass, then from the learned table, and with "Tune length 12 bars" the second pass matches the line the engine gives for that chart. PLAY mode: gates in give notes out, and a second pluck on the same string stops the first.
5. **Line checks**, on the C++ engine over the golden blues: the root on beat 1 in at least 90% of bars in WALK; no repeated consecutive pitches in WALK at ACTIVITY 0; every pitch within MIDI 28 to 55; note count rising with ACTIVITY.
6. **CPU**, as in section 5.

Have the test write a MIDI file and a WAV for each preset (16 bars) from the C++ host.

## 11. Demos

Trios: Comper's own notes through Joanna (as `tools/render_demos.cpp` already does), Walker's audio from the C++ engine, and quiet General MIDI drums as in `tools/make_demos.py`. One per preset pair, plus: the bass alone on a blues; ACTIVITY at 0.2, 0.5 and 0.95; LOOSE at 0, 1 and 2; ATTACK at 0, 1 and 2.2; a tone tour (gut and soft, steel and ringing, GROWL full, PICKUP only, AGE full). Balance the bass against the piano by loudness as the design sketches did (equal RMS, drums at 0.4).

## 12. Manual

A Walker chapter in `docs/MANUAL.md` and the PDF. Plain language. Contents: what it is; quick start (put it next to Comper, patch OUT to a mixer); the three ways it can be driven (link, cables, played as a voice); the feels; what each PLAYER control does to the line, with the words walking, two-feel, approach note, skip and dead note explained once; the hands, and why the bass sits ahead of the beat; the sound controls, with ATTACK first; the outputs and using the notes on another voice; the 12 presets and their Comper partners; patch recipes (piano trio with a drum module, duo ballad, bass as a sequenced voice, bass line driving a synth bass alongside the upright, cable mode from another chord sequencer); limits (4/4, pizzicato only, the blind first pass in cable mode).

## 13. Notes for the builder

- Record anything you had to decide that this document did not in `docs/DECISIONS.md` under a Walker heading.
- Order of work: engine and golden line test first, then the sound engine and its check, then the host, then the module and panel.
- The sound check is the point of the sound design: it is how a model that cannot hear holds a fitted sound. Get it passing before tuning CPU.
- Things I am least sure of and want reported back, not silently changed: the soul and house cells; whether `LEAD` is long enough at very slow tempos with AHEAD at 1; the 4 ms late decision in cable mode; whether six voices are enough for long ballad notes.
- Known open point from the design: individual harmonic levels are within about 7.5 dB of the recordings since the body began to ring (they were within 6). Leave it; note it in the manual's limits only if asked.
