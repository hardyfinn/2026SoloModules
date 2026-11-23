# A scratching machine for Ableton Live: research and proposal

Fable, 2026-11-23. Design stage, nothing built. Working name: **Cuts** (yours to change).

The descriptions of how each scratch is performed are from memory of the technique, not from
sources checked today. You know this better than the model does, so treat the table in section 2
as a list to correct. All timing figures are estimates until you have listened.

## 1. What it is

One deck, one fader, one phrase. The device holds a short sound on a modelled record and
scratches it in time with Live: it moves a record hand and a fader hand the way a DJ does, and
what you hear is whatever those two hands produce. Nothing is a stored effect. A chirp is not a
"chirp sound"; it is a push with the fader closing and a pull with the fader opening, and it
comes out as a chirp because of that.

It is the scratching half of Doubles taken out and given the whole panel. Doubles had one
scratch (the baby) used as an ornament. This has a vocabulary.

## 2. The vocabulary

Fifteen words in the prototype. Each has a demo, four bars over a plain beat at 96 BPM, and a
chart of record position against time (heavy line where the fader is open), which is how
scratches are usually written down.

| # | Word | Record hand | Fader hand | You hear |
|---|---|---|---|---|
| 01 | **Baby** | Push, pull | Open throughout | The basic "zigga" |
| 02 | **Forward** | Let go, record plays | Cut at the end, pull back closed | The sound, forwards only, clean |
| 03 | **Stab** | Fast push | Open for the first part only | A short, bright, pitched-up hit |
| 04 | **Chirp** | Push, pull | Closes on the push, opens on the pull | Two clipped notes with a bird-like edge |
| 05 | **1-click flare** | Push, pull | Open, one click shut in mid-stroke | Two notes per stroke (the orbit) |
| 06 | **2-click flare** | Push, pull | Open, two clicks per stroke | Three notes per stroke, triplet feel |
| 07 | **Crab** | Push, pull | Four finger taps on one stroke | A fast roll of four |
| 08 | **Tear** | Push, then the pull in two halves | Open | Three notes, no fader |
| 09 | **Scribble** | Tight shake, about 15 a second | Open | A buzz on the front of the sound |
| 10 | **Transform** | Slow drag, or let it play | Closed, tapped open in rhythm | The sound chopped into a rhythm |
| 11 | **Drag** | Slow push and pull | Open | Low and growling |
| 12 | **Tip** | Tiny strokes on the very front of the sound | Open | Fast ticks, almost percussion |
| 13 | **Lazer** | Hard flick, then a slow pull | Open | A zip up and a slide down |
| 14 | **Release**, **Echo** | Let the phrase play; or forwards | Open; or each forward quieter | The phrase itself; a fading repeat |
| 15 | **Reverse** | Push silently, pull | Opens for the pull only | The sound backwards |

Left out for now, could be added: hydroplane (a finger dragged against the turning record),
twiddle (two-finger crab), and the named combinations (chirp-flare, boomerang and so on), which
the machine builds from the words above instead of storing.

Long pitch slides and anything that "races" are not in the vocabulary, following what you said
on Doubles.

## 3. The model

### 3.1 The record hand

Every stroke is a movement of a given length of record in a given time, starting and ending at
rest. A push peaks early (a hand starts it), a pull is even, a flick peaks earlier still, a drag
sits at its speed for longer. A hand has a top speed (about three times playing speed averaged
over a stroke). When there is not enough time, the stroke takes the time it needs and the next
thing is late, as it would be. It never plays faster than a hand can move.

Coming back to the cue point always has a small error, which grows with the distance pulled.
Moving to another cue point takes real time (roughly a quarter of a second for a second of
record), fader closed.

### 3.2 The fader hand

A sharp cut, about a millisecond. Flare clicks are about 22 ms shut. Crab taps are four
openings about 60 ms apart at most. The fader hand has its own timing error against the record
hand, which is what makes a real chirp or flare sound slightly different every time.

### 3.3 The cartridge (new, not in Doubles)

Doubles reads the buffer at the record's speed and nothing more. A real cartridge puts out a
voltage that follows how fast the groove moves under the needle, and the phono stage then
applies its bass-up, treble-down curve. The prototype does both:

- A held record is silent, and a stroke swells from nothing and dies to nothing by itself.
- Slow movement is quiet and dark, fast movement is loud and bright. Measured on the demos: the
  spectral centre of the drag is about 250 Hz, the baby 670 Hz, the stab 1,260 Hz, from the same
  sound.
- At playing speed the two stages cancel exactly, so a released phrase is the phrase.

I think this is most of why modelled scratching usually sounds like a sampler and a real deck
does not. It is the part I most want your ear on.

### 3.4 Hands

The same four dials as Doubles, meaning the same things:

- **TIMING**: where strokes and fader moves fall against the grid, and against each other.
- **WEIGHT**: how heavy the hand is. Slower pick-up after a let-go, pushes that peak earlier.
- **SLOP**: uneven stroke lengths, missed cue points, a fader that closes a touch late.
- **RISK**: outright fluffs. A click that does not close, a pull that goes back too far.

## 4. The device

### 4.1 The record

- Drop a sound onto the waveform strip, or record up to 4 bars from the track as in Doubles.
- **Cue points** (the stickers from Doubles), up to 8, found at the starts of sounds and
  movable by hand. A scratch works on one cue point at a time. On "ahh" there is one. On a spoken
  phrase each syllable is one, and the same bar of scratching sounds quite different on each
  (demo 20).
- **CUE** picks the cue point. **WALK** lets the machine move between them, in the gaps.

### 4.2 Three ways to play it, as in Doubles

- **Hands on.** One button per word, plus RELEASE. Press and it is played on the next grid
  point, on the current cue point. A **SPEED** setting (8ths, 16ths, triplets, 32nds) says how
  long a stroke is.
- **Routine.** Sixteen slots, each a word with a speed and a cue point, or a rest.
- **Instinct.** The machine improvises. In the prototype it thinks in four-bar phrases: a bar,
  the same bar again (sometimes with one thing changed), a different bar, and an ending that
  often lets the phrase play out. That shape is why it reads as someone scratching and not as
  random events. Its dials:
  - **DENSITY**: how much it rests.
  - **SKILL**: which words it will reach for. Low is babies, forwards and drags. High adds
    flares, crabs, tears and tips.
  - **MOTIF**: how often it repeats itself.
  - **RELEASE**: how often a phrase ends with the sound played out.
  - **FUNK**: swing on the 16ths, and how many triplet figures.
  - A switch per word, so the vocabulary can be narrowed to, say, babies, chirps and stabs.
- **PHRASE**: scratch for N bars, rest for M bars, so it can answer a vocal or leave room.

### 4.3 What comes out

Only the scratching. The beat is the rest of your Live set. So there is no OVER setting here.

### 4.4 What is reused from Doubles

The buffer, record and cue-point code, the queue between the planner (JavaScript) and the engine
(gen~), the hands dials, the panel style and the build script. The stroke engine is new: Doubles
has one stroke shape and no fader work inside a stroke. The cartridge stage is new.

## 5. The demos

In `demos/` (over a plain beat) and `demos-solo/` (scratch only). `charts/all-words.png` shows
the hand and fader for each word.

- `00-the-record`: the test sound. A synthesized open vowel with a falling pitch, a gap, and a
  three-part word. It is a stand-in. I cannot fetch the classic scratch records.
- `00-gallery-all-15-words`: 01 to 15 end to end.
- `01` to `15`: one per word.
- `20`: the same bar on four cue points.
- `21` to `24`: written sentences. 24 uses most of the words.
- `30` to `35`: Instinct. Tight DJ, machine, loose, sparse and simple, dense and flashy, walking
  the cue points.
- `40`, `41`: the same bar at four hands settings, then with each dial alone.
- `50`, `51`: one bar of your baby-cry loop as the record (bar 1 and bar 5). The cue points were
  picked by level jumps, not by ear, so they may sit on dull places.

**These were checked by measurement and by the charts, not by ear.** Hand speeds, grid landing
and the cartridge response are what I intended. Whether a chirp sounds like a chirp is for you
to say.

## 6. Questions

1. **Instrument or effect?** Doubles is an audio effect. I suggest this one is an
   **instrument**: each word sits on a key, so scratches can be played from pads and written as
   MIDI clips, with note length setting the stroke speed, and Instinct and Routine still there
   for when it plays itself. The cost is that the sound is dropped in as a file, not recorded
   from the track.
2. **Which words?** All fifteen, or a smaller first set? Anything named wrongly, performed
   wrongly, or missing?
3. **The cartridge model** (3.3): keep it always on, or put it on a dial so it can be turned
   down to a clean sampler read?
4. **Name.** Cuts, or yours.
5. **Reference sounds.** The scratch sounds you would actually use, and if you have them, a few
   seconds of clean real scratching of any kind. Babies, chirps and flares would settle the
   stroke shapes and click lengths faster than anything else.
