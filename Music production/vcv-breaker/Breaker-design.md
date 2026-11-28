# Breaker: design (2026-11-28, mockup v1)

36 HP, charcoal + orange, plugin Breaker 2.0.0 in 2026SoloModules (MIT). Untested in Rack.

## What it does
1. **LOAD** a WAV (mono or stereo, any rate). Breaker finds every hit (onset detection), and sorts each into **K**ick, **S**nare, **H**at or **O**ther by its sound (low-end weight, mid "crack" + noise, top-end fizz). It also guesses the tempo and bar count.
2. The top screen shows the waveform with a coloured tag on each hit. **Click a tag to cycle K → S → H → O** if it guessed wrong. **SENS** finds more or fewer hits (re-analyses).
3. The lower screen is a 4-lane, 16-step pattern (one bar at a time; **BARS** = 1, 2 or 4). The generator writes it; the playhead triggers a hit of the matching type from the break.

## Pattern generator
- **STYLE**: BREAK (the break's own rhythm, as found), BREAKS, JUNGLE, HIP HOP, BROKEN TECHNO. Each style is a set of per-step weights per lane (kick anchors, backbeat snare, hat grid), drawn from the groove field guide; the generator rolls a pattern from those weights.
- **DENSITY**: fewer or more hits. **GHOSTS**: quiet snare/hat ghost notes (played softer). **FILLS**: chance of a snare/hat roll fill on the last bar of the phrase. **SWING**: 50 to 75%.
- **VARIETY**: 0 = each lane always uses the same hit; up = picks among all hits of that type (e.g. the break's 9 different snares).
- **NEW** (button + trigger in): roll a fresh pattern (locked lanes stay).
- **MUTATE**: chance each bar that a few steps move, appear or vanish (drift, Thunder-style). **LOCK** (button + gate in): freezes the pattern as you hear it. Right-click a lane letter to lock just that lane. Right-click menu: "Clear mutation" (back to the last NEW).
- **Edit by hand**: click a grid cell to toggle a step, shift-click for a ghost.

## Sound
- One-shot playback; each hit plays from its start to the next hit (or **DECAY** shortens it). Re-triggering the same lane cuts the previous one (choke per lane).
- **PITCH** knob ±12 st, Akai-style repitch (speed and pitch together) + **V/OCT** in. **LEVEL** (excluded from Ctrl+R).

## Clock
- **CLOCK** in = 16th notes; **RESET** in. With CLOCK unpatched it free-runs at the break's detected tempo (× pitch), started/stopped by **RUN**.

## Jacks
- In: CLOCK, RESET, NEW, LOCK, MUTATE CV, STYLE CV, DENSITY CV, V/OCT.
- Out: KICK, SNARE, HAT, OTHER audio (patching one removes it from the mix), L / R mix, K/S/H/O GATE (to drive other drums too), ACCENT gate (strong beats), CLK (16ths out, handy when free-running).

## Saved in the patch
Path of the WAV, hit list with any reclassifications, current pattern, lock state.
