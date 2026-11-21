# Vinylizer Fable

One 50 HP VCV Rack module that puts audio through the chain of a 1990s record,
modelled from the physics of the medium. No crackle, no pops, no hiss.

Ten processes, in signal order, each with an ON button and a CV input:

1. **ELLIPTIC**: bass folded to mono below a crossover, as mastering engineers did for the lathe.
2. **HF LIMIT**: cutter-protection limiter acting on treble acceleration.
3. **CUT**: cut level, side length (groove pitch, bass high-pass, excursion limiter) and cutter head colour.
4. **MEDIUM**: lacquer, DMM or acetate dub plate; groove echo (true pre-echo in the menu) and wear.
5. **TRACING**: the stylus tip rolled over the groove wall in real micrometres (4x oversampled); inner groove distortion.
6. **TRACKING**: tracking angle error and mistracking from tracking force and skating.
7. **CARTRIDGE**: MM coil inductance with load capacitance, tip resonance, crosstalk, azimuth.
8. **ARM**: arm resonance and record warps.
9. **PLATTER**: off-centre hole, wow and flutter for direct, belt or cheap decks.
10. **PHONO**: input overload, RIAA playback with part tolerance, tube / op-amp / DJ mixer stage, subsonic filter.

The record curve and playback curve always run as a matched pair, so with every process off the module is flat.
NEEDLE sets where the stylus is on the side; PLAY SIDE moves it across in real time. FORMAT presets: 7" 45, 12" 45, EP 33, LP 33, DUB PLATE.

Two expanders live in the same plugin and act inside the main module (no audio jacks of their own):

- **Vinylizer Master** (12 HP, touching the left edge): the master before the lathe. Tape at 30 or 15 ips (drive, print-through, noise reduction error, hiss), 1630 / DAT / CD master converters, 14 or 16 bit, emphasis mistakes, the lathe's digital delay line, CD-style limiting.
- **Vinylizer Pressing** (14 HP, touching the right edge): a poor pressing as shapes in the groove wall, read by the same stylus and cartridge as the music. Stock (virgin, regrind, styrene, flexi, picture disc), surface noise, non-fill, stamper wear, disc weight, dust and stylus fluff, scratches, cue burn. TURN (once per turn) and SKIP outputs. DUST and SCRATCH default to zero.

Manual: `docs/VinylizerFable-Manual.pdf`.
`make test` runs offline engine checks and writes `vinylizer-demo.wav` (dry, 12" 45, LP 33 at the label, worn dub plate).
`python3 res/make_panel.py` and `python3 res/make_expanders.py` regenerate the panels; `python3 tools/preview.py out.png` and `python3 tools/preview_expanders.py out.png` draw previews.
2.2.0 (2026-11-21) adds the Master and Pressing expanders.
2.1.0 (2026-11-21) follows Fable's review (`docs` in the project): tracing that grows smoothly with level, Hertz-contact treble loss toward the label, a hotter level calibration (5 V = 12 cm/s), playback filters run oversampled (no 18 kHz cliff), bass mistracking from the cantilever spring, a cutter depth limit, a sliding-filter HF limiter, vertical tracking angle error, an IEC position on SUBSONIC, wear by wavelength, and no cutter coil heating.

Sources for constants: Shure M44-7 data sheet (720 mH, 400-500 pF load), JIS S 8502 (disc dimensions), Stereophile "Cut and Thrust" (IEC 7950 us). Other values are typical, not measured.

Panel labels are 2.8 mm (controls) and 2.5 mm (jacks), a little under the repo standard, to fit ten columns.
