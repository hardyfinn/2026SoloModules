# Gongoozler

Gong and tam-tam physical model for VCV Rack (Hær, 2.0.0, 28 HP, MIT).

Four gong types (tam-tam, wind gong, opera gong, bossed/gamelan gong), up to
256 modes, and the things that make a gong a gong: hard hits **bloom** from a low
hum into a shimmering wash (energy cascade), the plate **roars** as it stretches,
opera gongs **glide** in pitch, and paired tones **beat** (ombak).
Strike TRIG, VEL, V/OCT; MALLET and POSITION per hit; MUTE (towel) and HAND (grab).

- Guide: `docs/guide.html` (PDF in the project files)
- Engine (no Rack code, notes on the physics): `src/GongEngine.hpp`
- Panel: `python3 res/make_panel.py` (needs `pip install fonttools`); preview: `python3 tools/preview.py`
- Offline checks: `make test` (stability over 120 random patches, bloom, glide, speed)
- Demo WAVs: `make demo`
