# Bulgaria

**Bulgaria Fable** by Haer. One VCV Rack module, 20 HP. Design by Fable, build by Opus.

Bulgaria harmonises one melody the way Bulgarian village choirs do. It takes the melody's pitch and gate and gives back up to eight voices: the melody, a drone, and voices that sit a deliberately mistuned second away from the melody so they beat and ring against it. **It makes no sound itself.** You patch its pitch, gate and push outputs into your own oscillators, envelopes and filters.

![Panel](panel.png)

## The idea in two minutes

The sound people know from *Le Mystère des Voix Bulgares* grew out of village singing, mostly from the Shope and Pirin regions of western Bulgaria. Its core is simple:

- **A drone.** One or more singers hold the tonic, usually in a loud chest voice, often without a break.
- **A melody that clashes with it.** The lead keeps landing one scale step from the drone or from another voice and stays there. To western ears that is a dissonance to be resolved; here it is the point.
- **Seconds that aren't tempered.** Singers pitch those seconds wherever they ring hardest, often well narrower than a piano's 200 cents. Reported values run from about 130 to 200 cents. The tunings in this module are estimates in that range, not measurements of one choir.
- **A bright, straight sound.** No vibrato, loud and nasal. Two close pitches then beat fast and hard. That beating is the "rub".
- **Ornaments.** *Tresene* is a throat shake: quick upward flicks on a held note. The *izvik* is a whooped leap upward at the end of a phrase. Notes are often scooped into from below.

Bulgaria turns each of those into a control.

## Quick start

1. Patch a sequencer's pitch to **V/OCT** and its gate to **GATE**.
2. Patch **POLY** (V/OCT) to a polyphonic oscillator, such as VCV VCO set to sawtooth.
3. Patch **P GATE** to a poly ADSR, and use it on a poly VCA after the oscillator. Mix the poly signal down (VCV Sum or Mixer).
4. Load the *Init (Shope Three)* preset. You now hear three parts: your melody retuned to the Shope mode, a voice rubbing a second under it, and a drone.

The rub rings best with **bright, loud oscillators with no vibrato**: sawtooth or pulse, filters open. Keep the voices at similar levels. For more, patch **PUSH** to filter cutoff or VCA level (recipe 2).

## Controls

### DRONE

| Control | What it does |
| --- | --- |
| ROOT | Drone note, C to B. Changes take effect at the next note. |
| OCT | Octave of the drone, -2 to +2. 0 puts the root in the octave above C4 (0 V). |
| MODE | The scale: Shope, Aeolian, Dorian, Phrygian, Hijaz, Mixolydian, Major. Each has an equal-tempered and a village tuning (table below). |
| TUNE | Fades from equal temperament to the village tuning. At 0 your melody passes through untouched, chromatic notes included; at 100% it is snapped to the mode and village-tuned. The added voices always use whole scale steps. CV: +10% per volt. |

### CHOIR

| Control | What it does |
| --- | --- |
| VOICES | How many voices, 1 to 8. Sets the channel count of the three poly outputs. CV: 0 to 10 V adds 0 to 7 voices. |
| SPREAD | Turns the open voices (octave drones and the fifth) into more close-rubbing voices: above 25% voice 8, above 50% voice 6, above 75% voice 4. Read at each note start. |
| Lights 1 to 8 | Green: the voice is sounding. Red: it is rubbing. Brightness follows PUSH. |

### RUB

| Control | What it does |
| --- | --- |
| RUB | How far the rubbing voices sit from the lead. In CENTS: 25 to 250 cents per step (default 170). In HZ: a beat rate of 1 to 80 Hz per step (default about 31). CV: +10% of the range per volt. |
| UNIT | CENTS or HZ (see below). |
| WHEN | RULE: the voices follow the tradition's rules on every note. CHANCE: the CHANCE knob decides, note by note, whether voice 2 rubs or sings a plain third. |
| CHANCE | How often voice 2 rubs in CHANCE. |
| CADENCE | What happens on a phrase's last note (marked by PHRASE). TIGHT narrows the rub to 60% over 150 ms and holds it: the clash gets harsher. RESOLVE closes the rubbing voices to a unison with the lead between 0.35 and 0.65 s into the note. |

**Cents or Hz?** A fixed width in cents sounds rougher as you go up: 170 cents beats at about 12 Hz on A2 (110 Hz) and about 49 Hz on A4 (440 Hz). In HZ, the voice is placed so the beating rate is the same in every register. Around 25 to 40 Hz is the harshest rub; under 8 Hz it becomes a slow throb; above 60 Hz it turns into a buzz. HZ assumes your oscillators track 1 V/oct with 0 V at C4 (VCV's standard).

### ORNAMENT

Each ornament has a knob that sets how often it fires by itself, and a trigger input below it that fires it at once.

| Ornament | What it does | Fires by itself |
| --- | --- | --- |
| SLIDE | The lead scoops up into the note from about 160 cents below; rubbing voices follow from 90 cents below, more slowly. | At note starts, with the knob's probability. |
| SHAKE | *Tresene*: three quick upward flicks of about 190 cents, 90 ms apart, on the lead. PUSH dips on each flick. | When a note has been held 0.3 s. Above 75% it repeats every 0.6 s. |
| WHOOP | *Izvik*: when the note ends, the lead leaps up nearly an octave in 170 ms with its gate held open and its push falling away. Voice 5 joins it when active. | Only on a cadence note (PHRASE), when the gate falls. |

A triggered whoop while the gate is still high leaves the whooping voices silent until the next note.

### SINGERS

| Control | What it does |
| --- | --- |
| LOOSE | Human singers. Scales slow pitch drift (about 9 cents RMS on each voice at 100%, half that on the lead), late entries (up to 35 ms), wander in the rub width (up to 12%), uneven ornament depths (up to 40%) and wander in PUSH. At 0 every voice is exact. CV: +10% per volt. |

### Inputs

| Jack | What it takes |
| --- | --- |
| V/OCT | The melody. |
| GATE | The melody's gate. A rise starts a note; a pitch change while it is high is a legato note. Unpatched, the gate counts as always on and every pitch change is a new note. |
| DRONE | V/OCT for the drone root, rounded to the nearest semitone. Overrides ROOT and OCT. |
| PHRASE | A trigger on the **last note** of a phrase marks it as a cadence. A trigger while the gate is low marks the next note. |
| RUB, TUNE, VOICES | CV for those knobs. |
| SLIDE, SHAKE, WHOOP (under the knobs) | Ornament triggers. |
| LOOSE CV (under the knob) | CV for LOOSE. |

### Outputs

| Jack | What it gives |
| --- | --- |
| LEAD, L GATE | The retuned melody alone (voice 1). |
| DRONE, D GATE | The drone alone (voice 3), available even when VOICES is under 3. |
| POLY | V/OCT of all voices, one channel each, lead and drone included. |
| P GATE | Gates of all voices, 0 or 10 V. The drone follows your gates. |
| PUSH | 0 to 10 V per voice. High while a voice is rubbing and leaning in, spiking on the whoop, dipping on each shake, 0 when the gate is low. Patch it to filter cutoff or level so the clashing voices push forward. |

## The voices

Every incoming note is snapped to the nearest step of the mode. Each voice is then placed by a rule, counted in scale steps from the lead. "Near side" means below the lead when the melody is at or above the root, above it when the melody is below the root.

| Voice | Name | Where it goes |
| --- | --- | --- |
| 1 | Lead | Your melody, retuned by TUNE. |
| 2 | Rub | One step from the lead on the near side, pulled to the RUB setting. In CHANCE it sometimes sings a plain third instead. |
| 3 | Drone | The root. When the lead lands on the root (not at a cadence), the drone drops one step so the clash never disappears. |
| 4 | Low drone | The root an octave down. Above 75% SPREAD: rubs 3 steps above the lead. |
| 5 | Upper rub | One step from the lead on the far side, so the lead is squeezed from both sides. |
| 6 | Fifth | A fifth above the root. Above 50% SPREAD: rubs 3 steps on the near side. |
| 7 | Under rub | Two steps on the near side, stacked on voice 2 into a three-note cluster. |
| 8 | High drone | The root an octave up. Above 25% SPREAD: rubs 2 steps on the far side. |

A rubbing voice starts each note on the plain scale step and closes to the RUB setting over about a tenth of a second, the way a singer finds the clash. When the lead is one step above the root, voice 2 lands a few cents or Hz off the drone, so it beats against the drone too.

Voices glide between notes (12 ms on the lead, 22 ms on the others), and jump without gliding after a rest of 50 ms or more.

## Village tunings

Cents above the root. TUNE fades from the first row of each mode to the second.

| Mode | Tempered | Village |
| --- | --- | --- |
| Shope | 0 200 300 500 700 900 1000 | 0 170 310 500 700 870 1010 |
| Aeolian | 0 200 300 500 700 800 1000 | 0 190 305 500 700 810 1000 |
| Dorian | 0 200 300 500 700 900 1000 | 0 195 300 500 700 890 1000 |
| Phrygian | 0 100 300 500 700 800 1000 | 0 125 305 500 700 815 1000 |
| Hijaz | 0 100 400 500 700 800 1000 | 0 130 385 500 700 810 1000 |
| Mixolydian | 0 200 400 500 700 900 1000 | 0 195 385 500 700 885 1000 |
| Major | 0 200 400 500 700 900 1100 | 0 200 390 500 700 890 1090 |

Shope and Dorian share a tempered scale; Shope is the strongly bent one.

## Presets

All presets have ROOT C and OCT 0. Change ROOT to suit your patch.

| # | Name | What it is |
| --- | --- | --- |
| 1 | Init (Shope Three) | Drone, lead, one rubbing voice at 170 cents. |
| 2 | Shope Duet | Two parts, tight rub, always whoops at phrase ends. |
| 3 | Pirin Drone | Aeolian, wider rub that resolves at cadences, loose. |
| 4 | Rhodope Wide | Mixolydian, wide seconds, lots of sliding. |
| 5 | Thrace Hijaz | Hijaz mode, narrow 130-cent rub, shaking. |
| 6 | Harvest Song | Heavy throat shakes and whoops, very loose. |
| 7 | Koutev Choir | Six parts, partly tempered, resolving cadences. |
| 8 | Mystere Eight | Eight parts, a little spread. |
| 9 | Tight Cluster | Eight voices all rubbing, 120 cents. |
| 10 | Squeezed Lead | Five parts: the lead rubbed from both sides. |
| 11 | Hard Rub 30 Hz | Beat rate held at 30 Hz in every register. |
| 12 | Slow Throb | 4 Hz beating, half-tempered. |
| 13 | Buzz 60 Hz | Beating so fast it buzzes. |
| 14 | Quarter-Tone Smear | 50-cent rub with spread: a smeared cluster. |
| 15 | Machine Choir | Tempered, exact, 34 Hz: no humans at all. |
| 16 | Techno Cluster | Eight voices at 34 Hz, whoops on phrase ends. For riffs over a kick. |
| 17 | Acid Diaphony | Two parts, 28 Hz, lots of slides: for 303-style lines. |
| 18 | House Stab Rub | Five parts, CHANCE 60%, resolving: for chord stabs. |
| 19 | Dub Drone | 8 Hz throb, CHANCE 40%, slides. |
| 20 | Coin Toss | Init, but voice 2 rubs on half the notes. |
| 21 | Mostly Sweet | Major, CHANCE 20%: mostly thirds, the odd clash. |
| 22 | Whoop Only | Lead alone, retuned, with all the ornaments. |
| 23 | Drunk Wedding | Hijaz, everything loose and ornamented. |
| 24 | Glass Unison | Tempered cluster with a 2 Hz shimmer. |

## Patch recipes

### 1. First patch

- Sequencer (VCV SEQ-3 or any) CV → **V/OCT**, gate → **GATE**.
- **POLY** → VCV VCO V/OCT. Use its SAW output.
- **P GATE** → VCV ADSR gate (poly). ADSR → VCV VCA CV; VCO SAW → VCA in.
- VCA out → VCV Sum → Audio.
- Bright, loud and steady makes the rub ring. Avoid vibrato and chorus.

### 2. Let the clash push forward

- As recipe 1, with a poly filter (VCV VCF) after the VCO.
- **PUSH** → VCF cutoff CV (attenuverter about +40%), and/or **PUSH** → a second VCA's CV.
- Rubbing voices open up as they lean in; the drone sits back; the whoop flares and falls.

### 3. Mark the phrase ends

- A trigger on the last step of your sequence → **PHRASE**. Many sequencers have an end-of-cycle output; otherwise use a gate sequencer with only the last step on.
- CADENCE TIGHT: the last note squeezes harder. RESOLVE: it settles into unison.
- Turn up WHOOP for the leap at the end of each phrase.

### 4. Move the drone

- A second, slow sequencer or a quantizer → **DRONE**. The whole harmony follows the new root from the next note.

### 5. Play the rub

- An LFO (VCV LFO, slow triangle) or an envelope → **RUB** CV. In CENTS the clash widens and narrows; in HZ the beating speeds up and slows down.
- Try switching UNIT while a low line plays: in CENTS low notes throb gently and high notes buzz; in HZ every note rubs the same.

### 6. Split the choir

- **LEAD** → one voice (say a brighter VCO), **DRONE** → a darker voice, low.
- **POLY** with VOICES 5 or more → a third poly voice for the cluster. The lead and drone are on POLY too, so leave that voice's channels 1 and 3 quiet with VCV Split / Merge if you want them only once.

### 7. Ornaments from triggers

- A random trigger source (Audible Instruments Bernoulli Gate on a clock) → **SHAKE**: shakes at random but in time.
- A sequencer's end-of-pattern output → **WHOOP**: a whoop every pattern.
- A clock divider → **SLIDE**: scoop into every fourth note.

### 8. Eight-voice cluster over a kick

- Preset *Techno Cluster*, ROOT A, OCT -1.
- A 16th-note sequence at 126 bpm → V/OCT, GATE; the last step of every second bar → PHRASE.
- POLY → poly sawtooth VCO, P GATE → short ADSR, PUSH → filter cutoff. Add a kick.

## Notes

- The tunings are estimates based on reported ranges for this singing, not measurements.
- Demo audio was made with a plain sawtooth stand-in synth. The tone in the demos is not the module's; Bulgaria only makes pitch, gate and push.
- Nothing here has been tested in Rack yet.
