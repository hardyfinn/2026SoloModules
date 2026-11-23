# Gamelan instrument pairs: design

Three voice modules for the Irama plugin (2.1.0). Each is a **pair** of instruments, and each is wired to
take Irama's outputs without thinking. Design by Fable, build by Opus.

| Module | Pair | HP | Irama outputs it takes |
|---|---|---|---|
| **Ageng** | gong ageng + second gong (suwukan / lanang), plus kempul | 18 | GONG, KEMPUL (+ SARON or SELEH V/OCT for the kempul's pitch) |
| **Gangsa** | polos + sangsih players, every note an umbang + isep pair, kantilan doubling | 20 | POLOS, SANGSIH V/OCT + GATE |
| **Reyong** | two kettle-gong players (bonang barung + panerus / reyong low + high), plus kenong and kethuk | 22 | BONANG (or POLOS / SANGSIH for imbal), KENONG, KETHUK |

Shared rules: 0 V = C4. Stereo out. Dark teak / gold panels like Irama; control labels 3.2 mm, jack labels
2.9 mm; Hær logo. MIT. All sound is modal synthesis in one shared header, `src/BronzeDSP.hpp`, with no Rack
dependency so `test/` can render it offline.

## What makes bronze sound like bronze (the things the model must get right)

1. **Two-stage strike.** A mallet is in contact for 0.3-6 ms. Soft padded mallets (gong, bonang) = long contact,
   few high partials; hard wood or horn (gangsa, reyong) = short contact, bright, with an audible click.
   Model: each mode's strike gain is multiplied by `exp(-(f * contact)^2)` (a mallet low-pass), plus a short
   noise click whose level rises with hardness.
2. **Partials die at different speeds.** High partials decay several times faster than the fundamental. Each
   mode has its own decay: `T(f) = T0 * (f0 / f)^slope`, slope 0.6-1.1.
3. **Beating.** Nothing in gamelan is one pure tone. Gongs beat against themselves (two close modes); gangsa are
   built in detuned pairs. Model: every mode can be a **twin** (two resonators a few Hz apart).
4. **Pitch is not fixed during the note.** Bossed gongs start slightly sharp and settle; hard-hit bars do the
   same a little. Model: frequency scaled by `1 + glide * energy` where energy follows the mode envelope.
5. **Damping is a playing technique, not a release time.** Balinese players stop the previous bar with the hand
   as they hit the next. A damped note decays in 15-40 ms with a small dull thud. Model: per-voice `damp`
   target that multiplies every mode's decay, plus a tiny low thud on damping.
6. **No two sets are tuned alike.** A seeded per-bar tuning error (SET knob) makes the instrument a
   particular instrument. The seed is saved with the patch.

## Shared engine: `BronzeDSP.hpp`

```
struct Mode    { ratio, gain, decayMul, twinHz }      // static description of one partial
struct Res     { re, im }                             // complex one-pole resonator (state)
struct Voice   { Res r[MAX_MODES * 2]; float freq, vel, damp, energy, ...; }
```

- Resonator: `z = z * (g * e^{jw}) + x`, output `im(z)`. `g = exp(-1 / (T * sr))`. Coefficients are
  recomputed only at a strike, at a damp change, or every 32 samples while pitch glide is active.
- Strike: add `gain * vel^curve * mallet(f)` to `re` of each resonator (phase-coherent click), twins get the
  same kick so the beat starts in phase (loud) as on a real pair.
- Voice allocation is **by pitch** (a real instrument has one bar per note): a new note within 8 cents of a
  ringing voice re-strikes that voice; otherwise take a silent voice, else the quietest.
- Idle voices are skipped (energy below -90 dB).
- Soft limiter on the output: `x / (1 + |x| * 0.2)` scaled to about ±5 V at a normal strike, 8 V peaks.

## 1. Ageng: the gong pair + kempul

**Sound.** A gong ageng is a deep-rimmed bossed gong, fundamental 40-65 Hz. Its sound is: a soft thud, then
a slow swell (the fundamental takes 0.2-1 s to reach full level), a slow wave in the loudness (the ombak, 1-3
per second), and a decay of 10-30 seconds. The second gong (Java: gong suwukan; Bali: gong lanang) is smaller
and a few tones higher; the two alternate. The kempul is a smaller hanging gong, pitched to the melody, with
a shorter, clearer note.

**Modes** (ratios to the fundamental; gains; decay multipliers; twins):

| Partial | Gong | Gain | Decay | Twin |
|---|---|---|---|---|
| fundamental | 1.00 | 1.00 | 1.0 | OMBAK Hz |
| octave | 2.00 (+SIZE stretch to 2.04) | 0.55 | 0.6 | OMBAK x 1.6 |
| twelfth | 2.96 | 0.22 | 0.35 | - |
| double octave | 4.02 | 0.14 | 0.25 | - |
| shimmer | 5.4, 6.8, 8.3 | 0.08, 0.05, 0.03 (x BRIGHT) | 0.12 | - |
| rim growl | 1.48 | 0.10 (x hard strike) | 0.2 | 0.7 Hz |

Kempul: same table, fundamental 2-3 octaves higher, decay 4x shorter, bloom a quarter as long, growl off.

**Bloom.** The fundamental and octave are not struck directly: the strike fills a hidden store, which leaks
into the mode over BLOOM time (`x(t) = k * exp(-t / tb)` fed as continuous excitation, in phase with the
resonator). Result: amplitude rises as `1 - exp(-t/tb)` then decays. Upper partials are struck directly, so a
hard hit speaks at once and then the low swell arrives under it.

**Glide.** `f = f0 * (1 + 0.012 * GLIDE * energy)`: starts up to 20 cents sharp on a hard hit, settles in ~0.5 s.

**Controls**

| Control | Range | Does |
|---|---|---|
| TUNE | 32-98 Hz (C1-G2), default 49 Hz | first gong's pitch |
| INTERVAL | 0-12 semitones, default 3 | second gong above the first |
| SIZE | 0-1 | decay 6-30 s, octave stretch, shimmer density |
| BLOOM | 0-1 | swell time 0.02-1.2 s |
| OMBAK | 0-6 Hz, default 1.6 | the gong's own beating |
| MALLET | soft-hard | contact 6 ms to 0.8 ms, click, growl |
| BRIGHT | 0-1 | upper partial level |
| KEMPUL TUNE | ±12 semitones | offset on the kempul's V/OCT |
| KEMPUL RING | 0.5-8 s | kempul decay |
| ALT switch | OFF / ON | GONG input alternates first and second gong each hit (Balinese wadon / lanang) |
| LEVEL trims | gong, kempul | mix |

**Jacks.** In: GONG trig, GONG 2 trig, KEMPUL trig, KEMPUL V/OCT (unpatched = TUNE + 2 octaves), VEL (0-10 V,
all three), DAMP gate (hand on the gong: everything stops in ~0.3 s), TUNE CV (V/oct). Out: L, R (the two
gongs sit slightly left and right, kempul centre), plus a mono SUB out (fundamentals only) for sidechain or
a sub chain.

## 2. Gangsa: polos + sangsih, umbang + isep

**Sound.** Bronze bars over tuned bamboo tubes, hit with a hard wooden hammer. Very bright attack, a
fundamental made loud and long by the tube, partials near 2.7 and 5.2 that die in a quarter of a second, and
the fast shimmer of the pair. The other half of the sound is the hand: each note is choked as the next is
struck, which is what makes kotekan a crisp line instead of a wash.

**Modes per bar**

| Partial | Ratio | Gain | Decay |
|---|---|---|---|
| fundamental (+ tube) | 1.00 | 1.00 | 1.0 |
| tube ghost | 1.00 x (1 + tube mistuning ±0.4 %) | 0.25 x TUBE | 0.25 |
| 2nd | 2.71 | 0.30 x BRIGHT | 0.28 |
| 3rd | 5.15 | 0.14 x BRIGHT | 0.12 |
| 4th | 8.30 | 0.06 x BRIGHT | 0.06 |

Each note sounds **two bars**: umbang at the pitch, isep `OMBAK` Hz above (constant Hz, so high and low notes
shimmer at the same speed). If the V/OCT input is 2-channel (Irama with its OMBAK up), channel 2 is used as
the isep pitch instead and the knob is ignored for that input. KANTILAN adds the same pair an octave higher
with a shorter ring (0.6x), at the knob's level. So one note = up to 4 bars = 20 resonators.

**Damping.** DAMP knob 0 = bars ring (Javanese-like wash), 1 = fully choked. Damp trigger depends on the
switch: NEXT = a part's previous note is choked when that part strikes its next note (how it is played);
GATE = choked when the gate falls (so gate length plays the damping, for hand sequencing). Choke time
`400 ms -> 18 ms` as DAMP goes 0.05 -> 1; a soft 180 Hz thud at -30 dB accompanies a hard choke.

**Controls**

| Control | Range | Does |
|---|---|---|
| OMBAK | 0-12 Hz, default 6.5 | umbang / isep beating |
| KANTILAN | 0-1, default 0.4 | level of the octave-up pair |
| MALLET | 0-1, default 0.7 | hardness: contact 2.5-0.25 ms, click |
| DAMP | 0-1, default 0.75 | how hard the hand chokes |
| RING | 0.3-6 s, default 1.8 | undamped decay of the fundamental |
| BRIGHT | 0-1 | upper partials |
| TUBE | 0-1 | bamboo resonator: fundamental boost + hollow ghost |
| SET | 0-1 | per-bar tuning errors up to ±12 cents (seeded; right-click "New set") |
| WIDTH | 0-1 | polos left / sangsih right |
| DAMP switch | NEXT / GATE | see above |

**Jacks.** In: POLOS V/OCT, GATE; SANGSIH V/OCT, GATE; VEL (poly 2: ch1 polos, ch2 sangsih; mono = both);
DAMP CV; OMBAK CV. Both pitch inputs are also polyphonic beyond the ombak rule only if "Poly = chord"
is ticked in the menu (then each channel is its own note, for playing it from a keyboard or Peal).
Out: L, R, and POLOS, SANGSIH separately (so each can go through its own effects).
8 notes per part ring at once.

## 3. Reyong: two kettle players + kenong and kethuk

**Sound.** Kettle gongs: a pot with a raised boss, resting on cords. Struck on the boss it gives a clear pitch
with a strong partial near the octave and a hollow body tone; struck on the shoulder or rim it gives a clack.
Javanese bonang are played with padded sticks, mellow, each note damped by setting the stick back down.
Balinese reyong are played with cord-wound sticks, brighter, and use three strokes: open (*byong*), damped
(*byot*), and the wooden end on the rim (*cek*).

**Modes per kettle**

| Partial | Ratio | Gain (boss) | Gain (shoulder) | Decay |
|---|---|---|---|---|
| fundamental | 1.00 | 1.00 | 0.45 | 1.0 |
| body | 1.52 | 0.16 | 0.42 | 0.35 |
| octave | 2.02 (twin 1.2 Hz) | 0.42 | 0.30 | 0.55 |
| 3rd | 3.01 | 0.14 | 0.26 | 0.25 |
| 4th | 4.23 | 0.07 | 0.20 | 0.15 |
| 5th | 5.9 | 0.03 | 0.14 | 0.08 |

STRIKE knob crossfades the two gain columns (boss -> shoulder). The *cek* is not modal: a 3 ms band of noise
at 1.8-5 kHz plus two very short modes at 2.3 and 3.9x.

**Sympathy.** All the kettles share one frame. Each strike sends `SYMPATHY * 4 %` of its excitation into every
other voice that has been used in the last few seconds, so chords and fast figures leave a faint halo.

**Controls**

| Control | Range | Does |
|---|---|---|
| STYLE switch | JAVA / BALI | Java: padded stick, contact 3 ms, ring x1.6, B part = panerus (octave up). Bali: cord stick, contact 0.6 ms, ring x1, B part same octave |
| STRIKE | boss - shoulder | partial mix |
| MALLET | 0-1 | hardness around the style's contact time |
| RING | 0.2-5 s | open decay |
| DAMP | 0-1 | choke amount when damped; like Gangsa, NEXT / GATE switch |
| BYOT | 0-1 | chance that a stroke is played damped at once (Balinese *byot*), for instant variety |
| CEK | 0-1 | level of the rim click |
| SYMPATHY | 0-1 | frame resonance |
| SET | 0-1 | tuning errors |
| WIDTH | 0-1 | A left / B right |
| KENONG RING | 0.5-6 s | kenong decay |

**Jacks.** In: A V/OCT, GATE; B V/OCT, GATE (B normalled to A an octave up in Java style: one BONANG cable
plays barung and panerus together); CEK trig (rim click on both); BYOT gate (while high every stroke is
damped); VEL; KENONG trig + V/OCT (a large kettle, one octave below A's range, long ring; unpatched pitch
follows A); KETHUK trig (small flat kettle, always damped, "tuk": fixed pitch near A3, set by right-click).
Out: L, R, and COLO (kenong + kethuk alone).

## Patch: Irama into the three

```
Irama GONG ----------> Ageng GONG          Irama POLOS V/OCT, GATE --> Gangsa POLOS
Irama KEMPUL --------> Ageng KEMPUL        Irama SANGSIH V/OCT, GATE -> Gangsa SANGSIH
Irama SARON V/OCT ---> Ageng KEMPUL V/OCT  Irama BONANG V/OCT, GATE --> Reyong A
Irama KENONG --------> Reyong KENONG       Irama SARON V/OCT ---------> Reyong KENONG V/OCT
Irama KETHUK --------> Reyong KETHUK       Irama KENDANG -------------> Reyong CEK (Bali)
```

Leave Irama's OMBAK at 0 and use Gangsa's own (or turn Irama's up and Gangsa follows it).

## Variations worth trying once built

- Gangsa with DAMP at 0 and RING long, fed from Irama in Java style: a saron-like wash.
- Ageng's SUB out into a compressor sidechain on the kotekan: every gong breathes the mix.
- Reyong in Bali style with BYOT at 0.5 from Gatra's POLOS / SANGSIH triggers and a fixed pitch: a
  tuned percussion pair for techno.

## Tests (`make test`)

1. Every voice decays to silence (no runaway) after a 10 V strike at any setting corner.
2. Gangsa: measured beat rate of a struck note = OMBAK ±0.3 Hz at C4 and C6.
3. Gangsa: with DAMP 1, level 60 ms after the next note has fallen by more than 30 dB; with DAMP 0 by less than 6 dB.
4. Ageng: the fundamental's peak comes later than 0.5 x bloom time; decay time rises with SIZE.
5. Reyong: STRIKE at shoulder lowers the fundamental's share of the energy.
6. Output peak below 10 V for a full tutti.
