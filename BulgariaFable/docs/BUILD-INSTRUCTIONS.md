# Bulgaria: build instructions

Design by Fable, 2026-11-28, for the model that builds it. "Bulgaria" is Hardy's name for the module. Hardy's brief: apply the theory of Bulgarian folk a cappella singing (Le Mystère des Voix Bulgares) to synthesizer patterns; "I'm keen on getting some of that microtonal rub that they do so well."

Bulgaria is a **harmoniser, not a voice**. It takes one melody (V/Oct and gate) and puts out up to eight voices of pitch, gate and "push" CV. It makes no audio. Hardy patches the poly outputs into their own oscillators.

The proposal Hardy approved is at https://claude.ai/artifact/Lyn2DzYdi2ZiaVi31ZccKE. The Python prototype is `/mnt/project-files/bulgaria-vcv/prototype/bulgaria_proto.py` with `make_sketches.py`; the four sketches Hardy heard are in `/mnt/project-files/bulgaria-vcv/sketches/`. Hardy's verdict on them: "It sounds pretty good, but I suspect I want to be able to change it", so the RUB knob with CV is the central control.

**This document is the definition.** The prototype is an offline sketch with lookahead-free rules but simplifications; where they differ (section 9 lists the known differences), this document decides. Nobody has listened to anything beyond the four sketches, and I (Fable) checked those by numbers, not by ear. Say in the README what you changed and why.

Hardy's decisions, all final unless Hardy says otherwise:

- Harmoniser only. No built-in voice, no clock or metre generator.
- Three parts by default, clusters up to eight.
- Rub switchable between a width in cents and a beat rate in Hz.
- Tradition rules decide when to rub, with a probability option.
- The melody is retuned (TUNE amount).
- Drone from ROOT knob and MODE selector, with a V/Oct override input.
- PHRASE trigger marks the **last** note of a phrase ("Phrase trigger at the end. Yeah, let's try that").
- Poly outputs carry all voices including lead and drone; separate lead and drone jacks as well.
- Per-voice poly PUSH CV.
- The drone follows the input gates "for now" (Hardy is unsure about holding through rests; do not add a hold switch, but keep the gate logic in one place so it is easy to add).
- Ornaments fire by rule with an amount knob each, and from trigger inputs.
- LOOSE knob (Hardy's AGE idea applied to human singers).
- 24 factory presets ("twice as many" as the 12 proposed).

## 1. What to build

- Plugin folder `BulgariaFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-ww3s0f` (not main). Use `LiveWireFable` on branch `claude/project-thread-ebi3d9` as the template for Makefile, plugin.json, panel script, Haer logo, offline test harness, preset generator, PDF tooling and README: `git fetch origin claude/project-thread-ebi3d9`, then `git show FETCH_HEAD:LiveWireFable/...`. Do not copy that folder into this branch. `.github/workflows/build.yml` builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release; check it exists on this branch and bring it from the template branch if not. Apple silicon only.
- plugin.json: slug `BulgariaFable`, name `Bulgaria Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `Bulgaria`, name `Bulgaria`, tags Polyphonic, Quantizer, Utility, Random. Description: "Harmonises one melody the way Bulgarian village choirs do: a drone and up to seven more voices that sit a deliberately mistuned second from the lead. Rub set in cents or as a beat rate in Hz, village tunings, slides, throat shakes and the phrase-end whoop. Pitch, gate and push CV out; makes no sound itself."
- Panel 20 HP, text labels, Haer logo as on the other Fable modules.
- Deliverables: C++ module, panel SVG from a script, 24 factory presets, offline tests (`make test`), MANUAL.md, PDF manual with patch recipes, README, demo audio (section 8), the Python prototype in `tools/proto/`, this file in `docs/BUILD-INSTRUCTIONS.md`.
- Put the engine in `namespace bulgaria` in `src/dsp/BulgariaEngine.hpp` with no Rack includes, so the tests run offline.
- All inputs are mono: read channel 0.
- After the build, log the module in Hardy's creations tracker (see project memory `claude-creations-tracker`) and update project memory `bulgarian-harmony-vcv-project`.

## 2. Conventions

- Pitch inside the engine is in **cents relative to C4** (0 V). Output volts = cents / 1200. Frequency for the Hz rub: `f = 261.6256 * 2^(cents/1200)`.
- The engine runs per sample. Rules are evaluated on events; smoothing, ornaments, lean-in and drift run every sample (decimating the slow noises is fine).
- "Scale step" or "degree" `L` is an integer counting mode steps from the root: `L = 7*o + d`, d in 0..6. Negative values are below the root.
- Random numbers come from one seeded generator in the engine (xorshift is fine), reseeded from Rack's random on module creation, with a fixed seed in tests.

## 3. Root, modes, tuning

**Root.** ROOT knob (12 steps, C to B) plus OCT knob (5 steps, -2 to +2, default 0) give the root: `rootCents = 100*ROOT + 1200*OCT`. If DRONE is patched, the root is instead the DRONE voltage rounded to the nearest semitone (with 20 cents of hysteresis), and both knobs are ignored. A root change takes effect at the next note event.

**Modes.** Seven, each with an equal-tempered and a village tuning in cents:

| MODE | Tempered | Village |
| --- | --- | --- |
| SHOPE | 0 200 300 500 700 900 1000 | 0 170 310 500 700 870 1010 |
| AEOLIAN | 0 200 300 500 700 800 1000 | 0 190 305 500 700 810 1000 |
| DORIAN | 0 200 300 500 700 900 1000 | 0 195 300 500 700 890 1000 |
| PHRYGIAN | 0 100 300 500 700 800 1000 | 0 125 305 500 700 815 1000 |
| HIJAZ | 0 100 400 500 700 800 1000 | 0 130 385 500 700 810 1000 |
| MIXOLYDIAN | 0 200 400 500 700 900 1000 | 0 195 385 500 700 885 1000 |
| MAJOR | 0 200 400 500 700 900 1100 | 0 200 390 500 700 890 1090 |

The village values are my estimates from the ranges reported for this singing (seconds between about 130 and 200 cents), not measurements. SHOPE and DORIAN share a tempered scale on purpose: SHOPE is the strongly bent one. Default MODE is SHOPE.

`degCents(L) = 1200*o + temp[d] + TUNE*(vill[d] - temp[d])`, relative to the root.

**Snapping the input.** `s = round((inCents - rootCents)/100)` (semitones from root). Split `s = 12*o + r`, r in 0..11. Among the seven tempered degrees and 1200 (which is degree 0 of the next octave), pick the one nearest to `100*r`; a tie goes to the lower. That gives `L`.

**The lead's pitch.** `leadCents = rootCents + degCents(L) + (1-TUNE)*(100*r - snapped)`, where `snapped` is the tempered value the note snapped to (0..1200, with 1200 meaning the next octave's root), so the bracket is the distance of the played semitone from that degree (zero for in-scale notes). So at TUNE 0 the lead is exactly the semitone that came in, chromatic notes included, and at TUNE 1 it is fully snapped and village-tuned. Harmony voices always use `degCents` of whole degrees. TUNE default 1.

## 4. Note events

- GATE patched: a rise starts a note. While the gate is high, a change of the snapped semitone `s` starts a new **legato** note (no gate dip on any voice). A fall ends the note. Pitch changes within 2 ms after a gate rise are adopted silently as part of that note.
- GATE unpatched: the gate is treated as always high; every change of `s` is a legato note.
- PHRASE: a rising edge is acted on 2 ms later (this absorbs a trigger that arrives a sample before or after the step it belongs to). At that moment, if a note is sounding it becomes a **cadence** note, with the cadence clock starting then; if the gate is low, the next note to start is a cadence note. With nothing patched no note is a cadence.
- Each note draws, per voice v, a stagger `d_v = 35 ms * LOOSE * u`, u uniform 0..1, and `d_1 = 0` for the lead. The voice's own note start (pitch target change, gate rise, lean-in clock) is `t0 + d_v`. All voices' gates fall together with the input gate (except a whoop, section 6).

## 5. The voices

VOICES (1 to 8, default 3) sets how many are active and the channel count of the three poly outputs. Voice order is fixed:

| # | Name | Rule (L = lead degree; "near side" is below the lead when L >= 0, above when L < 0) |
| --- | --- | --- |
| 1 | LEAD | `leadCents`. |
| 2 | RUB | Rub, 1 step, near side. In CHANCE mode, with probability 1 - CHANCE drawn per note, instead a plain scale third on the near side (`L -/+ 2`), not rubbing. |
| 3 | DRONE | Degree 0. If L == 0 and the note is not a cadence, degree -1, so the second never disappears. |
| 4 | LOW DRONE | Degree -7. Never dodges. |
| 5 | UPPER RUB | Rub, 1 step, far side. |
| 6 | FIFTH | Degree 4. |
| 7 | UNDER RUB | Rub, 2 steps, near side. |
| 8 | HIGH DRONE | Degree 7. |

SPREAD (0..1, default 0, read at each note start) turns the open voices into more cluster: above 0.25 voice 8 becomes rub, 2 steps, far side; above 0.5 voice 6 becomes rub, 3 steps, near side; above 0.75 voice 4 becomes rub, 3 steps, far side.

The DRONE jack pair always carries voice 3's values, computed even when VOICES < 3.

**A rubbing voice** with step count k and direction sgn (-1 below, +1 above) has pitch `leadCents + iv(t)`:

- `sc = degCents(L + sgn*k) - degCents(L)`: the plain scale interval.
- CENTS mode: `target = sgn * k * W`, W from the RUB knob, 25 to 250 cents, linear (knob default 0.644 gives 170).
- HZ mode: `target = 1200*log2(max(fLead + sgn*k*B, 0.5*fLead) / fLead)`, B from the RUB knob as `1 + 79*x^2.2` Hz (1 to 80; the default knob position gives about 31). `fLead` uses the lead's current smoothed pitch without ornaments.
- RUB CV adds 0.1 per volt to the knob position, clamped 0..1. W and B are read continuously, so the knob and CV move a sounding rub.
- On a cadence note with CADENCE = TIGHT, W or B is multiplied by 0.6, crossfaded over 150 ms from the cadence clock.
- Wander: `target *= 1 + 0.12 * LOOSE * n_v(t)`, n_v a slow noise per voice (section 7).
- Lean-in: `iv = sc + lean * (target - sc)`, `lean = 1 - exp(-t/0.12 s)`, t counted from the voice's own note start. The singer lands on the scale step and closes to the clash.
- On a cadence note with CADENCE = RESOLVE: `iv` is multiplied by `clamp(1 - (tc - 0.35)/0.30, 0, 1)`, tc = seconds on the cadence clock. The voice closes to a unison with the lead.

Because the rub follows `leadCents`, when the lead is one step above the root, voice 2 lands a few cents or Hz off the drone and beats against it too. That is intended.

**Glide.** Every voice's pitch (before ornaments and drift) passes through a one-pole smoother: 12 ms for the lead, 22 ms for the others. A voice whose gate has been low for 50 ms or more jumps to its target without glide.

## 6. Ornaments

Each has an amount knob (0..1) and a trigger input. Depths marked * are multiplied per firing by `1 + 0.4*LOOSE*u`, u uniform -1..1.

- **SLIDE** (default 0.3). By rule: at each note start, with probability SLIDE. The lead gets an offset of -160* cents decaying as `exp(-t/45 ms)`; every rubbing voice gets -90* cents decaying as `exp(-t/70 ms)` from its own note start. Trigger: the same, starting now on the sounding note.
- **SHAKE** (default 0.3). The throat shake, tresene. By rule: when a note has been held 0.30 s, once, with probability SHAKE; above SHAKE 0.75 it is tried again every 0.6 s while the note holds. A shake is three flicks starting 0, 90 and 180 ms after it fires. Each flick is a `sin^2` bump 28 ms long and +190* cents high on the lead only, and the lead's PUSH is multiplied by 0.55 during each flick. Trigger: one shake now.
- **WHOOP** (default 0.5). The izvik. By rule: when a cadence note's gate falls, with probability WHOOP. For 170 ms the gates of the lead and of voice 5 (if active) stay high, their pitch gets an offset of `1000 * x^1.6` cents (voice 5: 1150), x running 0 to 1, and their PUSH is `10 V * (1-x)^0.7`. Other voices' gates fall normally. Trigger: a whoop starts now; if the input gate is still high afterwards, the whooping voices stay silent (gate low) until the next note event. A note that starts during a whoop cancels it.

A legato note change is a note start for SLIDE and resets the SHAKE clock. A note with gate fall and no cadence has no whoop.

## 7. LOOSE and PUSH

**LOOSE** (0..1, default 0.3, CV 0.1 per volt) scales together:

- Drift: each voice has a slow noise, white noise through two one-poles at 0.5 Hz, normalised to unit RMS. Pitch offset = `9 cents * LOOSE * noise` (half that for the lead). A second, independent noise of the same kind per voice is `n_v` for the rub wander.
- Stagger, section 4.
- Ornament depth variation, section 6.
- PUSH wander: `+/- 0.5 V * LOOSE` from a third slow noise per voice.

At LOOSE 0 every voice is exact and deterministic except the probability draws.

**PUSH** is 0 to 10 V per voice, meant for filter cutoff or level so that clashing voices lean in. Target values, then an asymmetric one-pole (30 ms rising, 60 ms falling), and 0 V whenever the voice's gate is low:

| Voice state | Target |
| --- | --- |
| Lead, no voice rubbing | 7.0 |
| Lead, at least one voice rubbing | 7.0 + 3.0 * lean of voice 2 (or of the first rubbing voice) |
| Rubbing voice | 5.5 + 4.5 * its lean |
| Drone, low drone, fifth, high drone, voice 2 on a plain third | 6.0 |
| Drone while the lead is exactly one step from it | 7.5 |
| Cadence note, TIGHT | +1.0 on the lead and all rubbing voices |
| Shake flick, whoop | as section 6 |

Clamp to 0..10.

## 8. Panel, presets, manual, tests, demos

**Panel (20 HP).** Lay it out with the script; this is the inventory and grouping, top to bottom.

- Title BULGARIA, Haer logo.
- Row "DRONE": ROOT (12 steps, note names printed), OCT (5 steps), MODE (7 steps, names printed around the knob), TUNE.
- Row "CHOIR": VOICES (1 to 8, numbers printed), SPREAD, then eight small bicolour lights numbered 1 to 8: green = sounding, red = rubbing, brightness from PUSH.
- Centre: large RUB knob with two printed scales (CENTS 25 to 250 outside, HZ 1 to 80 inside), CENTS/HZ switch, RULE/CHANCE switch, CHANCE knob (default 0.5), CADENCE switch (TIGHT/RESOLVE).
- Row "ORNAMENT": SLIDE, SHAKE, WHOOP knobs, each with its trigger input directly beneath.
- LOOSE knob.
- Inputs: V/OCT, GATE, DRONE, PHRASE; CV for RUB, TUNE, VOICES (0 to 10 V adds 0 to 7 voices), LOOSE.
- Outputs (inverted label plates as on the other Fable modules): POLY V/OCT, POLY GATE, POLY PUSH; LEAD V/OCT, LEAD GATE; DRONE V/OCT, DRONE GATE.

Tooltips in real units: RUB shows "170 cents" or "31 Hz" according to the switch; MODE and ROOT show names. Gates are 0/10 V.

**Presets (24).** ROOT C, OCT 0 in all. Columns: MODE, TUNE, VOICES, SPREAD, rub (c = cents, Hz), RULE or CHANCE value, CADENCE, SLIDE, SHAKE, WHOOP, LOOSE.

| # | Name | MODE | TUNE | VOICES | SPREAD | RUB | RULE | CAD | SLIDE | SHAKE | WHOOP | LOOSE |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Init (Shope Three) | SHOPE | 1 | 3 | 0 | 170 c | RULE | TIGHT | 0.3 | 0.3 | 0.5 | 0.3 |
| 2 | Shope Duet | SHOPE | 1 | 2 | 0 | 160 c | RULE | TIGHT | 0.4 | 0.5 | 1 | 0.4 |
| 3 | Pirin Drone | AEOLIAN | 1 | 3 | 0 | 190 c | RULE | RESOLVE | 0.5 | 0.2 | 0 | 0.5 |
| 4 | Rhodope Wide | MIXOLYDIAN | 0.8 | 4 | 0 | 210 c | RULE | RESOLVE | 0.6 | 0 | 0 | 0.4 |
| 5 | Thrace Hijaz | HIJAZ | 1 | 3 | 0 | 130 c | RULE | TIGHT | 0.5 | 0.7 | 0.6 | 0.4 |
| 6 | Harvest Song | SHOPE | 1 | 3 | 0 | 150 c | RULE | TIGHT | 0.2 | 0.9 | 1 | 0.7 |
| 7 | Koutev Choir | AEOLIAN | 0.7 | 6 | 0 | 180 c | RULE | RESOLVE | 0.4 | 0.2 | 0.3 | 0.4 |
| 8 | Mystère Eight | DORIAN | 1 | 8 | 0.3 | 170 c | RULE | TIGHT | 0.4 | 0.3 | 0.6 | 0.5 |
| 9 | Tight Cluster | PHRYGIAN | 1 | 8 | 1 | 120 c | RULE | TIGHT | 0.2 | 0 | 0.4 | 0.3 |
| 10 | Squeezed Lead | SHOPE | 1 | 5 | 0 | 140 c | RULE | TIGHT | 0.3 | 0.4 | 0.8 | 0.3 |
| 11 | Hard Rub 30 Hz | AEOLIAN | 1 | 3 | 0 | 30 Hz | RULE | TIGHT | 0.2 | 0 | 0.5 | 0.2 |
| 12 | Slow Throb | DORIAN | 0.5 | 3 | 0 | 4 Hz | RULE | RESOLVE | 0.3 | 0 | 0 | 0.3 |
| 13 | Buzz 60 Hz | PHRYGIAN | 1 | 4 | 0 | 60 Hz | RULE | TIGHT | 0 | 0 | 0.5 | 0.1 |
| 14 | Quarter-Tone Smear | SHOPE | 1 | 5 | 0.6 | 50 c | RULE | TIGHT | 0.6 | 0.2 | 0.3 | 0.6 |
| 15 | Machine Choir | AEOLIAN | 0 | 6 | 0.3 | 34 Hz | RULE | TIGHT | 0 | 0 | 0 | 0 |
| 16 | Techno Cluster | PHRYGIAN | 1 | 8 | 0.5 | 34 Hz | RULE | TIGHT | 0.3 | 0 | 1 | 0.25 |
| 17 | Acid Diaphony | PHRYGIAN | 1 | 2 | 0 | 28 Hz | RULE | TIGHT | 0.8 | 0.2 | 0.7 | 0.3 |
| 18 | House Stab Rub | DORIAN | 0.6 | 5 | 0 | 22 Hz | 0.6 | RESOLVE | 0.2 | 0 | 0 | 0.3 |
| 19 | Dub Drone | AEOLIAN | 0.8 | 4 | 0 | 8 Hz | 0.4 | RESOLVE | 0.5 | 0 | 0.2 | 0.5 |
| 20 | Coin Toss | SHOPE | 1 | 3 | 0 | 170 c | 0.5 | TIGHT | 0.3 | 0.3 | 0.5 | 0.4 |
| 21 | Mostly Sweet | MAJOR | 0.5 | 4 | 0 | 170 c | 0.2 | RESOLVE | 0.3 | 0.1 | 0 | 0.3 |
| 22 | Whoop Only | SHOPE | 1 | 1 | 0 | 170 c | RULE | TIGHT | 0.5 | 0.6 | 1 | 0.3 |
| 23 | Drunk Wedding | HIJAZ | 1 | 6 | 0.4 | 190 c | 0.7 | TIGHT | 0.9 | 0.9 | 1 | 1 |
| 24 | Glass Unison | MAJOR | 0 | 4 | 1 | 2 Hz | RULE | RESOLVE | 0 | 0 | 0 | 0.1 |

A number in the RULE column means CHANCE mode with that CHANCE value. Convert rub values to knob positions with the inverse maps.

**Manual** (MANUAL.md and PDF, same tooling as the template). Sections: what it is (and that it makes no sound); a two-minute background on the singing (drone, seconds, non-tempered tuning, tresene, izvik), honest that the tunings are estimates; every control and jack; the voice table; cents versus Hz with the worked numbers (170 cents is 12 Hz of roughness at A2 and 49 Hz at A4; in HZ it is the same everywhere); the preset list; patch recipes. Recipes, at least these eight, each with exact cable lists using stock VCV modules where possible:

1. First patch: sequencer into V/OCT and GATE; POLY V/OCT into a poly sawtooth VCO; POLY GATE into a poly ADSR and VCA; mix down. Say plainly that bright, vibrato-free, loud oscillators make the rub ring.
2. PUSH to filter cutoff and to level.
3. Marking phrase ends: a trigger on the last step of the sequence into PHRASE; TIGHT versus RESOLVE.
4. Sequencing the drone through the DRONE input.
5. Rub under CV: an LFO or envelope into RUB; switching CENTS and HZ across registers.
6. Lead and drone to separate voices with the cluster on a third.
7. Ornaments from triggers: a random trigger source into SHAKE, an end-of-pattern trigger into WHOOP.
8. Eight-voice cluster over a kick at 126 bpm (the fourth sketch).

**Tests** (`make test`, offline, fixed seed, 48 kHz unless stated). With LOOSE 0, SLIDE 0, SHAKE 0, WHOOP 0, root C4, SHOPE, TUNE 1, VOICES 3, CENTS 170, RULE, and a held input of +3 semitones:

- After 1 s: lead 310 cents, voice 2 at 140, drone 0 (each within 1 cent).
- Lean-in: voice 2 starts within 5 cents of 170 (the scale step, 140 below the lead) and is within 3 cents of 140 by 0.6 s.
- HZ at knob value giving 32 Hz: `fLead - fVoice2` is 32.0 +/- 0.1 Hz for inputs of -9, 0, +7 and +15 semitones.
- Input on the root, not a cadence: drone at -190. As a cadence: drone at 0.
- Input of +2 semitones (lead one step above the root): voice 2 at 170 - 170 = 0 cents; drone PUSH target 7.5.
- Cadence RESOLVE: 0.7 s after PHRASE, voice 2 is within 1 cent of the lead. TIGHT: voice 2 is 102 cents below the lead.
- PHRASE one sample before the gate rise of a new legato note marks the new note, not the old one.
- TUNE 0: inputs of every semitone from -12 to +12 pass to the lead exactly.
- WHOOP 1: after a cadence note's gate falls, lead gate stays high 170 ms and lead pitch ends 1000 cents up; drone gate falls at once.
- VOICES 1..8 give that many channels; DRONE jack values are valid at VOICES 1.
- SPREAD thresholds change voices 8, 6, 4 as specified.
- LOOSE 1: per-voice stagger within 0..35 ms; drift RMS about 9 cents (lead about 4.5).
- Fuzz: every parameter and input random every 50 ms for 60 s, at 44.1, 48 and 96 kHz: no NaN, all pitches within +/- 6 V, PUSH within 0..10.

**Demos.** The module is silent, so demos need a stand-in synth. Add a test tool that runs the C++ engine over a note list and writes per-voice pitch, gate and push; render those with the saw synth from the prototype (`render` and `reverb` in `bulgaria_proto.py`), to MP3 in `/mnt/project-files/bulgaria-vcv/demos/`: the four sketch scenes re-rendered from the C++ engine, plus one short clip per preset on the same tune (use the 126 bpm pattern and a kick for presets 15 to 19). Say in the README and manual that the tone in the demos is not the module's.

## 9. Where the prototype differs from this document

- The prototype's voice 2 doubles the drone at a fixed 6 cents or 2 Hz when the lead is 0 or 1 step from the root. Here voice 2 always rubs against the lead (section 5).
- The prototype has no SPREAD, no OCT, no DRONE input, no legato handling, no trigger inputs, no shake repeat, and only four modes.
- The prototype's TUNE does not preserve chromatic notes at 0 (it always snaps).
- The prototype applies stagger to gates only; here it applies to the voice's whole note start.
- The prototype's cadence clock starts at the note start; here it starts 2 ms after the PHRASE edge.
- The prototype's PUSH is 0..1 and has no drone lift, cadence lift or wander.

## 10. Finish

Build for Apple silicon through CI, confirm it is green, publish to the "preview" release as the other folders do, commit on `claude/project-thread-ww3s0f`, and report to Hardy with the manual PDF, the panel preview and three or four demos to start with (Hardy asked for fewer samples; name the one to hear first). State that nothing has been tested in Rack.
