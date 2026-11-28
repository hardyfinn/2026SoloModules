# Rider: build instructions

Design by Fable, 2026-11-24. For the model that builds it. Rider is the drummer for Comper and Walker: it reads where the band is, what the pianist is about to play in the coming bar and where the bass runs, improvises a drum part one bar at a time, and plays it on a modelled jazz kit. `docs/RIDER-RESEARCH.md` says why and what was measured. Where the two differ, this document wins.

There are **two reference files, and they are the definition**:

- `tools/proto/rider_ref.py`: the player. What is hit, when, how hard. The C++ must reproduce it stroke for stroke.
- `tools/proto/rider_kit_ref.py`: the kit, written the way the module runs it. The C++ must reproduce its output (section 5 and the test in section 10). Hardy approved this sound by ear on 2026-11-24 against recordings of a real kit; do not "improve" it. Every table in `tools/proto/rider_kit.json` is fitted to those recordings.

Already generated and committed, do not hand-edit:

| File | From | What |
|---|---|---|
| `test/rider_golden.json`, `test/rider_golden.h` | `tools/proto/make_rider_reference.py`, `tools/rider_golden_to_h.py` | 174 drum parts (35,815 strokes) with the bar-by-bar input each was given |
| `test/rider_kit_cases.json`, `test/rider_kit_cases.h` | `tools/proto/make_rider_kit_reference.py`, `tools/rider_golden_to_h.py` | 69 kit renders with their reference measurement |
| `src/dsp/RiderKitData.hpp` | `tools/rider_kit_to_h.py` | the kit tables and the 26 band filters as C++ |
| `presets/Rider/*.vcvm`, `test/rider_presets.h` | `tools/proto/export_rider_presets.py` | the 12 presets, paired by number and name with Comper's and Walker's |

Hardy's decisions (2026-11-24): a third module inside the Comper Fable plugin; its own modelled kit plus trigger outputs; brushes in this version (the ballad feel); ENERGY can lead the band through a BAND amount that starts at zero; trading fours is for a later version; the name is Rider.

## 1. What to build

- A third module in `ComperFable/` on branch `claude/project-thread-7vm3pn` (not main). Bump the plugin to `2.2.0`. Module slug `Rider`, name `Rider`, tags Drum, Sequencer, Physical modeling, Random. Description: "Drummer for Comper and Walker: an improvising jazz drummer that keeps time on the ride, comps on snare and bass drum, catches the pianist's pushes, sets up the ends of phrases and plays a modelled jazz kit with sticks or brushes. Follows the band with no cables. Stereo audio and triggers out". DRUM, JAZZ and KIT must be findable in Rack's browser (the description does that).
- Panel 30 HP (152.4 mm), text labels, Haer logo, same visual language as Walker (`res/make_walker_panel.py`; write a sibling `res/make_rider_panel.py`).
- Deliverables: C++ module, panel SVG from a script, the 12 factory presets (re-run the export script if a parameter id moves), offline tests, a Rider chapter in `docs/MANUAL.md` and the PDF, README section, demo WAVs, a Rider section in `docs/DECISIONS.md`, and the changes to Comper and Walker in section 9. Apple silicon only; the repository workflow publishes to the "preview" release. Rack compiles as C++11: no default member initialisers on anything you brace-initialise.

## 2. Architecture

Headers under `src/dsp/`, no Rack includes in the first four:

1. **`RiderEngine.hpp`**: a line-by-line port of `rider_ref.py` (`RP`, `BarIn`, `Rider`). Namespace `rider`. It uses Comper's `Rng` and `autoSwing` from `Engine.hpp`; do not copy them. Doubles, same order of operations and of random draws. Two streams: `r = Rng(seed * 5 + 1)` decides what is played, `h = Rng(seed * 5 + 2)` is the hands. `hit()` draws exactly two gaussians from `h`, time then loudness, even when LOOSE is 0.
2. **`RiderKit.hpp`**: the kit, the real-time form of `rider_kit_ref.py` (section 5). Namespace `riderkit`, includes `RiderKitData.hpp`.
3. **`RiderHost.hpp`**: transport (link or clock), planning each bar just ahead of time, the stroke queue, the kit, the outputs. Sample-driven `process()`. No Rack types.
4. **`Link.hpp`**: the message the three modules pass along (section 4.1). Extended, see below.
5. **`Rider.cpp`**: the Rack module.

### 2.1 Things in the Python that need care in C++

- Keep the four `stats` counters (pushes, caught, comps, fills); the display uses them.
- `plan_bar()` is the whole planning step: one call per bar, state on the object (`figure`, `kfig`, `landing`, `drift`, `prev_hits`). The feel is read from the parameters at each call; the kept figures of one feel are simply left alone while another feel plays.
- **Short-circuit and draw order.** Every `r.u()` in the reference sits where it does on purpose. Where Python writes `if A: if r.u() < x:` the draw happens only when A holds; keep the nesting. `ghost = (r.u() < ...) and not caught` draws first, then tests. `pick_fill` draws at most twice. The ride pattern is one draw against the running sum of five weights (`while k < 4 and x >= w[k]`).
- `fill_pos` is a list of (beat, sub) pairs; `fill_beats` is the set of its beats when the fill is big or has more than one stroke. Membership tests are plain linear searches.
- `self.figure` holds (bar parity, beat, sub) triples; `kfig` holds (bar parity, slot) pairs. `kicks` in `soul_bar` is `sorted(set(...))`: sort ascending and drop duplicates.
- `piano_at` returns the nearest hit's slot; ties go to the lower slot (the `d < bd - 1e-12` test).
- Velocity: `int(max(1.0, min(127.0, vel + h.g() * 4.0 * L)))` truncates toward zero.
- A stroke is (beat from the bar line, offset in seconds, voice, velocity, length in beats). Only `BR_SWEEP` has a length.
- Voices are numbered as in `rider_ref.py`: 0 RIDE, 1 BELL, 2 CRASH, 3 HATF, 4 HATC, 5 HATO, 6 SNARE, 7 STICK, 8 KICK, 9 TOM_HI, 10 TOM_LO, 11 BR_TAP, 12 BR_SWEEP, 13 BR_HATF. The kit has a fifteenth, 14 SNARE_EDGE, which it chooses itself for a SNARE below velocity 42.
- Feels are numbered as Comper's GROOVE: 0 SWING, 1 BOSSA, 2 BALLAD, 3 SOUL, 4 HOUSE.

## 3. What the player does (summary; the Python is the definition)

Five layers, in every feel:

1. **Time.** Swing: the ride pattern (five variants, the plain one most often, the busier ones as ENERGY rises, straight quarters above 190 bpm), hi-hat with the foot on 2 and 4, a feathered bass drum on all four beats (FEATHER) that drops out at high energy. Ballad: brushes, one legato sweep per beat with taps on 2 and 4. Bossa: eighths on the closed hat (ride above ENERGY 0.55), the bass drum's "one, and of two" figure, a two-bar cross-stick clave. Soul: hat eighths, sixteenths as it heats up, ride bell at high energy, backbeat (cross-stick when quiet). House: four on the floor, open hat off the beat, snare on 2 and 4.
2. **Conversation.** Snare and bass drum comping in a two-bar figure that is kept or replaced by REPEAT; ghost notes; bass-drum bombs; never more than two comps in a row (three at high energy).
3. **Listening** (LISTEN, only when Comper's figure for the bar is known). A strong piano hit on an upbeat is a push: the drummer may catch it (snare, or bass drum with snare or ride when loud). It leaves the pianist's other hits alone, answers on an upbeat when the pianist has rested for a while, and thins out comps that would land on a bass note off the beat.
4. **Form.** Set-ups at the end of every 4 and 8 bars and into the top of the chorus (FILLS), small or big, with toms when it is big and loud. A set-up lands on the bass drum (and a crash if big) on the next 1, or with the pianist on the "and of 4" if there is a push there.
5. **Shape.** ENERGY scales all of it: loudness, how busy, bombs, crashes, which cymbal.

The hands: one slow drift shared by all limbs, per-stroke scatter, the snare a few milliseconds late, the hi-hat foot a little early, fills pushing, AHEAD moving everything up to 12 ms either way. LOOSE scales all of it.

## 4. The host

### 4.1 The link (`Link.hpp`)

`CompLink` gains three sections. Keep every existing field. Add:

```cpp
	// --- added in 2.2.0. Each section is valid only while its flag is set.
	bool hasComp = false;          // everything above, and the figure below, come from a Comper
	int planBar = -1;              // the bar the figure is for (bar 0 starts at phi 0); -1 none yet
	int planSlots = 8;             // 8 or 16
	int planKind[16] = {};         // -1 no hit, 0 short, 1 long (Comper's cellmap)
	double planBeat[16] = {};      // beats from the bar line (Comper's slotBeats)
	bool planPush[16] = {};        // Comper's pushOk
	bool hasBass = false;          // from a Walker
	int bassN = 0;                 // planned bass notes that fall off the beat, up to 16
	double bassBeat[16] = {};      // their beats, on the same clock as phi
	bool hasDrum = false;          // from a Rider
	float bandX = 0.f;             // -1..1: BAND * 2 * (ENERGY - 0.5)
```

The message now travels along a row of any length and order (Comper, Walker, Rider in any arrangement, each at most once is what we support; more than one of a kind must not crash). **Every module does the same thing each sample:**

1. Read the message from the left neighbour and from the right neighbour. A neighbour counts if it is a Comper, Walker or Rider and its message's `magic` is right and its `stamp` moved within the last 2 samples (per side: keep a last stamp and a stale counter per side, as Walker does now).
2. For its own use, merge: each section is taken from whichever side has that flag set (left wins a tie).
3. Send to the right neighbour: its own section, plus the sections that arrived from the left. Send to the left neighbour: its own section, plus the sections that arrived from the right. Never send a section back to the side it came from. Set `magic`, and its own ever-increasing `stamp`. Write into the neighbour's facing `producerMessage` and call `requestMessageFlip()`, as Comper's `sendLink()` does now.

Put the three steps in one small Rack-side helper (`src/LinkPort.hpp`: four `CompLink` buffers, `attach(Module*)`, `read(Module*, side)`, `send(Module*, side, const CompLink&)`) and use it in all three modules. Each hop costs one sample; nothing depends on that.

Own sections:

- **Comper**: all the old fields and `hasComp = true`. After `host.process()`, when its host has begun a new bar, copy the figure: `planBar` (the bar just begun), `planSlots = eng.slots`, `planKind[s] = eng.cellmap[s]`, `planBeat[s] = eng.slotBeats[s]`, `planPush[s] = eng.pushOk[s]`. Add `int planBar = -1;` to Comper's `Host`, set where `eng.beginBar(nextBar, ...)` is called and cleared to -1 in `doReset`.
- **Walker**: `hasBass = true`, and the off-beat notes it has planned and not yet played: add `int offBeats(double* out, int max) const` to `WalkerHost` that walks the note queue and returns `origin + note.beat` for every queued note whose nominal beat has a fractional part above 0.05 and is not behind `phi`.
- **Rider**: `hasDrum = true`, `bandX`.

Walker's `readLink()` and Comper's `sendLink()` are replaced by the helper. Walker uses the Comper section exactly as before (`linkOk` is now `merged.hasComp`). Comper turns ROOTS off when a Walker is anywhere in the row (`merged.hasBass`), not only next to it.

### 4.2 Transport

Three sources, as Walker: `RS_LINK` when the merged message has `hasComp`; else `RS_CABLE` when CLOCK is patched; else idle (silent, but the kit still rings out).

- **Link**: `phi`, `spb`, `running` and `resetCount` from the message, as `WalkerHost` does. A new `resetCount` resets the player. This holds whether Comper plays from its chart or from external chords.
- **Cable**: copy `WalkerHost::cableTransport()`'s clock and reset handling (median of three periods, pulses per beat from the menu, first pulse after a reset is beat 0).
- Changing source resets.

`chorusBars`: with a link and Comper on its chart, `max(1, round(steps * beatsPerStep / 4))`. Otherwise the menu's "Chorus length" (default 12).

### 4.3 Planning a bar

`nextBar` starts at 0 after a reset. Each sample, while `running` and `phi >= nextBar * 4 - 0.08`, plan the bar. One wait: when a Comper section is present and not `external`, hold the plan until `planBar >= nextBar` or `phi >= nextBar * 4 + 0.02`, whichever comes first. (After a reset Comper's first figure is one or two samples behind; without the wait the drummer would play bar 1 deaf.) To plan:

1. If SEED changed since the last plan, reset the engine with the new seed (a new take starts at a bar line).
2. Feel: FEEL 1..5 is feel 0..4. FEEL 0 (FOLLOW) is the link's `groove` when there is a Comper section, else SWING.
3. Build the `BarIn`. `has_piano` is true when the merged message has `hasComp`, is not `external`, and `planBar == nextBar`; then copy slots, kinds, beats and pushes. Otherwise no piano (Comper on external chords decides too late to be heard in advance; the drummer plays its own figures). `bass_off`: every `bassBeat[i]` in `[nextBar * 4, nextBar * 4 + 4)`, minus `nextBar * 4`, when `hasBass`. `force_fill` from the FILL input (below).
4. Parameters as they stand now; `swing` is the link's `swing` when there is a Comper section, else 0. `bpm = 60 / spb`.
5. `plan_bar(nextBar, bpm, chorusBars, p, bi)`. Queue each stroke with its onset beat `nextBar * 4 + beat` and its offset in seconds. `nextBar++`.

Comper begins its bar 0.12 beat early, so its figure is on the link in time.

**Firing.** Each sample, for each queued stroke: `until = ((onsetBeat - phi) * spb + offs)` seconds. When `until <= 0`, play it now (strokes planned late, or early offsets, simply play at once). A sweep's length in seconds is `dur * spb` at that moment. The kit quantises to its 16-sample tick itself. While LAY OUT is high, strokes are dropped instead of played; the triggers stay silent too.

**FILL input.** A rising edge asks for a set-up in the next bar that is planned: `force_fill = ENERGY >= 0.5 ? 2 : 1`, cleared once used.

### 4.4 The kit's clock

The kit always runs at 48 kHz. `RiderHost` keeps `acc += 48000 * dt` and runs one kit sample each time `acc >= 1`. At a 48 kHz engine rate that is one for one and the output is the kit's sample. At any other rate, read the kit's stream with 4-point Hermite interpolation at the fractional position (keep the last four kit samples per channel). No Rack resampler.

### 4.5 Outputs

- LEFT, RIGHT: kit output times 18 (a loud stroke is about 5 V), times LEVEL (LEVEL is the kit's `level`).
- Six triggers, 1 ms at 10 V, when a stroke of that group is played: KICK (voice 8), SNARE (6, 7, 11), HAT (3, 4, 5, 13), RIDE (0, 1), CRASH (2), TOM (9, 10). The sweep gives no trigger.
- VEL: polyphonic, 6 channels in that order, each holding its group's last velocity as `10 * vel / 127` V.
- ENERGY: `10 *` the energy in use (knob plus CV), so a patch can follow the drummer too.

For the tests, log every stroke as it is played: sample time, bar, beat from the bar line, voice, velocity.

## 5. The kit in real time (`RiderKit.hpp`)

Port `rider_kit_ref.py`'s `Kit`. The reference renders a whole take at once; the module runs the same arithmetic sample by sample. One 16-sample **tick** (`CTRL`) is the unit for everything except the audio.

State:

- `store[3][2][26][58]`: energy stores, by family (CYM, HAT, DRUM), channel, band and decay constant. 58 constants: the 8 `RATES`, the 2 `CLICK` rates, then for each (rate, tau) pair two more (`store_rates()` and `swell_index()` give the order).
- `dec[3][58]`: what each store is multiplied by per tick. It depends on DECAY and AGE (`rate_mult`); recompute when either moves. The click's two rates never change. In a swelling pair only the slow part follows `rate_mult`; the `1/tau` part does not.
- Per channel: a noise generator, a delay line for the 15 colour taps, and 26 band filters of two biquads each (`SOS`, transposed direct form II, which is what `scipy.signal.sosfilt` does).
- Per voice: its partials as rotating phasors (below).
- `gain[2][26]` at the last tick and at the coming tick.

**Blocks.** The kit works in blocks of 16 samples and is one block late: its output is the reference delayed by exactly 16 samples (0.33 ms). Block `k` covers reference samples `16k .. 16k + 15` and needs the band gains at tick `k` and at tick `k + 1`, so before making block `k` run **tick `k + 1`**:

1. Apply every stroke whose tick is `k + 1`, in the order they were given (the reference sorts by tick, stably). A stroke given at kit time t belongs to tick `ceil(t * 48000 / 16)`; a stroke that arrives for a tick already run goes to the next one. Its wash and click go into the stores now. Its partials start at reference sample `16 (k + 1)`, the first sample of the next block.
2. Band power at tick `k + 1` = the sum of that band's stores over the three families (per channel), plus the sweep's power if the hand is down at that tick. Clamp at 0; `gain = sqrt`.
3. Multiply every store by its `dec`.

Then make the 16 samples: sample `n` uses `gain[k] + (gain[k + 1] - gain[k]) * (n - 16k) / 16`. The test harness renders `dur` seconds plus one block and throws the first 16 samples away, so its output lines up with the reference sample for sample.

**The wash, per sample and channel:** `x = (u - 0.5) * sqrt(12)` with `u` from `Rng(seed * 2 + 11 + channel)`; `col = sum over taps of TAP_GAIN * x[n - TAP_DELAY]`; each band filters `col`; output is the sum over bands of `gain * filtered`. Run all 26 bands always (they must stay warm); it is 104 biquads a sample.

**A stroke** (`render()`'s inner block is the definition):

- `rs = Rng(seed * 7919 + 31 * idx + 5)`, `idx` counting strokes since the kit was reset, in the order they are applied.
- SNARE below velocity 42 becomes SNARE_EDGE.
- Level in dB from velocity: `32 * log10(vel / 96)`, clamped to the voice's table range (see the code), plus `rs.g() * 1.5 * age`.
- `tl`, `cl`: the voice's tail and click level tables read at that level (`curve()`: linear between layers, end slopes beyond).
- If the voice closes the hat (HATF, HATC, BR_HATF): multiply the whole HAT store family by 0.03 and the open hat's ringing partials by 0.1, before adding this stroke.
- Wash: for each band and each of its components `(rate, tau, c)`: `e = c * lin^2 * gain[band]^2 * 10^((tl[band] + tilt[band]) / 10) / BANDPOWER[band]`, split between the channels by `0.5 * (1 -/+ pan)`. A plain component adds `e` to its rate's store. A swelling one adds `e` to its rate's store, `-2e` to the pair's first store and `+e` to the pair's second (that sum is `e * exp(-rate t) * (1 - exp(-t / tau))^2`, which never goes negative).
- Click: bands whose upper edge is above 180 Hz, the two click stores, `click[band][k] * lin^2 * cgain[band]^2 * 10^((cl[band] + tilt[band]) / 10) / BANDPOWER[band]`.
- Partials, in table order, **three draws each in this order**: phase `rs.u() * 2 pi`, level scatter `rs.g() * 1.5` dB, detune `rs.g() * 12 * age` cents (the third draw only for tuned voices). Frequency moves with TUNE and AGE for tuned voices only; decay with `rate_mult`; level with `tl` at the partial's own band.

**Partials as phasors.** Each partial of each voice keeps a complex `z` that is multiplied every sample by `exp(-lambda / 48000) * exp(i * 2 pi f / 48000)`; its output is `Im(z)`, sent to the two channels by the voice's pan. A stroke does not replace `z`, it adds to it, so strokes pile up as they do on a real cymbal. The stick is in contact for a moment, so the new part fades in over `k = int(min(0.006, 1 / f) * 48000)` samples with `0.5 - 0.5 cos(pi n / k)`: keep a second phasor `zNew` for the stroke in progress, output `Im(z) + ramp * Im(zNew)`, and fold `zNew` into `z` when the ramp ends. If another stroke arrives during a ramp, fold the old one in at its current ramp value first. Frequency and decay are fixed at the stroke (the reference never changes them afterwards): when TUNE, DECAY or AGE have moved, the new stroke's part uses the new values in `zNew` and they become the partial's values when it is folded in. Skip a voice's partials entirely while all of them are below 1e-7; drop the reference's `mm` cut-off (it only saves the Python time, the tail it removes is below -100 dB).

**The brush sweep.** A sweep (start, length, velocity) sets the hand's level over its length: the gesture formula in `render()` (a rise and fall that never quite reaches zero, with 6% fades at the ends), the maximum over overlapping sweeps. At each tick where the hand is down: `drift = 0.995 * drift + rd.g() * 0.12` with `rd = Rng(seed * 7 + 3)` (draw only on those ticks), level `lv = 20 log10(g) + drift`, band power from `SWEEP_BANDS` interpolated over `SWEEP_LX` at `lv` clamped into the table, plus the excess `lv - clamped`, plus tilt; bands with upper edge at or below 130 Hz are skipped. It is added to the band power directly, not to a store. Note the reference evaluates a sweep that starts at sample `a` on ticks from `ceil(a / 16)` to `min(last tick, (a + m) // 16)`.

**Controls** (`KitParams`): `level`; `mix[5]` (RIDE covers ride, bell and crash; HATS; SNARE covers snare, cross-stick and brushes; KICK; TOMS), linear, applied at the stroke; `tune`, `decay`, `bright`, `width`, `age` as in `rate_mult`, `tilt_db` and the stroke code. Everything is read at the stroke except `dec`, which follows DECAY and AGE at once.

**Cost.** Target under 6% of one core at 48 kHz on the busiest preset. If the partials are the cost, process them in blocks of 16 with the phasor renormalised as `Upright.hpp` does, before trimming any table.

## 6. Panel

30 HP. A display across the top like Walker's, then three rows of knobs, then two rows of jacks. Group labels PLAYER, HANDS, KIT, IN, OUT.

- Display, left to right: the feel in use, `BAR 7/12` (bar in the chorus), the source (`LINK`, `CLOCK` or `-`), and what the drummer just did, held for a bar: `FILL`, `BIG FILL`, `CAUGHT` (it caught a push), `BOMB`, or nothing. Take these from the engine's `stats` counters changing during a plan.
- PLAYER row: FEEL (snapped, 6 positions), ENERGY (large), COMP, LISTEN, FILLS, FEATHER, REPEAT, BAND, SEED with the NEW button beside it.
- HANDS: LOOSE, AHEAD. KIT, first part, on the same row: RIDE, HATS, SNARE, KICK, TOMS.
- KIT, second row: TUNE, DECAY, BRIGHT, WIDTH, AGE, LEVEL.
- IN: CLOCK, RESET, ENERGY, COMP, FILLS, FILL, LAY OUT.
- OUT: LEFT, RIGHT, KICK, SNARE, HAT, RIDE, CRASH, TOM, VEL, ENERGY.
- Lights: HIT (any stroke), LINK (a Comper section is arriving), FILL (a set-up is being played).

Check the rendered panel for overlapping labels the way `tools/preview_walker.py` does, and commit `docs/rider-panel.png`.

## 7. Parameters

Ids in this order (the preset files depend on it):

| Id | Range, default | Tooltip |
|---|---|---|
| FEEL | 0..5 snapped, 0 | Follow Comper's groove, Swing, Bossa, Ballad (brushes), Soul, House |
| ENERGY | 0..1, 0.5 | Energy (how hard the band is playing: level, busyness, bombs, crashes) |
| COMP | 0..1, 0.5 | Comp (snare and bass drum conversation) |
| LISTEN | 0..1, 1 | Listen (catch the pianist's pushes, leave room, answer in the gaps) |
| FILLS | 0..1, 0.5 | Fills (set-ups at the ends of phrases and into the top of the chorus) |
| FEATHER | 0..1, 0.5 | Feather (quiet bass drum on all four beats) |
| REPEAT | 0..1, 0.6 | Repeat (keep the two-bar comping figure) |
| BAND | 0..1, 0 | Band (how far Comper and Walker follow ENERGY) |
| SEED | 0..999 snapped, 1 | Seed (a new take at the next bar line) |
| NEW | button | New seed |
| LOOSE | 0..2.5, 1 | Loose (0 machine, 1 a good drummer, 2.5 drunk) |
| AHEAD | 0..1, 0.5 | Ahead (0 lays back, 1 plays on top of the beat) |
| RIDE, HATS, SNARE, KICK, TOMS | 0..2, 1 | Level of the cymbals / hi-hats / snare and brushes / bass drum / toms |
| TUNE | 0..1, 0.5 | Tune (drums, five semitones either way) |
| DECAY | 0..1, 0.5 | Decay (how long cymbals and hi-hats ring) |
| BRIGHT | 0..1, 0.5 | Bright (dark to bright) |
| WIDTH | 0..1, 1 | Width (mono to the full kit) |
| AGE | 0..1, 0 | Age (old kit: darker cymbals, dead heads, slack tuning, uneven strokes) |
| LEVEL | 0..2, 1 | Level |

SEED, NEW and LEVEL are not randomised. CVs: ENERGY, COMP and FILLS add 0.1 per volt, clamped to 0..1.

## 8. Right-click menu

- Clock pulses per beat: 1, 2, 4, 24 (as Walker).
- Chorus length without a chart: 8, 12, 16, 32 bars (default 12).

Save both in the module's JSON (`ppb`, `chorusBars`).

## 9. Changes to Comper and Walker

Neither engine changes, and none of their golden tests may move.

- Both use the link helper (4.1) and send their own sections. Comper gains the four link buffers it did not need before.
- **BAND.** With `x = merged.hasDrum ? merged.bandX : 0`: Comper adds `0.35 * x` to DENSITY (clamped 0..1) before `host.process()` and multiplies its VEL outputs by `1 + 0.25 * x` (clamped to 10 V). Walker adds `0.35 * x` to ACTIVITY (clamped) and multiplies its audio and its VEL output by `10^(3 * x / 20)`. At BAND 0 nothing changes.
- Walker's LINK light and Comper's roots rule follow the merged message.
- Comper's manual chapter and Walker's get one line each about Rider.

## 10. Tests (`make test`, no Rack needed)

Add two programs and wire them into the `test` target.

**`test/render_rider.cpp`**

1. *Player.* For every case in `rider_golden.h`: `Rider(seed)`, then bar by bar `plan_bar` with the case's `RGBar` (and its energy when it is not negative). Stroke count, voice and velocity must be equal; time `(bar * 4 + beat) * spb + offs` within 1e-6 s; length within 1e-6. Print one line per failing case with the first difference. All must pass.
2. *Linked host.* For each of the 12 presets: Comper's `Host` (seed 11, the preset's parameters and chart, as `tools/render_trio.cpp` sets it up) and `RiderHost` (seed 11, the Rider preset from `rider_presets.h`) side by side at 48 kHz, the link filled each sample as the modules do, no Walker, 16 bars. The host's stroke log must equal case `linked NN_name`: same strokes in the same order with the same bar, voice and velocity, and beat within 1e-9. (This is the test that the figure crosses the link in time.)
3. *Clock.* No link, a 120 bpm clock at 1 pulse per beat (24,000 samples apart), chorus 12, seed 1, each FEEL from SWING to HOUSE in turn, 12 bars: the log equals `alone FEEL / 120 bpm / seed 1` in bar, voice and velocity, and beat within 1e-9. (120 because the host assumes it until it has heard two pulses.)
4. *Behaviour.* A reset mid-take replays the same strokes; a new SEED takes effect only at a bar line; a FILL trigger with FILLS at 0 puts snare or tom strokes in beats 3 and 4 of the next bar; LAY OUT high produces no strokes and no triggers; the six trigger outputs fire for the right voices.
5. *Other rates.* At 44.1 and 96 kHz the same clocked take logs the same strokes, and the output's energy over the take is within 1 dB of the 48 kHz render.
6. *Cost.* Render 30 s of preset 02 and print the fraction of real time; fail above 10% (the target is 6%).

**`test/render_rider_kit.cpp`**

For every case in `rider_kit_cases.h`: a `Kit` with the case's controls and seed, the strokes and sweeps, `dur` seconds. Measure as the reference did: each channel through the 26 `SOS` band filters (fresh filter state), the sum of squares over each window `[a, b)` in samples `int(a * 48000) .. int(b * 48000)`, `10 log10(sum + 1e-16)`. Compare with `cells` (channel, band, window). Over the cells within 60 dB of the case's loudest cell: mean absolute difference at most 0.3 dB and no cell beyond 2 dB; the peak within 5% of `peak`. With the draws in the right order the differences are a few hundredths of a dB; anything larger means an order or a formula is off, and the test exists to tell you so. If a case fails, render it with wash, click and partials one at a time against the Python (`Kit.render` is short enough to bisect). Write each case to `test/out/rider/NAME.wav`.

## 11. Demos

`tools/make_rider_demos.py` renders the 12 presets through the two reference engines with the Comper and Walker stems; listen to nothing, but use its files as the target. Add the drummer to `tools/render_trio.cpp` (Rider's host on the same link, writing `NAME.drums.wav`, stereo) and a `tools/make_rider_demos_cpp.py` that mixes piano, bass and drums from the C++ engines: the 12 presets, ENERGY at 0.1 and 0.9, LISTEN at 0, a building blues, BAND at 0 and 1 with an ENERGY ramp, and the kit controls at their ends. Put the WAVs and a zip in `/mnt/project-files/jazz-drums-vcv/demos/`.

## 12. Manual

A Rider chapter in `docs/MANUAL.md`, then rebuild `docs/Comper-Manual.pdf`: what a jazz drummer does and how Rider does it, every control, the link (any order, what each module tells the others, BAND), using it alone with a clock, the trigger outputs for other drum modules, the kit and how it was modelled, and patch recipes (the trio with no cables; Rider alone under your own sequence; ENERGY from an envelope follower on a soloist; the triggers into another kit; a long ENERGY ramp with BAND up so the whole band builds).

## 13. Notes for the builder

- The kit tables put the kit at the level of the recordings; do not normalise voices against each other. The balance between them is the real kit's.
- The model is deliberately not a modal cymbal. A bank of a few hundred modes was tried and rings like whistles; `docs/RIDER-RESEARCH.md` has the measurement. Do not go back to it.
- Nothing in the player may look further ahead than the coming bar. That is what lets it run from the link.
- Keep the engines free of Rack types so the tests build with a plain compiler, as the other two modules do.
- Commit and push to `claude/project-thread-7vm3pn` only. Check the Mac arm64 workflow goes green, as for Walker.
