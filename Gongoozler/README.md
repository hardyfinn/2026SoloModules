# Gongoozler (+ Gonzo)

Gong and tam-tam physical model for VCV Rack (Hær, 2.0.0, 28 HP, MIT).

Four gong types (tam-tam, wind gong, opera gong, bossed/gamelan gong), up to
256 modes, and the things that make a gong a gong: hard hits **bloom** from a low
hum into a shimmering wash (energy cascade), the plate **roars** as it stretches,
opera gongs **glide** in pitch, and paired tones **beat** (ombak).
Strike TRIG, VEL, V/OCT; MALLET and POSITION per hit; MUTE (towel) and HAND (grab).

**Gonzo** (36 HP, same plugin) is the same engine pushed past real gongs: 2 cm to 20 m,
mode-spacing LAW and ORDER, DENSITY, BOW and audio EXCITE, freeze, reversed TILT and a bloom
that sinks. At its violet ticks it is a Gongoozler tam-tam. Design notes: `docs/gonzo-design.md`.

- Guide: `docs/guide.html` (PDF in the project files)
- Engine (no Rack code, notes on the physics): `src/GongEngine.hpp`
- Panels: `python3 res/make_panel.py`, `python3 res/make_panel_gonzo.py` (needs `pip install fonttools`); preview: `python3 tools/preview.py`
- Offline checks: `make test` (stability over 120 random patches, bloom, glide, speed)
- Demo WAVs: `make demo`
