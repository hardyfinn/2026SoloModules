# WMD Fracture in VCV Rack: feasibility

Researched 2026-11-21. Fracture is WMD's 8 hp "Multi-Particle Percussion Synthesizer" (shown at NAMM in January 2018, shipping by April 2018, $269, now listed as discontinued on ModularGrid). It covers the range from a single drum-machine clap to a full audience applauding. This note says what is documented, what would have to be guessed, and what a similar module of our own would look like. Nothing is designed in detail or built yet.

## Verdict

**Yes, there is enough to build a module that behaves similarly.** WMD's own manual page describes the engine in plain words, lists every control and every switch position, and gives the electrical specs. The engine is digital and simple in structure, so a behavioural model is the same kind of thing as the original, not an approximation of analog circuitry.

**The one thing that cannot be had is WMD's sound set.** The character of the hardware lives in 23 groups of proprietary micro-samples recorded by WMD. Those are not published and should not be copied. Our module would need its own particle sources, which means it will share Fracture's behaviour and range but not its exact timbre. I think that is a fair trade, and for realism it is an opportunity (see "Particle sources" below).

## What is documented

Source for almost all of this is WMD's manual page, docs.wmdevices.com/fracture, backed up by the launch press text.

**Engine, in WMD's words:** a pseudorandom "particle oscillator" triggers the most available voice of a 22-voice sample player. The oscillator's rate, and the probability that each tick actually fires a sample, are modulated over time to make a burst of micro-samples. Each voice's sample choice, pitch, amplitude and pan come from pseudorandom generators seeded by the CV inputs. WMD describe it as classic analog clap circuits (repeating noise impulses) restated as granular synthesis. Grains are in the 1 to 50 ms region.

**Controls (six knobs, each with a CV input that sums with the knob, full sweep 5 V):**

| Control | What it does |
|---|---|
| SURFACE | Picks one of 23 particle types: claps, snaps, drum sticks, ping-pong balls, toggle switches and other transient micro-samples. Each surface is a group of similar samples played in pseudorandom order. |
| DENSITY | Rate of the particle oscillator, so how many particles are attempted. Read as "how many people". |
| DECAY | Length of the burst after a trigger. |
| FREQ | Spectrum and/or pitch of the burst, depending on PUNCH. |
| SPREAD | Pitch and pan variance of particles around centre. High settings push toward noise. |
| REVERB | Built-in reverb, knob sweeps from Room at one end to Hall at the other. Can be modulated after the trigger for late tails. |

**TAIL switch (where the decay envelope goes):**
1. Amplitude only. Particles arrive at a constant rate and each is quieter than the last.
2. Amplitude and probability. Particles get both quieter and rarer. Tight, punchy bursts with a gentle tail.
3. Probability only. Particles have semi-random loudness and just thin out.

**PUNCH switch (filter and pitch):**
1. Fixed-resonance bandpass after the particle engine, FREQ sets its centre.
2. Same, and FREQ also moves the centre pitch of the particles.
3. As 2, but the bandpass is mixed in parallel with the dry signal, for more punch.

**Jacks:** TRIG starts a burst. TICK plays one single particle on its rising edge. If TICK is high at the moment TRIG arrives, the burst is accented (louder and longer). PITCH takes over centre pitch from FREQ. INF (infinite) switches off the decay envelope so particles run freely, DECAY then sets directly whatever TAIL would have enveloped, nothing sounds at minimum DECAY, and a trigger or the panel button bumps the density to give a burst inside a continuous cloud. OUT L and OUT R are true stereo, L alone for mono.

**Specs:** 8 hp, gate threshold 2 V, output up to 20 V peak to peak, latency 1 ms maximum.

## What is not documented and would be guessed

| Unknown | How I would settle it |
|---|---|
| The 23 sample sets themselves | Our own sources, see below. This is the big one. |
| Density range in particles per second | Classic clap circuits repeat at roughly 80 to 120 Hz, and applause from a crowd is far sparser per person. I would span about 2 to 400 particles per second and tune by ear against demo videos. |
| Decay time range | Estimate about 20 ms to several seconds from demos. |
| Envelope shape | Exponential, as in the analog circuits it is modelled on. |
| How random the particle timing is | "Pseudorandom oscillator" is all WMD say. I would use a jittered clock whose jitter grows as density falls, which is how real groups of clappers behave. |
| Spread amounts (pitch and pan width) | Guess around plus or minus an octave at maximum, full-width pan. |
| Bandpass Q and slope, FREQ range | A 12 dB state-variable bandpass with moderate Q, roughly 200 Hz to 8 kHz. |
| PITCH scaling | Not stated. I would make it 1 V/oct, which is what a Rack user expects. |
| Reverb algorithms | Unknown. Our own small room and hall, with the knob crossing from one to the other as on the hardware. |
| Accent amounts, LED behaviour | Small details, chosen by ear. |

I found no teardown, firmware dump, schematic or designer interview beyond the launch demo (WMD's Alex Anderson at NAMM 2018), and no existing VCV Rack version. I have read about the demos but have not listened to them, so the by-ear tuning above is still to do and would happen at the modelling stage with you as the judge.

## Particle sources: three options

1. **Modelled particles (my recommendation).** Each particle is synthesised: a very short noise burst through a small set of resonances that match the object. Hand claps are well studied. Published research on clap synthesis (Peltola, Erkut, Cook and Välimäki, 2007, building on Repp's 1987 measurements) gives resonance frequencies for eight hand shapes from flat palms to cupped hands, and shows that a crowd is convincing when each clapper has their own slightly different hand shape, tempo and timing drift. That gives us real variation per particle instead of a handful of samples repeating, which is where the realism comes from. Snaps, sticks, ping-pong balls, switches, coins and so on can be done the same way with different resonances.
2. **Our own recorded samples.** Record or source public-domain single impulses and ship them. Closest to how the hardware works, but quality depends on finding good clean recordings.
3. **User samples.** Let the module load a folder of short WAVs as an extra surface. Cheap to add on top of either of the above, and something the hardware cannot do.

I would do 1 for the built-in surfaces and add 3.

## What a similar module would look like

One module, our own name and panel, text labels. Working name **Clapfall** (short, searchable, yours to change). About 12 hp so the labels breathe.

- Six knobs with CV: SURFACE, DENSITY, DECAY, FREQ, SPREAD, REVERB.
- Two three-way switches doing what TAIL and PUNCH do, labelled in words (for example ENV: AMP / BOTH / CHANCE, and FILTER: BAND / BAND+PITCH / PARALLEL).
- TRIG, SINGLE (one particle), ACCENT, PITCH (1 V/oct), FREE (run continuously), stereo out.
- 22 voices as on the hardware, or more, since CPU is no constraint here.
- Around 20 surfaces: several hand claps from flat to cupped, a group of mixed clappers, finger snaps, sticks, rimshot clicks, ping-pong, switches, coins, plus a few useful for your music such as a tight 808-style and 909-style noise particle so the classic drum-machine claps fall out of the same engine, and a gospel or soul group clap.
- Additions worth considering later: a CROWD control separate from DENSITY (number of people versus how fast each claps), a SYNC amount that pulls a loose crowd into time over a few bars as real audiences do, and the user sample folder.

Faithfulness would be "behavioural from the manual", the same level as the Eightfold and Density plugins, with its own voice.

## Naming and artwork

WMD still exists and the name Fracture, the panel art and the samples are theirs. The module should carry its own name and panel, describe itself as inspired by multi-particle clap synthesis in general, and the manual should be written fresh, not paraphrased line by line from WMD's.

## Sources

- WMD manual page: https://docs.wmdevices.com/fracture
- WMD product page: https://wmdevices.com/products/fracture
- Launch press text: https://ask.audio/articles/wmd-unveils-singularsounding-fracture-multiparticle-percussion-synth-module and https://sonicstate.com/news/2018/04/23/fracture-multi-particle-percussion-synth-/
- NAMM 2018 coverage: https://www.gearnews.com/namm-2018-wmd-announce-fracture-multi-particle-percussion-synthesizer and https://www.synthtopia.com/content/2018/02/05/wmd-fracture-multi-particle-crowd-percussion-synthesizer/
- ModularGrid entry (specs, discontinued): https://www.modulargrid.net/e/wmd-fracture
- Perfect Circuit listing: https://www.perfectcircuit.com/wmd-fracture.html
- Clap research, cited from memory and to be re-read at the design stage: L. Peltola, C. Erkut, P. Cook, V. Välimäki, "Synthesis of Hand Clapping Sounds", IEEE Trans. Audio, Speech and Language Processing, 2007; B. Repp, "The sound of two hands clapping", JASA, 1987.
