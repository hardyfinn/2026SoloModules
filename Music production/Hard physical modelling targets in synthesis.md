---
title: Hard physical modelling targets in synthesis
date: 2026-11-23
source: Music production · project chat
topic: Music
tags: [physical-modelling, synthesis, acoustic, ideas]
summary: |
 Ten acoustic sounds that were hard to model realistically until recently.
 Each comes with why it is hard and what has changed.
---

# Hard physical modelling targets in synthesis

## Summary
Ten acoustic sounds that were hard to model realistically until recently.
Each comes with why it is hard and what has changed.

## Question
> I used Fable to build a very good cymbal  synthesis model. This is a pretty high complexity task of modeling due to the nature of the materials involved. What I'm interested in is you telling me other areas of synthesis which modeling has been difficult for until recently. Basically, more high specification, high realistic, natural acoustic modeling. Give me 10 examples of what we could build next on this line.

## Answer
Ten hard physical-modelling targets, in the same family as the cymbal (nonlinear, many coupled modes, hard to get right):

1. **Gongs and tam-tams:** the cymbal's big sibling. Strong nonlinearity makes energy cascade from low modes to a shimmering wash that blooms seconds after the strike.
2. **Bowed strings (violin, cello):** stick-slip friction is notoriously unstable. The aim is a playable bow with pressure, speed and position, plus the body resonance.
3. **Brass (trumpet, trombone):** the lips are a nonlinear valve and the bore carries shock waves at high volume, which is what gives the "brassy" blare when you push it.
4. **Reed instruments (clarinet, sax, oboe):** reed-bore coupling, register breaks, multiphonics and growl.
5. **Piano:** hammer, stiff strings with inharmonicity, sympathetic strings and soundboard, two-stage decay and longitudinal string "phantom" partials.
6. **Plucked strings with a real body (acoustic guitar, oud, sitar):** string-to-body coupling, wolf tones and sitar's buzzing bridge.
7. **Human voice (a physical vocal tract):** glottal flow plus a shaped tube, with breathiness, vocal fry, throat singing and overtone singing.
8. **Flute and air-jet instruments (flute, shakuhachi, ocarina):** the edge-tone jet is chaotic, and breath noise is half the character.
9. **Drum membranes with real shell and snare coupling:** nonlinear tension modulation (the pitch drop after a hit), a snare rattle and rim modes.
10. **Rooms and bodies as instruments (spring, plate and tube-bell resonators):** wave dispersion gives the chirpy "boing" of springs and plates. It's a physical model of the resonator itself.

My top three for you are **gong/tam-tam** (an easy extension of what you've done and brilliant for ambient and techno textures), **bowed string** (a very expressive sound that suits slow-evolving patches) and **brass**.

[…]

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Modelling a steel pan and a hang drum]], [[Why a synthesised piano does not sound like a piano]]
