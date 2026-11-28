# Cymbal synthesis for VCV Rack: research notes (2026-11-21)

## What makes a real cymbal sound like a cymbal
- A cymbal is a thin shallow shell (a spherical cap with a raised bell), not a flat plate. Lathing grooves and hammering add stiffness and irregularity, which split degenerate mode pairs and spread the spectrum.
- Modal density is huge: hundreds of modes below 5 kHz and thousands up to 20 kHz. Low modes are dense and inharmonic; there is no single pitch except at the bell.
- **Linear at soft strikes, nonlinear at hard ones.** Soft hits ring like a bank of inharmonic resonators. Hard hits trigger geometric nonlinearity (von Kármán coupling): energy cascades from low to high modes over tens of milliseconds. That is the crash "bloom", where high-frequency shimmer peaks after the attack. Physicists describe it as wave turbulence.
- Shell curvature makes some modes soften or harden with amplitude, which gives pitch glides. Chinas (inverted flange) and gongs show this most.
- The bell is a stiff region with its own clearer, more pitched modes. Bell hits sound like a bell; edge hits excite the low bow modes and the nonlinearity most.
- Rivets (sizzle) are rattling contacts: a separate nonlinear buzz driven by plate motion.

## Excitation
- Stick contact follows a Hertz law, force = k * compression^alpha (alpha about 1.5). Contact time sets brightness: tip about 0.5 to 1 ms, shoulder longer and heavier, felt mallet several ms with a softer, more nonlinear law.
- Strike position sets which modes are excited, via mode shapes (Bessel-type radial shape times cos(m*theta)). Position also sets how hard the nonlinearity is driven.
- Brushes are many tiny stochastic contacts. Rolls and swells are repeated mallet contacts that pump energy until the cascade takes over.
- Choke is a sudden large increase in damping, mostly in the low modes, with a short contact rattle.

## Synthesis approaches, from most to least accurate
1. **Finite-difference shell models** (Bilbao, NESS project, Edinburgh). Most faithful, including bell and curvature, but far from real-time on a CPU.
2. **Modal von Kármán models** (Ducceschi and Touzé). Physically exact coupling between modes, energy-conserving schemes. Cost grows with the cube or fourth power of the mode count, so real-time CPU use is limited to tens of modes; a DAFx paper runs a crash in real time on a GPU with approximations.
3. **Modal bank plus approximated nonlinearity** (what fits VCV). Several hundred SIMD two-pole resonators with mode data from shell theory, plus a cheap energy-transfer stage: energy taken from low modes in proportion to amplitude squared and fed into high modes and shaped noise bands, plus amplitude-dependent pitch glide. Captures bloom, choke, position and strike type at a few percent CPU per voice.
4. Analogue circuit models (TR-808 style square-wave clusters). Not realistic, side interest only.

## Implications for the design
- Approach 3 is the realistic target. A "hero" quality setting can raise mode count and use a small exact coupling core (option 2) for the lowest 20 to 30 modes.
- Stereo is cheap: read mode shapes at two azimuths for two mic positions.
- Types (splash, crash, ride, china, sizzle, stack, bell) become presets over shared physical parameters: diameter, thickness, profile height, bell size, lathing, hammering, rivets.

## Sources
- Touzé et al., tapered cymbals (JASA 2019): https://perso.ensta-paris.fr/~touze/PDF/2019_BaoCT_TaperedCymbals_JASA.pdf
- Ducceschi and Touzé, energy-conserving modal synthesis for thin plates: https://hal-ensta-paris.archives-ouvertes.fr/hal-01194569
- Modal approach for damped impacted plates: https://researchportal.ip-paris.fr/en/publications/modal-approach-for-nonlinear-vibrations-of-damped-impacted-plates/
- Accurate and efficient modal algorithm for nonlinear plates (Edinburgh): https://www.research.ed.ac.uk/en/publications/simulations-of-nonlinear-plate-dynamics-an-accurate-and-efficient/
- Real-time modal synthesis of crash cymbals on a GPU (DAFx): https://dafx.de/paper-archive/details/qe8SzTmE_uIgihtLswWFFA
- Bilbao, nonlinear shell vibration (cymbals): https://www2.ph.ed.ac.uk/~sbilbao/cympage.html
- NESS project, cymbals and gongs: https://www.ness.music.ed.ac.uk/target-systems/cymbals-and-gongs
- Perrin et al., normal modes of cymbals: https://www.ioa.org.uk/system/files/proceedings/r_perrin_gm_swallowe_sa_zietlow_tr_moore_the_normal_modes_of_cymbals.pdf
- Sound On Sound, analysing metallic percussion: https://www.soundonsound.com/techniques/analysing-metallic-percussion
