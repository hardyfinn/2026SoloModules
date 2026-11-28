---
title: "TB-303 component values that shape the sound"
date: 2026-11-15
source: "Fable 5.2 · Design Roland TB-303 for VCV Rack"
topic: Music
tags: [tb-303, analog-circuits, filters, synth-design]
summary: |
 PWM falls naturally out of the 303 circuit, because it makes its square by comparing the saw to a fixed level, and a band-pass mode suits its ladder filter.
 The most influential fixed values beyond the basics are the shared accent pot, the accent sweep ganged to resonance, the 3 ms filter attack, the 4 ms release, the squaring transistor and the untrimmed VCA click.
---

# TB-303 component values that shape the sound

## Summary
PWM falls naturally out of the 303 circuit, because it makes its square by comparing the saw to a fixed level, and a band-pass mode suits its ladder filter.
The most influential fixed values beyond the basics are the shared accent pot, the accent sweep ganged to resonance, the 3 ms filter attack, the 4 ms release, the squaring transistor and the untrimmed VCA click.

## Question
> switch to fable for this design discussion - I like the core modules. Let's look at enhancements which we can add to a separate module in the plug in. For the sound generation, i would like to add PWM to the oscillator and wave folding before the filter. The 303 circuit board also features additional potentiometers which should be exposed as front panel knobs (gate trim, resonance trim etc). Do you have suggestions for what else is suitable for adapting? Maybe a bandpass version of the filter could be switched in? Keep the original modules stable and let's make new oscillator and filter modules. Adding scala microtuning support to the sequencer would be great.

## Answer
Two things for you to look at: four cleaner panel mock-ups, and a proposal for the enhancements in [03-enhancements-proposal.md](/mnt/project-files/tb303-vcv/03-enhancements-proposal.md).

**Panels.** The dirty look came from the brushed-metal texture, so all four are flat with no texture. A is clean silver, B is white with a thin orange rule, C is matte black, D is warm grey. A card below asks which you want.

**Enhancements.** I'd split the voice into two new modules and leave SBV, SBS and SBX untouched:

- **SBO Oscillator**: pulse width with PWM, and a wavefolder with a symmetry control. PWM falls out of the original circuit naturally, because the 303 makes its square by comparing the saw to a fixed level. I'd also add a saw/square mix, a sub-octave, sync, FM, built-in slide and a drift knob.
- **SBF Filter**: the ladder with an LP / BP / HP switch. Band-pass suits this filter well. I recommend it carries the envelopes, accent circuit and VCA too, because a bare filter would lose the accent sweep. Trimmers become knobs: cutoff trim, resonance trim (which lets it self-oscillate) and more.
- **Scala**: a menu item on SBS that loads .scl files. Keys become scale degrees. With no scale loaded, nothing changes.

Two things I need from you:
- **Gate trim**: I read it as trimming note length against the incoming gate. Is that what you meant?
- **Trimmer list**: I'm sure of the tuning, width and filter trimmers but not the full set of six, so I'll check the service notes when writing the spec.

The other choices are at the end of the doc, each with a recommendation.

## Question 2
> 1. The scala tuning should also live in the original sequencer as well as in SBT. 2. What other fixed component values would be the most variably influential on the sound? 3. Correct SBV corrected to match 500hz halfway.

## Answer 2
All three are in the round 2 spec now, which is updated on main: [04-build-instructions-round-2.md](/mnt/project-files/tb303-vcv/04-build-instructions-round-2.md).

**1. Scala in the sequencer.** SBS gets a Tuning menu with the same loader and built-in scales as SBT. Patches with no scale loaded play exactly as before.

**2. Most influential fixed values.** Beyond the ones already in (BASS, POLES, ENV BIAS, SAW CURVE), these are the ones I'd rank highest, and I've added them as knobs:
- **Accent split**: on the board one pot sets both how much an accent opens the filter and how much louder it gets. A separate ACCENT VCA knob lets you have screaming but quiet accents, or loud but dark ones.
- **Sweep ungang**: the accent shape is tied to Resonance by a dual pot. A switch frees it, so you can have stacking, swelling accents at low resonance.
- **ENV ATTACK**: the filter envelope's fixed 3 ms attack, up to 300 ms, for swelling sweeps.
- **RELEASE**: the fixed 4 ms note-off, up to 1 s, so notes ring and overlap.
- **SQUARE SHAPE** (on SBO): how hard the transistor squares the saw, from rounded to hard-edged.
- **CLICK**: the note-on thump of an untrimmed VCA, for a percussive front.

Strike any you don't want.

**3. SBV cutoff.** The spec corrects it to 500 Hz at half way (range 125 Hz to 2 kHz). Existing patches will sound darker until you turn the cutoff up by about a third.

Say "switch to Opus and build" when you're ready.

## Sources
- Project file: `/mnt/project-files/tb303-vcv/03-enhancements-proposal.md`
- Project file: `/mnt/project-files/tb303-vcv/04-build-instructions-round-2.md`

Related: [[Fitting a TB-303 model from recordings]]
