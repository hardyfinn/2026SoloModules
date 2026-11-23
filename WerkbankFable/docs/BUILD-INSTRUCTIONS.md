# Werkbank: build instructions

Design by Fable, 2026-11-23. For the model that builds it. `RESEARCH.md` in this folder says why the set is what it is.

The **reference engines** are in `tools/proto/` and are the definition of the sound. Where prose here is ambiguous, the Python decides. They are per-sample and written to be ported line for line.

| File | Defines |
|---|---|
| `wb_common.py` | splitmix64 unit personality, xorshift32 noise, polyBLEP, slow drift |
| `zap_blech_ref.py` | ZAP voice, BLECH six-voice drum engine and its three BOX tables |
| `dynamo_orch_ref.py` | DYNAMO voice, ORCHESTER key voice and its eight disc tables |
| `vokoder_ref.py` | VOKODER and its internal carrier |
| `sprecher_ref.py`, `words.txt` | SPRECHER: phoneme table, dictionary, letter-to-sound rules, scheduler, FORMANT and CHIP engines |
| `make_reference.py` | renders 40 test cases to WAV and writes `ref_bands.json` |

Reference renders are in `/mnt/project-files/kraftwerk-vcv/reference/`.

## 1. What to build

- Plugin folder `WerkbankFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-bp6v0t` (not main). Use `SawstackFable` and `TapeizerFable` on the same branch as templates for the Makefile, plugin.json, panel script (`res/make_panel.py`, `res/haer_logo.py`), offline test harness (`test/render.cpp`, `make test`), PDF tooling (`tools/make_pdf.py`, `tools/pdf.js`) and README. The repository workflow already builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `WerkbankFable`, name `Werkbank Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. Seven modules, slugs and names below.
- **No band name, song title, album title or studio name** on any panel, in plugin.json, in preset names or in the manual title. The manual and README may say once, in prose, that the set is inspired by the instruments used on German electronic pop records of 1975 to 1981, name the band there, and say it is not affiliated.
- Panels: text labels, the Haer logo, same visual language as Sawstack. Render a preview PNG of every panel and look at it before finishing.
- Deliverables: seven C++ modules, panel SVGs from a script, factory presets, offline tests, MANUAL.md, one PDF manual for the set with patch recipes, README, demo WAVs rendered by the C++ engines into `/mnt/project-files/kraftwerk-vcv/demos/`.

| Slug | Name | HP | Tags | One-line description |
|---|---|---|---|---|
| `Takt` | TAKT | 36 | Sequencer, Quantizer | Two-row 16/32-step pitch sequencer with built-in interval quantiser, live transpose and eight patterns |
| `Blech` | BLECH | 26 | Drum, Synth voice | Six analog rhythm-box drum voices played by triggers or pads, with DYNAMICS, PITCH and TONE pedal inputs |
| `Zap` | ZAP | 12 | Drum, Synth voice, Polyphonic | Synth percussion: a pinged resonant filter with a swept pitch |
| `Vokoder` | VOKODER | 28 | Vocoder, Effect | 20-band vocoder with voiced/unvoiced switching, formant shift, freeze and a built-in carrier |
| `Sprecher` | SPRECHER | 28 | Speech, Synth voice | Speech synthesiser: type words, clock them, sing them by V/OCT. Formant and speech-chip engines, seven languages for numbers |
| `Orchester` | ORCHESTER | 16 | Synth voice, Polyphonic | Optical-disc keyboard: looped choir, strings, flute and five more, with disc wobble |
| `Dynamo` | DYNAMO | 22 | Synth voice, Polyphonic | Two-oscillator ladder-filter bass and lead voice |

Voltage conventions for every module: audio 1.0 unit = 5 V. Triggers and gates rise at 1 V with a Schmitt trigger (0.1 V low, 1 V high). V/OCT 0 V = C4. Outputs are clamped to ±11 V.

**AGE** (BLECH, ZAP, VOKODER, SPRECHER, ORCHESTER, DYNAMO): a knob 0 to 1, default 0.3. What it scales is in each engine. The unit personality comes from `unit(seed, n)` in `wb_common.py`; the seed is random when the module is created, saved in the patch JSON, and replaced by the context-menu item "New unit". Hiss, hum and bleed only appear above AGE 0.6, so every module is silent at rest at the default. Smooth every knob with a 5 ms one-pole. Do not smooth CV inputs.

## 2. TAKT

No audio. All logic below; there is no Python for it.

**Controls**
- Row A and row B, 16 steps each. Per step: a vertical slider (0 to 1, default 0), a three-position switch **PLAY / TIE / REST** (default PLAY), a latching **ACCENT** button with a light, and a step light above the slider.
- Per row: **LAST** knob (1 to 16, snapped, default 16).
- **RANGE** switch: 1, 2 or 4 octaves of slider travel (default 2).
- **CHAIN** switch: PARALLEL (two 16-step rows running together) or SERIES (one 32-step row: A then B; in SERIES the row B jacks carry the same signals as row A, and LAST B sets where the 32-step loop ends when LAST A is 16. If LAST A is below 16 the loop is A only).
- A one-octave row of 12 **KEY** buttons with lights, laid out like a keyboard, and a **KEYS** switch: SCALE or TRANSPOSE. In SCALE the keys toggle which notes are allowed (default all 12 on; at least one must stay on). In TRANSPOSE the keys are radio buttons choosing a transposition of 0 to 11 semitones (default 0). The lights show whichever the switch selects.
- **WHEN** switch: NOW or BAR. BAR means a new transposition (from keys or the TRANSPOSE input) takes effect only when row A plays step 1.
- **GATE** knob: gate length 5% to 100% of the step (default 50%). At 100% consecutive PLAY steps still re-trigger: the gate drops for 1 ms.
- **PATTERN** knob: 1 to 8, snapped. **RUN** button (latching, lit, default on) and **RESET** button.

**Jacks**
- In: CLOCK, RESET, RUN (gate; when patched it replaces the button), TRANSPOSE (V/OCT, rounded to the nearest semitone and added to the key transposition), GATE CV (±5 V sweeps the knob range), PATTERN CV (1 V per pattern, added to the knob, clamped 1 to 8).
- Out, per row: V/OCT, GATE, ACCENT (10 V while the gate of an accented step is high). Plus END (1 ms trigger when row A wraps to step 1).

**Behaviour**
- Each rising CLOCK edge advances both rows (or the chained row). After RESET, or on load, the next clock plays step 1. RESET within 1 ms of a clock counts as before it.
- Pitch of a step: `semis = round(slider × 12 × RANGE)`, then moved to the nearest allowed note of the SCALE mask (compare by semitone distance, ties go down; the mask repeats every octave with C at 0). Output volts = `semis / 12 + transposition / 12`. The pattern is quantised first and transposed second, so a transposed pattern keeps its shape exactly.
- PLAY: the pitch updates and the gate goes high for GATE × the clock period (the period is the time between the last two clocks; 125 ms before two clocks have arrived). TIE: the pitch updates and the gate stays high from the previous step through the whole of this one with no gap (if the previous step was REST, TIE behaves as PLAY with a full-length gate). REST: no gate; the pitch output holds.
- Slider tooltips show the resulting note name (for example "G3"), using the current RANGE and SCALE.
- Patterns: each of the 8 patterns stores all 32 sliders, 32 modes, 32 accents and both LAST values. The panel always shows and edits the current pattern. A pattern change from the knob or CV takes effect when row A next plays step 1 (immediately if stopped). Context menu: Copy pattern, Paste pattern, Clear pattern, Randomise pitches in scale. Patterns are saved in the patch JSON.
- Default content of pattern 1 (so the module does something when first patched): row A sliders give semitones `0 12 0 7 0 12 3 10 0 12 0 7 5 12 3 15` at RANGE 2, modes `P R P P P R P P P R P P P P R P`, accents on steps 1 and 9. Patterns 2 to 8: all sliders 0, all PLAY.

**Panel (36 HP, 182.88 mm).** Left block (x up to about 44 mm): PATTERN, RUN, RESET, RANGE, CHAIN, GATE, WHEN, KEYS switch and the 12 keys, then the six input jacks. Right block: 16 columns at 8.4 mm pitch starting at x = 52 mm. Row A: step lights y 12, sliders y 16 to 44, mode switches y 50, accent buttons y 57. Row B: lights y 66, sliders y 70 to 98, switches y 104, accents y 111. LAST A and LAST B at the right end of each row. Output jacks along the bottom right on a darker plate: A V/OCT, A GATE, A ACCENT, B V/OCT, B GATE, B ACCENT, END. Move things as needed so nothing collides; keep the grouping.

**Presets (8):** Init; Octave Motor (the default pattern); Fifths Climb; Minor Arpeggio 5 against 16 (LAST B = 5); Bass and Answer (two rows); Thirty-Two (SERIES); Rest Machine (many RESTs, accents); Tied Line (TIEs for glide).

## 3. BLECH

Engine: `blech_render` and `BOX` in `zap_blech_ref.py`. Port it exactly, including the one shared noise source and the trigger-pulse feed-through.

**Controls**
- Six voice columns, in this order: BASS, SNARE, HAT, CYMBAL, TOM, CLAVE. Each has a large round **pad** button (momentary, styled as a metal plate, with a light that flashes on a hit), a TUNE knob (-1 to +1 octave, default 0), a DECAY knob (multiplier 0.25× to 4×, exponential, default 1×), a TRIG input and an output jack.
- **BOX** switch, three positions: 1, 2, 3 (default 1).
- Three pedal controls, each a knob plus a CV jack: **DYNAMICS** (0 to 1, default 0.7; CV 0 to 10 V replaces the knob when patched, as a real pedal would), **PITCH** (-1 to +1 octave, default 0; CV ±5 V adds), **TONE** (0 to 1, default 1; CV ±5 V adds, 10 V = full range).
- **ACCENT** input: if high when a trigger arrives, that hit's value is 1.6 instead of 1.0.
- **AGE**, **LEVEL** (0 to 1, default 0.8; 1 = unity), **MIX** output.
- Context menu: "Trigger voltage sets level" (off by default; when on, hit value = trigger volts / 10 × 1.6, minimum 0.2), "New unit".

**Behaviour.** MIX is the sum of the six voices × LEVEL. A voice whose own output is patched is removed from MIX. Mapping to the engine: `tune[v]`, `decay[v]`, `dyn`, `ped_pitch`, `tone`, `age` as named. Changing BOX takes effect on each voice's next hit.

**Panel (26 HP).** Left block: BOX, DYNAMICS, PITCH, TONE with their jacks, ACCENT, AGE. Six columns to the right: pad (y about 28), TUNE (52), DECAY (70), TRIG jack (92), output jack (112, darker plate). LEVEL and MIX at the far right or under the left block.

**Presets (8):** Init Box 1; Soft Box 2; Electro Box 3; Low and Long (TUNE down, DECAY up); Tight and High; Dull Pedal (TONE 0.3, DYNAMICS 0.4); Hot Pedal (DYNAMICS 1); Tired Box (AGE 1).

## 4. ZAP

Engine: `zap_render` in `zap_blech_ref.py`.

| Control | Range | Default | Notes |
|---|---|---|---|
| START | 20 Hz to 16 kHz, exponential | 4 kHz | pitch at the moment of the hit |
| END | 20 Hz to 16 kHz, exponential | 100 Hz | pitch it settles to. May be above START |
| SWEEP | 1 ms to 1 s, exponential | 30 ms | time constant of the pitch envelope |
| RESONANCE | 0 to 1 | 0.8 | ring length; above 0.9 the filter oscillates by itself |
| DECAY | 5 ms to 8 s, exponential; the top of the knob is HOLD | 0.4 s | VCA time to -60 dB. HOLD (knob at maximum) = `decay >= 8`, VCA stays open |
| NOISE | 0 to 1 | 0 | noise burst into the filter |
| CLICK | 0 to 1 | 1 | strength of the kick |
| DRIVE | 0 to 1 | 0 | output saturation |
| AGE | 0 to 1 | 0.3 | |
| LEVEL | 0 to 1 | 0.8 | |

- Inputs: TRIG, V/OCT (shifts START and END together), ACCENT (hit value 1.6 if high at the trigger), and CVs for START and END (1 V/oct), SWEEP and DECAY (time doubles per volt), RESONANCE (0.1 per volt). Output: OUT. A manual trigger button with a light.
- Polyphony: the number of voices is the larger of the TRIG and V/OCT channel counts, up to 16. Each voice has its own state and noise seed (`seed32 + 7919 × channel`). A mono TRIG with a poly V/OCT fires all voices together. Output is poly, with a context-menu option "Sum to mono" (default on).
- **Panel (12 HP):** three columns. Knobs in rows: START, END, SWEEP / RESONANCE, DECAY, NOISE / CLICK, DRIVE, AGE / LEVEL beside the button. Then two rows of jacks: TRIG, V/OCT, ACCENT, START, END / SWEEP, RES, DECAY, OUT (darker plate).
- **Presets (12):** Init Zap; Deep Kick (400 to 45 Hz, long); Electro Kick (short sweep, DRIVE); Pew Snare (6 kHz to 180 Hz with NOISE); Tick; Rim; Low Tom; High Tom; Laser (START 12 kHz, SWEEP 250 ms, RES 0.95); Riser (END above START); Sine Drone (HOLD, RES 1, play by V/OCT); Tired Zap (AGE 1).

## 5. VOKODER

Engine: `vok_render` and `carrier` in `vokoder_ref.py`. 20 bands, centres from `CENTRES` (geometric from 100 Hz to 8 kHz), Q 3.6, two cascaded band-pass biquads for analysis and two for synthesis, alternating output polarity. Recompute coefficients only when AGE or the unit changes. Use `simd::float_4` across bands if it is easy; correctness first.

**Controls**
- 20 band sliders with a light per band that follows the band envelope. Slider 0 to 1, default 0.5, gain = (2 × slider)².
- MOD GAIN (0 to 4×, default 1). Modulator in units = volts / 5 × MOD GAIN.
- ATTACK (1 to 30 ms, exponential, default 4 ms), RELEASE (10 to 600 ms, exponential, default 40 ms).
- FORMANT (-5 to +5 bands, default 0) with CV input (1 band per volt).
- SIBILANCE (0 to 1, default 0.7), VOICING (0 to 1, default 0.5: lower = more of the signal treated as consonant).
- FREEZE button (latching, lit) and FREEZE gate input (either one freezes).
- Carrier section: SHAPE (saw to pulse, 0 to 1, default 0), WIDTH (pulse width 0.05 to 0.5, default 0.3), TUNE (-2 to +2 octaves, default -1), CHORD switch with four positions: OFF, 5TH, MIN, MAJ (default OFF).
- AGE, LEVEL (default 0.8).

**Jacks.** In: MOD, CARRIER (external; when patched it replaces the internal carrier, units = volts / 5), V/OCT (poly: one carrier note per channel, each with the CHORD notes; the sum is divided by the square root of the channel count), FORMANT CV, FREEZE. Out: OUT, VOICED (10 V gate while the detector says voiced and speech is present), ENV (speech envelope, 0 to 10 V).

**Behaviour.** With nothing in V/OCT the internal carrier plays one note at TUNE. Engine arguments: `attack`, `release` in seconds, `shift` = FORMANT, `sib`, `thresh` = VOICING, `freeze`, `age`. Output volts = engine output × 5 × LEVEL.

**Panel (28 HP).** The 20 sliders across the upper half (6.6 mm pitch) with lights above and band frequencies printed small under every fifth slider (100, 300, 900, 2.7k, 8k approximately: use the true centres rounded). Below: a row of knobs MOD GAIN, ATTACK, RELEASE, FORMANT, SIBILANCE, VOICING, AGE, LEVEL; a CARRIER group with SHAPE, WIDTH, TUNE, CHORD; FREEZE; jacks along the bottom.

**Presets (8):** Init; Clear Robot (fast attack and release, CHORD OFF, saw); Choir of Machines (MAJ chord, slow release); Small Robot (FORMANT +3); Giant Robot (FORMANT -3, TUNE -2); Telephone (outer sliders down); Whisper Only (SIBILANCE 1, VOICING 0); Tired Unit (AGE 1).

## 6. SPRECHER

Engine: everything in `sprecher_ref.py`. This is the largest module. Port in this order: tables, `parse` and `lts`, `word_targets`, the scheduler as a live state machine, `formant_engine`, `chip_engine`.

**Tables.** Write `tools/proto/export_tables.py` to generate `src/dsp/SprecherTables.hpp` from `PH`, `LTS` and `words.txt`, so the Python stays the single source. Text is UTF-8: lower-case ASCII letters, and treat ä, ö, ü, ß as single letters in the German rules.

**Controls**
- A text field (multi-line, about 62 × 30 mm) and under it a one-line display of the word being spoken with its phonemes. Default text: `1 2 3 4 5 6 7 8`.
- LANGUAGE: seven-position snapped knob, EN DE FR ES IT JA RU (default EN). It chooses the number words and the dictionary tried first, and the letter-to-sound rules for unknown words (FR and RU have none of their own: say so in the manual).
- ENGINE switch: FORMANT or CHIP (default FORMANT).
- PITCH (55 to 440 Hz, exponential, default 110 Hz), INFLECT (0 to 1, default 0.3), RATE (0.5× to 2×, exponential, default 1×) with CV (±5 V = ±1 octave of rate), SIZE (0.7 to 1.4, default 1; display as "small" to "giant") with CV (0.07 per volt), WHISPER (0 to 1, default 0), AGE, LEVEL.
- HOLD switch (default on): while the STEP gate stays high, the stressed vowel of the word is sustained; the rest of the word follows when the gate falls. A trigger shorter than the word just plays the word.
- SPEAK button (same as a STEP trigger) and RESET button.

**Jacks.** In: STEP, RESET, WORD (0 to 10 V selects a word: index = floor(volts / 10 × count), clamped; when patched, STEP speaks the selected word instead of the next one), V/OCT, RATE CV, SIZE CV. Out: OUT, F1 (first formant as 0 to 10 V: (F1 - 200 Hz) / 80 Hz per volt, clamped), BUSY (10 V while a word is sounding), END (1 ms trigger when a word finishes).

**Behaviour**
- The text is parsed on the UI thread whenever it changes and handed to the audio thread without blocking (swap a pointer; keep the old list alive until the audio thread has let go). Tokens are as in `parse`: `[PH PH]` phonemes typed directly, digits as number words 0 to 20 (longer numbers digit by digit), other words through the dictionaries and then `lts`. Up to 256 words.
- STEP rising: start the next word (wrapping), or the WORD-selected one. A new word interrupts the current one; the parameter smoothing covers the join. RESET: the next STEP speaks the first word.
- Live scheduler: one frame of `word_targets` every 5 ms of scaled time. HOLD logic exactly as in `schedule`: on the last frame flagged hold, stay there while the gate is high.
- Pitch: if V/OCT is patched, f0 = PITCH × 2^volts. If not, f0 = PITCH × 2^(semis / 12), where `semis = INFLECT × (3 × stress − 4 × progress + 1)`, `stress` is the frame's stress flag, `progress` runs 0 to 1 across the word's natural length, and the result is smoothed with a 40 ms one-pole. (The Python `pitch_track` does the same thing offline. All reference cases use INFLECT 0.)
- RATE scales durations only. In CHIP the 8 kHz clock is not changed by RATE.
- Output volts = engine output × 5 × LEVEL.

**Panel (28 HP).** Text field top left with the word display under it. Right of it: LANGUAGE with its seven labels round the knob, ENGINE, HOLD. Middle row of knobs: PITCH, INFLECT, RATE, SIZE, WHISPER, AGE, LEVEL. SPEAK and RESET buttons above their jacks. Jacks along the bottom, outputs on a darker plate.

**Presets (12):** Count to Eight (EN); Zählen (DE numbers); Seven Tongues (set LANGUAGE to JA; notes say turn the knob); Chip Counter (CHIP, DE); Music Machine ("we are the music machine"); Computer World words ("computer data memory program energy"): name this preset "Data Words"; Sung Vowels (HOLD on, notes: patch a gate and V/OCT); Small Voice (SIZE 0.75, PITCH 220); Giant (SIZE 1.35, PITCH 65); Whisperer; Spelling (letters "h a e r"); Tired Clock (AGE 1).

## 7. ORCHESTER

Engine: `orch_voice`, `DISCS`, `FORM`, `ADD`, `note_personality` in `dynamo_orch_ref.py`.

- The disc angle is **one value for the whole module**, advancing 1 / (REV × fs) per sample, always running. Every voice reads it. This is what makes chords wobble together.
- Polyphonic, 16 voices from V/OCT and GATE. On each gate rise, compute the note personality from the disc and the nearest semitone (`note_personality`), and reset that voice's oscillator phases as in the Python. Pitch follows V/OCT continuously.
- Shared hiss and hum as at the end of `orch_render`: level × the largest key envelope among the voices, added once to the mix.
- Voices are summed and multiplied by 0.5 × LEVEL (the 0.5 is already inside `orch_voice`).

| Control | Range | Default |
|---|---|---|
| DISC | 8 positions: CHOIR, STRINGS, FLUTE, ORGAN, CELLO, HORN, VOX, REED | CHOIR |
| WOBBLE | 0 to 1 | 0.5 |
| TONE | 0 to 1 | 0.5 |
| ATTACK | 5 ms to 2 s, exponential | 5 ms |
| RELEASE | 30 ms to 4 s, exponential | 30 ms |
| OCTAVE | -1, 0, +1 switch | 0 |
| AGE | 0 to 1 | 0.3 |
| LEVEL | 0 to 1 | 0.8 |

Jacks. In: V/OCT (poly), GATE (poly), DISC CV (1.25 V per disc, added to the knob), WOBBLE CV, TONE CV (10 V = full range). Out: OUT. A disc change takes effect on each voice's next gate, as if the disc had been swapped.

**Panel (16 HP).** DISC as a large knob at the top with the eight names round it; WOBBLE and TONE under it; ATTACK, RELEASE, OCTAVE; AGE, LEVEL; jacks at the bottom.

**Presets (10):** one per disc as found (eight), plus Slow Choir (ATTACK 0.4 s, RELEASE 1.5 s), Warped Disc (WOBBLE 1, AGE 1).

## 8. DYNAMO

Engine: `dynamo_voice` in `dynamo_orch_ref.py`, at 2× the engine rate, decimated with the half-band filter from Sawstack's `Voice.hpp`.

| Control | Range | Default | Engine P[] |
|---|---|---|---|
| OCTAVE | switch: -2, -1, 0 | -1 | added to pitch |
| WAVE 1, WAVE 2 | switch: SAW, PULSE | SAW, SAW | 0, 1 |
| DETUNE | -12 to +12 semitones | +0.10 | 2 |
| WIDTH | 0.05 to 0.95 | 0.5 | 3 |
| MIX | 0 to 1 | 0.5 | 4 |
| SUB | 0 to 1 | 0 | 5 |
| NOISE | 0 to 1 | 0 | 6 |
| CUTOFF | 0 to 1 (20 Hz to 20 kHz) | 0.45 | 7 |
| RESONANCE | 0 to 1 | 0.25 | 8 |
| ENV | 0 to 1 (0 to 7 octaves) | 0.45 | 9 |
| KEY | 0 to 1 | 0.5 | 10 |
| F ATTACK, A ATTACK | 1 ms to 5 s: 0.001 × 5000^knob | 2 ms | 11, 14 |
| F DECAY, A DECAY | 5 ms to 10 s: 0.005 × 2000^knob | 0.25 s, 0.4 s | 12, 15 |
| F SUSTAIN, A SUSTAIN | 0 to 1 | 0.1, 0.7 | 13, 16 |
| GLIDE | 0 to 2 s: 2 × knob² | 0 | 17 |
| DRIVE | 0 to 1 | 0.3 | 18 |
| AGE | 0 to 1 | 0.3 | 19 |
| LEVEL | 0 to 1 | 0.8 | output |

Jacks. In: V/OCT, GATE, ACCENT (read when the gate rises), CUTOFF CV with an attenuverter trimpot (1 octave per volt at full), WIDTH CV (0.09 per volt). Out: OUT. Polyphonic up to 16 from V/OCT and GATE, each voice with its own unit personality (`unit(seed + channel, 8)`) and noise seed; output poly with context-menu "Sum to mono" (default off).

**Panel (22 HP).** Sections with captions: OSCILLATORS (OCTAVE, WAVE 1, WAVE 2, DETUNE, WIDTH, MIX, SUB, NOISE), FILTER (CUTOFF and RESONANCE large; ENV, KEY; F ATTACK, F DECAY, F SUSTAIN), AMPLIFIER (A ATTACK, A DECAY, A SUSTAIN, GLIDE, DRIVE, AGE, LEVEL), jacks at the bottom.

**Presets (12):** Init; Motor Bass (the `dynamo_bass` reference settings); Clipped Pluck (`dynamo_pluck`); Sliding Lead (`dynamo_lead`); Hollow Pulse Melody; Sub Bass; Octave Bass (DETUNE 12); Resonant Blip; Soft Pad Lead; Noise Sweep; Factory Fresh (AGE 0); Tired Unit (AGE 1).

## 9. Tests (`make test`)

One harness, `test/render.cpp`, with every engine in `src/dsp/*.hpp` free of Rack dependencies. It renders the 40 cases of `make_reference.py` with the same settings and compares with `ref_bands.json` (convert it to a header with a small script).

1. Third-octave bands, 50 Hz to 16 kHz, for every case: within 1.5 dB wherever the reference band is above -60 dB. Cases with no noise and AGE 0 (`zap_kick`, `zap_tom`, `zap_drone`, `blech_box1_bass`, `blech_box1_tom`, `blech_box1_clave`, `orch_organ`) within 0.5 dB.
2. ZAP: self-oscillation level at RESONANCE 1 with HOLD is 0.82 ± 0.03 units at 50 Hz and 1 kHz. Ring time at RESONANCE 0.7 and 100 Hz matches the Python within 5%.
3. BLECH: each voice's decay time to -60 dB at DECAY 1× is within 10% of its table T60. A hit on one voice does not change another voice's output by more than -40 dB relative at AGE 0.
4. DYNAMO: tuning at C2, C4, C6 within 2 cents at AGE 0. Two seconds after the gate falls (A DECAY 0.4 s) the output is below -90 dB at AGE 0.3. No alias component above -60 dB relative for a saw at C6 with the filter open.
5. ORCHESTER: two voices started at different times have the same wobble phase (measure pitch modulation of each: phase difference under 5 degrees). All eight discs give RMS within 3.5 dB of each other for the reference chord.
6. VOKODER: silence in gives exact silence out at AGE 0.3. A white-noise modulator is classed unvoiced; a 150 Hz low-passed saw is classed voiced. FORMANT +2 moves the output spectral centroid up by between 30% and 80% for the reference phrase.
7. SPRECHER: `parse` gives the same phonemes as the Python for a list of 60 test tokens covering all seven languages, typed phonemes, unknown words and multi-digit numbers (generate the expected list from Python). Every dictionary word renders without NaN and with RMS between -30 and -3 dB. HOLD: a 1 s gate on "music" gives a word at least 0.9 s long.
8. TAKT: unit tests of the logic with a fake clock: quantiser (all 12 single-note masks, tie-break downward), transposition NOW and BAR, PLAY/TIE/REST gate timing, LAST, SERIES chaining, pattern change on step 1.
9. All audio engines: checks pass at 44.1 kHz and 96 kHz for band comparison within 2.5 dB (resample the comparison, not the reference). No NaN or infinity with every control and CV at its extremes, including ±12 V on every input.
10. CPU: report real-time factor at 48 kHz for each module, and for 16 voices of ZAP, DYNAMO and ORCHESTER. Target: each module under 10% of one core on the build machine in its worst normal case; say so plainly if one misses.

## 10. Manual

`docs/MANUAL.md` and `docs/Werkbank-Manual.pdf`, made with the Sawstack tooling. Plain language, full sentences. Contents: what the set is; a page per module with a panel picture and every control; AGE explained per module in terms of what you hear; how to type into SPRECHER (words, numbers, languages, `[phonemes]`, with the full phoneme list and an example word for each, and the complete dictionary as an appendix); the presets; and at least fourteen patch recipes, including:

1. One pattern, many chords: TAKT into DYNAMO, a keyboard into TRANSPOSE with WHEN on BAR.
2. Five against sixteen: two rows of different length.
3. The pad kit: BLECH played from a clock divider, with an LFO on the PITCH pedal.
4. Pedal dynamics: an envelope follower or a slow random voltage into DYNAMICS.
5. Tuned zaps: TAKT row B into ZAP's V/OCT as a melodic drum.
6. A whole kit from one ZAP using a poly trigger.
7. The counting robot: a clock into SPRECHER's STEP, numbers 1 to 8, change LANGUAGE each bar.
8. The singing robot: TAKT gate and V/OCT into SPRECHER with HOLD on.
9. Robot choir: SPRECHER into VOKODER's MOD, ORCHESTER or a poly keyboard into V/OCT, CHORD on MAJ.
10. Vocoding a drum loop with a chord.
11. Freezing a vowel: FREEZE from a gate, play the carrier.
12. The optical choir with a phaser after it (name a suitable phaser already in Rack, and Heathrow from this repository).
13. SPRECHER's F1 output driving DYNAMO's CUTOFF, so the bass "talks".
14. A full track from the seven modules, step by step.

State plainly what is a guess: the disc revolution time, the BOX component values, and that the speech rules are approximate and were tuned without listening.

## 11. Order of work

1. Port the engines to `src/dsp/` and get `make test` green, one engine at a time: ZAP, BLECH, DYNAMO, ORCHESTER, VOKODER, SPRECHER. Then the TAKT logic and its tests.
2. Modules, panels, presets. Look at every panel preview.
3. Push; confirm the Mac arm64 CI build is green and the plugin is on the preview release.
4. Manual, PDF, README, demo WAVs from the C++ engines.
5. Record any deviation from this spec in the README.
