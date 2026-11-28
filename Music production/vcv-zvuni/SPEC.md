# Zvuni: module spec (draft)

*Zvuni* is Bulgarian for "it rings". That is what village singers say they are listening for when two voices rub a second apart.

A VCV Rack 2 voice module built around how Bulgarian women's singing behaves: a drone, a melody that clings close to it, a second voice tuned by ear for maximum "ring", bright open-throat tone, yelps at phrase ends, and limping (aksak) meters. It is not a sample player. Everything is synthesised so it is free to use on the EP.

Status: built as v2.0.0 (repo hardyfinn/zvuni). Decisions from the open questions: full three-voice synth plus all CV outputs; name Zvuni; the aksak clock is its own module (Aksak) in the same plugin, and Zvuni has an ACCENT input instead; 4 channels; white panel (32 HP). See README.md for the module as built.

---

## 1. The idea in one paragraph

Most "dissonance" modules let you set an interval in semitones or cents. Singers don't think that way. They move the second voice until the beating against the other voice feels strongest, and that sweet spot is a **beat rate** (roughly 25 to 50 beats per second in their range), not a fixed interval. As the pitch rises, the same beat rate means a *narrower* interval in cents. Zvuni's main trick is a second voice that tracks a target **roughness** instead of a target interval. That keeps the rub at the same intensity wherever the melody goes, and lets one knob, **Tension**, steer a whole arrangement from calm to tense and back.

---

## 2. Signal flow

```
            V/OCT DRONE ──► [Drone voice] ───────────────────────────┐
                                                                     │
 V/OCT MELODY ──► [Cling: pull toward drone] ──► [Melody voice] ─────┼──► MIX L/R
                                   │                                 │
                                   └──► [Partner tuner] ──► [Partner voice]
                                          ▲  Tension / Ring / Mode   │
                                          │                          │
 CLOCK ──► [Aksak clock] ──► accents ─────┴──► Resolve / Yelp ───────┘
```

Three voices, each a formant voice (section 4):

| Voice | Pitch comes from | Role |
|---|---|---|
| **Drone** | DRONE V/oct (normalled to the melody's lowest held note if unpatched) | Holds the ground. Usually the tonic |
| **Melody** | MELODY V/oct, after Cling | The tune |
| **Partner** | Calculated from the melody (section 3) | Sits a microtonal second above or below the melody and makes it ring |

---

## 3. The partner tuner (the heart of it)

### 3.1 How roughness is measured

Two tones of frequency *f1* and *f2* are roughest when their difference is about a quarter of the ear's critical bandwidth at that pitch. Zvuni uses the standard approximation:

- Critical bandwidth: `CB(f) = 25 + 75 × (1 + 1.4 × (f/1000)²)^0.69` Hz
- Peak roughness when: `|f2 − f1| ≈ 0.25 × CB(f)`

Worked examples (peak-roughness gap):

| Melody note | CB | Gap for peak ring | As an interval |
|---|---|---|---|
| A3 220 Hz | ~103 Hz | ~26 Hz | ~193 cents (almost a whole tone) |
| A4 440 Hz | ~113 Hz | ~28 Hz | ~108 cents (about a semitone) |
| E5 660 Hz | ~129 Hz | ~32 Hz | ~83 cents |

So the "ringing second" naturally gets narrower in cents as the singer goes higher. That matches what you hear in the recordings, and it is why tempered seconds sound a bit dead by comparison.

### 3.2 Controls

| Control | What it does | CV |
|---|---|---|
| **Tension** (big knob) | The macro. 0 = unison. Up to about 60% = the gap grows to peak roughness. 60 to 80% = past the peak towards a wide whole tone (rub fades, becomes "open"). 80 to 100% = glides out to a perfect fifth (fully open). Arrange a track by riding this one knob | ±5 V, attenuverter |
| **Ring** | Fine-tunes where "peak" sits, as a multiple of the quarter-critical-band (0.5x to 2x). Low = slow, wobbly beating. High = fast, buzzy, almost a new tone | 0 to 8 V |
| **Mode** (3-way) | **Hz**: partner tracks roughness (the singer). **Cents**: fixed interval set by Tension (tempered-style, for comparison). **Seek**: the partner slowly hunts around the target and settles, like a singer finding the spot. Seek speed is in the menu | – |
| **Side** (2-way) | Partner above or below the melody. "Below" is more typical of Shop-region songs | Gate in flips it |
| **Drift** | Small slow random wander of the partner's tuning (0 to ±25 cents) so it never sits perfectly still | 0 to 8 V |

### 3.3 Cling

Village melodies circle the drone in small steps and keep landing a second away from it.

| Control | What it does |
|---|---|
| **Cling** | 0 = melody plays exactly what comes in. Turning up pulls each note toward the nearest "ringing" position against the drone (using the same roughness maths), more strongly on accented aksak steps. Fully up, the melody can only ever land in unison, a ringing second, or the fourth/fifth |
| **Range** | Limits the melody to a span around the drone (a 3rd up to an octave). Notes outside fold back in. Village songs often stay within a fifth |

---

## 4. The voice (the tone)

Each voice is a small formant synth, not a filtered saw:

- **Source:** a glottal pulse oscillator (a pulse with a shaped open/closed phase, like vocal cords). **Open throat** (below) sets how sharp the closing edge is: sharper means more upper harmonics, brighter and more "chesty".
- **Formants:** three resonant band-pass filters (F1, F2, F3) per voice, set by a vowel table and morphed smoothly.

| Control | What it does | CV |
|---|---|---|
| **Vowel** | Morphs a → e → i → o → u (default sits on an open "a/e", the typical sound) | 0 to 8 V |
| **Open throat** | Brightness and push. Raises F1 a little, boosts F2/F3, steepens the glottal edge. Fully up is the hard, forward, slightly nasal belt | 0 to 8 V |
| **Nasal** | Adds an anti-resonance around 1 kHz and a narrow peak near 2.5 kHz. A little goes a long way | – |
| **Tresene** | The fast shake on held notes. Not a smooth vibrato: a quick, uneven pitch flutter (6 to 9 Hz) and a slight loudness flutter, which only starts after the note has been held a moment | 0 to 8 V |
| **Breath** | Noise mixed into the glottal source | – |

### Village ↔ Choir

| Control | What it does |
|---|---|
| **Ensemble** | Fully left = **Village**: one singer per part, tuning exactly as the tuner says, very dry and raw. Turning right adds singers per part (up to 6) with small differences in tuning, timing and vowel, and pulls the partner's tuning slightly closer to tempered, like a big choir. Fully right = the State-choir sound |

---

## 5. Phrase behaviour

### 5.1 Izvikvane (the yelp)

The upward vocal flip at the end of a phrase.

| Control / jack | What it does |
|---|---|
| **YELP** trigger in | Fires a yelp on the melody (and partner, if **Both** is on) |
| **Auto** switch | Also fires automatically when the gate ends after a note held longer than the **Hold** threshold |
| **Height** | How far it flips up: 3 to 12 semitones (default 7) |
| **Snap** | How fast: 20 to 150 ms. Short = glottal crack. Long = whoop |

On a yelp, the voice jumps up, the glottal edge sharpens for a moment, and the level drops away quickly. A short noise burst can be mixed in (menu).

### 5.2 Resolve (the cadence)

Phrases end by opening the rub to unison or octave.

| Control / jack | What it does |
|---|---|
| **RESOLVE** gate in | While high, Tension is overridden and glides to the **To** target |
| **To** (3-way) | Unison, octave, or fifth |
| **Glide** | 10 ms to 4 s |

Patch the aksak clock's **End of cycle** output to RESOLVE for automatic cadences.

---

## 6. Aksak clock

Built in, so the module can drive the rest of the rig with Bulgarian meters.

| Control | What it does |
|---|---|
| **Meter** | Presets: 5 (2+3), 7 (2+2+3) Rachenitsa, 7 (3+2+2), 9 (2+2+2+3) Daychovo, 11 (2+2+3+2+2) Kopanitsa, 13 (2+2+3+2+2+2), 15 (2+2+2+2+3+2+2), Custom |
| **Custom** | Up to 8 groups, each 2 or 3, entered on a small button row |
| **Mutate** | Chance per cycle that one group changes between 2 and 3 (keeping the meter "limping" but never the same). At 0 the meter is fixed. Good for the Autechre side of things |

Jacks: **CLOCK** in (16ths from Live via the disting NT), **RESET** in. Outputs: **Step**, **Accent** (start of each group), **Long** (start of each 3-group only), **End of cycle**.

Accents also feed Cling (stronger pull on accented steps) and can fire yelps (menu).

---

## 7. Jacks summary

**Inputs:** Drone V/oct, Melody V/oct, Melody Gate, Partner on/off gate, Clock, Reset, Yelp, Resolve, CV for Tension / Ring / Drift / Vowel / Open throat / Tresene / Ensemble.

**Outputs:**
- **Drone**, **Melody**, **Partner** audio (separate, for processing each in Live)
- **Mix L / R** (drone centre, melody and partner panned slightly apart; Ensemble widens it)
- **Partner V/oct** (to send the partner pitch to Mangrove, Just Friends or anything else)
- **Roughness** CV (0 to 10 V: how hard the voices are ringing right now)
- **Interval** CV (the current melody-partner gap, 1 V = 1 semitone)
- Clock outputs as in section 6

**Polyphony:** Melody V/oct accepts up to 4 channels. Each channel gets its own partner. Outputs are polyphonic to match.

---

## 8. Display

A small screen across the top, in the style of Tapejeans:

- A **roughness curve** for the current melody note: x = gap from unison to a fifth, y = how rough. A dot shows where the partner sits. You can see Tension walking the dot up the hill and over.
- A strip showing the **aksak groups** as boxes (2s narrow, 3s wide) with the current step lit.
- Text line: melody note, partner offset in cents, beat rate in Hz.

---

## 9. Panel layout (about 24 HP)

```
┌────────────────────────────────────────────┐
│  ZVUNI           [ display: curve + meter ] │
├──────────────┬──────────────┬──────────────┤
│  PARTNER     │  VOICE       │  PHRASE      │
│  (Tension)   │  Vowel       │  Height Snap │
│  Ring Drift  │  Open throat │  Auto  Hold  │
│  Mode  Side  │  Nasal Tres. │  To   Glide  │
│  Cling Range │  Breath Ens. │              │
├──────────────┴──────────────┴──────────────┤
│  AKSAK: Meter [2][2][3][ ][ ]  Mutate      │
├────────────────────────────────────────────┤
│  inputs row                  outputs row   │
└────────────────────────────────────────────┘
```

---

## 10. Patch ideas with your rig

1. **Tension as arrangement.** Hapax sends a drone note and a slow melody. Map Tension to a Live macro (CV Tools via the disting NT) and automate it across the track: verses near unison, the build climbing to peak roughness, the drop snapping out to a fifth with RESOLVE. The loop stays the same. The arrangement is the interval.
2. **Roughness as a modulation source.** Patch the Roughness output to a filter cutoff or to the kick's sidechain amount. The more the voices rub, the more the rest of the track reacts.
3. **Aksak hats over Detroit kick.** Kick stays on four from Live. Zvuni's clock at Rachenitsa (7) drives hats and Accent drives an open hat. Turn Mutate up slowly over the track.
4. **Partner to hardware.** Send Partner V/oct to Mangrove and the melody to Just Friends, both into Three Sisters in formant mode. Zvuni works as a tuning brain for the modular even with its own voices muted.
5. **Cents vs Hz A/B.** Switch Mode between Cents and Hz while playing a melody up an octave to hear why the singers do what they do.

---

## 11. Open questions for you

1. **Scope:** keep the three-voice synth, or make a lighter version that only outputs pitch CVs (tuner + clock) and leaves sound to your modular? The full voice is most of the work.
2. **Name:** Zvuni is a placeholder. Other options: *Diaphon*, *Shop Talk*, *Second Nature*.
3. **Clock:** built in (as above), or a separate small Aksak module that other modules can use too?
4. **Polyphony:** 4 melody channels enough?
5. **Panel style:** dark like Metroplex, or white like the Tapejeans variant?

## 12. Build notes (for later)

- Same toolchain and packaging as the other plugins (`.vcvplugin` for Mac arm64, plus an HTML guide).
- Roughness maths runs per sample-block, not per sample, so cost is low. Formants are three biquads per voice, so 3 voices × up to 6 ensemble singers × 4 poly channels is the worst case (about 216 biquads). Fine for Rack, but worth a CPU check. Village mode is much cheaper.
- Unit tests: partner gap matches the table in 3.1 within 1 Hz; Cents mode matches the interval exactly; aksak presets produce the right accent pattern.
