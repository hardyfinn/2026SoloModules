# Lilt (relabi generator)

Four LFOs run at ratios of a felt **pulse**. Their weighted sum wanders through
four threshold lines, and every crossing is a trigger. Because the LFOs sit
near (not on) simple ratios of the pulse, the triggers lean towards the beat,
fall away from it and come back: John Berndt's *relabi*.

- **PULSE** 20-300 BPM, or patch **CLOCK** (1, 2, 4 or 24 per beat, right-click).
- **LFO 1-4**: RATIO (1/8-8 cycles per pulse; SNAP to musical fractions), LEVEL in the sum (negative flips it), raw ±5 V OUT.
- **DRIFT** pulls each LFO off its ratio by its own amount (up to 15 %).
- **MUTATE** keeps changing that drift, nudges phases and levels; **FREEZE** gate holds it.
- **GRID** pulls triggers onto 1/4-pulse ticks (0 = free relabi, full = locked).
- **THRESH / SPREAD** place the four lines; **EDGE** fires on rising, falling or both (gates toggle on BOTH).
- Outputs: TRIG 1-4, GATE 1-4, BEAT, SUM, ACC (how steeply the sum crossed at the last trigger, 0-10 V).
- **RESET** puts every LFO back on the one.

36 HP. `make test` renders a demo WAV offline. Panel: `python3 res/make_panel.py`.
Guide: `docs/guide.html`.
