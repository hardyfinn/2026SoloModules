---
title: Porting Peter Blasser modules to VCV
date: 2026-11-20
source: Music production · Peter Blasser modules in VCV
topic: Music
tags: [vcv-rack, peter-blasser, eurorack, chaos]
summary: |
 Swoop (8 to 9 out of 10) and Fourses (8) port well to VCV; Rollz (6) loses more.
 Their character comes from coupling rules that code copies exactly, plus analog imperfection that must be added on purpose.
---

# Porting Peter Blasser modules to VCV

## Summary
Swoop (8 to 9 out of 10) and Fourses (8) port well to VCV; Rollz (6) loses more.
Their character comes from coupling rules that code copies exactly, plus analog imperfection that must be added on purpose.

## Question
> New thread - suitability of Peter blasser modules converting to vcv, especially swoop, rolz, fourses,

## Answer
Swoop and Fourses would port well to VCV. Rollz would lose more in the move. Here are my scores out of 10 for how close a VCV version could get to the real thing:

- **Swoop: 8 to 9.** It's one "bounce" oscillator, a triangle that turns round at two limits you control with CV. It's simple to model and would be quick to build.
- **Fourses: 8.** It's four of those oscillators stacked, so each one's limits are set by its neighbours. The coupling that makes it chaotic can be copied exactly. The organic wander comes from small differences between real parts, so I'd have to add that on purpose.
- **Rollz: 6.** Its feel comes from rough analog timing and from your finger on the brass touch plates. VCV can only stand in for the touch with knobs and CV.

My suggestion is one plugin built on a shared oscillator core: a Swoop-style module first, then Fourses, then Rollz with a struck "gong" voice. They'd get our own names and  panels, not Blasser's. The doc also covers his other instruments and the naming caveats: Peter Blasser modules in VCV. You can edit it or comment on it there.

[…]

### Full write-up

#### Verdict

Swoop and Fourses port well, because their character comes from a simple rule (a slope that turns round at moving limits), not from rare parts. Rollz ports less faithfully, because its feel depends on analog thresholds and on your skin bridging brass touch nodes.

| Module | What it is | Authenticity in VCV (out of 10) | Hardest part | Effort |
| --- | --- | --- | --- | --- |
| Swoop (Mobenthey, 8 HP) | One bounce oscillator: a triangle whose upper and lower limits are CV | 8 to 9 | Clean audio-rate turnarounds without aliasing | Small |
| Fourses (Mobenthey, 12 HP) | Four bounce oscillators stacked so each one's limits are its neighbours' voltages | 8 | The "paradox wave" when limits cross, and analog drift that keeps it alive | Medium |
| Rollz (Rollz-5, LabRolz, EuRolz) | Several rhythm oscillators (astable multivibrators) coupled through brass touch nodes | 6 | Touch plates and current-coupling feel | Medium to large |

The best route is one plugin built around a shared "horse" core (Blasser's own name for a bounce oscillator), starting with Swoop, then Fourses on the same core, then Rollz.

#### Why Blasser circuits are a special case

Blasser builds instruments from a few cheap parts wired so they push on each other. The sound comes from the wiring pattern, which code copies exactly, plus small analog imperfections, which code has to fake on purpose.

- **Coupling is easy to copy.** Each oscillator reads its neighbours' voltages. In VCV that is a few lines per sample, and the chaotic behaviour appears by itself.
- **Imperfection must be added.** Real parts differ by a few percent, comparators have offsets of a few millivolts, and everything drifts with heat. Without this, a digital Fourses locks into neat, repeating patterns. Each module instance needs its own random tolerance seed plus slow drift.
- **Turnarounds alias.** A hard flip from rising to falling at audio rate makes digital fizz. It needs oversampling (4 to 8 times) or band-limited corners.
- **Touch plates have no VCV equivalent.** Brass nodes let your finger's resistance carry current between circuits. The closest substitute is a knob or CV per node pair, or MIDI pressure from a controller.
- **Low-pass gates are already solved.** We have vactrol-style decays elsewhere in our plugins, so any "bong" or "plink" voice is the easy part.

#### Fourses: 8/10

**Circuit.** Four "horses" stacked top to bottom. Each one rises until it hits its upper limit, then falls until it hits its lower limit. Its upper limit is the horse above it and its lower limit is the horse below; the top is capped at +8 V and the bottom at -8 V. Each horse has a rate knob that also tilts the slope (fast up and slow down, or the reverse), a CV input, a bounds insert to override a limit, and a position output. When a lower limit rises past an upper one the horse flips rapidly, which Blasser calls the paradox wave.

**What's published.** The Mobenthey shop pages describe the coupling exactly. Blasser documents a "horse" opcode for his Shbobo language (up speed, down speed, upper limit, lower limit), and his paper-circuit pages cover the Ciat-Lonbarde Fourses kit. Someone has also made a web port, and a Max thread rebuilds it in gen\~. So the behaviour is well understood.

**What's hard.** The paradox wave needs care so it doesn't turn into digital noise. The organic wander needs per-horse tolerance and slow thermal drift. The original kit's "greased perspex tube" and "intersexon" linking circuit are physical and only loosely describable.

**VCV approach.** 16 to 20 HP (to fit our 3.2 mm labels). Four horses, each with RATE, TILT, CV attenuverter, upper and lower bound insert jacks, triangle and square outs. Run the core at 8 times oversampling with a little hysteresis and a hidden "age" setting for tolerance. Optional poly out of all four.

#### Swoop: 8 to 9/10

**Circuit.** One Fourses horse on its own, with everything exposed. Two bounds inputs set the limits, so the frequency follows from how far apart they are. It free-runs with nothing patched. Up and down "basis" knobs set the two slopes, with attenuverters for up, down and both. A switch picks CV, low audio or audio range. Triggers can force an up or down swoop, and two gates fire at the end of each swoop.

**What's published.** The shop page and manual describe every control. No schematic is posted for the Eurorack version, but the behaviour is the same horse rule as Fourses.

**What's hard.** Very little: anti-aliased corners at audio rate, and how the trigger inputs interrupt a swoop mid-flight (needs listening to a real one, or a video, to match).

**VCV approach.** 10 HP. It is also the test bed for the shared horse core, so building it first de-risks Fourses. Useful as a modulation source in your techno patches: patch a slow LFO into one bound and the triangle speeds up and narrows as the bounds close.

#### Rollz: 6/10

**Circuit.** A "rolz" is a rhythm oscillator built as an astable multivibrator: it charges, flips, and charges again, at rhythm rather than pitch rates. Rollz-5 has five, LabRolz (the Plumbutter add-on) and the crucFX EuRolz (12 HP) have three plus a centre section. Each rolz has a rate knob and "verso" and "inverso" inputs that push it faster or slower. The brass nodes are current-control points inside the multivibrators, so touching or bridging them with a finger, or patching between them, couples the rhythms into lurching, rolling patterns. On EuRolz the centre section can lock high or low when tempo is below centre.

**What's published.** Rollz-5 paper-circuit builds and build guides circulate, and Blasser's paper pages cover the kit. Exact component values for the Eurorack versions were not found in this check.

**What's hard.** The feel lives in analog details: where each multivibrator's threshold sits, how much current a finger passes (roughly 100 kΩ to a few MΩ depending on pressure and sweat), and how two rolz pull each other into and out of sync. A clean model sounds tidy and "quantised"; it needs uneven thresholds and noise on the coupling. Touch is the biggest loss.

**VCV approach.** 16 HP, three or five rolz. Model each as a relaxation oscillator whose charge current is the rate knob plus currents flowing in from other rolz. Replace brass nodes with a small coupling matrix: a "touch" knob per neighbour pair, with CV, so a gate or MIDI pressure can play the role of your finger. Outputs: gates, plus the raw sawtooth-like charge voltage as CV. Pair it with a simple struck resonator ("gong") voice for the Rollz-5 sound.

#### Other Blasser instruments worth a look

These come from memory, not from this check, so treat the details as approximate.

| Instrument | Why it suits VCV | Likely authenticity |
| --- | --- | --- |
| Shbobo Shnth | Digital, programmed in Blasser's own documented Shbobo language; the horse opcode comes from it | High |
| Gongs (Plumbutter section) | Struck resonant filters; the natural voice for Rollz triggers | High |
| Sidrax Organ | Cross-coupled oscillators for drones; same coupling idea as Fourses | Medium to high |
| Cocoquantus | Dual looping delay with odd clocking; overlaps our Loopshifter and Tapejeans | Medium |
| Deerhorn | Played by hand proximity; loses its point without the antennas | Low |

#### Licensing and naming

Behaviour can be recreated, but names and panel art belong to Blasser. Same rule as the Eurorack authenticity check:

- Use our own module names and  panels, not Swoop, Fourses, Rollz, Ciat-Lonbarde or Mobenthey.
- Copy no artwork or text from his paper circuits; write the DSP from the described behaviour.
- Say "inspired by Peter Blasser's horse oscillators" in the guide, which credits him without implying endorsement.
- Blasser shares his ideas openly but sells kits for a living. If this ever goes to the public VCV library, a courtesy note to him first would be the decent thing; for private use none of this matters.

#### Recommendation

Build one plugin with a shared horse core, in three steps: Swoop-style (small), then Fourses-style on the same core (medium), then Rollz-style with a gong voice (larger). If you want it, the only question is whether it goes in 2026SoloModules.

#### Sources

- [Swoop product page (Ctrl-Mod)](https://www.ctrl-mod.com/products/swoop)
- [Fourses product page (Ctrl-Mod)](https://www.ctrl-mod.com/products/fourses)
- [Recreating a horse/Fourses oscillator (Cycling '74 forum)](https://cycling74.com/forums/recreating-a-horsefourses-oscillator/)
- [crucFX EuRolz (ModularGrid)](https://modulargrid.net/e/crucfx-eurolz)
- [Implied chaos and sound circuits of Ciat-Lonbarde (Make:)](https://makezine.com/?p=54317)

## Sources
- The conversation itself; no outside sources were cited.

Related: [[How authentically Eurorack modules recreate in VCV]]
