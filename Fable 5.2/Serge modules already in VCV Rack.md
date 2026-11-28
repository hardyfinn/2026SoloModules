---
title: "Serge modules already in VCV Rack"
date: 2026-11-14
source: "Fable 5.2 · Build Serge modules for VCV Rack"
topic: Music
tags: [serge, vcv-rack, modular-synthesis, la-bestia]
summary: |
 VCV Rack had no real Serge set: only Befaco Rampage (a DUSG cousin), a NYSTHI Serge-style Programmer and a Serge folder mode on one TriggerFish oscillator.
 La Bestia II is fully covered by the proposed set, La Bestia III is not (its GTO, GTS, Dual Sine and Fuzz are 2020s redesigns), and Sing To Me is covered except its two Buchla 258 oscillators.
---

# Serge modules already in VCV Rack

## Summary
VCV Rack had no real Serge set: only Befaco Rampage (a DUSG cousin), a NYSTHI Serge-style Programmer and a Serge folder mode on one TriggerFish oscillator.
La Bestia II is fully covered by the proposed set, La Bestia III is not (its GTO, GTS, Dual Sine and Fuzz are 2020s redesigns), and Sing To Me is covered except its two Buchla 258 oscillators.

## Question
> New fable thread - Serge for VCV rack

## Answer
I need four steers from you before I go further, and I've put a first proposal in [01-research-and-proposal.md](/mnt/project-files/serge-vcv/01-research-and-proposal.md).

**What I found.** VCV Rack has no real Serge set. Befaco Rampage is a DUSG cousin, NYSTHI has a Serge-style Programmer, and one TriggerFish oscillator has a Serge folder mode. I found nothing for the Smooth & Stepped Generator, the full Wave Multipliers, the Resonant EQ, the Variable Slope filter, the NTO or the TKB. That comes from the VCV Library search and a community thread, so it is a good survey but not an exhaustive one.

**Proposed set, 12 modules.**
- Core: DUSG, Smooth & Stepped Generator, NTO oscillator, Wave Multipliers, Variable Q filter, Resonant EQ.
- Voice: Triple Waveshaper with Ring Mod, Wilson Analog Delay, Variable Slope filter.
- Control: TKB touch keyboard sequencer, Random with Analog Shift Register, a utility panel (comparator, processor, cross-fader).

**Your call on these.** My default is in brackets.
1. **Which modules and how many?** (The 12 above. Frequency Shifter, Phaser, Extended ADSR, Quantizer and logic are on the bench.)
2. **Faithful or inspired-by?** (Faithful circuit behaviour and paperface-style panels with blue/black/red jacks, plus modern extras such as polyphony and oversampling. No invented modules unless you want some.)
3. **Build it here, or write build instructions as before?** (Build it here.)
4. **Which repo?** None exists for this. If you create an empty one and add it under [Project settings](#project-settings/resources), I'll use that.

One thing to know: "Serge" is a live trademark, so if this ever goes to the public VCV Library it should carry its own names.

## Question 2
> Good review. Please create a design for Opus to build. I am not sharing this on VCV library, so Serge is a good name. Please build as faithful as possible to the original circuits, no modern extras. Paperface style panels. Let's do the core modules, plus voice except for  wilson delay, plus control without TKB. Does this cover the modules in the 'la Bestia' and 'sing to me' panels?

## Answer 2
On your earlier question about the two panels:
- **La Bestia II** is fully covered.
- **La Bestia III** is not. Its GTO, GTS, Dual Sine and Fuzz blocks are 2020s redesigns, not original circuits.
- **Sing To Me** is covered except its two Buchla 258 oscillators, which aren't Serge. The NTO stands in.

Two choices I made that you may want to reverse. I added four small modules (Dual VCA, Dual Audio Mixer, Dual Channel Stereo Mixer, Peak & Trough) to reach that coverage. And all jacks use Serge's own levels (5 V peak-to-peak audio, 0 to 5 V control), which is faithful but half the size of normal Rack signals.

## Question 3
> Comment on `Serge-Manual.pdf` (pinned on page 5 at 27% from the left, 59% from the top):
>
> what is VCM?

## Answer 3
VCM is the Wave Multipliers module, Serge's three-section waveshaper. Section 2 is the wavefolder used in that patch. Its full section is earlier in the manual, under "VCM: Wave Multipliers". VCM is my short name for it, like DSG and NTO. I can add a table of the short names to the top of the manual if that would help.

## Sources
- Project file: `/mnt/project-files/serge-vcv/01-research-and-proposal.md`

Related: [[Next Serge modules to add]], [[VCV Rack tags and module search]]
