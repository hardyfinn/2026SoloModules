# Comper: build instructions

Design by Fable, 2026-11-23. For the model that builds it. `RESEARCH.md` in this folder says why the module exists and what was measured on the first prototype. Where the two differ, this document wins.

The **reference engine** is `tools/proto/comper_ref.py`. It is the definition of what the module plays. Where prose here is ambiguous, the Python decides. `tools/proto/check_ref.py` shows the checks the reference passes; `tools/proto/make_reference.py` writes `test/golden.json` (99 cases, 8,129 notes) and `test/detect.json` (676 chord-recognition cases), which the C++ must reproduce. `tools/proto/presets.py` holds the 12 factory presets and 14 built-in progressions and is part of the design.

Hardy's decisions (2026-11-23): VCV Rack, not Max for Live. Notes out only, no sound of its own; the piano is **Joanna** in this same repository (poly V/OCT with 0 V = middle C, poly GATE, poly VEL 0 to 10 V read at each key press, SUS pedal CV 0 to 10 V). One module. Chord recognition from a poly input is in version 1. No right-hand fill engine in version 1.

## 1. What to build

- Plugin folder `ComperFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-7vm3pn` (not main). Use `SawstackFable` in this working tree as the template for the Makefile, plugin.json, panel script, Haer logo, offline test harness, PDF tooling and README. The repository workflow already builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `ComperFable`, name `Comper Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `Comper`, name `Comper`, tags Sequencer, Polyphonic, Random, Clock. Description: "Jazz piano comping player: rootless, shell, quartal and spread voicings with real voice leading, swung two-bar rhythm figures that push into the next chord, a LOOSE knob for human hands, and a LISTEN input that leaves room for a soloist. Notes out; bring your own piano". The word COMP must be findable in Rack's browser (it is, through the name).
- Panel 36 HP (182.88 mm), text labels, the Haer logo as on Sawstack.
- Deliverables: C++ module, panel SVG from a script, 12 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README with a row added to the repository README table if one exists, demo WAVs.

## 2. Architecture

Three layers. Keep them in separate headers under `src/dsp/` with no Rack includes in the first two, so the offline tests can compile them.

1. **`Engine`** (`Engine.hpp`): a line-by-line port of `Rng`, the tables, `Params`, `Ctx`, `Engine` and `detect_chord` from `comper_ref.py`. Doubles everywhere, the same order of operations and of random draws, candidates iterated in the same order, strict `<` when taking a minimum (the first minimum wins). `int(x)` in the Python is truncation of a non-negative double. `math.floor(x + 0.5)` stays exactly that. The chart callbacks in `Ctx` can be `std::function` or a small interface. The engine knows nothing about time; it is asked once per bar (`begin_bar`) and once per slot (`decide`), and answers with notes whose times are offsets in seconds from the slot's nominal time.
2. **`Host`** (`Host.hpp`): the real-time version of `simulate()`. Owns the beat clock, the chord source, the soloist follower, the queue of note-ons and note-offs, long-note release, the pedal, and voice allocation. Sample-driven: `void process(const HostIn&, HostOut&, double sampleTime)`. No Rack types.
3. **`Comper.cpp`**: the Rack module. Reads params and jacks into `HostIn`, writes `HostOut` to the jacks, draws the display.

## 3. What the engine does (summary; the Python is the definition)

- **Voicings.** Ten chord qualities (`Q_NAMES`), four voicing styles plus MIXED. Each style is a short list of recipes (semitones above the root, bottom to top, almost never containing the root). Every recipe is realised at every allowed position of its lowest note (`WINDOW`, shifted by REGISTER), optionally opened (drop-2) when SPREAD is up, and thinned to VOICES notes keeping the guide tones.
- **Voice leading.** From all candidates, pick the cheapest: LEAD times (hand movement plus 1.5 times top-note movement), plus distance of the top note from a slowly wandering target, plus a muddiness penalty for close intervals low down, plus a little randomness. On a repeated chord the previous voicing is usually kept (70%) and otherwise re-voiced.
- **Rhythm.** Each groove has sparse, medium and busy one-bar cells. DENSITY weights the three pools. A two-bar figure is held for REPEAT of the time, otherwise replaced; a kept figure is sometimes varied by one hit. Slots are swung eighths (SWING, BOSSA, BALLAD) or sixteenths (SOUL, HOUSE). Swing follows tempo unless SWING is turned.
- **Push.** An upbeat hit takes the chord of the beat it leads into (probability PUSH), so chords arrive early the way a pianist plays them.
- **Out.** Dominants become altered, or are replaced by the dominant a tritone away, more often when they resolve down a fifth and most often into a minor chord. Colour recipes (#11, upper-structure triads) join the candidates. A soft chord a semitone off can be slipped in one slot before a hit; the hit it leads to is then guaranteed.
- **Hands.** Every chord gets: a lay-back of 12 ms on the beat and 6 ms off it, timing scatter, a roll of 4 to 18 ms (3.5 times that in BALLAD) bottom to top 70% of the time, top-first 15%, scattered 15%, a louder top note and quieter inner notes, accents on long upbeat hits, a slow loudness drift, staggered releases, and occasionally a dropped inner note or a ghosted chord. LOOSE scales all of it; TIMING, ROLL, WEIGHT and SLOP trim each family. LOOSE 0 is a sequencer: one velocity, no roll, on the grid.
- **Listening.** While the soloist is sounding, hits are mostly dropped and the survivors are short and soft. In the first two and a half beats after the soloist stops, the figure's hits are certain, extra answers may appear on upbeats, and everything is a little louder. With a solo pitch patched, the top-note target stays at least four semitones under the soloist's lowest recent note.
- **Roots** for solo piano: a low root (MIDI 32 to 43) with the chord on beat one and whenever the chord is new.
- **Pedal**: lifts as the hands land and re-catches 70 ms later under long chords.

## 4. The host, in detail

### 4.1 Clock and transport

- `phi`: a double, beats since reset. Four beats to the bar, always (4/4 only in version 1; say so in the manual).
- CLOCK unpatched: tempo from the TEMPO knob, `phi += dt * bpm / 60`.
- CLOCK patched: pulses per beat from the menu (1 default, 2, 4, 24). Measure the period between the last two pulses (median of the last three once available) to get `spb`. `phi` advances at `1/spb` between pulses. At each pulse, snap `phi` to the nearest multiple of `1/ppb`, except the first pulse after a reset, which is beat 0. If no pulse arrives, let `phi` run on for at most half a pulse interval past the expected pulse, then hold. Before two pulses have been seen, use 120 bpm.
- RUN (button toggles, gate input high = run when patched). Not running: all gates low, pedal 0, `phi` frozen. Starting again continues from where it stopped.
- RESET (trigger): `phi = 0`, bar 0, every note off, queue cleared, chart at step 1, and the engine re-seeded from SEED so the same take plays again (menu option "Reset replays the same take", on by default; when off, only the position resets).
- NEW button: SEED becomes a new random integer 0 to 999 and the engine is re-seeded on the next bar line.
- A changed SEED knob re-seeds at the next bar line (`Engine::reset(seed)`, keeping `prev` voicing would break reproducibility, so do a full reset).

### 4.2 Chord source

Priority: POLY (either CHORD jack patched), else CV (ROOT jack patched), else CHART.

- **CHART.** STEPS (1 to 16) steps, each BEATS (1, 2, 4 or 8) beats long, each with a ROOT trimpot (12 positions, C to B) and a QUAL trimpot (10 positions in `Q_NAMES` order). `chord_at(beat)` = step `floor(beat / BEATS) mod STEPS`, root plus KEY. `next_change(beat)` = the first different chord after that step, searching forward cyclically; none if the whole chart is one chord. Decisions are taken `LEAD_BEATS` (0.12) beats before each slot's nominal time, with `ctx.lead_s = 0.12 * spb`.
- **CV.** ROOT is V/oct: nearest semitone, pitch class only, plus KEY. QUAL is 0 to 10 V, one quality per volt (`clamp(floor(v), 0, 9)`); unpatched, the quality comes from the QUAL trimpot of step 1. The module cannot see the future here, so `chord_at` returns the present chord for any beat, `next_change` is null, and decisions are taken `EXT_LATE_S` (4 ms) *after* the slot's nominal time (`ctx.lead_s = -0.004`). Pushes therefore do not happen in this mode; the manual must say that the chart is the way to get them.
- **POLY.** CHORD V/OCT (poly) and CHORD GATE (poly). Sounding notes are the channels whose gate is above 1 V, or all channels if the gate jack is unpatched. Convert to MIDI (`round(v * 12) + 60`), run `detect_chord(notes, last_quality)`. A new result must be stable for 3 ms before it is taken. When nothing sounds, keep the last chord. KEY does not apply. Timing as in CV mode.
- **When an external chord changes** (CV or POLY): release every held long note now; then, unless the current slot holds a figure hit or a hit landed within the last 0.15 beat, strike the new chord at once with probability `0.5 + 0.5 * DENSITY` (`Engine::strike(bar, slot, L_, -4, ctx, follow=true)`, with `slot` the most recent slot). `simulate()` shows the exact order of the random draw and the calls; port that order. In the real-time host the change is noticed on the sample it happens, not at the next slot; that difference from `simulate()` is expected.

### 4.3 Deciding and scheduling

- Before the first decision of each bar: copy the current knob values into `Engine::p`, call `begin_bar(bar, bpm)`.
- For each slot, when `phi` crosses `bar*4 + slot_beats[slot] - lead` (lead = 0.12 beat in CHART mode, minus 4 ms in the others): build `Ctx` (spb, lead_s, chord callbacks, soloist state), copy the knobs that may change inside a bar (everything except groove), call `decide`.
- Each returned note has `t` (seconds from the slot's nominal time), pitch, velocity, and either a duration (short notes: off at `on + dur + extra`) or `long`.
- **Long notes** are held in a list with their nominal strike time `T_p` and a cap (`cap_beats()` times spb: 2.6 beats, 3.8 in BALLAD). They are released when the next *hit* is decided (side-slips do not release): `off = min(T_p + cap, max(T_p + 0.9*spb, T_next - 0.12*spb))`, then `off = max(off, min(now, T_p + cap))`, then add the note's own `extra`. With no next hit they end at the cap. A note is never shorter than 20 ms.
- All note-ons and note-offs go into one queue ordered by sample time. Once queued, an event does not move if the tempo changes.
- Pedal events are queued the same way.

### 4.4 Voice allocation and the outputs

- Channels: 12 by default; menu choice 8, 12 or 16.
- Note-on: if the same pitch is already sounding on a channel, drop that channel's gate now, cancel its pending note-off, and raise the gate again on the same channel 2 ms later with the new velocity. Otherwise take the channel whose gate has been low longest (at least 1 ms). If none is free, steal the channel with the oldest note-on: gate low now, new note 2 ms later.
- V/OCT and VEL of a channel are written on the same sample its gate rises and then held. V/OCT = `(pitch - 60) / 12`. VEL = `velocity / 127 * 10` V. GATE is 10 V.
- PEDAL: 0 to 10 V (`value * 10`), slewed over 5 ms.
- BASS V/OCT: the root of the chord the source says is current (after KEY, before any substitution), placed in MIDI 28 to 39, as V/oct. BASS GATE: high while running, dropping to 0 for 5 ms at every chord change (menu: "Bass retriggers every bar" adds bar lines).
- CHANGE: 1 ms, 10 V trigger at every chord change of the source. EOC: 1 ms trigger when the chart wraps to step 1 (CHART mode only).
- SCALE: polyphonic, one channel per note of `SCALES[quality]`, at `(root + degree) / 12` V (ascending from the root, 0 V = C4). For quantisers and weighted-random pitch modules, so a soloist can stay in the changes.

### 4.5 The soloist follower

- SOLO takes a gate or audio. `sounding` is true if `|v| > 0.5 V` at any time in the last 30 ms.
- `solo_gap`: beats since `sounding` last went false (99 if it never has). `solo_recent`: `sounding` went true within the last 2.5 beats.
- SOLO V/OCT (optional): sampled 2 ms after each rise of `sounding` and whenever it moves by more than half a semitone while sounding; keep the last 16 notes with their end times; `solo_low` is the lowest pitch among notes that ended less than 2 beats ago or are still sounding, as a MIDI number; none if the jack is unpatched.
- `solo_connected` is whether the SOLO jack is patched. LISTEN does nothing without it.

## 5. Panel

36 HP. Keep the grouping below; adjust positions so nothing collides. Render a preview PNG and look at it before finishing.

**Chart band (top, y about 9 to 52 mm).**
- A display, about 118 by 15 mm at the left. It shows, drawn live: the current chord large ("Dm7"), the next chord smaller with an arrow, the source (CHART, CV or POLY), the notes of the last voicing played ("F3 A3 C4 E4"), and the seed. Use Rack's built-in font; note names from `NOTE_NAMES`, octave = `pitch / 12 - 1`.
- To the right of the display: STEPS, BEATS, KEY (small knobs).
- Sixteen columns, about 10.4 mm apart: a step light, the ROOT trimpot, the QUAL trimpot, and under them the step's chord name drawn live in small text (dim for steps beyond STEPS). The light of the current step is lit.

**Knob band (y about 58 to 90 mm)**, four captioned groups:

| Group | Controls |
|---|---|
| VOICING | STYLE, SPREAD, REGISTER, VOICES, LEAD, OUT, ROOTS (three-position switch) |
| RHYTHM | GROOVE, DENSITY, SWING, REPEAT, PUSH, SEED, NEW (button) |
| HANDS | LOOSE (large knob), TIMING, ROLL, WEIGHT, SLOP (trimpots), PEDAL |
| BAND | LISTEN, TEMPO, RUN (lit button) |

Lights: a HIT light that flashes on every strike, a SOLO light while the soloist is sounding.

**Jack rows (y about 100 and 114 mm).** Inputs: CLOCK, RESET, RUN, KEY, ROOT, QUAL, CHORD V/OCT, CHORD GATE, SOLO, SOLO V/OCT, and CV inputs for DENSITY, OUT, LOOSE, LISTEN, REGISTER. Outputs on a darker plate: V/OCT, GATE, VEL, PEDAL, BASS V/OCT, BASS GATE, CHANGE, EOC, SCALE. Mark polyphonic jacks (CHORD V/OCT, CHORD GATE, V/OCT, GATE, VEL, SCALE) with the word POLY or a thicker ring, consistently.

## 6. Parameters

| Param | Range | Default | Meaning |
|---|---|---|---|
| ROOT 1 to 16 | 0 to 11, snapped | chart of "ii-V-I major" | C, Db, D ... B. Tooltip shows the note name |
| QUAL 1 to 16 | 0 to 9, snapped | as above | maj7, 6, m7, m6, mMaj7, 7, 7alt, m7b5, dim7, 7sus |
| STEPS | 1 to 16, snapped | 4 | Chart length |
| BEATS | 1, 2, 4, 8 | 4 | Beats per step |
| KEY | -6 to +5 semitones, snapped | 0 | Transposes the chart and CV chords. KEY CV is V/oct, rounded to a semitone, added |
| STYLE | 0 to 4, snapped | 1 | SHELL, ROOTLESS, QUARTAL, SPREAD, MIXED |
| SPREAD | 0 to 1 | 0.25 | Close to open (drop-2) voicings |
| REGISTER | 60 to 79 (MIDI) | 69 | Where the top note sits. Display as a note name. CV: 1 V per octave around the knob (12 semitones per volt), clamped |
| VOICES | 2 to 5, snapped | 5 | Most notes in a chord |
| LEAD | 0 to 1 | 1 | Voice-leading strength |
| OUT | 0 to 1 | 0 | Altered dominants, substitutes, side-slips. CV 0 to 10 V adds 0 to 1 |
| ROOTS | 0, 1, 2 | 0 | OFF, SOME, ALL |
| GROOVE | 0 to 4, snapped | 0 | SWING, BOSSA, BALLAD, SOUL, HOUSE |
| DENSITY | 0 to 1 | 0.5 | CV 0 to 10 V adds 0 to 1 |
| SWING | -1 to 1 | 0 | 0 is automatic. Tooltip shows the resulting ratio |
| REPEAT | 0 to 1 | 0.6 | Keep the two-bar figure |
| PUSH | 0 to 1 | 0.85 | Upbeat hits take the coming chord |
| SEED | 0 to 999, snapped | 1 | |
| NEW | button | | New seed |
| LOOSE | 0 to 2.5 | 1 | CV 0 to 10 V adds 0 to 2.5 |
| TIMING, ROLL, WEIGHT, SLOP | 0 to 2 | 1 | Trims, multiply LOOSE |
| PEDAL | 0 to 1 | 0 | Pedal depth; 0 never pedals |
| LISTEN | 0 to 1 | 0 | CV 0 to 10 V adds 0 to 1 |
| TEMPO | 40 to 260 bpm | 120 | Used when CLOCK is unpatched |
| RUN | toggle | on | |

All CV sums are clamped to the parameter's range. NEW and RUN must not be touched by Rack's randomise; nor should the chart, SEED, TEMPO or KEY (set `randomizeEnabled = false` on them).

Everything, including the chart, is a parameter, so a preset recalls a whole tune.

## 7. Right-click menu

- Clock pulses per beat: 1, 2, 4, 24.
- Output channels: 8, 12, 16.
- Bass retriggers every bar (off).
- Reset replays the same take (on).
- Load progression: the 14 entries of `PROGRESSIONS` in `presets.py`. Loading sets STEPS, BEATS and the ROOT and QUAL knobs, nothing else.
- Type a chart: a text field. Tokens separated by spaces; `|` is ignored; each token is one step; `%` repeats the previous step; chord spelling as `parse_chart` accepts (C, C#, Db ... then maj7, M7, 6, 6/9, m, m7, m9, m11, m6, mMaj7, 7, 9, 13, 7alt, 7b9, 7#9, m7b5, dim, dim7, sus, 7sus; a bare letter is a 6 chord). Up to 16 steps. If any token fails to parse, change nothing and show the bad token in the field.
- The 12 factory presets appear in Rack's own preset menu (the `presets/Comper/` folder).

Store the menu options in the patch JSON.

## 8. Presets

`tools/proto/presets.py` defines 12 presets and an init patch. Write `tools/proto/export_presets.py` (as in Sawstack) to turn them into `.vcvm` files in `presets/Comper/`: each preset names a progression (sets STEPS, BEATS, the chart knobs) and a tempo (the TEMPO knob), then parameter values; everything else is default.

## 9. Tests (`make test`, no Rack needed)

`test/render.cpp` compiles `Engine.hpp` and `Host.hpp` and must check:

1. **Golden, engine level.** Port `simulate()` into the test and reproduce every case in `test/golden.json`: same number of notes, same pitch and velocity for each, on and off times within 1 microsecond. Convert the JSON to a header with a small script (as Sawstack does with `ref_bands.h`) or parse it directly. If a case differs, the port is wrong; do not loosen the tolerance. Find the first differing random draw.
2. **Chord recognition.** Every row of `test/detect.json`.
3. **Host against golden.** Run the real `Host` sample by sample at 48 kHz with the internal clock on every golden case that has no soloist and is not external. Reading its gate, V/OCT and VEL outputs back into notes must give the same pitches and velocities, with on times within 1.5 samples plus any 2 ms retrigger delay the allocator had to add. Report how many notes needed that delay.
4. **Host behaviour.** With an external clock at a steady tempo the result matches the internal clock within 2 ms. A tempo ramp from 100 to 160 bpm leaves no stuck notes. RUN off drops every gate within one sample. RESET twice gives the same notes twice. No channel ever carries two notes at once; every gate is low at least 1 ms before it rises again. With 8 channels and DENSITY 1 nothing crashes and stolen notes are counted.
5. **The checks in `check_ref.py`**, repeated on the C++ engine: voice leading on under 1.6 semitones per voice and off over 2.5; a guide tone in every non-quartal voicing; DENSITY monotone; LISTEN cuts hits on top of the soloist to under 60% of the unlistening figure; LOOSE 0 is one velocity and no roll.
6. **Listening through the host.** Feed the solo line of a golden listening case as a gate signal; hits on top of the soloist must fall by at least a third against LISTEN 0.
7. **CPU.** `process()` averaged over a busy patch must be far under 1% of a core at 48 kHz; report the figure.

Have the test write a MIDI file for each preset (16 bars) from the C++ host.

## 10. Demos

Render one WAV per preset from the C++ host's MIDI, plus four demonstrations: LOOSE at 0, 1 and 2 on the same blues; LEAD 0 then 1; OUT 0 then 0.9; LISTEN 0 then 1 with the solo line audible. Joanna is in this repository with an offline harness (`make test` in `Joanna/`); if its engine can be driven from note lists without much work, render the demos through it, since that is the pairing Hardy will use. Otherwise use a General MIDI piano through fluidsynth (`apt-get update && apt-get install -y fluidsynth fluid-soundfont-gm` works in the cloud container). For context, add the walking bass and ride cymbal from `tools/proto/sketch_band_v0.py` (`walk_bass`, `swing_drums`, `simple_bass`, `straight_drums`) under the swing, bossa, soul and house demos, kept quiet. Say in the README which piano made the demos. Copy the demos and the manual PDF to `/mnt/project-files/jazz-comping-vcv/`.

## 11. Manual

`docs/MANUAL.md` and a PDF made with the Sawstack tooling. Plain language; Hardy knows VCV Rack well and does not write C++. Explain jazz terms the first time they appear (shell, rootless, quartal, guide tones, tritone substitute, side-slip, comping itself). Contents: what it is and is not (4/4 only, notes only); quick start with Joanna (V/OCT, GATE, VEL, PEDAL to SUS); the chart, typing a chart, the progressions; the three chord sources and what each can and cannot do (pushes need the chart); every control; the 12 presets; and at least ten patch recipes, including: trio with a bass voice from BASS and a soloist quantised by SCALE; duo with LISTEN; chords from a sequencer by ROOT and QUAL CV; chords played on a MIDI keyboard into POLY; house stabs into House Keys; two Compers as left and right hand (different REGISTER and STYLE, same SEED or not); LOOSE and DENSITY under an LFO for a set that breathes; ballad with pedal; a Comper driving a pad instead of a piano; weighted-random melody over the changes.

## 12. Notes for the builder

- Record anything you had to decide that this document did not in `docs/DECISIONS.md`.
- The golden test is the point of the design: it is how a model that cannot hear checks a musical engine. Get it passing before building the panel.
- Things I am least sure of and want reported back, not silently changed: the bossa, soul and house cells; whether 4 ms late decisions in CV and POLY mode are enough when the chord arrives through several cables (each cable adds a sample); the 30 ms soloist hold.
