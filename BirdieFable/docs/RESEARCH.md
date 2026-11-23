# Birdsong module: research and proposal

Designed on Fable, 2026-11-22. Status: proposal with sketches, waiting on Hardy's answers. Nothing is built.

## The idea in one paragraph

Model the bird, not the recordings. A bird's voice box (the syrinx) is a pair of small flapping membranes, one on each side, each driven by two things the bird controls: air **pressure** and membrane **tension**. Everything a bird sings is a short gesture in those two controls, shaped afterwards by the windpipe, throat and beak. The module runs that physical model live, and adds a small phrase generator that fires streams of gestures. Because it is a model and not a sample set, every control can be pushed past what a bird can do, which is where the synthetic material comes from.

## How it works

**1. The source (syrinx).** A well-studied two-equation oscillator from the birdsong physics literature (Mindlin and colleagues, University of Buenos Aires; the same model that fooled live zebra finches in playback tests). I ran it and mapped where it sings:

- PRESSURE above a threshold starts the sound. More pressure is louder, brighter and slightly sharper, so every note gets a natural pitch-and-brightness swell for free.
- TENSION sets pitch. High tension gives a pure whistle. As tension drops toward the edge of the singing range, the pitch falls fast and the wave turns spiky and rich in harmonics. That one edge is the difference between a robin and a crow, and it is why low bird sounds are harsh and high ones are pure.
- There are **two** of these, like the two sides of a real syrinx. Used together they give the two-note chords and clashing beats of thrushes and starlings.
- SIZE rescales time inside the model. It moves pitch exactly, so it can take a V/oct input and track properly. Small is a wren, large is a goose, very large is a bass voice no bird has.

**2. The tract.** Three cheap filters after the source: TRACHEA (a short tube that adds hollow resonances, long for ducks and geese), THROAT (a resonance that follows the pitch, which is how real songbirds get a clean whistle; turn the tracking off and it becomes a fixed formant) and BEAK (open is bright, closed is muffled, as in a dove's coo).

**3. Gestures.** One note (a "syllable") is a pressure envelope plus a tension move: slide up, slide down, arch, dip, or held. BUZZ adds fast wobble (50 to 300 times a second) for insect-like and raspy notes. BREATH adds noise to the air pressure, which roughens the tone the way real turbulence does.

**4. Phrases.** A seeded generator invents two to four syllable types and arranges them into a short stream: repeats (trills), alternations, a motif with a tail, or a run. Controls for how many notes, how fast, how much the phrase drifts in pitch and speeds up or slows down. Same seed, same phrase, so a good one can be kept.

## Sketches

In `sketches/`. All twelve come from the same model with different settings; none use samples. `spectrograms.png` shows them all.

| File | What it shows |
|---|---|
| 01-whistles | Clear slurred notes: down, up, arch |
| 02-trills | The same note repeated at 12, 25 and 40 per second |
| 03-chirps | Short sparrow-like chirps |
| 04-buzzes | Buzz at three speeds, then notes into a buzz |
| 05-two-voices | Both sides of the syrinx at once |
| 06-coo | Large bird, low tension, closed beak |
| 07-caw | Driven hard at the rich low-tension edge with noisy breath |
| 08-quack-honk | Pulse-like source through a long resonant windpipe |
| 09-generated-phrases | Eight seeds from the phrase generator, untouched |
| 10-stretched-large | Same seeds, bird eight times larger and three times slower |
| 11-held-drone | One note held for six seconds while tension sweeps into the rich edge and back; then two voices beating |
| 12-bass-bird | SIZE at 48: a 16th-note bass line at 124 bpm where every note is a syllable |

I checked these by spectrogram, not by ear. Pitch ranges land where the real birds are (whistles 3 to 5 kHz, trills 4 to 6 kHz, coo about 500 Hz). The caw and quack are the ones I trust least.

## Proposed module

Working name **Syrinx**. One module, mono voice.

- **Voice:** PRESSURE, TENSION, SIZE (with V/oct), SECOND VOICE (interval and mix), BUZZ rate and depth, BREATH.
- **Gesture:** LENGTH, SLIDE amount, SHAPE (down, up, arch, dip, held), ATTACK and RELEASE.
- **Tract:** TRACHEA, THROAT (amount and tracking), BEAK.
- **Phrase:** NOTES (1 to 32), RATE, PATTERN, DRIFT, ACCEL, VARY, SEED with a NEW button.
- **Stretch:** HOLD (freeze the current note as a sustained oscillator), TIME (slow every gesture down up to 16 times without changing pitch).
- **Inputs:** NOTE trigger (one syllable), PHRASE trigger (one stream), CLOCK, V/oct, CV for the main controls.
- **Outputs:** audio, plus the model's own PRESSURE and TENSION gestures as CV, so the bird's phrasing can play other modules.

Cost is low: sixty seconds of audio rendered in 1.5 seconds in the sketch, with the model run at 16 times the sample rate.

## Sources and limits

The oscillator equations are written from memory of Perl, Arneodo, Amador, Goller and Mindlin (2011) and Amador et al. (2013); I could not open the papers from here to check them line by line. I did verify the behaviour numerically: it starts at a pressure threshold, tension sets pitch from about 300 Hz to 7 kHz at bird size, and the wave becomes rich at the low-tension edge, all as the papers describe. Throat tracking follows Riede et al. (2006) on cardinals and Fletcher et al. (2006). The tract in the sketch is simplified and should be refined in the build spec.

The sketch source is `sketch-voice-model.c`.
