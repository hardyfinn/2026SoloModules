# Sawtooth stack voice: research and proposal

Written 2026-11-23. Design stage only. Nothing is built yet.

## The short version

The Verbos Electronics Sawtooth Stack is a complete analog voice built around five oscillators that share one pitch control. One knob, SPREAD, fans them apart: two go down, two go up, one stays put. At the bottom of the knob they sit in unison and beat like a supersaw. At the top they are spread over several octaves like an organ registration. Two more knobs shape all five at once: TIMBRE fades each oscillator from sine to saw, and RICHNESS multiplies the saw so it sounds like a hard-synced oscillator being swept. The five are placed across the stereo field, pass through a stereo VCA, and two simple rise/fall envelopes are built in.

It is a small design with few controls, and that is the appeal. I propose one VCV module that keeps the hardware's panel almost exactly, with three small additions that Rack makes cheap.

Nine sound sketches from a first prototype are in `sketches/`. Start with `00-gallery.wav`.

## What the hardware is

Sources: the Verbos tech card, the Verbos product page, and four retailer pages. There is no full manual, and nobody publishes the numbers that matter most for a model, so some of what follows is my reading. I mark those parts.

| Part | What the sources say |
|---|---|
| Oscillators | Five discrete triangle-core VCOs, each with its own waveshaper taken from the Verbos Foundation Oscillator. |
| FREQUENCY | One knob and one V/OCT input for all five. A second frequency CV input with an attenuverter. |
| SPREAD | "With the Spread all the way down, the 5 oscillators are in unison." Turning up, "2 oscillators tune down and 2 oscillators tune up, stretching their pitches across several octaves." CV input with attenuverter. |
| TIMBRE | Crossfades from sine to saw. CV input with attenuverter. |
| RICHNESS | "Multiplies the saw wave, mimicking the effect of a hardsynced VCO when additional voltage is applied." CV input with attenuverter. |
| Stereo | The five are "stretched from left to right through a stereo VCA". With one output jack patched, all five come out of it. |
| AMPLITUDE | VCA level, CV input with attenuverter. A separate OUTPUT level knob, 10 V peak to peak at full. |
| Envelopes | Two rise/fall envelopes, 2 ms to 10 s per stage, linear 0 to 10 V. Each has a gate input, an envelope output and an end pulse output "for looping and chaining". Fall time has a CV input with attenuverter. |
| Normalling | Envelope 1 feeds RICHNESS, MIDI velocity feeds TIMBRE, envelope 2 feeds AMPLITUDE, each through that input's attenuverter, until you patch something else in. |
| MIDI | Notes, velocity and a CC for each main control. An autotune button. |
| Size | 42 HP. |

Not in the hardware: no filter, no sub oscillator, no FM or sync input, no separate outputs per oscillator. The tone shaping is all in TIMBRE, RICHNESS and SPREAD.

## What is not published, and what I assumed

1. **How far SPREAD goes and in what shape.** "Several octaves" is all there is. I assumed the inner pair moves one octave each way and the outer pair two, so the top of the knob is a five-octave stack of octaves, and I gave the knob a curve so the bottom third is fine detune (a few cents up to about a semitone). This is the biggest guess in the design. If you have the hardware or a demo you trust, tell me what the top of the knob sounds like and I will fit it.
2. **What RICHNESS does exactly.** I read "multiplies the saw wave" literally: the saw is amplified up to eight times and wrapped back into range, so one cycle holds up to eight teeth. That is mathematically the same as a hard-synced oscillator, which matches the tech card's description.
3. **Where each oscillator sits in the stereo field.** I interleaved them (centre oscillator in the middle, the low and high pairs alternating sides) so the bass is not all on one side at wide spread.
4. **Envelope behaviour with a held gate.** I assumed the rise always completes, the level holds while the gate stays high, then it falls. A short trigger gives a plain rise/fall.
5. **Analog looseness.** Five analog oscillators never sit in perfect unison. I gave each one a slow wander of about 2.5 cents and a small fixed tuning error, so unison slowly phases instead of sounding like one loud saw.

## Proposed module

One module, about 22 HP, text labels.

**Knobs:** FREQUENCY, SPREAD, TIMBRE, RICHNESS, AMPLITUDE, OUTPUT, RISE 1, FALL 1, RISE 2, FALL 2. Small attenuverters for FREQ, SPREAD, TIMBRE, RICHNESS, AMPLITUDE, FALL 1, FALL 2.

**Inputs:** V/OCT, FREQ CV, SPREAD, TIMBRE, RICHNESS, AMPLITUDE, GATE 1, GATE 2, FALL 1, FALL 2, and VEL, which stands in for MIDI velocity and is normalled to TIMBRE.

**Outputs:** L, R, ENV 1, ENV 2, END 1, END 2.

**Left out:** the MIDI socket, CC control and autotune. Rack's own MIDI to CV module does that job, and a software oscillator does not need tuning.

**Three additions I recommend:**

- **Polyphony.** A poly cable on V/OCT and the gates gives each note its own five-oscillator stack and its own envelopes. The hardware plays one note. This turns it into a pad machine at no cost to the panel.
- **DRIFT** in the right-click menu: none, the default analog amount, or loose.
- **STACK voicing** in the right-click menu: what the top of SPREAD reaches. Octaves (my guess at the hardware, the default), fifths and octaves, or the harmonic series (the five oscillators land on harmonics 1 to 5, which with TIMBRE at sine makes it a small additive voice).

## The sketches

All from the Python prototype in this folder. They are sketch quality: the final module will use proper band-limited oscillators.

| File | What it shows |
|---|---|
| 01-spread-sweep-saw | One held note, full saw, SPREAD from zero to full. Unison, then swarm, then the octave stack. |
| 02-spread-sweep-sine | The same sweep with TIMBRE at sine. Five sines fan out into an organ chord. |
| 03-timbre-sweep | Sine to saw at a light swarm setting. |
| 04-richness-sweep | RICHNESS up and down, first in unison, then lightly spread. |
| 05-swarm-bass-env-to-richness | A bass line using the normalled patch: envelope 2 on amplitude, envelope 1 on RICHNESS. |
| 06-chorale-lead | Slow notes, light swarm, velocity opening TIMBRE. |
| 07-looped-envelope-to-spread | A looping envelope sweeping SPREAD so the stack opens and closes. |
| 08-octave-stack-plucks | Short plucks at wide SPREAD, envelope 1 opening TIMBRE. |
| 09-drone-all-moving | A drone with SPREAD, TIMBRE and RICHNESS all under slow modulation. |

## Questions

1. **Faithful only, or faithful plus the three additions?** I recommend the additions. They live in the right-click menu and in poly cables, so the panel still reads like the hardware.
2. **Name.** The panel should not carry the Verbos name. I suggest **Sawstack**, in a plugin called Sawstack Fable.
3. **Where it lives.** I suggest the same arrangement as Cicada and Birdie: a folder `SawstackFable` in 2026SoloModules on its own branch, with a preview release from CI, Apple silicon only.
4. **SPREAD range.** Keep my guess (two octaves each way at full) unless you know better.

Say "go" to accept all four recommendations and I will write the build spec.

## Addendum, same day: analog character

Hardy asked whether a drift control would help, whether saturation would help, and what else would make the voice feel alive. Answer: drift alone covers only the slow pitch wander. The hardware's looseness is many small errors at once. The prototype in `sawstack_proto2.py` models them, all scaled by one **AGE** amount, with saturation on its own **DRIVE** knob.

What AGE does, in order of how much it matters:

1. **Component mismatch.** Each oscillator gets a fixed tuning error (up to 5 cents), a V/OCT scale error (about 5 cents per octave at full), a SPREAD scale error, a level mismatch and a slightly different waveshaper. So the detune changes from note to note and the octave stack is never quite octaves.
2. **Slow drift.** Two random wanders per oscillator (2.5 s and 25 s time constants) plus a shared one (20 s) standing in for supply and temperature. Up to about 5 cents each.
3. **Pitch jitter.** Fast noise smoothed at 400 Hz, which roughens the saw edges so beating does not sound like a clean chorus.
4. **Imperfect shapes.** The sine is a bent triangle through an over-driven shaper that folds its tip slightly and carries a DC offset. The saw reset is rounded by a one-pole at 9 to 14 kHz, and carries a small glitch at the triangle's turnaround.
5. **Saturation in two places.** An asymmetric stage per oscillator, then the output stage driven by the sum of five, so unison peaks compress together and the beating changes tone as well as level. DRIVE pushes both.
6. **Coupling.** Oscillators pull toward each other when nearly in tune (a weak Kuramoto term), giving the "locking and letting go" feel near unison.
7. **Loose envelopes.** Segments bent toward capacitor curves, times scattered a few percent per note, and a trace of VCA bleed.
8. **Noise floor.**

Renders are in `analog/`. Each of six pieces at three settings:

| Setting | AGE | DRIVE |
|---|---|---|
| 1-as-before | 0.25 | 0 |
| 2-aged | 0.6 | 0.35 |
| 3-tired-and-driven | 1.0 | 0.75 |

`00-side-by-side.wav` plays each piece at all three settings in a row. `00-gallery-2-aged.wav` and its siblings play one setting throughout.

This replaces the right-click DRIFT option with two panel knobs, AGE and DRIVE. Both default to the hardware-ish middle rather than to zero.

Hardy also asked for AGE across the other synthesis modules. Plan: build it here first, then factor the pieces into a shared `age.hpp` once it is clear which earn their place, and add a one-knob AGE to the other modules a module at a time.
