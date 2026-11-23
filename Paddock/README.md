# Paddock

Three modules in the spirit of Peter Blasser's coupled analog circuits (Ciat-Lonbarde /
Mobenthey). Our own designs, names and panels; nothing is copied from his schematics or
artwork. Built around one shared "horse" core: a bounce oscillator that rises to an upper
bound, turns, and falls to a lower bound.

| Module | HP | What it is |
|---|---|---|
| Swerve | 12 | One bounce oscillator. Separate UP and DOWN slopes, HIGH and LOW bounds (knob + CV), LFO to audio, one-shot mode, TOP and BTM pulses, paradox when the bounds cross |
| Gallop | 24 | Four bounce oscillators stacked so each one's bounds are its neighbours. Rate, tilt, CV, bound inserts, paradox lights, mix + poly out, scope |
| Tumble | 20 | Five rhythm cores coupled by TOUCH knobs (pull together / push apart), each striking its own gong. Gates, stereo gongs, sync, tempo |

Every module behaves like one physical unit: a saved seed gives it its own small part
tolerances and slow drift. Right-click: **Character** (Clean / Factory / Worn) and
**New unit** (re-roll the tolerances).

`make test` runs the offline checks and writes three demo WAVs. `python3 res/make_panel.py`
redraws the panels; `python3 tools/preview.py panel-preview.png` draws the preview.
