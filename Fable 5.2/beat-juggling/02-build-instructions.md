# Doubles: build instructions for Opus

Fable, 2026-11-22. Design is complete and approved by Hardy. This file plus `doubles_engine.py` (the reference engine) and `01-research-and-proposal.md` (the why) are everything needed to build.

Items marked **check** are from memory and must be verified, or the stated fallback used.

## 0. What is being built

**Doubles**, a Max for Live audio effect for Ableton Live Suite 12.3 (Max 9), Apple silicon Mac. A short piece of audio (1 to 16 bars) is recorded into a buffer, "the record". Two modelled turntables each play a copy of it and a crossfader cuts between them. Beat juggling moves are played from buttons, a 16-slot routine, or chosen by weighted odds ("Instinct"). The hands are modelled: TIMING, WEIGHT, SLOP and RISK, with a HANDS macro. Default is "tight DJ" (all four at 0.5).

Hardy's decisions, do not reopen: Max for Live not VCV; no live audio processing (record first, then juggle); 16 bars maximum; name Doubles; HANDS default 0.5; all four hands controls on the panel; text labels.

Deliverables, in `hardyfinn/2026SoloModules`, folder `Doubles/`, on this thread's branch (not main):

| File | What |
|---|---|
| `Doubles.amxd` | The device. |
| `doubles.js` | Logic: record handling, sticker finder, routine, Instinct, persistence. |
| `doubles_strip.js` | jsui drawing of the waveform strip. |
| `Doubles.maxpat` | The same patcher as a plain Max patch (fallback, section 9). |
| `ref/doubles_engine.py`, `ref/doubles_rt.py`, `ref/tests.py` | Offline reference, real-time reference, tests. |
| `tools/build_device.py` | Script that writes the patcher JSON and wraps it as .amxd. The device is generated, never hand-edited. |
| `docs/Doubles-Manual.pdf` | Manual with patch recipes. |
| `README.md` | Install steps and the acceptance checklist of section 10. |

Copy the .amxd, the two .js files and the manual to `/mnt/project-files/beat-juggling/build/` as well.

## 1. Architecture

```
plugin~ L R ─┐                       ┌─ plugout~ L R
             ├─ gen~ doubles_core ───┤
plugphasor~ ─┘   (decks, fader,      └─ meters / strip data (control rate)
                  planner, recording)
buffer~ dbl_rec   stereo, the record (written and read by gen~)
buffer~ dbl_stk   sticker positions in samples, up to 256
buffer~ dbl_fill  8 fill patterns x 16 steps
js doubles.js     everything slow: sticker finder, routine, Instinct, files
jsui doubles_strip.js   the strip
live.* objects    the panel; every control is a Live parameter
```

Rule for the split: anything whose timing is heard is inside gen~ and sample-accurate. JavaScript only ever decides things at least half a beat before they sound, and writes them into gen~ params. JavaScript runs on Max's low-priority thread and can stall for tens of milliseconds, so it must never trigger a sounding event directly.

gen~ is one codebox (GenExpr). It is embedded in the patcher JSON, so there is no separate .gendsp file.

## 2. Time

- All musical time in gen~ is in **beats** (quarter notes) as a 64-bit float `B`.
- `plugphasor~` gives a 0 to 1 ramp per beat, sample-accurate (**check** it runs in Live 12.3; it is the standard object for this). gen~ counts wraps to keep an integer beat count, and JS sends `songbeat` (from `live.observer` on `live_set current_song_time`, or `plugsync~`) a few times a second. If `|B - songbeat| > 0.25` beats, gen~ takes the reported integer part and treats it as a transport jump (section 4.7). Fallback if plugphasor~ misbehaves: integrate `B += tempo / (60 * samplerate)` per sample while playing and apply the same re-sync rule.
- `tempo` and `playing` come in as params from `plugsync~` or `live.observer`.
- 4/4 is assumed. Other time signatures still work but bars are counted as 4 beats. Say so in the manual.

## 3. The record

### 3.1 Recording (in gen~, with `poke`)

- `dbl_rec` is sized by JS when RECORD is armed: `bars * 4 * 60 / tempo` seconds plus 0.5 s, at the current sample rate. If that exceeds 120 s, refuse and show "too long at this tempo".
- LENGTH: 1, 2, 4, 8, 16 bars, or FREE.
- Armed: recording starts at the next bar line (first sample where `floor(B / 4)` increases). Fixed lengths stop after exactly `bars * 4` beats. FREE stops at the first bar line after RECORD is switched off, minimum 1 bar, maximum 16.
- While armed or recording the input is passed to the output and the decks are silent. Once a record exists the input is ignored. With no record and not armed, the input is passed through, so an empty device is transparent.
- On completion gen~ reports (control-rate outlet): length in samples `L`, length in beats `Lb`, tempo at recording `tempo_rec`, and `anchor = B at start mod Lb`. JS then runs the sticker finder and saves the record (3.4).
- Apply a 3 ms fade at each end of the record so the loop point never clicks.

### 3.2 Playing position

- The record spans `Lb` beats. The **straight position** in record beats at song beat `B` is `rbS(B) = (B - anchor) mod Lb`. Straight play always returns here, so the device stays locked to the song however long a move lasted.
- Base platter rate (record samples per output sample): `rate0 = L * tempo / (Lb * 60 * samplerate)`. Tempo changes after recording repitch the record like a turntable. There is no time-stretch in version 1.
- The buffer is circular for the decks (position wraps), as in the reference.

### 3.3 Stickers (JS, once per record)

Port `find_stickers` from `doubles_engine.py` exactly: short-time energy flux on the mono sum, hop 128, threshold 25% of the largest flux, minimum spacing 70 ms, each mark refined to the first sample of the differenced signal above 35% of the local maximum. Maximum `16 * bars` marks, capped at 256. Read the buffer in blocks with the JS `Buffer` object inside a `Task` so the UI is not frozen (16 bars is about 1.6 million samples). Write positions (samples, ascending) to `dbl_stk` and the count to a gen~ param.

Measured on the reference material: on a clean synthetic break 23 of 24 marks were within 0.3 ms, one was 30 ms late and three quiet hits were missed. On Hardy's full-band loop many soft attacks sit 10 to 25 ms off the grid. This is acceptable because of the snap rule: a cue point snaps to the nearest sticker only if one lies within 40 ms of the requested grid position, otherwise the grid position is used. Cue 2 ms ahead of the snapped point so the attack is whole.

Each sticker can be switched off by clicking it on the strip, and a new one added by shift-clicking.

**Kick and snare for fills.** JS measures three bands over the 60 ms after each sticker: below 110 Hz, 150 to 400 Hz, 1.5 to 8 kHz (two-pole filters are enough). FILL K defaults to the sticker within 40 ms of a beat with the largest low band, preferring the first bar. FILL S defaults to the sticker within 40 ms of beat 2 or 4 of any bar with the largest product of the mid and high bands. Both are shown on the strip (K and S flags) and can be reassigned by the FILL K and FILL S number boxes.

### 3.4 Keeping the record

Buffers are not saved with a Live set, so:

- After each recording JS writes the buffer to `~/Music/Doubles/doubles-YYYYMMDD-HHMMSS.wav` (create the folder) with `writewave`.
- A hidden, stored-only Live parameter of type blob (a `pattr` with parameter mode enabled, **check** the exact attribute names against a Max 9 patch) holds a dict: file path, `Lb`, `tempo_rec`, `anchor`, switched-off stickers, added stickers, FILL K, FILL S. It travels with the Live set and with presets.
- On load, if the file exists it is read back and the stickers recomputed. If not, the strip shows "record missing" and the device passes audio through.
- A `live.drop` over the strip accepts an audio file instead of recording. Length in bars is the power of two from 1 to 16 nearest to `duration * tempo / 240`, shown in the BARS box and editable. `anchor` is 0.
- CLEAR empties the buffer and the stored dict. It does not delete files.

## 4. The engine (gen~ codebox)

`doubles_engine.py` is normative for every formula and constant in this section. It is an offline renderer: it knows each deck's next play in advance. The real-time form below produces the same behaviour with a short look-ahead.

### 4.1 Plays

Everything a deck does is a **play**:

```
play = { bs, be,          start and end, in song beats
         rb,              record position in beats at bs, or CONTINUE (carry on from wherever the platter is)
         snap,            1 = snap rb to a sticker (section 3.3)
         rate,            1 normally; phase uses 1.0018
         gate,            0 = fader open for the whole play
                          1 = open on even slots of length gd beats counted from g0, 2 = odd slots
         gd, g0, amp,     amp 1 normally, 0.72 when both decks are open together
         kind }           PLAY, HOLD (audible stop) or SPIN (audible pull-back of `dist` beats)
```

Each deck has a FIFO of up to 4 pending plays. The planner (4.5) fills the FIFOs so that the latest scheduled `bs` is always at least **1 beat** ahead of now, or to the end of the current plan.

### 4.2 Platter

Per deck, per sample: `pos` (record samples, 64-bit), `vel` (record samples per output sample divided by `rate0`, so 1.0 is normal speed), and a mode.

- **RUN** towards `rate`: `vel += (rate - vel) * (1 - exp(-1 / tau))`.
- **STOP** (hand on the record): `vel *= exp(-1 / tau_stop)`.
- **CUE** (silent pull to the next cue point): a Hann-shaped velocity pulse of `dc` samples whose integral is `dist`, added to the decaying old velocity, exactly as in `render_deck`. `need = reach + |dist| / vmax`; `dc = min(need, time available)`.
- **SPIN** (audible): velocity shape `exp(-3u) * (1 - exp(-60u)) * sqrt(1 - u)`, `u` from 0 to 1 over the spin, scaled so the integral is `dist`.
- **Release.** For a cued play the record is let go `pre` samples before `bs`, from `prel = p0 - rate * (pre - tau * (1 - exp(-pre / tau)))`, so that it reaches `p0` at `bs`. The fader opens at `bs`. On a cued release add the push term `rate * push_draw * (exp(-k / (2.5 tau)) - exp(-k / (0.6 tau)))`, `k` samples since release.
- If the hand cannot get there (`time available < need`) the release is late and the hit arrives late. That is intended. Count it for the strip's LATE lamp.
- A play that follows straight on from the same deck's previous play (`bs` equal to the previous `be`, record position continuous within 64 samples, same rate) needs no cue. The platter just keeps running.
- Read the buffer with 4-point Hermite interpolation (the reference uses linear; Hermite is better at backspin speeds). When `|vel * rate0| > 2`, lowpass the read by averaging 2 or 4 taps to keep the backspin from aliasing harshly.

### 4.3 Hands

Constants as a function of the four controls (T = TIMING, W = WEIGHT, Sl = SLOP, R = RISK, each 0 to 1; times in seconds):

| Quantity | Formula | Driven by |
|---|---|---|
| `tau` spin-up | 0.002 + 0.018 W | WEIGHT |
| `tau_stop` | 0.003 + 0.012 W | WEIGHT |
| `pre` | tau × (3.2 − 1.2 W) | WEIGHT |
| `push` (std dev of push draw) | 0.10 W | WEIGHT |
| `vmax` fastest pull, × real time | 6 / max(W, 0.05) | WEIGHT |
| `reach` | 0.045 W | WEIGHT |
| release scatter std dev | 0.007 T, as AR(1): `e = 0.6 e + N(0, 0.8 × 0.007 T)` | TIMING |
| fast-passage factor | `fast = min(0.25 / gap_seconds, 2)`; `jit = e (1 + 0.5 fast) − 0.002 T fast` | TIMING |
| fader scatter std dev | 0.003 T | TIMING |
| fader opens early by | 0.002 Sl | SLOP |
| fader closes late by | 0.004 Sl | SLOP |
| cue point error std dev | min(0.002 Sl + 0.012 Sl × seconds pulled, 0.030), only when the pull is more than 8 samples | SLOP |
| level scatter per cut | × min(1 + N(0, 0.07 Sl), 1.12) | SLOP |
| held-record tremor | velocity noise 0.03 Sl, lowpassed to about 100 Hz | SLOP |
| fader cut time | 0.0008 + 0.0012 Sl, raised-cosine | SLOP |
| fluff chance per cued play | 0.05 R; a fluff adds N(0, 0.035) to the release time | RISK |
| catch-up | 0.8 of the error, see 4.4 | fixed |
| copy B pitch error | +0.12% × COPY | COPY |
| copy B tone and level | one-pole lowpass at 20 kHz − 9 kHz × COPY, gain 1 − 0.06 COPY | COPY |

`N` is a normal draw: sum of 6 `noise()` values scaled is adequate. Draw once per event, not per sample. In the reference `vmax`, `reach`, the fade time, and the copy B differences were tied to the macro `h`; here they are assigned as above, and COPY is a fifth small dial because the second copy is a property of the record, not of the hands.

**HANDS** is a macro dial. Moving it sets TIMING, WEIGHT, SLOP and RISK to its value (done in JS with `live.object` or by parameter linking). Moving any one of the four afterwards leaves the others alone. Default 0.5. With all four at 0 the device is an exact, clickless slicer: every juggled slice must match the record sample for sample apart from the 2 ms spin-up.

### 4.4 Catching up

The reference adds a pre-computed nudge. In real time: for a cued play longer than 1.2 s, from 0.35 s after `bs` until 0.95 s after `bs`, add to the velocity a Hann pulse whose integral is `0.8 × err`, where `err` is the difference between where the platter will be at `bs + 0.35 s` and where it should be (`p0` advanced at `rate`). Compute `err` once, at `bs + 0.35 s`, from the actual position. This models the DJ hearing the drop and nudging the record back. Verified on the test loop: an 11 ms error is under 2.5 ms four beats later.

### 4.5 Planner

The planner turns the current **plan** into plays. A plan is, for each half bar, a move and its settings. Plans come from the move buttons, the routine or Instinct (section 5). The planner works in whole slots and always schedules at least one beat ahead.

Notation: `b0` start beat of the move, `n` its length in beats, `L` the live deck (last to sound), `O` the other. "free deck at b for rb" means: a deck whose previous play ends at `b` at record position `rb` (it can simply carry on); failing that the deck whose hand has been free longest (see `Seq.free`).

| Move | Plays generated (port of the `Seq` methods) |
|---|---|
| STRAIGHT | One play on the free deck at `rbS(b0)`, no snap, extended while straight continues. |
| LOOP, unit `u` | Slot k (k = 0, 1, ...) of length `u` on deck `(first + k) mod 2`, all from the same `rb` = `rbS(b0)` snapped. |
| DOUBLE, distance `d` | Deck X carries on straight; deck Y plays the same material `d` beats late (starts at `b0 + d` from `rbS(b0)`). X is gated on even slots of length `d`, Y on odd slots. Neither platter stops. |
| CHASE, unit `u`, shuffle `s` | Slot k on deck `(first + k) mod 2` plays record position `rb0 + floor(k / 2) × u`. Slot lengths alternate `u (1 + s)` and `u (1 − s)`. The record advances at half speed and each hit is stated twice. |
| TRIPLE, over 1 beat | Three slots of one third of a beat, decks alternating, all from the same `rb`. |
| PAUSE, hold `h` | Live deck: HOLD from `b0 − 0.03` to `b0 + h` (fader open, the stop is heard), then a CONTINUE play to the end of the move (the release is heard). |
| FILL, pattern | 16 steps of a sixteenth. `K` plays the FILL K sticker on one deck, `S` the FILL S sticker on the other, `.` is silence. A hit lasts until the next hit, at most two steps, and only 0.45 of the gap when the same deck must play again within two steps. The first hit goes to the free deck. |
| SPIN, `back` beats | Live deck: SPIN of `−back` beats lasting 0.85 of the last beat of the move. |
| PHASE | Both decks open at amp 0.72. X straight; Y starts 9 ms behind at rate 1.0018 so it sweeps through and past. |
| DRAG | Both decks open at amp 0.72, both straight. Each press of DRAG multiplies the live deck's velocity by `1 − 0.35 × hann` over 0.30 s. PUSH does the same with `+0.35`. |

When a move ends, STRAIGHT resumes on the free deck at `rbS`. Pending plays that start after the change point are flushed from the FIFOs and replaced.

The 8 fill patterns in `dbl_fill` (K = 1, S = 2, rest = 0), user-editable later but fixed in version 1:

```
1  K.KS.K.SK.KS.SSS      5  K.K.S..KK.S.KSSS
2  KK.S.KKS.K.SKSKS      6  K...S.K.K.S.SKSK
3  K..SK.S.KKSS.KSS      7  KKSSKKSSK.S.KSSS
4  KSKKS.KSKSS.SSSS      8  K.S.K.S.KSKSKSSS
```

### 4.6 Mixer

`out = deckA × gainA + deckB × gainB`, then the soft limit from the reference (linear below 0.85, tanh knee above), then LEVEL. Gains are the gate values smoothed by the raised-cosine cut. STRAIGHT LEVEL scales the gain of STRAIGHT plays only, so the device can sound only during moves.

### 4.7 Transport

- **Start:** both platters are already at speed. The straight deck is placed at `rbS(B)` with `vel = 1`.
- **Stop:** the live deck does a STOP with the fader open for 150 ms (the record is heard stopping), then silence. A BRAKE toggle (default on) switches this off for a clean stop.
- **Jump or loop in the arrangement:** flush all plays, place the free deck at `rbS(B)`, cut to it. With HANDS above zero this is a normal cued drop with zero time available, so it may be a few milliseconds late and is then nudged back. That is acceptable.

## 5. Where plans come from

MODE tab: **HANDS-ON**, **ROUTINE**, **INSTINCT**. Move buttons work in every mode and override the plan from the next QUANT boundary for as long as they are held.

### 5.1 Move buttons

Ten momentary buttons: LOOP, DOUBLE, TRIPLE, CHASE, SHUFFLE, PAUSE, FILL, SPIN, PHASE, DRAG. Each is a Live parameter, so it can be MIDI-mapped, key-mapped and automated. A press takes effect at the next QUANT boundary (1/16, 1/8, 1/4, 1 bar; default 1/4) and a release at the next boundary after it. If several are held the most recent wins. LATCH makes the buttons toggles. gen~ reads the buttons as params and does the quantising itself.

Settings: LOOP LEN (1/4, 1/2, 1, 2, 4 beats; default 1), DOUBLE DIST (1/16, 1/8, 1/4; default 1/8), CHASE UNIT (1/8, 1/4; default 1/4), SHUFFLE (0 to 60%; default 50), PAUSE (1/16, 1/8, dotted 1/8; default 1/8), FILL PAT (1 to 8), SPIN BACK (1, 2, 4, 8 beats; default 4).

TRIPLE, PAUSE and SPIN are one-shots: TRIPLE triples the next beat, PAUSE holds at the next boundary and rejoins at the bar line, SPIN spins through the last beat of the current bar.

**Pads.** Eight pads play stickers directly: pads 1 to 4 on deck A, 5 to 8 on deck B, each assignable to a sticker (defaults: K, S, and the next strongest two, on both sides). A pad press is a play from that sticker starting now (no quantise) until release; the other deck keeps doing what it was doing but closed.

### 5.2 Routine (JS)

16 slots, each a menu: STRAIGHT, LOOP, DOUBLE, TRIPLE, CHASE, SHUFFLE, PAUSE, FILL 1 to 8, SPIN, PHASE. SLOT LEN: BAR or HALF. The routine position is `floor((bar − routine_anchor) / slot_len) mod 16`; `routine_anchor` is set when ROUTINE is selected or RESET is pressed. One beat before each slot starts, JS writes that slot's plan into gen~'s "next" registers.

How a slot is rendered: LOOP, DOUBLE, CHASE, SHUFFLE and PHASE fill the slot using the button settings. TRIPLE: beats 2 and 4 of the bar tripled, the rest straight. PAUSE: two beats straight, then the pause, rejoining at the bar. SPIN: three beats straight, spin on the fourth. FILL n: pattern n over the bar (a HALF slot uses the pattern's second half).

### 5.3 Instinct (JS)

Port `instinct()` from `make_demos_loop.py`. Once per bar, one beat before the bar line, JS picks the next bar's plan:

- Bars are counted in 4-bar phrases. Weights per place in the phrase:

| Place | straight | loop | double | chase | shuffle | pause |
|---|---|---|---|---|---|---|
| 1 | 6 | 2.5 | 1.5 | | | |
| 2 | 4 | 1.5 | 3 | 1.5 | | |
| 3 | 2.5 | 1.5 | 2.5 | 2.5 | 3 × FUNK | 1 |
| 4, first half | 1 | 1 | 2 | 2 | 4 × FUNK | |

- Place 4, second half, one of: fill 4, triple 2.5, backspin 2, chase in eighths 1.5, straight 1. The same fill pattern is never used twice running.
- Every weight except straight is multiplied by `2 × dens`, where `dens = DENSITY × (0.6 + 0.8 × progress)` and progress runs from 0 to 1 over ARC bars (8, 16, 32; default 16) and then starts again.
- At places 2 and 3, with chance 0.45, repeat the previous bar's move instead of drawing (never repeat straight or pause).
- Settings drawn per move: loop unit from {1/2, 1, 1, 2}; double distance from {1/16, 1/8, 1/8, 1/8}; chase unit from {1/8, 1/4}; shuffle from {33%, 50%}; pause hold from {1/16, 1/8}.

DENSITY and FUNK are dials (defaults 0.6 and 0.5).

## 6. Panel

Max for Live device view: 169 px high, about 1040 px wide. Dark panel, Live's own `live.dial`, `live.text`, `live.tab`, `live.menu`, `live.numbox`, so it looks native. Every control has a text label and a parameter name of the same words. Sections left to right:

| Section | Width | Contents |
|---|---|---|
| RECORD | 90 | RECORD (toggle, red when recording, flashing when armed), LENGTH menu, BARS box, CLEAR, status line (EMPTY / ARMED / RECORDING / 8 BARS 107.0) |
| STRIP | 300 | jsui: waveform, stickers as ticks (K and S flagged), two playheads (A orange, B blue), a bar under the strip showing which deck is open, LATE lamp. Accepts file drop. |
| MOVES | 250 | Two rows of five buttons; under them the seven settings menus; QUANT, LATCH |
| PADS | 110 | 2 × 4 pads |
| HANDS | 130 | HANDS (large), TIMING, WEIGHT, SLOP, RISK, COPY (small) |
| PLAY | 110 | MODE tab; DENSITY, FUNK, ARC; ROUTINE opens a floating window with the 16 slot menus, SLOT LEN and RESET (16 menus do not fit in the device view) |
| OUT | 50 | STRAIGHT, LEVEL, BRAKE |

PITCH per deck from the proposal is left out of version 1 to keep the panel readable; tempo already sets the pitch.

Strip data: gen~ sends both deck positions (0 to 1) and gate states at 30 Hz to the jsui.

Parameters: give every `live.*` object a unique Long Name and Short Name, sensible ranges, and initial values with "Initial Enable" on. Move buttons and pads: automation on. Menus: enum.

## 7. Real-time reference first

Do not write GenExpr straight from this document.

1. Write `ref/doubles_rt.py`: a sample-by-sample Python loop with exactly the state variables and control flow the codebox will have (no numpy vector tricks inside the loop), fed the uploaded loop at `/mnt/project-files/beat-juggling/source/baby-cry-8bar.wav` and a scripted list of button presses and plans.
2. `ref/tests.py` must show:
   - HANDS 0: for LOOP, DOUBLE, CHASE, TRIPLE and FILL, each sounding slot correlates above 0.99 with the expected slice of the record at zero lag (allowing the 2 ms spin-up at the start of a slot).
   - HANDS 0: straight play before and after any move equals the record sample for sample.
   - HANDS 0.5 and 1.0: mean absolute timing error of cued hits within 50% of the offline engine's on the same routine (about 3 to 6 ms at 0.5), no NaN, peak below 1.0, no sample-to-sample step larger than the record's own largest step × 1.5 (click test).
   - Catch-up: error at a drop is reduced by at least 60% four beats later.
   - Transport start mid-record, a jump, a tempo change of ±10%: output stays locked (`rbS`) within 3 ms after one bar.
3. Only then transliterate the loop body into GenExpr, line for line, keeping variable names. GenExpr notes: declare `History` for state, `Buffer` for the three buffers, `Param` for controls; no arrays (use a `Data` object or a small buffer for the FIFOs); `peek` and `poke` for buffer access; functions must be defined before use; all `if` branches are fine in codebox.

## 8. Building the .amxd without Max

`tools/build_device.py` writes the patcher as JSON (boxes, patchlines, the embedded gen~ patcher containing one codebox, and `"project"`-free plain device) and wraps it:

```
"ampf" | uint32 LE 4 | "aaaa" | "meta" | uint32 LE 4 | uint32 0 | "ptch" | uint32 LE length | JSON bytes
```

`aaaa` marks an audio effect. **Check** this header against any existing unfrozen .amxd before relying on it (search GitHub for small open-source Max for Live audio effects and compare the first 32 bytes; also confirm whether the JSON is followed by a newline or a null byte and whether `length` counts it). The patcher's top-level needs `"openinpresentation": 1`, a `"devicewidth"`, and each panel object needs `"presentation": 1` and a `"presentation_rect"`. Copy attribute names from a real device's JSON, do not invent them.

Lint in the build script: valid JSON; every patchline's source and destination ids exist; outlet and inlet indices are within each object's counts; every `live.*` object has a unique `varname` and long name; presentation rectangles do not overlap and fit in 169 px.

The two .js files sit beside the .amxd. Max finds them because a device's own folder is on its search path. The README tells Hardy to keep the folder together, and that once it works he can press Freeze in the Max editor to make one self-contained file.

## 9. Fallback

Nothing here can be opened in Live from the cloud, so ship `Doubles.maxpat` as well (the same JSON without the header). If the .amxd will not load, Hardy drags Live's empty "Max Audio Effect" onto a track, clicks its edit button, deletes the contents, pastes from the .maxpat and saves as Doubles.amxd into the same folder. The README gives these steps with the exact menu names.

Expect one or two fix rounds from what Hardy reports. Ask for the text of the Max Console window (it lists every object that failed to load and every gen~ compile error with line numbers).

## 10. Acceptance checklist for Hardy (in the README)

1. Drop the device on a drum track. Audio passes through unchanged.
2. LENGTH 2 bars, press RECORD, start Live. It arms, records two bars from the next bar line, then the strip shows the waveform with ticks.
3. The loop now plays from the device in time with the song, and the track's own clip can be stopped.
4. Hold LOOP: the current beat repeats. Release: it rejoins the song in the right place.
5. Try each move button.
6. HANDS to 0: cuts become exact. HANDS to 1: loose and late. Back to 0.5.
7. MODE to INSTINCT: moves appear by themselves, busier towards the end of each 16 bars.
8. Open ROUTINE, set a few slots, MODE to ROUTINE.
9. Save the set, close, reopen: the record is still there.
10. The Max Console shows no errors.

## 11. Manual

PDF, same style as the other manuals: what it is, signal flow, every control, the ten moves with a one-line "what the DJ does" each, the hands model in plain words, and at least eight recipes (for example: extend a break with LOOP and DOUBLE; half-time breakdown with CHASE; Instinct as a fill generator with STRAIGHT at zero over the original clip; build a new beat from kick and snare on the pads; phase the two copies for a flanged bar; a 16-bar routine written out; juggling a bass line or a vocal instead of drums; HANDS at 0 as a clean slicer). Include how to MIDI-map the buttons, since an audio effect does not receive notes.
