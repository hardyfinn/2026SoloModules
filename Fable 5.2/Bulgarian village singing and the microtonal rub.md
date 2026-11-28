---
title: "Bulgarian village singing and the microtonal rub"
date: 2026-11-28
source: "Fable 5.2 · Design Bulgarian harmonies module"
topic: Music
tags: [bulgarian-folk, microtonality, drone, vocal-harmony]
summary: |
 In the Shop and Pirin village singing behind Le Mystère des Voix Bulgares, one voice leads, another holds a tonic drone, and the melody lands and stays a second away from it.
 Those seconds are untempered, often 130 to 200 cents, sung loud and straight with no vibrato and a bright nasal tone, which makes the beating ring hard.
---

# Bulgarian village singing and the microtonal rub

## Summary
In the Shop and Pirin village singing behind Le Mystère des Voix Bulgares, one voice leads, another holds a tonic drone, and the melody lands and stays a second away from it.
Those seconds are untempered, often 130 to 200 cents, sung loud and straight with no vibrato and a bright nasal tone, which makes the beating ring hard.

## Question
> New thread, Bulgarian harmonies. I'm interested in creating a VCV rack module that applies some of the theory around Bulgarian folk a cappella singing to synthesizer patterns. These Bulgarian folk traditions utilize very harsh ringing in multiple voices to create an otherworldly sound. Examples here would be the group La Voix Mysteria del Bulgaria. Please ask me questions. I'm not sure quite how we implement this, but I'm keen on getting some of that microtonal rub that they do so well.

## Answer
Before we start, a quick heads-up: this thread is running on Opus 5.5, not Fable. You normally have Fable do the design and DSP modelling, so say the word and I'll switch to Fable before I write the proposal. The questions below don't depend on which model you use, so you can answer them either way.

Here's what I'm working from. In the village singing behind Le Mystère des Voix Bulgares, which comes mostly from the Shop and Pirin regions, one voice leads the melody. A second voice holds a drone on the tonic, often in a chest voice, and the melody keeps landing a major or minor second away from it and stays there. Those seconds aren't tempered. Singers often pitch them somewhere between 130 and 200 cents and keep them very loud and straight, with no vibrato and a bright, nasal tone. That's what makes the beating ring so hard. Then there are ornaments on top: tresene (a throat shake), sliding into notes, and the izvik, a whooped flip upward at the end of a phrase. Koutev's choral arrangements stack this into dense clusters of four to eight voices.

My questions:

1. **What kind of module?** It could harmonise a pattern you feed in (pitch and gate in, extra voices out as poly V/Oct for your own oscillators). It could be a voice with its own sound (a nasal, formant-y choir). Or it could be both.
2. **How many voices?** Village style is two or three parts: drone, melody and one voice rubbing against it. Choir style is clusters of up to eight.
3. **How do you want to control the rub?** One option is a fixed "second width" knob in cents. The other is a beat-rate knob in Hz, where the module keeps the beating steady as the notes change. Or should the intonation drift on its own so the rub breathes?
4. **Who decides when to rub?** Should it follow the tradition's rules (lean on the second at phrase ends and resolve to the unison or drone), or should a probability knob decide?
5. **Ornaments:** do you want tresene, slides and the izvik whoop, and should they fire automatically or from their own triggers?
6. **Asymmetric metres** (7/8, 9/8, 11/16): build a clock or accent generator in, or leave rhythm to your sequencers?
7. **Name and home:** a new folder in 2026SoloModules on its own branch, as usual?

## Sources
- None cited in the answer.

Related: [[Bulgarian dance metres]]
