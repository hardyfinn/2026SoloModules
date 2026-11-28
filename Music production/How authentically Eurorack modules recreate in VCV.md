---
title: How authentically Eurorack modules recreate in VCV
date: 2026-11-20
source: Music production · Eurorack to VCV authenticity check
topic: Music
tags: [vcv-rack, eurorack, emulation, modules]
summary: |
 Most of the 22 modules could be recreated convincingly; analog modulation and envelope modules come out best.
 Closed digital designs (Spectraphon, Cyclonix Shapeshifter, Mungo d0) are hardest; ratings are from documents, not tests.
---

# How authentically Eurorack modules recreate in VCV

## Summary
Most of the 22 modules could be recreated convincingly; analog modulation and envelope modules come out best.
Closed digital designs (Spectraphon, Cyclonix Shapeshifter, Mungo d0) are hardest; ratings are from documents, not tests.

## Question
> New thread. Checking how authentically we could recreate certain Eurorack modules.
>
> It's quite a long list. Mobenthey swoop. Industrial music electronics Hertz donut. Industrial music electronics piston Honda. OTO machines boum. Intelligel flurry. Make noise spectrophon. Topo Brillo sports modulator 2. Mungo Enterprises D0. Making sound machines Stolper beats. WMD Javelin. TakTakTak Drumbo. Doepfer A111-6. Frap Tools Falistri. Future Sound Systems Cric. Buchla Easel Command 208C. Intelligel Cyclonics Shapeshifter. Whimsical raps Mannequins Silhouette. ADDAC 506. Toadstool Tech Ectocore. ALM Jumble Henge. Buchla Source of Uncertainty. Crucfx eurolz.

## Answer
All 22 modules are assessed in Eurorack to VCV: authenticity check. Each one is rated out of 10, from manuals, store pages and reviews rather than tests on real units.

- **Easiest:** Ectocore scores 9 because its firmware is open source. Stolperbeats, Swoop, Sport Modulator 2, Falistri and Javelin all score 8.
- **Hardest:** Spectraphon, Cyclonix Shapeshifter and Mungo d0 score 4 to 5, because they're closed digital designs with little documented.
- **Not Eurorack:** the Boum and the Cric are desktop units, and the Easel Command and 208C are Buchla's 200 format. All of them could still be VCV modules.

[…]

### Full write-up

#### The short version

Most of this list can be recreated convincingly. The analog modulation and envelope modules come out best, because their behaviour is simple to measure and model. **Ectocore is the standout**, because its firmware is open source and could be ported almost directly. The hardest to copy are **Spectraphon, Cyclonix Shapeshifter and Mungo d0**. All three are digital with closed firmware and run large, undocumented algorithms, so for those we could capture the spirit but not the exact sound.

Ratings are out of 10 and mean how close a VCV version could get to sounding and behaving like the hardware. Ratings come from manuals, store pages and reviews; nothing was measured from real units.

Three items on the list aren't Eurorack. The OTO Boum and the Future Sound Systems Cric are desktop units, and the Easel Command and 208C are in Buchla's 200 format. All three could still be built as VCV modules.

#### Overview

| Module | What it is | Analog or digital | Sources we can use | Rating |
| --- | --- | --- | --- | --- |
| Toadstool Tech x Infinite Digits **Ectocore** | Sample and break mangler | Digital (RP2040) | Open-source firmware (schollz/\_core on GitHub) | **9** |
| Making Sound Machines **Stolperbeats** | 6-track drum trigger sequencer with shuffle styles | Digital | Manual only | **8** |
| Ieaskul F. Mobenthey **Swoop** | Bound/bounce CV processor and triangle oscillator | Analog | Manual, plus Peter Blasser's published circuit writings | **8** |
| Toppobrillo **Sport Modulator 2** | Dual slew and function generator | Analog | Manual | **8** |
| Frap Tools **Falistri** | Dual function generator | Analog | Very detailed manual | **8** |
| WMD **Javelin** | ADSR with accent stacking and VCA | Analog core, digital control | Manual | **8** |
| crucFX **EuRolz** | Three interacting Rolz CV oscillators (from Blasser's LabRolz) | Analog | DIY kit exists; Blasser circuits documented | **7–8** |
| ADDAC **506** | Stochastic function generator | Analog | Manual | **7–8** |
| Buchla **Source of Uncertainty (266)** | Noise, fluctuating, quantised and stored random voltages | Analog | Original 266 schematics circulate; manual | **7–8** |
| Doepfer **A-111-6** | Complete mini synth voice (VCO, VCF, VCA, envelope, LFO) | Analog | Detailed Doepfer manual | **7** |
| Buchla **Easel Command + 208C** | Music Easel voice (complex osc, LPGs, envelope, pulser, sequencer) | Analog | 208 schematics circulate widely; Arturia's Music Easel V as a reference | **6–7** |
| taktaktak **Drumbo** | Syncussion-style drum synth, 10 modes | Analog sound, digital control | Store pages; the original Syncussion is well documented | **6–7** |
| IME **Piston Honda mkIII** | 3D wavetable oscillator | Digital | Manual; factory wavetables not public | **6–7** |
| Intellijel **Flurry** | 16 noise types, random clocks, S&H, T&H, slew | Digital noise, analog S&H and slew | Manual | **6–7** |
| IME **Hertz Donut mkIII** | Digital FM complex oscillator | Digital | Manual | **6** |
| ALM / Worng **Jumble Henge** | 16-input stereo spectral mixer | Analog | Manual | **6** |
| OTO Machines **Boum** | Stereo warming unit: distortion, compressor, noise gate (desktop) | Analog | Manual, reviews | **5–6** |
| Mannequins **Silhouette** | Braided mixer with BBD delay, dynamics and blur (2025) | Analog with BBD | Manual (was still in progress at launch) | **5–6** |
| Future Sound Systems **Cric** | Pin-matrix analog synth (desktop, 3 osc, 2 DNA osc, 4 function gens, SVF) | Analog | Manual, reviews | **5** |
| Mungo Enterprises **d0** | Delay | Digital | Very sparse docs | **4–5** |
| Make Noise **Spectraphon** | Dual spectral oscillator and resynthesiser | Digital | Manual only | **4–5** (spirit 7) |
| Intellijel **Cyclonix Shapeshifter** | Dual complex wavetable oscillator (discontinued) | Digital | Long manual | **4–5** |

#### Easiest wins (7–9)

**Ectocore (9).** The firmware is open source, so the slicing, retrigger and mangling logic can be ported rather than guessed. The licence needs checking first: if it's GPL, our plugin source must stay GPL too, which is fine for personal use. The VCV version would load breaks from a folder instead of a flash chip.

**Stolperbeats (8).** It's a sequencer, so we're copying timing, not sound. The Trip, Shake, Push and Clave shuffle styles and the subdivision outputs are described in the manual. The exact groove maths isn't public, so a reference recording of each style at a fixed tempo would let us match it closely. It also fits your interest in off-grid grooves very well.

**Swoop, EuRolz, Sport Modulator 2, Falistri, ADDAC 506 (7–8).** These are analog function generators and chaotic modulators. Their circuits are classic integrator and comparator designs that model well, and Peter Blasser (Swoop, and the LabRolz behind EuRolz) has written openly about his circuits. Chaos is the one catch: a VCV EuRolz would wander in the same way as the hardware, but never with the same exact trajectory. For modulation that doesn't matter.

**Javelin (8).** It's an envelope with a VCA. Curve shapes, time ranges and the accent stacking are in the manual, and the VCA character is simple to approximate.

**Source of Uncertainty (7–8).** The 266 circuits are well known. Because it's random by nature, the result only has to match the statistics of the original, and that's easy to check.

**A-111-6 (7).** It's a straightforward analog voice. We'd get the topology right, but not the exact sound of one particular unit's filter and oscillator drift.

#### Doable with a bit of guesswork (5–7)

**Easel Command + 208C.** The 208 is one of the best-documented Buchla instruments. It builds on our Buchla 259t. The hard part is the vactrol low-pass gates, whose slow, uneven decay is what makes the Easel sound like itself.

**Drumbo.** The Syncussion design is well known, but taktaktak's 10 modes are its own, so we'd base the modes on demo videos and the manual.

**Piston Honda and Hertz Donut.** Both are closed digital oscillators. The Piston Honda's character lives in its wavetables: if you can export the tables from your own unit, they'd go in for personal use only, and we wouldn't share them. The Hertz Donut's FM routings are documented, but its aliasing and quirks would need reference recordings.

**Flurry.** The S&H, T&H and slew are easy. The 16 noise types are the unknowns; recordings of each would sort that.

**Jumble Henge.** It's a filter-based spectral mixer. It's doable once we know the band layout from the manual.

**Boum and Silhouette.** Both depend on analog nonlinearity: Boum's saturation and compression, and Silhouette's BBD delay and dynamics. Good models exist for both, but these are the parts people notice when they're slightly off.

**Cric.** All the parts are analog building blocks we've modelled before. Our DriftMatrix shows the matrix idea works in VCV. It's simply big: about one plugin with five or six modules.

#### Hardest (4–5)

**Spectraphon, Shapeshifter, d0.** These are digital, closed firmware with lots of hidden algorithm detail. Spectraphon's resynthesis and Shapeshifter's huge mode list could be rebuilt from the manuals, but the result would be our own take, not a match. d0 has almost no documentation to work from.

#### Legal and naming

- **Copy functions, not names or faces.** Module names like Spectraphon, Flurry or Silhouette, and brands like Buchla, Make Noise, Intellijel and Mannequins, are trademarks. We give each recreation its own name and our  panel, as we did with DistantPlanet for the Forbidden Planet.
- **Closed firmware is off limits.** We rebuild from manuals and recordings, and never extract or disassemble firmware.
- **Factory data stays private.** Wavetables, samples or presets taken from hardware are the maker's copyright: fine in your own rig, not to share.
- **Open source means following the licence.** For Ectocore that's the licence of the \_core repository.
- **Our existing "Buchla" plugin name** is fine privately, but should be renamed if it's ever shared publicly.

#### Suggested order

1. **Ectocore**, the most authentic result.
2. **Stolperbeats**, for grooves that suit the EP.
3. **Swoop + EuRolz + Source of Uncertainty**, as one Blasser/Buchla-flavoured modulation plugin for evolving patches.
4. **Easel Command + 208C**, alongside our 259t.

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Porting Peter Blasser modules to VCV]]
