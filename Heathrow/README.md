# Heathrow

A very controllable stereo phaser / flanger for VCV Rack. 42 HP.

**Engine A**
- **MORPH**: phaser to flanger, with usable in-between settings.
- **CENTER / DEPTH / RATE**: where the sweep sits, how far it moves, how fast (0.01-20 Hz, up to audio rate with RATE CV at 1 V/oct).
- **FEEDBACK**: negative (hollow) to positive (ringing).
- **STAGES**: 2-24 phaser stages. **RES** sharpens the phaser notches.
- **SPREAD**: right channel's LFO offset, 0-180°.
- **SHAPE**: sine, triangle, ramp, square, drift (smooth random), S&H, blending in between.
- **STEPS**: sweep moves in 2-16 jumps per cycle. **GLIDE** smooths them.
- **MIX**: centre dry, half-way the deepest notches, ends wet only; the negative side flips the wet.
- **ENV**: the input's level pushes the sweep up or down.
- **TZ**: through-zero flanging (the dry is delayed 3 ms so the sweep can pass through it).
- **LFO FREE / CLK**: with CLK, RATE picks cycles per clock (1/64 ... 4x) and the LFO locks to CLOCK.
- **SWEEP** input replaces the LFO (+-5 V; 2-channel poly gives a separate right).
- CV with attenuverters: MORPH, CENTER, DEPTH, RATE, FDBK, STAGES, SPREAD, MIX.

**Engine B** (Bi-Phase idea)
- **B ON/OFF** switches it out.
- **ROUTE**: SERIES (A then B), PARALLEL (A + B), SPLIT (A left, B right).
- **B SWEEP**: OWN LFO (B RATE), follows A, or A upside down.
- Own MORPH, CENTER, DEPTH, RATE, FDBK, STAGES, MIX, each with a CV input.
- Shares RES, TZ, SHAPE, STEPS, SPREAD, GLIDE, ENV and the clock with A.

**Memory**
- 6 slots hold every setting (A, B, switches and attenuverters).
- Tap a slot to recall it. Tap **SAVE**, then a slot, to store (SAVE again cancels).
- **TIME** glides knobs to the recalled settings (0 = jump, up to 20 s); switches jump straight away.
- **SLOT** CV: 0-10 V picks slot 1-6; a change recalls it.
- Right-click to clear slots. Slots are saved with the patch.

`make test` runs offline engine checks and writes `heathrow-demo.wav`.
`python3 res/make_panel.py` regenerates the panel; `python3 tools/preview.py out.png` draws a preview.
