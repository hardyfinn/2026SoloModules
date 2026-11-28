# Vinylizer Fable: review of the design and the DSP

Fable, 2026-11-21. Reviewed: the design doc (01), `src/VinylDSP.hpp` as built, and fresh measurements of the built engine at 48 kHz. Nobody has listened to the module in Rack yet. The physical constants in the code were written from memory and have not been checked against published sources, so anything marked "check" below should be looked up before it is changed.

## Verdict

The architecture is right and worth keeping: a matched record/playback curve with every nonlinear stage working on the pre-emphasised signal in between, and the stylus contact computed in real micrometres of groove wall. That is how the real chain behaves, and the code does it.

The weak point is that the module is currently too clean and too flat at normal levels, then changes abruptly. Three things cause that, and they are the changes that matter most (A1 to A3).

## What the built module measures (defaults, LP 33, 20 min side)

| Test | Result | Real records |
|---|---|---|
| Response, edge of the disc, 60 Hz to 14 kHz | within +0.3 to +1.0 dB | similar |
| Response at the end of the side, 14 kHz | +0.1 dB (0.6 dB less than the edge) | 3 to 6 dB down with a spherical tip |
| Response at 18 kHz | -10 dB, a sudden cliff | a gentler slope |
| 2nd harmonic, 3 kHz, end of side, 0.5 V in | 0.03% | around 1% |
| Same at 1 V, 2 V, 5 V | 2.6%, 4.6%, 10.7% | rises in proportion to level |
| Mistracking in the demo music at default settings | never | rare, but bass-heavy 12"s do skip |

## A. Fix first (accuracy problems in what is already there)

**A1. Tracing distortion switches on instead of growing with level.**
The tip is modelled as a circle tested at whole sample positions along the groove. For gentle slopes the best contact is always the centre sample, so there is no distortion at all until the curvature crosses a threshold, and then it jumps (0.03% to 2.6% for a 6 dB level change). Real tracing distortion is proportional to level, so quiet treble should already carry a little 2nd harmonic.
Change: find the contact point between samples. Either interpolate the wall parabolically around the best sample, or add the continuous small-slope term (tip centre rises by rho x (sqrt(1 + slope^2) - 1)) and keep the circle test only for the case where the tip bridges a trough. Acceptance: 2nd harmonic at 3 kHz doubles for each 6 dB of level, from 0.25 V upward, with no jump.

**A2. Almost no treble loss toward the label.**
Pure geometry loses nothing at low level, which is correct, but on a real record the vinyl gives way under the tip. The contact patch has a length, set by tracking force, tip radius and how soft the material is, and wavelengths near that length are averaged out. The code has a "scanning loss" filter but with fixed contact lengths (1.5 to 3 micrometres) that are too small to do anything audible.
Change: compute the contact length from FORCE, the tip radii and the medium (Hertz contact: grows with the cube root of force x radius; acetate softer than vinyl, check elastic moduli), then apply the loss as a function of contact length over groove wavelength. Target at the end of an LP side with a 0.7 mil spherical tip at 2.5 g: about 3 to 5 dB down at 15 kHz, under 1 dB at the edge. This also makes FORCE mean something tonally: heavy DJ tracking is duller at the inner grooves, which is true.

**A3. Level calibration is too conservative.**
A full-scale 5 V peak is mapped to 5 cm/s, the standard reference level. Real program peaks sit well above the reference: hot 12"s reach roughly 15 to 20 cm/s on peaks (check). So at LEVEL 0 the groove is cut about 8 dB cooler than a normal record, and the distortion, limiting and mistracking stages mostly idle.
Change: map 5 V to about 12 cm/s at LEVEL 0, and re-centre the HF LIMIT and phono overload thresholds to match. Re-run the measurements afterwards, since A1 and A3 interact.

**A4. The 18 kHz cliff.**
Three low-pass filters stack up at the top: cutter head (18.7 kHz at the default HEAD), plating loss, and the cartridge. Real 90s cutter heads were flat past 16 kHz and the roll-off came mostly from playback.
Change: move the cutter head corner to about 22 to 25 kHz at HEAD 0 and 16 kHz at HEAD 1, and make the plating/lacquer loss depend on groove wavelength (so it scales with groove speed, like A2) instead of a fixed frequency. The result should be a slope, not a wall.

## B. Add for realism (things the real chain does that the module does not)

**B1. Bass mistracking.** The stylus model only has tip mass, so it can only lose the groove on treble. Real cartridges also have a springy cantilever, and big low-frequency swings exceed what the spring allows at low tracking force. That is the skipping DJs fought with on bass-heavy 12"s. Add the cantilever's spring and damping to the contact model (compliance values per cartridge type: check). Lets a hot kick at 1 g throw the stylus, which the SKIP light should show.

**B2. Vertical depth limit at the cutter.** Out-of-phase bass moves the cutter up and down. Up is limited by leaving the lacquer, down by the lacquer thickness. The module currently lets any vertical swing through, so switching ELLIPTIC off costs nothing. Add an asymmetric limit on vertical groove displacement (check typical minimum and maximum groove depth, roughly 25 to 125 micrometres). Then ELLIPTIC off with wide stereo bass sounds broken in the way that made engineers use it.

**B3. HF LIMIT as a sliding filter.** The built limiter turns a fixed treble band down. The cutting-room units of the period worked more like a low-pass filter whose corner slides down on loud treble, which sounds softer and less like a de-esser (check behaviour of the Ortofon and Neumann units). Change the gain cell to a variable-corner low-pass driven by the same acceleration detector.

**B4. Vertical tracking angle error.** Only the horizontal arm-geometry error is modelled. The mismatch between the cutting angle and the cartridge's vertical angle (a few degrees is normal) adds 2nd harmonic to the vertical signal and is one of the larger stereo distortions. Add it with a fixed default of about 3 degrees, changed slightly by WARP.

**B5. IEC low-frequency roll-off.** Many 80s and 90s phono stages included the IEC amendment, an extra bass roll-off at 20 Hz. Offer it as the middle position of the SUBSONIC switch (OFF / IEC / ON).

**B6. Wear by wavelength.** WEAR is a fixed low-pass. Real wear removes the shortest wavelengths first, so it is worse at the inner grooves and on loud treble. Tie it to groove speed the same way as A2.

## C. Tone down or relabel (parts with weak physical basis)

**C1. Cutter coil heating.** The slow compression from "coil heating" at high HEAD settings is my least defensible piece. Real heads were protected by breakers and by the engineer; audible thermal compression is not a documented part of the sound. Suggest removing it, or keeping it at a third of the strength and saying so in the manual.

**C2. Cutter drive amp saturation.** Cutting amplifiers had large headroom and rarely clipped. Keep only a very high ceiling as a safety, and let B2 and the excursion limiter do the real limiting.

**C3. DJ mixer phono curve.** The "bass heavy, dull top" curve is a generalisation, not a measurement of any mixer. Keep it, but describe it in the manual as a typical budget stage.

**C4. Phono overload point.** The model clips the full pre-emphasised signal at one threshold. In real stages the overload margin varies with frequency and with the circuit. Acceptable as is; note it as a simplification.

## D. Constants to check before changing

- Cartridge inductance and resistance (DJ type is set to 600 mH / 1000 ohm; a well known DJ cartridge is nearer 720 mH / 630 ohm, check), tip masses, and recommended load capacitance.
- Null radii for Baerwald and Stevenson alignment, and the effective arm length (230 mm used).
- Groove pitch limits (0.04 to 0.25 mm used) and the pitch at which the cut level starts to drop (0.18 mm used).
- Pre-echo level range (-60 to -26 dB used).

## E. Leave alone

ELLIPTIC, the record/playback pair, the tracking-angle formula, the cartridge coil and load model, the eccentric-hole wow (correctly worse near the label), the warp model, side-length logic, the format presets, the panel and the controls. No panel changes are needed for A1 to A4; B5 changes one switch from two to three positions.

## Suggested order for Opus

1. A1, A2, A3, A4 together, then re-measure against the table above.
2. B1 and B2 (both make existing controls behave as they should).
3. B3 to B6, C1 to C3, and the manual updates.
4. Look up the D constants first where a source is available, and record the sources in the README.
