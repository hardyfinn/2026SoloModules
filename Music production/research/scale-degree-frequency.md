# How often each scale degree is used: what the research says

A short literature review on how often each scale degree appears in melodies, with some notes on using it in your rig.

**About the sources:** this is written from my knowledge of the literature, not from a fresh search. The rankings and main findings are well established. Treat any exact numbers as approximate and check them against the original papers before you quote them.

---

## 1. The short answer

Your hunch is mostly right, with one surprise.

- The **tonic (1), dominant (5) and third (3)** are the most-used melody notes in every Western corpus that has been studied, from folk songs to classical to rock.
- **The 2nd is not rare.** In melodies it is usually the **4th most common** degree. It shows up more often than 4, 6 or 7, because melodies move mostly by step and 2 sits between 1 and 3. What 2 rarely gets is the *ending* of a phrase or a long held note.
- In rock and pop, the **flat 7 and flat 3** appear much more than classical theory would predict. This comes from blues and the pentatonic scale.

So "how common" depends on what you count: every note, notes weighted by how long they last, notes on strong beats, or phrase endings. Each gives a different ranking.

---

## 2. The key studies

### Krumhansl & Kessler (1982): the "probe-tone" profiles
- **What they did:** played listeners a key-setting context (a scale or a chord progression), then a single tone, and asked them to rate how well it "fit".
- **Result (major key, from best to worst fit):** 1 > 5 > 3 > 4 > 6 > 2 > 7, with the chromatic notes far below.
  - Their often-quoted ratings are 1 = 6.35, 5 = 5.19, 3 = 4.38, 4 = 4.09, 6 = 3.66, 2 = 3.48, 7 = 2.88.
- **Why it matters:** this is the classic "tonal hierarchy". It measures how *stable* listeners hear each note, not how often it is used, but later studies found that the two closely match. Krumhansl's book *Cognitive Foundations of Musical Pitch* (1990) shows that the ratings correlate with how often each note is used in classical pieces.

### Aarden (2003) and Huron (2006): folk melody counts
- **Corpus:** the **Essen Folksong Collection**, thousands of mostly German folk melodies.
- **Result (major):** **5 is the most common note**, then 3 and 1 (roughly tied), then **2**, then 4, 6 and 7. The tonic is not the most frequent note. It is the most frequent *last* note.
- In David Huron's *Sweet Anticipation* (2006), chapter 9 on "scale degree qualia" connects these counts to feeling. Notes we hear a lot become *expected*, and expected notes feel stable, while rare ones (7, 4, chromatic notes) feel tense or "leaning". Aarden also showed that melodies are statistically more likely to move by step and to move *down*.

### Temperley (2007): *Music and Probability*
- Builds probabilistic key-finding models from the counts in a classical corpus (the Kostka–Payne textbook examples). The pattern matches: 1, 5 and 3 dominate, and 2 and 4 come next.

### Temperley & de Clercq (2013): rock melodies
- **Paper:** "Statistical analysis of harmony and melody in rock music", *Journal of New Music Research* 42(3). It uses the **Rolling Stone "500 Greatest Songs"** list (their first 200 songs, transcribed).
- **Results:**
  - 1, 5 and 3 are again the most common melody notes, and 2 is fairly common.
  - **Flat 7 and flat 3 are common** even in "major" songs, a sign of blues mixture. Rock melodies lean toward the **pentatonic scale** (1, 2, 3, 5, 6 or 1, b3, 4, 5, b7) and use 4 and 7 less than classical melodies do.
- **Related:** Temperley's "The melodic-harmonic 'divorce' in rock" (*Popular Music*, 2007) shows that rock melodies often ignore the chord underneath, for example a minor-pentatonic vocal over major chords. That is very much the soul and funk sound.

### Chord-level corpora (for context)
- The **McGill Billboard corpus** (Burgoyne et al., 2011) annotates the chords of about 1,000 Billboard chart songs. Over time, the **bVII and IV chords** grow and the classical **V** chord declines. This matches the rise of flat 7 in melodies.
- **Hooktheory's** "TheoryTab" database is not peer reviewed, but it is large and shows the same pattern: I, V, vi and IV dominate, and in major-key pop melodies 1, 3 and 5 lead.

### Older work
- Youngblood (1958) applied information theory to scale-degree counts in Romantic songs. It is one of the first of these studies.

---

## 3. Ways the counting changes the answer

| What you count | Typical leader | What it tells you |
|---|---|---|
| Every note | 5 (folk), 1 (rock) | Overall colour of the melody |
| Duration-weighted | 1 and 5 | What the ear "rests" on |
| Downbeats only | 1, 3, 5 (chord tones) | Rhythmic anchors |
| Phrase endings | **1** by far, then 5 | Sense of closure |
| Perceived fit (probe tone) | 1 > 5 > 3 | Stability and tension |

The main point: **2, 4, 6 and 7 are common as *moving* notes and rare as *landing* notes.** That gap is where melodic tension comes from.

---

## 4. Genre notes for your EP

- **Detroit techno leads and pads** often sit in **minor or Dorian** and lean on 1, b3, 5 and b7, with the 9th (which is the 2nd an octave up) added as colour. The 2nd you suspected was rare is a signature sound here, played as a *held chord tone* (min9 chords) and not as a melody note.
- **Autechre and Aphex Twin** often weaken the hierarchy on purpose. They use flatter distributions, many chromatic notes, and phrases that never land on 1. That is part of why their music feels unresolved and "evolving" instead of song-like.

---

## 5. Using this in your rig

The hierarchy can drive generative patches directly.

- **Marbles (X section):** the **STEPS** knob, turned right from noon, quantizes to the selected scale. Turning it further right keeps fewer notes, dropping the *least important* degrees first, until only the most important are left (root and 5th, then roughly root only). That is close to the Krumhansl hierarchy on a knob. Put **CV on STEPS** (an LFO, or a slow random from the Random8) and the melody moves from stable to adventurous over a section. This is a good cure for loop fatigue.
- **Marbles custom scale:** you can teach it your own scale *with weights*. The degrees you play more often while recording it get more weight (check the exact procedure in the Marbles manual). For example, weight 1, b3, 5 and b7 heavily and keep 2 and 9 light for a Detroit-style minor feel.
- **Metropolix / Hapax:** keep phrase-ending steps on 1 or 5 and put 2, 4 and 6 on off-beats or passing steps. Then, for the "lift" section of a track, *swap* this so phrases end on 2 or 6 and stop resolving.
- **DROID:** a `quantizer` circuit with a custom note set can be switched between a "stable" set (1, 3, 5) and a "full" set by a gate. It works like a section change at the press of a button.

---

## Further reading (in order of usefulness)

1. David Huron, *Sweet Anticipation: Music and the Psychology of Expectation* (MIT Press, 2006), chapter 9.
2. Temperley & de Clercq, "Statistical analysis of harmony and melody in rock music", *JNMR* (2013). The data is free online as the "Rock Corpus".
3. Carol Krumhansl, *Cognitive Foundations of Musical Pitch* (Oxford, 1990).
4. David Temperley, *Music and Probability* (MIT Press, 2007).
5. Hooktheory's "Trends" pages, for a quick, informal look at pop.
