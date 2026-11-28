# House keys, revised: 100 sounds from Derrick May, Kevin Saunderson and Mr. Fingers

2026-11-21. Replaces the proposal in part 3 of `01-research-and-proposal.md`. The Korg M1 sounds are removed at Hardy's request. The Ensoniq SQ-80 and Mirage are added. Hardy will review the first 100 sounds and cut them down.

## 1. What the three of them actually used

Marked **[artist]** when the producer said it, **[press]** when a writer did, **[memory]** when it is my recollection and not confirmed by a page I could read.

### Kevin Saunderson
- First studio: Yamaha DX100, Roland Juno-106, Fostex eight-track [press, Music Technology, Sep 1988].
- By 1988: Roland S-550 sampler, Casio CZ-5000, Roland JX-8P, Korg Poly-800, TB-303, Ensoniq Mirage; TR-909, 808, 727 and Alesis HR-16 [press, same].
- "My bass lines really came from the Yamaha DX-100" [artist]. "I mainly used the DX100 for the bass that I came up with in Big Fun" [artist].
- "I use the Juno 106 a lot... it's very easy to program" [artist, 1988].
- "Usually I start by trying to program a bass sound." "If you use presets you just end up sounding like everybody else" [artist, 1988]. So his sounds are edited presets and his own patches, not stock ones.
- Reese bass ("Just Want Another Chance", 1988): made on the Casio CZ by changing oscillator and filter settings [artist]. He names the CZ-5000 in two interviews.
- "Good Life" (1988): CZ-5000 and TR-909 [press, Wikipedia citing DJ Mag]. The stab "started out from a sampled chord" with a string layer from a sampler [artist].
- "The S550's taken over from the Mirage" for strings, orchestra and bells [artist, 1988].
- Effects: Roland SRV-2000 reverb "a lot", Lexicons, Yamaha SPX90 [artist].

### Derrick May
- Shared equipment with Saunderson and Juan Atkins. Saunderson says May used the DX100 "a lot" [artist].
- Atkins's studio, where May learned and worked: Korg Poly-800, Yamaha DX100, Sequential Pro-One, Ensoniq Mirage, Akai S900, Korg SQD-1 sequencer [press, Music Technology, Dec 1988].
- "Strings of Life" (1987): piano sequence by Michael James, chopped and looped; strings "almost certainly" from an Ensoniq Mirage, layered with a similar synth sound for more attack; no bassline [press, Attack Magazine]. Hardy confirms the Mirage was May's sampler.
- "Nude Photo" (1987): DX100 [memory].

### Mr. Fingers (Larry Heard)
- "I bought a Roland Jupiter-6 synthesizer", and made "Mystery of Love" and "Washing Machine" the first night [artist, 2018 lecture].
- "Can You Feel It" (1986): Roland Juno-60 and TR-909, straight to cassette [press, Wikipedia citing Heard in Vice].
- Its chords are "an organ played like strings, long chords held and not stabbed" [press, DJ Pierre].
- The "Mystery of Love" bass was a patch he altered and never saved [artist]. So his sounds are programmed, not presets.
- Recording: two cassette decks, one pass each, parts played by hand [artist]. The tape is part of the sound.

**What this changes.** Only the DX100 sounds are true presets. Everything else these three made was programmed on a small, known set of machines. So the module should model those machines and ship patches programmed the way they were used.

## 2. Five engines

| Engine | Models | Why |
|---|---|---|
| **FM** | Yamaha DX100 family (4 operators, the chip and its converter) | Saunderson's basses, May's leads, Solid Bass and Jazz Organ. Plays real factory voice data. |
| **POLY** | Roland Juno-60/106, Jupiter-6, JX-8P: one or two oscillators, sub, resonant filter, the Roland chorus | Mr. Fingers's bass and chords, Saunderson's Juno-106 sounds. |
| **PD** | Casio CZ-5000 phase distortion, two lines detuned | Reese bass, "Good Life". |
| **WAVE** | Ensoniq SQ-80 / ESQ-1: three 8-bit wave oscillators, sync and AM, analog 4-pole filter, four envelopes, three LFOs | The SQ-80 sounds Hardy asked for. |
| **SAMPLER** | Ensoniq Mirage chain: 8-bit, low variable sample rate, no smoothing, analog 4-pole filter | "Strings of Life" strings and piano. Modelled sources played through the chain. |

Limits, stated plainly:
- The DX100 voices can be exact, because a voice is a short list of numbers and the chip is well understood.
- The SQ-80's 75 waves and the Mirage's disk samples belong to Ensoniq and cannot be copied into the module. The waves will be rebuilt by synthesis: very close for saw, square, pulse, sine, formant and band-limited waves, a little less exact for the sampled attacks (pick, bow, piano thump, slap). The Mirage piano and strings will be models, with the 8-bit chain doing the work of making them sound right.
- I cannot listen to the records here. Sounds named after a record are built from the documented machine and how it was used. If Hardy uploads short clips of the sounds that matter most, I can fit those by measurement, as I did with the steel pan and handpan.

Shared by all engines:
- Poly V/OCT, GATE and VELOCITY inputs, up to 16 voices, so it plays from a keyboard or a sequencer.
- A **STAB** mode: one key plays a stored chord moved in parallel, which is what a sampled chord does and why those stabs sound the way they do.
- A **TAPE** stage modelled on mixing to cassette (Heard) and eight-track (Saunderson, May).
- A small text display for the sound name, because 100 sounds will not fit as panel labels.

## 3. The first 100 sounds

### FM (30), DX100 factory voices under their own names
Basses: 1 Solid Bass, 2 SyntheBass, 3 Mono Bass, 4 Elec Bass, 5 Fretless, 6 SynthBass 1, 7 SynthBass 2, 8 Pluck Bass.
Organs: 9 Jazz Organ, 10 Ham n Eggs, 11 Club Organ, 12 6 Tease, 13 Clickorgan, 14 Drawbars, 15 Organ 1, 16 Organ 2.
Keys: 17 OldElectro, 18 NewElectro, 19 High Tines, 20 E Piano 1, 21 Pianobells.
Strings and voices: 22 Strings, 23 Richstring, 24 5th String, 25 Synvox, 26 Voices.
Synths: 27 Easy Synth, 28 Heavysynth, 29 Metal Keys, 30 Hard Brass.

### POLY (24)
After Mr. Fingers: 31 Feel Bass (round sub-heavy bass after "Can You Feel It"), 32 Feel Chords (organ played like strings), 33 Mystery Bass, 34 Mystery Pad, 35 Washing Blips, 36 Clouds Pad, 37 Soft Flute lead, 38 Glass Keys, 39 Deep Sub, 40 Slow Brass.
After Saunderson's Juno-106: 41 106 Bass, 42 106 Strings, 43 106 Stab, 44 106 Pad, 45 PWM Pad, 46 Sub Organ, 47 Hollow Lead.
JX-8P: 48 Soundtrack Strings, 49 JX Piano, 50 JX Deep Bass.
Poly-800: 51 800 Bass, 52 800 Strings.
Jupiter-6: 53 Sync Lead, 54 Jupiter Stab.

### PD (12)
55 Reese Bass, 56 Reese Single (one line, no detune), 57 Good Life Keys, 58 PD Brass Stab, 59 PD Electric Piano, 60 PD Organ, 61 Resonant Bass, 62 PD Strings, 63 PD Vibes, 64 PD Lead, 65 PD Clav, 66 PD Bell.

### WAVE (22), SQ-80 style
67 Digital Piano, 68 Wave Bass, 69 Square Bass, 70 Grit Bass, 71 Slap Bass, 72 Analog Strings, 73 Formant Voices, 74 Alien Choir, 75 Glint Pad, 76 Breath Pad, 77 Steam Pad, 78 Bell Keys, 79 Metal Chime, 80 Wave Organ, 81 Reed, 82 Wave Brass, 83 Wave Clav, 84 Sync Lead, 85 AM Bells, 86 Pluck and Pad, 87 Vibes, 88 Wave Electric Piano.
These are placeholders by type. At spec stage I will match them to the SQ-80's own factory programs, whose names I could not read today.

### SAMPLER (12), Mirage chain
89 Life Strings (hard attack, quiet sustain), 90 Sustained Strings, 91 Low-Rate Strings (10 kHz, dark and grainy), 92 Cello, 93 Life Piano, 94 Dark Piano, 95 Choir, 96 Brass Section, 97 Orchestra Stab, 98 Upright Bass, 99 Vibes, 100 Chord Stab (a sampled minor chord on one key).

## 4. Panel sketch

SOUND (stepped knob plus previous and next buttons, text display), ENGINE lights, OCTAVE, TONE, DECAY, VELOCITY, TWIN (second detuned unit), CHORUS, STAB, GRIT (converter and noise), TAPE, LEVEL. CV for SOUND, TONE, DECAY and TWIN. Stereo out. Each sound maps TONE and DECAY to what matters for that sound, for example modulator level on FM and filter cutoff on POLY.

Optional LOAD for 4-operator SysEx, reusing the FM4 loader from Fallen.

## 5. Size and cost

Five engines in one module is a large build, roughly the size of Tudor or Density. The engines never run at the same time, so the processor cost is that of one engine. Cutting sounds later is cheap. Cutting a whole engine later is also easy, since each is self-contained.

## 6. Still open

1. Is the five-engine plan right, or should any engine go?
2. Can Hardy upload short clips of the sounds that matter most, so they can be fitted by measurement?
3. Name: HOUSE KEYS, or something else?

## Sources added in this pass

- Kevin Saunderson, "The Techno Wave", Music Technology, Sep 1988: https://www.muzines.co.uk/articles/the-techno-wave/2291
- Juan Atkins, "Future Shock", Music Technology, Dec 1988: https://www.muzines.co.uk/articles/future-shock/2304
- Kevin Saunderson interviews: https://ask.audio/articles/interview-kevin-saunderson-on-his-relationship-with-tech-music , https://www.redbullmusicacademy.com/lectures/kevin-saunderson/ , https://daily.redbullmusicacademy.com/2017/05/interview-kevin-saunderson/
- Derrick May interview: https://daily.redbullmusicacademy.com/2017/05/interview-derrick-may/
- Attack Magazine, "Strings of Life" deconstructed: https://www.attackmagazine.com/technique/deconstructed/rhythim-is-rhythim-strings-of-life-derrick-may/
- Larry Heard lecture, 2018: https://www.redbullmusicacademy.com/lectures/larry-heard-2018/
- Larry Heard and Fingers Inc.: https://www.electronicbeats.net/larry-heard-and-fingers-inc-on-the-story-of-a-house-classic
- Wikipedia: "Good Life", "Big Fun", "Can You Feel It", "Mystery of Love", Ensoniq SQ-80, ESQ-1, Mirage
- Ensoniq SQ-80 review, Sound On Sound, Feb 1988: https://www.muzines.co.uk/articles/ensoniq-sq-80/3465
- SQ-80 spec sheet: https://soundprogramming.net/synthesizers/ensoniq/ensoniq-sq-80/
