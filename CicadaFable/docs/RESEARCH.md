# Cicada: research and proposal

Written 2026-11-22. Design stage only. Nothing is built yet.

## The short version

Cicadas, crickets, katydids and grasshoppers all make sound the same way at heart: a slow muscle drives a mechanical part that produces a rapid series of tiny impacts, and each impact rings a small resonant structure on the body. The insect cannot sing at 4,000 Hz with a muscle, so it uses a mechanism that multiplies one muscle twitch into many clicks, and a resonator that turns the clicks into a tone.

That gives one model for the whole family, with three layers:

1. **Song** (the nervous system): when the muscle fires. Pulses, grouped into chirps, grouped into phrases.
2. **Mechanism** (the instrument): what one muscle twitch does. A cicada's tymbal pops a few ribs in a row. A cricket's wing drags a scraper over a file of teeth.
3. **Body** (the resonators): what rings. The tymbal plate or wing membrane sets the pitch and purity. A cicada's hollow abdomen is a second resonator that the animal moves to shape the sound, like a mouth.

Sixteen audio sketches made with a first prototype of this model are in `sketches/`. Start with `00-gallery.wav`.

## What the research says

### Cicadas (tymbal)

Source: the Bennet-Clark and Young papers in the Journal of Experimental Biology (1992, 1995, 1997, 1998), mostly on the Australian greengrocer *Cyclochila australasiae*, plus Fonseca's work on European species.

- Each side of the abdomen carries a **tymbal**: a stiff domed membrane with a row of ribs. A large muscle pulls it inward and the ribs buckle one after another. Each buckling rib gives one sharp click. When the muscle relaxes the tymbal springs back with a quieter out-click.
- In *Cyclochila* each rib's click is timed to roughly one cycle of the carrier, so three or four ribs build up a coherent ringing pulse at about 4.3 kHz. Each later rib rings a little lower than the one before.
- The two tymbals alternate. Each muscle runs at about 120 Hz, so the song has about 240 pulses a second. Other species run from about 50 to over 500.
- The abdomen is mostly a hollow air sac. With the eardrums as openings it acts as a Helmholtz resonator tuned near the tymbal frequency. This is what makes cicadas so loud (over 100 dB at close range in the big species) and fairly pure in tone.
- The animal retunes that cavity by raising and lowering the abdomen and opening the covers under it. That is the "wah" in many songs: the Japanese min-min cicada's "miiin-min-min" is the abdomen moving, not the tymbal changing.
- Carrier frequencies run from about 1.3 kHz (the 17-year periodical cicada, nearly a pure tone) through 4 to 7 kHz for most large species, up to 10 kHz and beyond for small ones.
- Songs are long gestures: a swell over many seconds and a die-off where pitch and rate sag, ticks followed by a buzz, accelerating chirp trains that sink in pitch, or a long tone with a falling slur.

### Crickets (file and scraper, resonant)

- One wing carries a file of teeth, the other a scraper. Sound is made on the closing stroke only. Each closure is one **pulse**.
- An escapement, like a clock's, lets exactly one tooth go per cycle of the wing's resonance. That is why crickets sound like a pure whistle (the wing's "harp" has a Q of about 25) and not a rasp.
- Pulses group into chirps (field crickets: 3 to 5) or run continuously as trills (tree crickets).
- Everything speeds up with temperature, close to linearly. This is Dolbear's law: for the snowy tree cricket, chirps in 13 seconds plus 40 gives the temperature in Fahrenheit.

### Katydids and grasshoppers (file and scraper, not resonant)

- Same file-and-scraper idea, but in most katydids the teeth are not locked to a resonance and the wing is heavily damped. The result is a band of noise, often from 8 kHz to well past 20 kHz, in ticks, rasps and buzzes.
- Grasshoppers rub a row of pegs on the hind leg against a wing vein. It is noisier still, in phrases of rapid leg strokes that usually swell.

### Flying insects (wing)

Not song, but the same idea in its simplest form: one impulse per wingbeat, softly coloured by the body. Mosquitoes sit around 400 to 600 Hz, flies and bees around 150 to 250 Hz.

## What I measured

The usual recording archives (Wikimedia, xeno-canto, archive.org) are blocked from where I work, so I measured the 80 insect and cricket clips in the public ESC-50 research dataset, used for measurement only.

| Clip | Carrier | Pulse rate | Pulse length | Pitch glide in a pulse | Pattern | Timing wobble |
|---|---|---|---|---|---|---|
| Field cricket type | 4541 Hz | 25 per second | 20 ms | down 236 Hz (5%) | 5 pulses, then 175 ms gap | 4 to 8% |
| Field cricket type | 4821 Hz | 30 per second | 16 ms | down 289 Hz (6%) | 4 pulses, then about 500 ms gap | 11% |
| Tree cricket type | 3116 Hz | 64.6 per second | 7.6 ms | down 136 Hz (4%) | continuous trill | 2% |
| Trilling cricket | 3355 Hz | 43.7 per second | 13.7 ms | down 137 Hz (4%) | continuous trill | 1.5% |
| Slow ground cricket | 2343 Hz | 24.2 per second | 21 ms | down 159 Hz (7%) | 21 pulses, then 0.5 to 0.8 s gap | 3.7% |

Across the set:

- Cricket carriers cluster between 2 and 5 kHz, with a second group (small ground crickets and katydids) between 7 and 16 kHz.
- Pulses are on for almost exactly half of each cycle, with a symmetric rise and fall.
- Every cricket pulse slides down in pitch by 4 to 7% from start to end. This small glide is a big part of why a real cricket does not sound like a beeping sine wave, and the model includes it.
- Trillers are very steady (1.5 to 2% wobble). Chirpers are looser (4 to 11%), and the gaps between chirps wander by 10% or more.
- The clips tagged "insects" are nearly all flying insects, with wingbeat tones from 150 to 440 Hz.

The three cricket sketches are fitted to these numbers and re-measured to check they match. **The cicada, katydid and grasshopper sketches are built from the published descriptions and figures, not fitted to recordings**, because I could not reach clean recordings of them. If you can drop a few recordings into the thread I will fit those too before the build.

## The sketches

All in `/mnt/project-files/cicada-vcv/sketches/`, all from one engine (`cicada_proto.py`), with no samples.

| File | What it shows |
|---|---|
| 00-gallery | About five seconds of each of 01 to 13, in order |
| 01-field-cricket-chirp | Fitted to a real clip. Five-pulse chirps |
| 02-tree-cricket-trill-cool-to-warm | Fitted, then the temperature is swept from a cool night to a warm one |
| 03-ground-cricket-slow | Fitted. Low, slow, long chirps |
| 04-katydid-ka-ty-did | Unlocked teeth: harsh two and three part rasps |
| 05-meadow-katydid-ticks-and-buzz | Very high, ticks that speed up into a buzz |
| 06-grasshopper-phrases | Noisy leg strokes in swelling phrases |
| 07-dog-day-cicada-swell | 220 tymbal pulses a second, slow pulsation, swell and die-off |
| 08-periodical-cicada-pharaoh | The low pure 1.3 kHz tone with the falling slur at the end |
| 09-periodical-cicada-ticks-and-buzz | Ticks, then a shrill buzz that rises and falls |
| 10-evening-cicada-kana-kana | Chirp train that speeds up, sinks and fades |
| 11-min-min-cicada-talking-abdomen | The abdomen resonator opening and closing: "miiin-min-min" |
| 12-mediterranean-cicada-rasp | Dry rasping bursts, four or five a second |
| 13-wing-mosquito-then-bumblebee | The wing mechanism, two fly-bys |
| 14-stretch-cricket-hats-124bpm | Cricket chirps triggered on a 124 BPM grid as hi-hats, with a kick for context. Second half loosens the tooth lock so they turn raspy |
| 15-stretch-tymbal-formant-bassline | The tymbal with its pulse rate following notes and the carrier swept like a filter: an acid-style line from the same engine |

The last two are the "stretched" direction. Because the model is a pulse train ringing a resonator, it is also a formant oscillator when the pulse rate is raised into the audio range, and a tuned percussion voice when it is triggered.

## Proposed module

One module, name **Cicada**, around 20 HP, stereo out.

### Mechanism and body

| Control | What it does |
|---|---|
| MECH (3-way switch) | TYMBAL (ribs buckle in a row, in-click and out-click, two alternating sides), FILE (teeth across a scraper on the closing stroke), WING (one impulse per beat) |
| PITCH | Carrier frequency of the main resonator, about 100 Hz to 16 kHz. V/OCT input |
| PURE | From pure whistle to rasp. Raises resonator Q and locks the teeth to the carrier at one end. Frees and scatters them at the other. Cricket at one end, katydid at the other |
| TEETH | How many impacts per muscle twitch. Tymbal: 1 to 12 ribs. File: pulse length |
| BODY | Tuning of the second resonator (the abdomen) against the carrier |
| OPEN | Position of the abdomen, from closed and muffled to lifted and loud. This is the "wah". Has a CV input, and the song engine can move it by itself |

### Song

| Control | What it does |
|---|---|
| RATE | Muscle rate: pulses per second, about 2 to 500. A TRACK switch makes it follow V/OCT so the insect becomes a formant voice |
| CHIRP | Pulses per chirp, 1 to 32, then continuous trill at full |
| GAP | Silence between chirps |
| SONG | Chooses the long gesture: steady, swell and die-off, ticks then buzz, speeding and sinking, tone with falling slur, pulsating, and a few more |
| LENGTH | How long one song phrase lasts, from under a second to endless |
| TEMP | Night temperature. Speeds up or slows down every rate together and nudges pitch, following the measured near-linear law |
| LOOSE | From machine-perfect to a live animal: timing and loudness wobble at the measured amounts, dropped pulses, the odd pause |
| DISTANCE | Air and leaves: treble loss and a scatter of short reflections |

### Jacks

- Inputs: V/OCT, GATE (sings while high), TRIG (one chirp per trigger), CLOCK (chirps lock to a clock), and CV for PITCH, RATE, PURE, OPEN and TEMP.
- Outputs: L and R (the two tymbals or wings sit slightly apart and are slightly detuned, as on the animal), PULSE (a trigger per pulse), CHIRP (a gate per chirp), ENV (the loudness of the song as a CV). The last three let the insect drive other modules.
- Polyphonic: each poly channel is a separate individual with its own small differences, so a chorus is a poly cable away without building one into the panel.

### Presets

About 16 factory presets named by type (field cricket, tree cricket, katydid, dog-day cicada, pharaoh, evening cicada, min-min and so on) plus a handful of stretched ones (hats, formant bass, clock-synced shaker).

## Questions

1. **Which insects?** I took "similar insects" to mean crickets, katydids and grasshoppers. I would also include flying insects as the third mechanism, since it costs almost nothing. Say if you want it narrower.
2. **One animal or a chorus?** I recommend one animal on the panel, with poly channels giving a chorus when wanted.
3. **Musical extensions.** I recommend including clock sync, trigger input and pitch tracking of the pulse rate (sketches 14 and 15). Say if you would rather keep it a pure nature model.
4. **Does anything in the sketches sound wrong?** I cannot hear them, only measure them. The cicadas in particular are built from the papers, so your ear is the check.
5. **Home.** I assume a `Cicada` folder in the 2026SoloModules repo on its own branch, like the other recent modules.
