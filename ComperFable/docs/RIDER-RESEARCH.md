# Rider: research notes

Fable, 2026-11-24. What a jazz drummer does, what was measured, and why the kit is built the way it is.

## The player

A jazz drummer does five things at once, and the module has one layer for each: keeps **time** on the ride cymbal with the hi-hat on 2 and 4 and a bass drum felt more than heard; holds a **conversation** on snare and bass drum that repeats and varies in two-bar figures; **listens**, catching the pianist's pushes, staying off the pianist's other hits and answering when there is space; marks the **form** with set-ups at the ends of 4- and 8-bar phrases and into the top of the chorus; and **shapes** a whole performance by how hard all of that is played.

A first prototype worked from whole takes (`/mnt/project-files/jazz-drums-vcv/drummer.py`). With LISTEN at 1 it caught 10 of 16 pushes in the medium-swing preset and with LISTEN at 0, none. The reference player (`tools/proto/rider_ref.py`) does the same job knowing only what a drummer on the stand knows at the bar line: Comper's rhythm figure for the coming bar and the bass notes Walker has planned off the beat. Comper chooses its figure at the bar line, so nothing has to be predicted. In the 12 presets it catches between a half and four fifths of the pushes it is offered.

## The kit

Recordings: Virtuosity Drums (CC0), a jazz kit played with sticks, overhead microphones; Karoryfer Swirly Drums (free), brushes. Each voice was measured as energy in 26 bands and 8 time windows from 0 to 4 s, at three to five loudness layers.

**A cymbal is not a few hundred modes.** The first model was a bank of 70 partials found in the recording plus 420 modes spread at random above 2.6 kHz. It matched the band energies within 2 dB and still looked wrong on a spectrogram: thin lines ringing on by themselves. Two measures showed why. Take single 23 Hz bins and see how much each wobbles around its own decay: steady tones give 0 dB, noise gives about 5.6 dB. The recorded ride, bell and crash give 4.4 to 6.2 dB in every band from 1 kHz up; the modal bank gave 3. And the time-averaged spectrum inside a band is uneven by 3.5 to 6 dB in the recordings, against 9 to 19 dB for the modal bank. Above about 1 kHz a real cymbal is, to the ear and to these measures, noise with a faint fixed colour: the plate has thousands of modes and they trade energy all the time.

So every voice is three parts:

- **Partials**: the peaks that stand out when the spectra of all recorded takes are averaged (the wash averages flat; true partials are in the same place every time). Up to 70 for a cymbal, 2 to 16 for a drum. Each has a measured level and decay.
- **Wash**: one noise source with a fixed fine colour (itself plus 14 faint copies up to 22 ms late), split into 26 bands. Each band's energy over time was fitted as a mix of decays, and of decays that swell first (a crash blooms for 50 to 200 ms; snare wires and the room answer a stroke 20 to 60 ms late). In the module those are energy stores that each stroke tops up, which is also what makes ride strokes build on one another.
- **Click**: the stick, a few milliseconds, fitted to the first 4 ms.

Against the recordings the real-time engine's band-by-time energies differ by 2 to 4 dB on average per voice (the four recorded takes of one ride stroke differ among themselves by 1.4 dB). The wobble measure is 5.0 to 6.2 dB like the recordings, and the colour 3 to 6 dB.

Loudness is a table per band over the recorded layers, so a soft stroke is darker and has less click in the way the real one is. The brush sweep is the wash held open: its spectrum at four hand pressures, shaped by a gesture.

Hardy compared recorded kit and model by ear on 2026-11-24 (drums alone, ride alone, brushes alone, and in the trio) and approved it.

## What is left out

Hi-hat half-open positions, rim shots, buzz rolls, a second and third tom, mallets, and trading fours are not in this version.
