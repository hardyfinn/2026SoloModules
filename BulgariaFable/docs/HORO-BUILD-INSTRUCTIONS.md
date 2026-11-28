# Horo: build instructions

Design by Fable, 2026-11-28, for the model that builds it. "Horo" (the Bulgarian chain dance) is my working name; Hardy may rename it.

Horo is a **clock and accent sequencer for Bulgarian uneven metres**, a companion to Bulgaria. It is fed a fast clock, one edge per short unit (a sixteenth or an eighth). It groups those pulses into the 2s and 3s of a dance metre and puts out beat gates, long-beat gates, a bar trigger, a phrase trigger made to patch into Bulgaria's PHRASE input, an accent CV, and the pulse clock itself with an optional lilt that stretches the long beats. It makes no audio.

Hardy asked for it in the Bulgaria thread ("he recommended an accent sequencer... Should it have a link to trigger the phrase here?") and said to have Fable design it and Opus build it. Hardy has not seen this design before the build. **This document is the definition.** Say in the README what you changed and why.

## 1. What to build

- A **second module in the existing plugin** `BulgariaFable` in hardyfinn/2026SoloModules. Bulgaria was built on branch `claude/project-thread-ww3s0f`. This thread may push only to `claude/project-thread-xj4sp9`, so: `git fetch origin claude/project-thread-ww3s0f`, create `claude/project-thread-xj4sp9` from it (reset the local branch to FETCH_HEAD if it exists and has nothing of ours), work there, push there. Do not push to ww3s0f or main. Tell Hardy that the branch with both modules is xj4sp9.
- plugin.json: bump the plugin version to `2.1.0`. Add module slug `Horo`, name `Horo`, tags Clock modulator, Sequencer, Utility. Description: "Clock and accent sequencer for Bulgarian uneven dance metres. Feed it a fast clock, pick ruchenitsa, daichovo, kopanitsa, buchimish or your own grouping of 2s and 3s; get beat and long-beat gates, bar and phrase triggers, an accent CV, and a pulse clock whose long beats can be stretched. The phrase trigger is made for Bulgaria's PHRASE input."
- Do not change Bulgaria's behaviour, parameters or presets. Bulgaria's 64 tests must still pass.
- Follow the conventions already in the folder: engine in `src/dsp/HoroEngine.hpp`, `namespace horo`, no Rack includes, so tests run offline; panel SVG from the panel script; presets from the preset generator; PDF from the existing PDF tooling; Haer logo; text labels; inverted plates for outputs.
- Panel **10 HP**.
- Deliverables: C++ module, panel, 12 factory presets, offline tests added to `make test`, a Horo chapter in MANUAL.md and a separate `docs/Horo-Manual.pdf` with patch recipes, README section, 4 demo clips (section 8), this file as `docs/HORO-BUILD-INSTRUCTIONS.md`.
- Check `.github/workflows/build.yml` runs for this branch (Apple silicon only, publishes to the "preview" release) and that the run is green.
- Afterwards: log Horo in Hardy's creations tracker (project memory `claude-creations-tracker`; one row per VCV module, plus the PDF if the tracker lists PDFs separately) and update project memory `bulgarian-harmony-vcv-project` and the index line in MEMORY.md.

## 2. Metres

A metre is a list of groups; each group is 1 to 9 pulses; a bar is at most 32 pulses and at most 16 groups. Eighteen slots on the METRE knob: fourteen factory, four USER.

| # | Display name | Count | Grouping |
| --- | --- | --- | --- |
| 1 | PRAVO | 6 | 3 3 |
| 2 | PAIDUSHKO | 5 | 2 3 |
| 3 | RUCHENITSA | 7 | 2 2 3 |
| 4 | LESNOTO | 7 | 3 2 2 |
| 5 | ELENINO | 7 | 2 2 1 2 |
| 6 | DAICHOVO | 9 | 2 2 2 3 |
| 7 | GRANCHARSKO | 9 | 2 3 2 2 |
| 8 | KOPANITSA | 11 | 2 2 3 2 2 |
| 9 | PETRUNINO | 12 | 3 2 2 3 2 |
| 10 | KRIVO | 13 | 2 2 2 3 2 2 |
| 11 | BUCHIMISH | 15 | 2 2 2 2 3 2 2 |
| 12 | YOVE | 18 | 3 2 2 2 2 3 2 2 |
| 13 | SANDANSKO | 22 | 2 2 2 3 2 2 2 3 2 2 |
| 14 | SEDI DONKA | 25 | 3 2 2 3 2 2 2 2 3 2 2 |
| 15 to 18 | USER 1 to 4 | | defaults: 3 3 2; 2 3 3; 3 2 3; 2 2 2 2 |

These groupings are from my memory of the folk-dance teaching literature, not checked against a source in this session; names and groupings vary by region and band. Say so in the manual in one sentence. Default metre: RUCHENITSA.

**User metres** are typed in the context menu: four text fields, "User 1" to "User 4", accepting digits separated by `+`, spaces, commas or nothing ("2+2+3", "2 2 3" and "223" are the same). Ignore other characters. If the result is empty or breaks the limits, keep the previous grouping. Stored in the patch JSON, and therefore in presets.

**Long and short.** In a bar, a group is **long** if it has more pulses than the bar's shortest group. If all groups are equal, none is long.

**Choosing.** Slot = METRE knob (stepped, 1 to 18) + round(METRE CV volts × 2), clamped to 1..18. So 0.5 V per slot, and a sequencer on METRE CV chains metres bar by bar (7 + 7 + 11 makes Sedi Donka the long way). A change of slot, of a user grouping, or of LILT takes effect **at the next bar line**, never inside a bar. Context menu option "Change metre at": "next bar" (default) or "next phrase".

## 3. Timing engine

One continuous position drives everything, so that pulses are never dropped or doubled, a stopped clock freezes the module cleanly, and with LILT at zero every output is sample-accurate with the input clock.

State: `T` (the clock period in seconds; 0 until two edges have been seen; kept across RESET), `elapsed` (seconds since the last clock edge), `kIn` (index in the bar of the last input edge), `kOut` (output pulses emitted so far in this bar), `bar` (index in the phrase), `N` (pulses in the bar), and a table `u[0..N]` of warped pulse times in units of pulses, with `u[0] = 0` and `u[N] = N`.

**CLOCK edge** (Schmitt trigger, rising through 1 V after being below 0.1 V; ignore edges within 1 ms after a RESET):

1. If a previous edge exists and `elapsed` < 2 s: if `T` is 0 or `elapsed` differs from `T` by more than 20 percent, `T = elapsed`; otherwise `T = 0.5*T + 0.5*elapsed`.
2. `elapsed = 0`, `kIn += 1`.
3. If `kIn == N` (or this is the first edge after RESET or load): emit at once any output pulses of the old bar not yet emitted (section 5, "queue"); then `kIn = 0`, `kOut = 0`, `bar = (bar + 1) mod BARS` (first edge: `bar = 0`); latch the metre slot, user groupings and LILT; rebuild `N` and `u`.

**Every sample:** `phi = kIn + (T > 0 ? min(elapsed / T, 0.999) : 0)`. While `kOut < N` and `phi >= u[kOut]`: emit output pulse `kOut`, `kOut += 1`. Then `elapsed += 1/sampleRate`.

Because of the 0.999 ceiling, a whole-number `u` is only reached on the clock edge itself. `u[0]` is always 0, so the bar's first pulse always fires on the same sample as its clock edge, and with LILT 0 all of them do.

**Lilt table.** LILT is -1 to +1, default 0, CV 0.2 per volt, clamped. Let `PL` and `PS` be the pulse counts in long and short groups. If `PL` or `PS` is 0, there is no lilt. Otherwise `a = 0.25 * LILT`, limited so that the short pulse never drops below 0.6 and (for negative LILT) the long pulse never below 0.6: `a = clamp(a, -0.4, 0.4 * PS / PL)`. A pulse in a long group lasts `1 + a`; a pulse in a short group lasts `1 - a * PL / PS`. `u` is the running sum. The bar keeps its length. Positive LILT stretches the long beats (ruchenitsa at full: the 3 lasts 3.75 pulses and each 2 lasts 1.625); negative squeezes them toward an even bar.

**Clock stopped.** If `T > 0` and `elapsed > max(4*T, 0.25 s)`, all gates go low. Nothing else changes; the next edge carries on from where it was.

**RESET** (input rising edge or the panel button): all gates low, queue cleared, and the next clock edge is pulse 0 of bar 0. `T` is kept.

## 4. Outputs

All gates and triggers are 0/10 V. On emitting output pulse `k`, which is pulse `j` of group `g` (length `n`, warped start `s = u[k]`, warped pulse length `d`):

- **PULSE**: goes high; falls when `phi >= s + 0.5*d`. This is the clock for a downstream sequencer, with the lilt in it.
- **BEAT**: if `j == 0`, goes high; falls when `phi` reaches the middle of the group (`u[groupStart] + 0.5 * (u[groupEnd] - u[groupStart])`). So long beats give longer gates.
- **LONG**: the same as BEAT, but only for long groups.
- **BAR**: 1 ms trigger if `k == 0`.
- **PHRASE**: 1 ms trigger at the phrase point (below).
- **ACCENT**: a held CV, updated on every output pulse: `10 V * (1 - A * (1 - w))`, A = ACCENT knob (0..1, default 0.8), and weight `w`:

| Pulse | w |
| --- | --- |
| First pulse of the phrase (bar 0, k 0) | 1.00 |
| First pulse of any other bar | 0.90 |
| First pulse of a long group | 0.75 |
| First pulse of a short group | 0.55 |
| Last pulse of a group of 3 or more (the pickup) | 0.35 |
| Any other pulse | 0.20 |

When `T` is 0 (only before the second clock edge ever), PULSE, BEAT and LONG fall after 1 ms instead.

**Phrase.** BARS knob: 1, 2, 3, 4, 6, 8, 12, 16 bars per phrase, default 4. The phrase point is in the last bar of the phrase (`bar == BARS - 1`). Context menu "Phrase trigger on": "last beat" (default: pulse 0 of the last group), "last long beat" (pulse 0 of the last long group; if none, the last group), "last pulse". This matches Bulgaria, whose PHRASE input marks the last note of a phrase and acts 2 ms after the edge.

**Queue.** If more than one pulse must be emitted in the same sample (only after a sudden tempo jump), emit one and hold the rest in a queue, emitting one per millisecond with PULSE low for at least 0.5 ms between. The count of output pulses always equals the count of input edges once the queue is empty.

## 5. Panel (10 HP)

Top to bottom; lay out with the script.

- Title HORO, Haer logo.
- **Display**, full width, about 24 mm tall, dark, same style as other Fable displays if the folder has one, otherwise a plain NanoVG widget. Line 1: metre name and count ("RUCHENITSA 7"). Line 2: grouping ("2 2 3"); for more than 8 groups shrink the font to fit. Line 3: one dot per pulse, with a gap between groups, wrapped to two rows above 16 pulses; long groups' dots slightly larger; the current pulse lit bright, the current group lit dim. Line 4, small: "BAR 3/4". If a metre change is waiting for the bar line, show the coming name on line 1 in a dimmer colour until it takes over.
- **METRE** large stepped knob (18 positions; tooltip gives name and grouping).
- Row of three small knobs: **BARS** (8 positions, values printed), **LILT** (centre detent look, labels "EVEN" at left, "LONG" at right), **ACCENT**.
- Inputs: **CLOCK**, **RESET** (with a small momentary button beside it), **METRE** CV, **LILT** CV.
- Outputs on inverted plates: **PULSE**, **BEAT**, **LONG**, **BAR**, **PHRASE**, **ACCENT**. Small lights at BEAT, BAR and PHRASE (stretch the 1 ms triggers to about 80 ms for the lights).

Mono throughout: read channel 0.

## 6. Presets (12)

Columns: METRE, BARS, LILT, ACCENT, phrase option (LB last beat, LL last long beat, LP last pulse). User groupings only where given.

| # | Name | METRE | BARS | LILT | ACCENT | Phrase |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Init (Ruchenitsa) | RUCHENITSA | 4 | 0 | 0.8 | LB |
| 2 | Ruchenitsa Played | RUCHENITSA | 4 | 0.5 | 0.9 | LB |
| 3 | Paidushko Limp | PAIDUSHKO | 4 | 0.6 | 1 | LB |
| 4 | Lesnoto Slow | LESNOTO | 2 | 0.4 | 0.7 | LB |
| 5 | Daichovo | DAICHOVO | 4 | 0.3 | 0.8 | LB |
| 6 | Kopanitsa | KOPANITSA | 4 | 0.3 | 0.8 | LL |
| 7 | Krivo | KRIVO | 4 | 0.2 | 0.8 | LL |
| 8 | Buchimish | BUCHIMISH | 2 | 0.2 | 0.8 | LB |
| 9 | Sedi Donka | SEDI DONKA | 2 | 0.2 | 0.8 | LB |
| 10 | Machine Seven | RUCHENITSA | 8 | 0 | 1 | LP |
| 11 | Squared Off | KOPANITSA | 4 | -1 | 0.6 | LB |
| 12 | Techno 3 3 2 | USER 1 (3 3 2) | 8 | 0.15 | 1 | LB |

## 7. Manual

A Horo chapter in MANUAL.md and `docs/Horo-Manual.pdf` from the same tooling. Sections: what it is (and that it makes no sound); two paragraphs on uneven metres (short and long beats, counted "quick quick slow", why a bar of 7 is three beats, not seven), with the one-sentence caution from section 2; how to clock it (in VCV's Clocked or CLK, use a ×4 output for sixteenths; tempo of the dance is in pulses, e.g. ruchenitsa around 280 to 520 pulses a minute); every control, jack and menu item; the metre table; what LILT does with the worked ruchenitsa numbers; the accent table; presets; patch recipes with exact cable lists, stock VCV modules where possible. At least these eight:

1. **First patch.** CLK ×4 into CLOCK. PULSE into SEQ 3's clock, BAR into its reset. Sequencer CV and gate into a VCO and envelope. ACCENT into the VCA level or filter cutoff.
2. **With Bulgaria, the phrase link.** PULSE clocks the sequencer; sequencer into Bulgaria V/OCT and GATE; Horo PHRASE into Bulgaria PHRASE. BARS 4. Try Bulgaria's CADENCE on TIGHT then RESOLVE, and WHOOP up. Explain the three "Phrase trigger on" choices and when the last long beat is the better one.
3. **Accents drive the rub.** ACCENT through an attenuator into Bulgaria's RUB CV, so strong beats clash harder; alternatively into LOOSE.
4. **Shake on the long beat.** LONG into Bulgaria's SHAKE trigger; BAR into SLIDE.
5. **Melody on the beats, not the pulses.** BEAT into the sequencer clock: the tune moves quick, quick, slow. BEAT also as Bulgaria's GATE for long notes on long beats.
6. **Tapan.** BAR to a kick, LONG to a low tom, PULSE through a VCA opened by ACCENT to a hat or rim. LILT up for a played feel.
7. **Chaining metres.** A slow sequencer clocked by BAR into METRE CV (0.5 V per slot): 7 + 7 + 11, or ruchenitsa answering kopanitsa.
8. **Your own metre.** Typing a grouping into a USER slot; 3 3 2 and 2 3 3 under a 4/4 kick.

State that nothing has been tested in Rack.

## 8. Tests and demos

**Tests** (offline, added to `make test`, 48 kHz unless stated; clock at 8 pulses a second):

- RUCHENITSA, LILT 0, 70 clocks: 70 PULSE rises, each on the same sample as its clock edge; BEAT rises on pulses 0, 2, 4 of each bar; LONG on pulse 4 only; BAR on pulse 0; BEAT gate lengths 1 pulse (the 2s) and 1.5 pulses (the 3) within 2 samples.
- Every factory metre: group lengths sum to the stated count; BEAT count per bar equals the number of groups.
- PHRASE, BARS 4, last beat: one trigger per 28 pulses, on pulse 4 of the fourth bar. Last pulse: pulse 6. KOPANITSA last long beat: pulse 4 of the last bar.
- ACCENT 1: values 10, 9, 7.5, 5.5, 3.5, 2 V on the right pulses; ACCENT 0: always 10 V.
- LILT 1, RUCHENITSA: pulse times within the bar are 0, 0.8125, 1.625, 2.4375, 3.25, 4.5, 5.75 pulses (within 2 samples); pulse 0 of every bar on its clock edge; 70 clocks in, 70 pulses out.
- LILT -1, RUCHENITSA: long pulse 0.75, short pulse 1.1875.
- LILT limit: USER "3 3 2" at LILT 1 gives short pulse 0.6, not less.
- Metre change mid-bar takes effect at the bar line; with "next phrase", at bar 0.
- User parsing: "2+2+3", "2 2 3", "223" equal; "" and a 40-pulse string rejected, previous kept.
- Clock stop for 3 s mid-bar then resume: gates low during the stop; total pulses out equals clocks in; bar position continues.
- Tempo doubling and halving mid-bar at LILT 1: pulses out equals clocks in within one bar; no two PULSE rises closer than 0.5 ms.
- RESET mid-bar: next clock gives BAR and the phrase's first accent (10 V).
- METRE CV: 0.5 V steps one slot; clamps at 18.
- Fuzz: random parameters, CV, resets and a jittering clock (period random within ±30 percent, occasional stops) for 60 s at 44.1, 48 and 96 kHz: no NaN, outputs within 0..10 V, pulse count conserved after each quiet second.

**Demos** (4 clips, MP3, in `/mnt/project-files/bulgaria-vcv/demos/horo/`). Horo is silent, so render through stand-ins: run the C++ Horo engine from a test tool, drive simple drum sounds (kick on BAR, low tom on LONG, tick on PULSE with level from ACCENT) and, where stated, the Bulgaria C++ engine through the existing saw stand-in synth with a tune advanced by PULSE and PHRASE patched across.

1. `horo-1-ruchenitsa-with-bulgaria` : ruchenitsa, 8 bars LILT 0 then 8 bars LILT 0.5, tapan plus Bulgaria preset 1, PHRASE linked. **The one to hear first.**
2. `horo-2-lilt-off-on` : kopanitsa drums only, 4 bars LILT 0, 4 bars LILT 1, 4 bars LILT -1.
3. `horo-3-metres` : drums only, 2 bars each of paidushko, ruchenitsa, daichovo, kopanitsa, buchimish.
4. `horo-4-chained` : 7 + 7 + 11 by METRE CV with Bulgaria preset 16 (Techno Cluster) and a kick on BAR.

Say in the README and manual that the tone in the demos is not the module's.

## 9. Finish

Build for Apple silicon through CI, confirm green, publish to the "preview" release as the other folders do, commit and push on `claude/project-thread-xj4sp9`. Copy the Horo manual PDF and a panel preview to `/mnt/project-files/bulgaria-vcv/`. Report to Hardy in under 80 words plus attachments: the manual PDF, the panel preview, demo 1 first. State that nothing has been tested in Rack, that the working name is Horo, and that it is 10 HP rather than the 8 first mentioned (for a readable metre display).
