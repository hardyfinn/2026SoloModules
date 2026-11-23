# Joanna

A physical model of an upright home piano and the room it stands in, for VCV Rack 2.
Aimed at the sound of a home piano for jazz and gospel: felt hammers, beating unisons,
imperfect tuning, wear, age, action noise, three pedals, lid, wall and room.

- 34 HP, stereo, 16-voice polyphonic (V/OCT, GATE, VEL)
- every piano has a serial number that fixes every quirk of every key; ANOTHER builds a new one
- TUNING (years since the tuner came), UNISON (honky-tonk), WEAR (played), AGE (old)
- SUSTAIN pedal (half pedalling via CV), SOFT (half-blow), FELT (practice felt)
- LID (shut / top open / front off), WALL, ROOM, FURNISH, DISTANCE (inside the piano .. next room)
- 20 presets in the right-click menu
- Guide: [docs/Joanna-Guide.pdf](docs/Joanna-Guide.pdf); design notes: [docs/joanna-design.md](docs/joanna-design.md)

## Build

    make RACK_DIR=path/to/Rack-SDK dist

Offline checks and demo WAVs, no Rack needed:

    make test

Panel: `python3 res/make_panel.py` (needs `pip install fonttools`).

MIT licence (see LICENSE.txt).
