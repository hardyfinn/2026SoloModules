# Vinyl: one VCV Rack module for the sound of a 90s record

Design stage, 2026-11-21. No crackle, no pops. Everything here comes from the physics of cutting, pressing and playing back a record, and the module's ten processes follow the signal in the order it really travelled.

## 1. The chain a 90s record went through

1. **Master tape or DAT arrives at the cutting room.** By the mid 90s most masters were digital (DAT, U-matic 1630, or early CD-R). They were often already loud and limited for CD, which is exactly what a lathe hates.
2. **Mastering for lacquer.** The engineer (on a Neumann VMS-70/80 or Scully lathe, SX-74 cutter head, Ortofon or Zuma electronics) has to stop the cutter from doing things that cannot be played back:
   - Bass that is out of phase between channels makes the stylus move up and down. Too much and the cutter lifts out of the lacquer or cuts through it, so bass is folded to mono below a crossover (the "elliptical equaliser").
   - High frequencies need huge cutter acceleration. Sibilance, hi-hats and bright synths can overheat the cutter head coils (they have fuses and helium cooling for a reason), so a fast HF limiter or de-esser sits in front of the cutter.
   - Groove width is set by the loudest bass. Long sides have to be cut quieter, with less bass, to fit. A 12" single at 45 rpm with one track a side could be cut hot and wide. A 25-minute LP side could not.
3. **The RIAA recording curve.** The cut is made with bass cut by about 20 dB and treble boosted by about 20 dB (time constants 3180 us, 318 us, 75 us). This keeps bass grooves narrow and lifts treble above the surface noise.
4. **The cutter head itself** has a feedback loop that flattens its response, but it still has a resonance and a top end that rolls off around 16-20 kHz. Lacquer also "springs back" slightly after the stylus passes, which softens the very top.
5. **Plating and pressing.** Lacquer becomes father, mother, stamper. Each step loses a little high-frequency detail. DMM (direct metal mastering, cut into copper) skips the lacquer and sounds tighter and brighter, with less pre-echo. Acetate dub plates, which 90s jungle and dub DJs played, wear audibly after 20-30 plays.
6. **Playback geometry.** A spherical or elliptical stylus is not the same shape as the cutting stylus, so it cannot follow a short wavelength exactly. That gives **tracing distortion**, mostly 2nd harmonic. It gets worse as the groove gets slower near the label, because the same frequency is packed into a shorter length of groove: the well known **inner groove distortion**. A pivoted tonearm also meets the groove at a slightly wrong angle across most of the side (**tracking angle error**), and the **pinch effect** squeezes the stylus up and down twice per cycle.
7. **Cartridge.** A moving magnet cartridge (Shure M44-7, Ortofon Concorde, Stanton 500 in DJ booths, Audio-Technica, Grado at home) is a coil with real inductance. With the cable and preamp capacitance it forms a resonant low-pass filter, giving a peak around 10-18 kHz and then a fall. The arm and cartridge also form a mechanical resonance around 8-12 Hz that reacts to warps and footfall. Stereo separation is only about 20-30 dB and falls off at high and low frequencies.
8. **Turntable.** Speed is never perfectly steady (wow and flutter). A hole punched slightly off centre makes the pitch swing once per revolution, 0.55 Hz at 33 rpm and 0.75 Hz at 45 rpm. Technics SL-1200 direct drive is very steady; a cheap belt drive is not.
9. **Phono preamp.** Undoes the RIAA curve: about +20 dB bass, -20 dB treble, plus 35-40 dB of gain. Real preamps never match the curve exactly. Passive RIAA networks with 5% parts tilt the response by a dB or two, cheap DJ mixer phono stages often had bass-heavy or rolled-off curves, tube stages soften peaks, and op-amp stages can clip on hot transients. Many add a subsonic filter.

The sound people call "vinyl" is the sum of these: mono, controlled bass; slightly softened and compressed treble; a gentle second-harmonic warmth that grows towards the label; a cartridge-shaped top end; and a pitch that is never quite still.

## 2. The ten processes

Order on the panel follows the chain: three CUT stages, one MEDIUM stage, six PLAY stages. Each has a bypass button so you can hear what it adds.

### 1. ELLIPTIC — bass to mono
Mid/side split. Below a crossover (40-300 Hz, default 150 Hz) the side signal is faded out, so bass becomes mono. The real effect is that stereo synth bass or reverb tails at the bottom collapse to the centre, and the low end gets more solid and focused.
Controls: FREQ, AMOUNT (how much side is removed), SLOPE (6/12/24 dB).

### 2. HF LIMIT — cutter protection
A fast limiter that acts only on the top end, the way a cutting room de-esser does. It measures the slope (velocity and acceleration) of the treble rather than its level, so sharp transients and sibilance are pulled down while sustained highs pass more freely. This is a big part of why records feel smooth on top and why hats sit back.
Controls: THRESH, FREQ (where the band starts, 3-12 kHz), RELEASE. Gain reduction light.

### 3. CUT — record curve, level and side length
Applies the RIAA recording curve (bass down, treble up) and drives a model of the cutter head: a resonance, a gentle rolloff above 16 kHz, and soft saturation of the cutter amplifier when pushed. A SIDE LENGTH control does what the engineer did for long sides: lowers the cut level, adds a high-pass and a peak limiter on the bass excursion. Short 12" single settings sound big and loud; a 25-minute side sounds smaller and politer.
Controls: LEVEL (hot cut), SIDE LENGTH (4-25 min), HEAD (cutter colour: clean or classic).
Note: the module always runs the record curve and the playback curve as a matched pair, so the overall tone stays flat unless the stages between them colour the sound. That is how the real chain worked, and it means every nonlinear stage in between (tracing, cartridge, preamp overload) acts on the pre-emphasised signal. That is the secret of the sound: distortion in the middle of the chain falls mostly on the treble, which the playback curve then pushes back down.

### 4. MEDIUM — lacquer, DMM or dub plate
Chooses what the groove is cut into and pressed from:
- **LACQUER**: normal pressing. Light HF softening from springback and three plating generations, faint pre-echo (a quiet copy of the signal about one revolution early, from groove walls deforming the neighbouring groove).
- **DMM**: tighter, brighter, almost no pre-echo.
- **DUB PLATE**: acetate. A WEAR control (plays) steadily takes off top end and adds more tracing distortion, just like a plate that has been rinsed all weekend.
Controls: TYPE switch, PRE-ECHO, WEAR.

### 5. TRACING — stylus shape and inner groove distortion
The core nonlinear stage. The stylus tip radius is compared with the recorded wavelength, which depends on frequency and on groove speed at the current radius. Where the wavelength gets close to the tip size, the model adds 2nd harmonic distortion and HF loss, and the pinch effect adds a little 2nd harmonic on the vertical (side) signal. Because it acts on the pre-emphasised signal, it softens and thickens treble without making the bass dirty.
Controls: STYLUS (spherical, elliptical, fine line), AMOUNT.
Driven by NEEDLE (see below).

### 6. TRACKING — arm geometry and mistracking
Tracking angle error from a pivoted arm: a small extra 2nd harmonic that is zero at two points on the side (the null points of the alignment) and larger between and beyond them. Skating force imbalance makes one channel slightly worse than the other. When the groove gets louder than the cartridge can follow (hot cut plus low tracking force), short bursts of harsh, buzzy mistracking appear on peaks. That is the "sibilance breakup" everyone heard on a worn needle.
Controls: FORCE (tracking weight, 1-5 g), ANTISKATE, alignment (Baerwald or Stevenson).

### 7. CARTRIDGE — coil, load and separation
A model of a moving magnet cartridge: coil inductance and resistance with the cable and preamp load capacitance, giving a resonant peak and fall in the top octave. Changing the load capacitance moves the peak, exactly as on a real phono input. Also sets stereo crosstalk (frequency dependent, worse at the frequency extremes) and azimuth error (a small time offset between channels that thins the mono sum at the top).
Controls: MODEL (DJ, home MM, MC), LOAD C (100-500 pF), SEPARATION, AZIMUTH.

### 8. ARM — low-frequency resonance
The arm and cartridge compliance form a resonant bump around 8-12 Hz. With warped records or bass feeding back from the speakers, it adds sub-bass movement and a slight swing in level. A WARP control adds a slow, once-per-revolution vertical wobble that excites the resonance.
Controls: RESONANCE (Hz), DAMPING, WARP.

### 9. PLATTER — speed, wow and flutter
Variable delay line modulated by: once-per-revolution eccentricity (off-centre hole) synced to 33/45 rpm, slow motor wow, fast flutter, and a PITCH control like the ±8% fader on a 1200 (with optional pitch-locked delay so it actually changes playback speed in feel, not just pitch). Drive types set realistic defaults: DIRECT (very steady), BELT (more wow), CHEAP (portable deck).
Controls: SPEED (33/45), DRIVE type, ECCENTRIC, WOW, FLUTTER, PITCH.

### 10. PHONO — the preamp
Applies the RIAA playback curve, then a preamp model:
- **TUBE**: soft symmetrical saturation and a touch of 2nd harmonic, slow recovery.
- **OPAMP**: clean until it clips, then hard.
- **DJ MIXER**: a typical 90s club mixer phono input: slightly wrong curve (bass a little heavy, top slightly dull), less headroom.
A TOLERANCE control shifts the RIAA corner frequencies the way 5% parts would, tilting the response a dB or two. SUBSONIC is a 20 Hz high-pass on or off. GAIN drives the input stage into overload.
Controls: TYPE, GAIN, TOLERANCE, SUBSONIC.

## 3. Two controls that tie it together

- **NEEDLE**: where the stylus is on the side, from outer edge to label. It sets groove speed for TRACING (much worse near the label), the tracking error null points for TRACKING, and a slight HF loss. Turn it by hand, send CV, or press PLAY SIDE and it moves slowly across the side over the SIDE LENGTH time, so a long patch really does get duller and thicker towards the end, like the last track on side B.
- **FORMAT** preset: 7" 45, 12" 45 (club single), LP 33, DUB PLATE. Sets speed, side length, cut level and medium in one go. You can then change anything.

## 4. Panel and I/O sketch

- Stereo in and out (mono input normalled to both channels).
- CV inputs for NEEDLE, PITCH, WOW, WEAR and the main AMOUNT controls.
- A main WET/DRY and an output level.
- Text labels, grouped CUT, MEDIUM, PLAY. Probably 24-28 HP to fit ten sections; an option is a compact main module plus a TECH expander holding the less used controls (load capacitance, tolerance, alignment, azimuth).

## 5. Not included on purpose

Surface noise, crackle, clicks and hiss. If you ever want them they can be added later as an optional input-level groove noise, since it also comes from the physics (vinyl compound, stamper grain), but nothing in this design depends on it.

## 6. Questions for Hardy

1. Name: a short one for Rack search. Ideas: **Lathe**, **Groove**, **Platter**, **Pressing**.
2. One wide module, or a main module plus a TECH expander?
3. PLAY SIDE auto needle sweep: keep it?
4. Keep all ten, or swap any for something else (for example a separate DJ mixer stage, or optional groove noise)?
5. Repo: a new one, or a folder in 2026SoloModules?
