# Joanna: design

"The old joanna" is rhyming slang for the piano in the front room or the pub.
Joanna is a physical model of one particular upright piano, with its tuning
history, its wear, its age, and the room it stands in. The target is the sound
of a home piano for jazz and gospel, not a concert grand: character first.

Hardy's four words steer everything: **non-perfect tuning, a home piano, an
older piano, a played piano.** Each gets its own control and its own physics.

Plugin: folder `Joanna/`, slug `Joanna`, version 2.0.0, MIT, about 34 HP,
stereo, 16-channel poly in. Conventions as Rimbaud (engine header in SI units,
`make test`, `make_panel.py`, `haer_logo.py`, presets header, guide).

---

## 1. Architecture at a glance

```
keys (poly V/OCT, GATE, VEL)
   -> 88 fixed keys, each with its own hammer, strings, damper, quirks
   -> hammer (nonlinear felt, implicit contact) <-> strings (modal, coupled unisons)
   -> bridge force bus  <->  sympathetic strings (all undamped strings listen)
   -> soundboard + cabinet (modal bank, lid, wall)
   -> room (image-source early reflections + FDN tail), two mics by DISTANCE
   -> L / R        (+ DRY: soundboard only, mono)
mechanical noises (key thump, key return, damper felt, pedal) -> soundboard / air
EXT in -> bridge bus (sing into the piano with the pedal down)
```

Everything that makes this piano *this* piano comes from one **serial number**
(a seed). The seed fixes, for each of the 88 keys: tuning error, the detuning
of each string in its unison, hammer hardness and wear, strike point, key
weight (velocity gain), damper quality, noise level, and which few notes are
"bad". Same serial = same instrument every time. A button builds another one.

## 2. Strings (modal, not waveguide)

Modal synthesis like Rimbaud and Gongoozler: every partial of every string is
a complex one-pole resonator `z <- z * exp((-sigma + i w) dt) + c * F`.
Exact frequencies and decays, unconditionally stable, SIMD friendly, and the
hammer can meet an already-moving string properly (repeated notes differ).

**Keys.** n = 0..87 (A0..C8), `f_et = 440 * 2^((n-48)/12)`.
Unisons: n < 10 one wound string, 10..27 two wound strings, n >= 28 three
plain strings (the break moves by a few keys with SIZE).

**Inharmonicity** (stiff strings): `f_k = k f0 sqrt(1 + B k^2) / sqrt(1 + B)`.
- wound bass, n < break: `B = 6e-4 * sizeFac * exp(-0.07 n)`
  (sizeFac 2.0 spinet .. 0.6 tall upright; short bass strings are the sound of
  a small piano)
- plain, n >= break: `B = 2.2e-4 * exp(0.073 (n - 28))` (about 1.6e-2 at C8)
The jump in B at the break is kept: it is the tonal break every upright has.

**Stretch tuning** is not a knob. A tuner's octaves are computed from B:
start from the equal-tempered octave F3..F4, go up matching 2:1 (partial 2 of
the lower note = partial 1 of the upper), go down matching 6:3. This gives the
Railsback curve of this piano automatically (roughly +25 cents at the top,
-15 to -30 at the bottom, more for a spinet).

**Partial count.** Up to `min(fmax(n), 0.45 fs)`, fmax 5 kHz in the bass
rising to 11 kHz in the treble, capped at 72 partials (single), 48 (bichord),
36 (trichord). Old strings and soft hammers leave little above that.

**Losses.** Internal + air: `sigma_int(n,k) = s0(n) + s1(n) f_k^2`, slow
(T60 of the fundamental about `28 * 2^(-n/18)` s). Bridge loss is what makes
the prompt sound: a target prompt T60 of `9 * 2^(-n/16)` s (A0 9 s, C4 1.7 s,
C8 0.2 s), shaped per partial by the soundboard's driving-point admittance
`Y(f)` from the *same* mode table used for the soundboard (section 4). So a
partial sitting on a soundboard resonance is louder and dies sooner, and
neighbouring notes differ: the warm unevenness of a real instrument, for free.

**Unisons: beating and the two-stage decay (Weinreich).** For each partial
the S strings of a key are coupled through the bridge. Build the S x S complex
symmetric matrix

```
M = diag(w_j - i sigma_int)  +  (-i g e^{i theta}) * ones(S,S)
w_j   = 2 pi f_k (1 + detune_j)          string j's own tuning
g     = bridge loss per string from Y(f_k), theta = its reactive angle
```

and take its eigenvalues/eigenvectors when the key's table is built (not at
audio rate). Each eigenmode is one resonator: frequency Re, decay -Im,
excitation weight `v^T h` (h = how hard the hammer meets each string),
output weight `v^T 1` (force on the bridge). Results that fall out of this
with no extra code:
- in-tune strings: one loud fast mode + quiet slow modes = prompt sound then
  aftersound (two-stage decay);
- mistuned strings: beating, slower for small detune, and once detune exceeds
  the coupling the strings decouple into a honky-tonk chorus;
- an uneven or worn hammer (h not equal) feeds the slow modes: longer singing
  aftersound, exactly as on real instruments.
Single bass strings use S = 2 as the two polarisations (vertical couples to
the bridge, horizontal barely; hammer gives the horizontal about 12 %, split
0.02 to 0.1 %), which gives the bass its two-stage decay.
To save CPU the full S modes are kept for the first 16 partials, 2 modes
(fast + strongest slow) up to 28, 1 above.

**Phantom partials (longitudinal).** Per voice: `s = sum of the first 12
partial signals weighted by k` (string slope), `L = highpass(s^2)`; add `L`
at a low level and also drive one resonator at the string's longitudinal
mode (about 14 to 18 x f0 in the bass, fixed by string length, Q about 60).
Level goes as amplitude squared, so it only speaks on hard playing: the
metallic clang of short wound bass strings struck hard. Notes below C5 only.

## 3. Hammer, action, dampers, pedals

**Hammer.** A mass with a nonlinear felt spring and hysteresis
(Hunt-Crossley): `F = K x^p (1 + mu dx/dt)`, x = felt compression.
m: 9.5 g (A0) to 4.5 g (C8). p: 2.2 to 3.0. K set per key so the contact
time at 2 m/s is `4 ms * 2^(-n/29)`. Hammer speed from velocity:
`v = 0.3 + 4.7 vel^1.6` m/s.
Contact is solved implicitly, as Rimbaud's stick: the modes' response to this
sample's force is linear (`y = y_free + G F`, G precomputed per key), so one
scalar Newton solve (3 iterations) per sample of contact. The hammer is thrown
(free flight), may bounce and re-contact, and is caught after it leaves.
- Strike point x0/L: 0.12 below key 60 tapering to 0.07 at the top, with
  per-key scatter. Excitation of partial k ~ sin(k pi x0/L).
- Worn hammers have a flat, grooved face: contact width w spreads the blow,
  multiply excitation by sinc(k pi w / L).
- HAMMER moves K and p together: soft, voiced felt ... hard, compacted ...
  tack piano at the far end (very high K, tiny mass added, slight bounce).
- Loud notes are brighter because the felt is nonlinear, not because of a
  filter. This is the main expressive axis and must be checked (section 8).

**Soft pedal (upright half-blow).** Hammers rest closer to the strings: speed
x 0.6, and the key travel gets lost motion (the first part of the key does
nothing), so key thump stays while the note gets quiet. Not a una corda.

**Felt (practice pedal).** A felt strip between hammers and strings: a second,
much softer spring in series (K_felt low, p 1.6, strong hysteresis) plus the
strip's own faint rustle. This is the "felt piano" sound: round attack, almost
no hammer brightness, close and intimate, action noise proportionally louder.

**Dampers.** On key release the damper felt lands over about 15 ms (treble)
to 80 ms (bass, heavy strings take longer and growl briefly). Its loss on
partial k is `d_k = d0 (floor + sin^2(k pi xd / L))`: partials with a node
under the felt survive a moment longer, the little "zing" of a released note.
Keys from about G6 up have no dampers at all and always ring. Per-key damper
quality from the seed: with AGE some leak (never fully stop the string).

**Sustain pedal** is continuous (0 to 10 V, half-pedalling works): all dampers
lift. Lifting drags felt off every string: a soft broadband whoosh into the
sympathetic strings, plus the pedal's wooden knock. Releasing drops all
dampers: a dull thud and a brief choked shimmer.

**Sympathetic resonance.**
- Sounding or held keys: every undamped voice also listens to the bridge bus
  (one sample late, small gain). Hold a key silently (gate high with VEL at
  0 V = hammer never reaches the strings), strike the octave below staccato,
  and the held strings sing. Real, and free with the modal strings.
- Pedal down, strings nobody played: a sympathetic bank of about 176
  resonators (2 partials for each of the 88 keys, at this piano's actual
  tuning and stretch) driven by the bridge bus, gain following the pedal.
  Gives the pedal-down halo, and its pitch content is this piano's own
  detuned tuning, which is a large part of an old upright's bloom.
- EXT input feeds the same bus.

**Mechanical noise** (level by NOISE, raised by WEAR, each excites the
soundboard bank so it sounds like wood, not like a sample):
- key bottoming on the keybed: low thump, timed just before the hammer lands,
  grows with velocity;
- hammer return and key return on release: a lighter double knock;
- damper felt landing; pedal knock, whoosh and thud as above;
- with WEAR: loose key bushings (a small click), and an occasional squeak of
  the sustain pedal on slow presses.

## 4. Soundboard and cabinet

Bridge force -> a modal bank of 48 modes, about 65 Hz to 3.2 kHz: the lowest
8 placed deliberately (first mode 70 to 115 Hz depending on SIZE, then typical
upright spacing), the rest at rising density with seeded jitter; Q 15 to 40.
Two sets of output weights give a stereo pair. Above 3 kHz the board is dense
and behaves like a smooth filter: a direct path with a gentle shelf.
The same table gives `Y(f)` for the string losses in section 2.

**AGE on the board:** an old board has lost crown: less bass coupling, lower
Q, and a weak "killer octave" (about C5 to C6: shorter sustain, thinner).

**Cabinet.** An upright is a box. LID, three positions:
- closed: boxy (a few cabinet air modes 180 to 600 Hz, top end shelved down);
- top open: brighter above, hammers audible;
- front off: the panel removed as for recording: brightest, most action noise.
**WALL:** distance of the back from the wall, 3 to 60 cm: the wall reflection
is a short delay off the back of the board (comb + bass lift when close, open
and even when pulled out).

**Rattle.** With WEAR above about half, one or two things in the house buzz
along with certain notes (a picture frame, a loose panel): band-pass at a
seeded frequency -> a small one-sided collision (as Rimbaud's wires) -> buzz.
Rare, quiet, and very "home". Off below half WEAR.

## 5. The room

A shoebox room by image sources: first-order reflections off four walls,
floor and ceiling plus the strongest second-order ones (12 taps), each with
its own absorption filter, feeding a spaced stereo pair by time and level.
Then an 8-line FDN tail sized to the room, RT60 and tone from FURNISH.

- **ROOM**: box room (2.6 x 3 m) ... front room (3.8 x 4.5) ... school or
  church hall (9 x 14 x 5 m, for gospel).
- **FURNISH**: carpet, curtains and a sofa (RT60 0.25 s, dull) ... bare boards
  and plaster (bright, lively, up to about 1.8 s in the hall).
- **DISTANCE**: one knob through four places:
  inside the piano (mics over the hammers: felt, action, wide) ->
  the player's seat -> across the room -> the next room (through the door:
  only the room's sound, low-passed, the piano you hear from the kitchen).

## 6. The three "life" controls, plus tuning

| Control | Hardy's word | What it changes |
|---|---|---|
| **TUNING** | non-perfect pitch | Time since the tuner came: 0 (yesterday) .. 20 years. Per-key pitch error (0.3 -> 9 cents rms), unison spread (0.4 -> 7 cents), a seasonal curve across the keyboard (middle goes its own way from bass and treble as the board moves), overall sag flat (0 -> -35 cents), and past about 8 years a few keys get one wild string. |
| **UNISON** | (honky-tonk) | Extra unison spread on top, 0 .. 25 cents: bar-room piano without waiting 20 years. |
| **WEAR** | a played piano | Hammers hard and flat where the piano is played most (middle 3 octaves), uneven voicing key to key, uneven key weight (each key answers velocity a little differently), loose action noise, timing lost-motion scatter of 0 to 6 ms, rattles. |
| **AGE** | an older piano | Dead bass strings (high partials of wound strings die fast: tubby), duller plain strings, tired soundboard (less bass, short killer octave), leaky dampers, lower overall sustain. |

Right-click: **Pitch** (A = 440 / 435 old pitch / 432 / follow FINE knob),
**Keys** (fixed keys, default: V/OCT snaps to the nearest key, because the
tuning lives in the piano; or free pitch: the fractional part retunes that
key, for microtonal patches), **Another piano** (new serial), presets.

## 7. Panel (about 34 HP)

Rows of knobs (control labels 3.2 mm, jack labels 2.9 mm, Hær logo on the
rail), sections banded as Rimbaud:

- PIANO: SIZE, HAMMER, TOUCH (velocity curve / default velocity), SUSTAIN
  (overall ring), SYMP, NOISE
- LIFE: TUNING, UNISON, WEAR, AGE, FINE (+-50 cents), [ANOTHER] button with a
  small serial-number plate ("No. 4127")
- HOUSE: LID (3-way switch), WALL, ROOM, FURNISH, DISTANCE, LEVEL
- PEDALS: three latching buttons with lights, SUSTAIN, SOFT, FELT, each with a
  jack (sustain continuous)
- IN: V/OCT, GATE, VEL (poly, 16), HAMMER CV, TUNING CV, DISTANCE CV, EXT
- OUT: L, R, DRY

## 8. Voices and CPU

- Key-indexed voice pool of 24 string sets (16 gates, but pedalled notes keep
  ringing). Restriking a sounding key reuses its strings. When full, steal the
  quietest released one.
- Per-key tables (eigenmodes, G, weights) rebuilt lazily at the next strike of
  that key after a life/tuning control moved: strings cannot be retuned while
  ringing, and neither can these.
- float_4 SIMD over modes, block of 8 or 16 samples for free-running modes,
  per-sample only during hammer contact (a few ms).
- Modes below -90 dB are switched off; voices compact; idle engine sleeps.
- Budget: 16 voices mid-keyboard ringing under 10 % of a core; one voice
  under 1 %.

## 9. Offline checks (`make test`)

1. Partial frequencies match the stiff-string formula within 1 cent.
2. Stretch curve monotonic, +15..+40 cents at C8, -8..-40 at A0.
3. Hammer contact time falls with velocity and with key number.
4. Spectral centroid rises with velocity (same key) by a clear margin.
5. Two-stage decay: early slope at least 2x the late slope (C4, small detune).
6. Beat rate of a bichord matches its detune once past the coupling range.
7. TUNING 0 beats slowly; TUNING max is audibly out (rms error in range).
8. Release: damped note falls 40 dB within 0.5 s (mid); top keys ignore release.
9. Sustain pedal holds; half pedal sits between.
10. Silent held key rings after a staccato octave below (sympathetic).
11. Pedal-down halo present, absent with pedal up.
12. SOFT is quieter and duller; FELT lowers the centroid a lot.
13. Repeated note on a ringing string differs from the first and stays stable.
14. Same serial = identical render; another serial differs.
15. No NaN / runaway at every control extreme, 16-note clusters, pedal down.
16. DISTANCE: direct-to-room ratio falls monotonically; next room has no direct.
17. No DC, peak under 10 V at ff 10-note chord, silence returns to exact zero.
18. Speed: 16 voices under budget.

Listening files: `joanna-demo.wav` (a played phrase through several pianos and
rooms) and a preset tour.

## 10. Twenty presets (right-click)

Each is a specific instrument (serial included) in a specific place.
Front Room · Tuner Came Yesterday · Gran's Piano · Sunday Service ·
Late Set · Felt Practice · Pub Joanna · School Hall · From the Kitchen ·
Rehearsal Room · Front Off · Under the Lid · Tack Piano · Spinet ·
Tall Old Upright · Damp Garage · Bare Boards · Carpet and Curtains ·
Stride · Last Hymn
