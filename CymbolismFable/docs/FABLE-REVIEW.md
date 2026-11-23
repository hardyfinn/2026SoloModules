# Cymbolism Fable: Fable's review of the build spec

Reviewed 2026-11-21 by Fable 5.2. The spec under review is `02-build-instructions.md`, which was written by Opus 5.5.

## Verdict

The overall architecture is right and I would keep it: real modes computed from the cymbal's shape, a nonlinearly coupled core, a large linear bank above it, physical beaters, one module. It is the correct family of methods, and it is more ambitious than the published real-time work (the 2023 real-time gong paper runs a small flat rectangular plate; the 2019 GPU crash paper is a linear filter bank whose authors say it lacks the bloom).

But the spec has four problems that would cost realism if built as written, plus some gaps. None needs a restart. The work-in-progress solver Opus began has already drifted from the spec on point A1 in the right direction.

## A. Things that are wrong or risky as written

**A1. The linear shell stiffness is in the wrong place.** The spec builds the modes from bending only and puts all curvature effects, including the linear stiffening from the bow, inside the nonlinear stepper. A cymbal's bow rises about 16 mm on a shell under 1 mm thick, so curvature dominates the low modes. As written, the 1,700 upper modes would be flat-plate modes with the wrong frequencies and shapes, and a very stiff linear term would sit inside an approximate scheme.
*Fix:* compute true linear shell modes (bending plus linearised membrane stiffness) and step them exactly. Only the genuinely nonlinear remainder goes to the nonlinear stepper. That remainder is not positive on its own, so the stepper needs the shifted form in A2.

**A2. The time stepper is an unvalidated bet.** The whole sound rests on the scalar auxiliary variable (SAV) method with one scalar for the entire shell nonlinearity. It is cheap and stable, but the literature reports spurious artefacts from the auxiliary variable drifting away from the true potential at audio rate (Russo, Webb, Ducceschi, Bilbao 2025, who fix it with a constraint borrowed from collision modelling). The spec's own test only checks that energy is conserved, which this failure passes. The spec's "re-anchor every few hundred samples" patch breaks the conservation the test checks.
*Fix:* (1) write a slow, trusted reference in Python first: the fully implicit energy-conserving scheme at 8x oversampling. (2) Compare candidate real-time schemes against it at crash amplitudes, by spectrogram and by band-energy curves. Candidates: scalar SAV with the 2025 constraint, and one auxiliary variable per stress mode (more accurate, heavier). (3) Ship the cheapest that matches. Add this as a pass/fail test.

**A3. The part people hear as "cymbal" is the least physical part.** With 128 core modes on an 18 inch crash, the fully coupled physics stops at about 1.7 kHz. Everything from there to 20 kHz, which is the wash and shimmer, comes from a statistical cascade that injects white noise into modes, with constants tuned until the spec's own test passes. That risks sounding like filtered noise riding on a low gong.
*Fix:* extend the real coupling upward sparsely. The low-order membrane stress modes (the first 12 to 16) are what scatter energy between high modes in a real cymbal. Let those stress modes include quadratic terms from upper modes, each upper mode coupled to its few nearest neighbours in frequency that satisfy the angular selection rule. It stays energy conserving, costs roughly as much as the core again, and makes the highs respond to the actual stress in the shell, with phase and timing, rather than to noise. Keep the statistical cascade only as a small residual, and in ECO.

**A4. The bell breaks the shallow-shell assumption.** The spec's equations are valid for slopes up to roughly 15 to 20 degrees. A bell wall reaches 35 to 45 degrees, and the BELL type is 25% rise. Bell frequencies would be off by tens of percent, and the bell is the one clearly pitched part of a cymbal.
*Fix:* compute the linear modes with a proper shell-of-revolution element (arc-length coordinate along the profile, true curvatures, all three displacement components). Same cost, standard method. Keep the shallow approximation only for the nonlinear coupling, which lives in the bow where it is valid.

## B. Gaps worth filling

**B1. Decay is underspecified, and it is half the realism.** One loss number per alloy cannot give a ride that hums for 15 seconds at 400 Hz while its 10 kHz sheen dies in one. Use the Chaigne and Lambourg plate damping model (thermoelastic, viscoelastic and radiation terms, each with its own frequency law), with per-type decay-time targets at 250 Hz, 1 kHz, 4 kHz and 10 kHz and a test that checks them.

**B2. The microphone model would sound thin.** Summing a few surface points with a square-root filter ignores that a mic straight above the cymbal hears only the axisymmetric modes, because the others cancel. Compute each mode's pickup weight with a Rayleigh integral over the surface at that mode's own frequency, for a mic placed off-axis as real overheads are, using acceleration, with the front-to-back cancellation of an unbaffled disc. It is done once per build, so it is free at run time, and it covers close and far mics with one method.

**B3. Add TENSION.** Hammering does more than randomise modes: it leaves in-plane stress that cymbal makers use to tune the instrument, from dead and floppy to tight and glassy. It is one extra term in the mode solver and one of the largest single effects on the sound. I would add a TENSION knob and keep HAMMER for irregularity.

**B4. Make SIZE instant.** If SIZE scales the whole cymbal uniformly (diameter, profile and thickness together), every frequency scales exactly and nothing needs rebuilding. SIZE then responds immediately and can be modulated or sequenced freely, like pitch. WEIGHT then sets how thin it is for its size. The display shows the resulting inches and millimetres.

**B5. PLAY needs a CV input.** A ride pattern with shoulder crashes on the accents is the most natural use, and the spec gives no way to sequence it.

**B6. Stick click.** Part of a ride "ping" is the stick itself. Two or three stick body modes driven by the contact force, mixed in quietly, are nearly free.

**B7. Give the engine its own thread.** Run the cymbal on a dedicated thread in 32-sample blocks (under 1 ms of added latency, constant). It stops one heavy module starving the rest of the patch and lets the core grow, perhaps to 250 modes or more. Offer it as a context-menu option, on by default.

**B8. Calibrate against real cymbals.** The spec has no check against a real instrument. Add a test comparing the first ten or so mode frequencies with published measurements (Perrin et al.) for a similar cymbal, within about 15%.

**B9. Listen early.** Render audio as soon as the core and upper modes run (before beaters, panel or manual) and send it to Hardy. If the core approach does not sound like metal at that point, nothing after it will fix it.

## C. Things to trim or defer

- **Swing on the stand:** real but subtle. Build last.
- **Bow:** the fiddliest beater numerically and the one most likely to delay the rest. Build after everything else works. It can ship in a point release.
- **Panel width:** 34 HP is tight for about 20 knobs and 17 CV jacks plus a display. Plan for 40 HP.
- **MOUNT** should not trigger a full rebuild. Treat it as a correction to the existing modes.

## What stays as specified

The ten types, the six beaters and their contact models, rivets, choke, tape damping, external exciter, mono output, the quality levels, the rebuild-and-swap behaviour with a cache, the display, the manual and patch recipes, and the house conventions.

## Effect on cost

A1, A4, B1, B2, B3, B4 and B8 only touch the solver, which runs in the background, so they cost nothing at run time. A3 roughly doubles the run-time cost at HIGH, which B7 pays for. A2 is build effort, not CPU.

## Sources

- Bilbao, Webb, Wang, Ducceschi, "Real-time Gong Synthesis", DAFx 2023: https://www.dafx.de/paper-archive/2023/DAFx23_paper_19.pdf
- Russo, Webb, Ducceschi, Bilbao, "Convergence analysis and relaxation techniques for modal scalar auxiliary variable methods applied to nonlinear transverse string vibration", POMA 2025: https://www.research.ed.ac.uk/en/publications/convergence-analysis-and-relaxation-techniques-for-modal-scalar-a/
- Skare and Abel, "Real-Time Modal Synthesis of Crash Cymbals with Nonlinear Approximations, Using a GPU", DAFx 2019: https://dafx.de/paper-archive/details/qe8SzTmE_uIgihtLswWFFA
- Chaigne and Lambourg, "Time-domain simulation of damped impacted plates. I. Theory and experiments", JASA 2001: https://hal-ensta.archives-ouvertes.fr/hal-00830699
- Perrin et al., "The normal modes of cymbals": https://www.ioa.org.uk/system/files/proceedings/r_perrin_gm_swallowe_sa_zietlow_tr_moore_the_normal_modes_of_cymbals.pdf

I read the first three through summaries of the papers in this session. The points about the shallow-shell limit, sparse upward coupling, the Rayleigh pickup and uniform scaling are my own analysis, not taken from a paper.
