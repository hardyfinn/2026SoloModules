# Kraftwerk-style modules for VCV Rack: research and proposal

Fable, 2026-11-23. Design stage only. Nothing is built yet.

## The short version

I propose a plugin of **seven modules**, with three optional extras, built around the records from *Trans-Europe Express* (1977) to *Computer World* (1981). That is the stretch that fed straight into electro, hip-hop, Detroit techno and house, so it is the part of their sound most useful to the music you make.

| Module | What it is | Modelled on |
|---|---|---|
| **TAKT** | 16/32-step pitch sequencer with built-in interval quantiser and live transpose | their custom Synthanorma Sequenzer |
| **BLECH** | six analog drum voices played by triggers, with "pedal" CVs for dynamics, pitch and tone | the gutted preset rhythm boxes behind their metal drum pads |
| **ZAP** | synth percussion voice: the pinged, swept resonant filter | the Minimoog/sequencer "pew" drums of 1978 to 1981 |
| **VOKODER** | 20-band vocoder with voiced/unvoiced detection and its own carrier | Sennheiser VSM-201 class studio vocoders |
| **SPRECHER** | speech synthesiser: type words, clock them, sing them by V/OCT | Votrax phoneme synth and the TI speech-chip toys |
| **ORCHESTER** | polyphonic optical-disc choir, strings and flute | Vako Orchestron |
| **DYNAMO** | mono bass and lead voice with AGE and 12 presets | Minimoog and ARP Odyssey as they used them |
| *RASTER (optional)* | 8 × 16 trigger grid for the drums | the trigger side of their sequencer |
| *PHASE (optional)* | dual phaser for strings and choir | Schulte Compact Phasing A, Mu-tron Bi-Phase |
| *RECHNER (optional)* | toy beeps and boings | pocket calculator, Stylophone, toy rhythm box |

Ten rough sound sketches are in `sketches/`. Start with `00-gallery-all-together.wav`. One caution, as before: I cannot hear them. I checked them by measurement only, and they are quick prototypes of the ideas, not the final models. The speech one is the roughest.

The names are German working words (beat, sheet metal, speaker, and so on). None of the band's own names, song titles or studio name would appear on a panel or in the plugin.

## 1. What made the sound

What follows is from the sources listed at the end unless marked *(my recollection)*, which means I am confident of it but did not find it on a page I fetched today. I will check those before the build spec.

**Sequencer.** In 1976 they commissioned a custom sequencer from Matten & Wiechers of Bonn, the Synthanorma Sequenzer. It was built around quantised control voltages, a feature the maker called Intervallomat: each step lands on a semitone instead of a free voltage, so patterns stay in tune and can be shifted cleanly. The band started using it on *Trans-Europe Express* and it is most audible on *The Man-Machine*, where short, bright, resonant notes lock to the drums. Sources disagree on the step count (16 or 32). The characteristic move is a short repeating pattern that is transposed by hand while it runs, so one pattern becomes a whole chord progression *(my recollection)*.

**Drums.** There was no drum machine in the usual sense. Wolfgang Flür took a preset organ rhythm box (a Farfisa Rhythm 10, and later sources also name a Vox Percussion King and a Maestro Rhythm King), bypassed its preset patterns, and wired each drum circuit to a metal plate that was hit with a metal stick to close a contact. Ralf Hütter said the pads were not touch-sensitive, so dynamics, tone and pitch came from foot pedals. A drum pad patent was filed in 1975. By 1978 the same circuits, plus synth percussion, were being fired by the sequencer instead of by hand: "we would take drum boxes and re-design them with our engineers and electricians into a playable form, and adjust these with the sequencers" (Hütter, 1992). The voices in those boxes are simple: a short pulse rings a damped tuned circuit for kick, toms, bongos and claves, and gated filtered noise makes snare, hat and cymbal *(the circuit description is my recollection of that class of machine)*.

**Zaps.** The electro percussion of "Numbers" and *The Man-Machine* is largely synth percussion: a filter set near self-oscillation, hit with a trigger, with its cutoff swept down fast by an envelope *(my recollection)*. This is the sound Afrika Bambaataa and the whole electro genre borrowed.

**Voices.** Two separate families.
- *Vocoders.* Used from *Autobahn* onward. An auction of studio gear listed a Sennheiser VSM-201 as the type used on *The Man-Machine* and *Computer World*; sources also name EMS and Roland units. The VSM-201 is a 20-band design with a detector that swaps in noise for consonants, which is why its speech is so clear *(band count and detector are my recollection)*.
- *Speech synthesis.* A Votrax phoneme synthesiser is listed from the *Radio-Activity* period. On *Computer World* the voices came from a Texas Instruments Language Translator and a Speak & Spell, both built on TI's low-bit-rate speech chips. Later Florian Schneider patented the Robovox (1990), a speech chip played from a MIDI keyboard for pitch and length and a second keyboard for phonemes, so the machine could sing.

**Synths and keyboards.** Minimoog (the *Autobahn* bass and most basses and leads after), ARP Odyssey, EMS Synthi A. The Vako Orchestron, a keyboard that reads looped waveforms off a spinning optical disc, made the choir and strings of *Radio-Activity* and *Trans-Europe Express*. Polymoog, Prophet-5 and Korg PS-3100 arrive around 1978 to 1981. On *Computer World* they also played toys: a Casio calculator, a Stylophone and a Mattel toy rhythm box.

**Effects.** A Schulte Compact Phasing A and a Mu-tron Bi-Phase over synths and keyboards.

## 2. The proposed modules

All have text labels and short searchable names. Voices get the **AGE** knob from Sawstack and 12 presets each.

### TAKT (sequencer, about 30 HP)
- Two rows of 16 steps, each a pitch slider. Rows run in parallel (two voices) or chained to 32 steps.
- Each step has a three-way switch: PLAY, REST, TIE. A separate LAST STEP control per row gives odd lengths (5 against 16 and so on).
- **INTERVALL**: the built-in quantiser. Semitones, or a scale chosen by a 12-key mask. A RANGE switch sets slider travel to 1, 2 or 4 octaves.
- **TRANSPOSE**: a V/OCT input and a row of 12 buttons. A switch chooses whether a new transposition takes effect at once or waits for step 1. This is the one-pattern-many-chords trick.
- GATE LENGTH with CV, because the short clipped note is the style. ACCENT row with its own output for filter or VCA.
- Outputs per row: V/OCT, GATE, ACCENT. Clock, reset, run in. Eight pattern memories, selectable by CV.

### BLECH (drum voices, about 24 HP)
- Six voices: BASS, SNARE, HAT, CYMBAL, TOM/BONGO, CLAVE/BLOCK. Each has a trigger input (your "metal plate"), a TUNE and a DECAY knob, and its own output, plus a mix out.
- Modelled at circuit level: struck damped resonators (bridged-T networks) and transistor noise through fixed filters, including the small pitch drop and the click of the trigger pulse leaking through.
- Three **PEDAL** CV inputs acting on all voices, as on the real rig: DYNAMICS, PITCH, TONE. Per-voice accent inputs as well.
- BOX switch: three sets of component values after three different rhythm boxes, so the same six voices come in three flavours.
- AGE: drifting tuning, uneven levels between hits, noise floor, hum, tired capacitors (shorter decays, duller noise).

### ZAP (synth percussion, about 10 HP)
- One voice. A resonant four-pole filter pinged by the trigger, with START pitch, END pitch, SWEEP time, RESONANCE (ring length), a NOISE amount into the filter, and a CLICK level.
- V/OCT input so the sequencer can play it as a tuned drum. CV on every control.
- From a deep electro kick through toms and "pew" snares to tiny ticks. 12 presets. AGE.
- Polyphonic: a poly trigger cable gives several zaps at once, so one module can be a whole kit.

### VOKODER (about 26 HP)
- 20 analysis and synthesis bands, with the band filters modelled as the gentle analog bandpass pairs of a studio vocoder, not as perfect digital ones.
- Voiced/unvoiced detector that sends noise through for S, T, F and SH. SIBILANCE level and detector threshold.
- Built-in **CARRIER**: saw, pulse or a three-note chord, with V/OCT in (poly, so you can play chords into it). An external carrier input overrides it. It works with nothing but a voice patched in.
- FORMANT shift (move all synthesis bands up or down), ATTACK/RELEASE of the envelope followers, FREEZE (hold the current vowel, with a gate input), and 20 band-level sliders that double as a display.
- AGE: band mismatch, envelope ripple, leakage between bands, noise.

### SPRECHER (speech synthesiser, about 28 HP)
- A text field. Type words; each gate at STEP speaks the next word, and RESET goes back to the first. A WORD CV input picks a word directly, so a sequencer can order them.
- A built-in dictionary of roughly 300 hand-tuned words (numbers 0 to 20, letters, and a vocabulary of technical and everyday words) in English and German, with numbers also in French, Spanish, Italian, Japanese and Russian. Anything not in the dictionary can be typed as phonemes.
- Two engines. **FORMANT**: a phoneme synthesiser after the Votrax (a buzz and noise source through four moving formant filters, with that machine's stepped, slightly drunk transitions). **CHIP**: the same speech re-coded the way the TI chips did it, at 8 kHz with a coarse ten-coefficient filter updated 40 times a second.
- **SING**: V/OCT sets the pitch and gate length sets how long the vowel is held, which is the Robovox idea. With nothing patched it speaks in a flat monotone, with an INFLECTION knob.
- RATE, VOICE SIZE (formant scaling, child to giant), WHISPER (noise only), and a clean formant CV output that follows the first formant, for driving other filters.
- Everything is synthesised from rules. There are no recordings and no ROM data from the original chips.
- This is the riskiest module. Rule-based speech is hard to make intelligible, which is why I am proposing a hand-tuned dictionary first and free typing second.

### ORCHESTER (about 16 HP)
- Polyphonic. DISC selector: CHOIR, STRINGS, FLUTE, ORGAN, plus four more.
- Modelled as the real thing works: one continuous looped waveform per note, read by light off a spinning disc. So no attack and no decay beyond a slight key click, a narrow frequency range, a faint whir, level flutter at disc speed, and a slow wobble from the disc being off-centre. All notes on a disc share that wobble, which is why chords shimmer together.
- Controls: DISC, WOBBLE, TONE, ATTACK and RELEASE (added, since the original has none), OCTAVE.
- AGE: scratched and dusty disc, worse wobble, dim lamp (noisier), motor drift.

### DYNAMO (mono voice, about 22 HP)
- Two oscillators plus sub and noise, a ladder filter, two envelopes, glide. Deliberately plain.
- What earns it a place is the 12 presets, each a patch from the style (motorik bass, clipped sequencer pluck, sliding lead, hollow pulse melody and so on), and AGE.
- This is the module with the most existing competition in the VCV library. If you would rather drive Sawstack or another voice from TAKT, it can be dropped.

### Optional extras
- **RASTER** (20 HP): 8 rows by 16 steps of trigger switches with per-row mute, last step, and an accent row. Many grid sequencers already exist in Rack, so this only earns its place by matching the set.
- **PHASE** (12 HP): two six-stage phasers, series or parallel, each with its own sweep, with the second able to follow the first inverted. For ORCHESTER above all.
- **RECHNER** (8 HP): a small toy voice. Square beeps, a "boing", a key-press click, 12 keys as trigger inputs or V/OCT.

## 3. How they fit together

TAKT row A plays DYNAMO, row B plays ZAP as tuned percussion. A clock divider or RASTER fires BLECH. ORCHESTER holds chords through PHASE. SPRECHER counts on every bar, or is patched into VOKODER as the modulator so the robot voice takes the chord from ORCHESTER. The gallery sketch is a crude version of that patch.

## 4. Questions for you

1. **Era.** I recommend 1977 to 1981 as the centre, with the Orchestron brought forward from 1975. The alternatives are the earlier, looser sound (*Autobahn*, *Radio-Activity*: flutes, phasers, tape echo) or the mid-80s digital one (samplers, FM).
2. **Which modules.** I recommend the seven in the main table. Say which optional ones you want, and whether DYNAMO stays.
3. **Plugin name.** I recommend **Werkbank** ("workbench"). Others: Taktwerk, Elektrowerk, Mensch. Module names are also yours to change.
4. **Where it lives.** I recommend a folder `WerkbankFable` in 2026SoloModules on its own branch, like Sawstack and Tapeizer, since that repo is already set up. A set this size could also have its own repo, as Serge and Tudor do, if you create one.
5. **SPRECHER languages.** English and German in full, numbers in five more. Tell me if another language matters more.

## 5. Sketches

| File | Shows |
|---|---|
| `00-gallery-all-together.wav` | sequence, electro kit and vocoded counting together |
| `01-blech-rhythm-box-voices.wav` | BLECH: kick, snare, hat, tom, clave from resonator and noise models |
| `02-zap-electro-kit.wav` | a kit where snare and percussion are zaps |
| `03-zap-sweep-of-settings.wav` | ZAP at six settings, four hits each |
| `04-takt-sequence-transposed.wav` | one 16-step pattern, transposed every two bars |
| `05-orchester-optical-choir.wav` | ORCHESTER choir disc, four chords |
| `06-sprecher-counting-formant-voice.wav` | SPRECHER counting 1 to 4 in German and English at three pitches and rates |
| `07-sprecher-speech-chip-mode.wav` | the same through the CHIP engine |
| `08-vokoder-chord-carrier.wav` | VOKODER, speech onto a chord |
| `09-vokoder-mono-melody-carrier.wav` | VOKODER, speech onto a bass melody |
| `10-rechner-toy-beeps.wav` | RECHNER beeps and boing |

Made by `make_sketches.py` in the same folder.

## Sources

- Gearnews, "Kraftwerk: their legendary synths, sequencers and sounds": https://www.gearnews.com/kraftwerk-their-legendary-synths-sequencers-and-sounds
- FACT, "Kraftwerk gear: synths, drum machines" (2017): https://factmag.com/2017/06/24/kraftwerk-gear-synths-drum-machines
- Zoë Blade's notebook, "Kraftwerk" (cites Karl Bartos's book and Sound On Sound): https://notebook.zoeblade.com/Kraftwerk.html
- MusicRadar on the Florian Schneider studio auction: https://www.musicradar.com/artists/it-revealed-what-went-on-behind-the-doors-of-kling-klang-and-for-once-the-truth-is-almost-up-there-with-the-fiction-what-weve-learned-about-the-inner-workings-of-kraftwerks-mythical-studio-via-a-recent-auction
- CDM, "That time Kraftwerk invented electronic drum pads" (quotes Wolfgang Flür): https://cdm.link/kraftwerk-drum-chillout/
- Music Technology, Dec 1992, Ralf Hütter interview: https://www.muzines.co.uk/articles/arts-and-krafts/2378?theme=2
- Mixdown, "Gear Rundown: Kraftwerk Autobahn": https://mixdownmag.com.au/features/gear-rundown-kraftwerk-autobahn/
- Wikipedia, "Robotic voice effects": https://en.wikipedia.org/wiki/Robotic_voice_effects

Not yet done: a survey of what the VCV library already offers in each of these categories. I will do that at the spec stage and say where an existing module already covers the ground.
