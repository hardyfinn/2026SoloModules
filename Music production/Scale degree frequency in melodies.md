---
title: Scale degree frequency in melodies
date: 2026-11-05
source: Music production · Weighted quantizer
topic: Music
tags: [music-theory, scale-degrees, melody, research]
summary: |
 In melodies 1, 5 and 3 are the most used scale degrees, and 2 is not actually rare.
 A literature review and a longer PDF with distribution data from the studies are included.
---

# Scale degree frequency in melodies

## Summary
In melodies 1, 5 and 3 are the most used scale degrees, and 2 is not actually rare.
A literature review and a longer PDF with distribution data from the studies are included.

## Question
> I'm interested in all of those, but right now, i have a question. I assume that there is a relative popularity of scale degrees used in popular music, for example the root and fifth are more common in pop melodies than the 2nd. Can you review the literature on this and give me some information on the analysis of this?

## Answer
Your hunch is mostly right, with one twist: **the 2nd isn't actually rare.**

- Across folk, classical and rock corpora, **1, 5 and 3** are the most-used melody notes. In folk melodies the 5th often beats the root, and the root wins as the *last* note of a phrase.
- **2 usually comes 4th**, ahead of 4, 6 and 7, because melodies move by step and 2 sits between 1 and 3. It's common as a passing note and rare as a landing note.
- In rock and pop, **b7 and b3** show up far more than classical theory predicts. That's the blues and pentatonic influence (Temperley & de Clercq's Rolling Stone corpus study).

The key sources are Krumhansl & Kessler's 1982 listening tests, Huron's *Sweet Anticipation* and Temperley's corpus work. The write-up also shows how to put this hierarchy on a knob with Marbles' STEPS control, as a tool against loop fatigue.

I wrote it from memory rather than a fresh search, so treat any exact numbers as approximate.

### Full write-up

### How often each scale degree is used: what the research says

A short literature review on how often each scale degree appears in melodies, with some notes on using it in your rig.

**About the sources:** this is written from my knowledge of the literature, not from a fresh search. The rankings and main findings are well established. Treat any exact numbers as approximate and check them against the original papers before you quote them.

---

#### 1. The short answer

Your hunch is mostly right, with one surprise.

- The **tonic (1), dominant (5) and third (3)** are the most-used melody notes in every Western corpus that has been studied, from folk songs to classical to rock.
- **The 2nd is not rare.** In melodies it is usually the **4th most common** degree. It shows up more often than 4, 6 or 7, because melodies move mostly by step and 2 sits between 1 and 3. What 2 rarely gets is the *ending* of a phrase or a long held note.
- In rock and pop, the **flat 7 and flat 3** appear much more than classical theory would predict. This comes from blues and the pentatonic scale.

So "how common" depends on what you count: every note, notes weighted by how long they last, notes on strong beats, or phrase endings. Each gives a different ranking.

---

#### 2. The key studies

##### Krumhansl & Kessler (1982): the "probe-tone" profiles
- **What they did:** played listeners a key-setting context (a scale or a chord progression), then a single tone, and asked them to rate how well it "fit".
- **Result (major key, from best to worst fit):** 1 > 5 > 3 > 4 > 6 > 2 > 7, with the chromatic notes far below.
  - Their often-quoted ratings are 1 = 6.35, 5 = 5.19, 3 = 4.38, 4 = 4.09, 6 = 3.66, 2 = 3.48, 7 = 2.88.
- **Why it matters:** this is the classic "tonal hierarchy". It measures how *stable* listeners hear each note, not how often it is used, but later studies found that the two closely match. Krumhansl's book *Cognitive Foundations of Musical Pitch* (1990) shows that the ratings correlate with how often each note is used in classical pieces.

##### Aarden (2003) and Huron (2006): folk melody counts
- **Corpus:** the **Essen Folksong Collection**, thousands of mostly German folk melodies.
- **Result (major):** **5 is the most common note**, then 3 and 1 (roughly tied), then **2**, then 4, 6 and 7. The tonic is not the most frequent note. It is the most frequent *last* note.
- In David Huron's *Sweet Anticipation* (2006), chapter 9 on "scale degree qualia" connects these counts to feeling. Notes we hear a lot become *expected*, and expected notes feel stable, while rare ones (7, 4, chromatic notes) feel tense or "leaning". Aarden also showed that melodies are statistically more likely to move by step and to move *down*.

##### Temperley (2007): *Music and Probability*
- Builds probabilistic key-finding models from the counts in a classical corpus (the Kostka–Payne textbook examples). The pattern matches: 1, 5 and 3 dominate, and 2 and 4 come next.

##### Temperley & de Clercq (2013): rock melodies
- **Paper:** "Statistical analysis of harmony and melody in rock music", *Journal of New Music Research* 42(3). It uses the **Rolling Stone "500 Greatest Songs"** list (their first 200 songs, transcribed).
- **Results:**
  - 1, 5 and 3 are again the most common melody notes, and 2 is fairly common.
  - **Flat 7 and flat 3 are common** even in "major" songs, a sign of blues mixture. Rock melodies lean toward the **pentatonic scale** (1, 2, 3, 5, 6 or 1, b3, 4, 5, b7) and use 4 and 7 less than classical melodies do.
- **Related:** Temperley's "The melodic-harmonic 'divorce' in rock" (*Popular Music*, 2007) shows that rock melodies often ignore the chord underneath, for example a minor-pentatonic vocal over major chords. That is very much the soul and funk sound.

##### Chord-level corpora (for context)
- The **McGill Billboard corpus** (Burgoyne et al., 2011) annotates the chords of about 1,000 Billboard chart songs. Over time, the **bVII and IV chords** grow and the classical **V** chord declines. This matches the rise of flat 7 in melodies.
- **Hooktheory's** "TheoryTab" database is not peer reviewed, but it is large and shows the same pattern: I, V, vi and IV dominate, and in major-key pop melodies 1, 3 and 5 lead.

##### Older work
- Youngblood (1958) applied information theory to scale-degree counts in Romantic songs. It is one of the first of these studies.

---

#### 3. Ways the counting changes the answer

| What you count | Typical leader | What it tells you |
|---|---|---|
| Every note | 5 (folk), 1 (rock) | Overall colour of the melody |
| Duration-weighted | 1 and 5 | What the ear "rests" on |
| Downbeats only | 1, 3, 5 (chord tones) | Rhythmic anchors |
| Phrase endings | **1** by far, then 5 | Sense of closure |
| Perceived fit (probe tone) | 1 > 5 > 3 | Stability and tension |

The main point: **2, 4, 6 and 7 are common as *moving* notes and rare as *landing* notes.** That gap is where melodic tension comes from.

---

#### 4. Genre notes for your EP

- **Detroit techno leads and pads** often sit in **minor or Dorian** and lean on 1, b3, 5 and b7, with the 9th (which is the 2nd an octave up) added as colour. The 2nd you suspected was rare is a signature sound here, played as a *held chord tone* (min9 chords) and not as a melody note.
- **Autechre and Aphex Twin** often weaken the hierarchy on purpose. They use flatter distributions, many chromatic notes, and phrases that never land on 1. That is part of why their music feels unresolved and "evolving" instead of song-like.

---

#### 5. Using this in your rig

The hierarchy can drive generative patches directly.

- **Marbles (X section):** the **STEPS** knob, turned right from noon, quantizes to the selected scale. Turning it further right keeps fewer notes, dropping the *least important* degrees first, until only the most important are left (root and 5th, then roughly root only). That is close to the Krumhansl hierarchy on a knob. Put **CV on STEPS** (an LFO, or a slow random from the Random8) and the melody moves from stable to adventurous over a section. This is a good cure for loop fatigue.
- **Marbles custom scale:** you can teach it your own scale *with weights*. The degrees you play more often while recording it get more weight (check the exact procedure in the Marbles manual). For example, weight 1, b3, 5 and b7 heavily and keep 2 and 9 light for a Detroit-style minor feel.
- **Metropolix / Hapax:** keep phrase-ending steps on 1 or 5 and put 2, 4 and 6 on off-beats or passing steps. Then, for the "lift" section of a track, *swap* this so phrases end on 2 or 6 and stop resolving.
- **DROID:** a `quantizer` circuit with a custom note set can be switched between a "stable" set (1, 3, 5) and a "full" set by a gate. It works like a section change at the press of a button.

---

#### Further reading (in order of usefulness)

1. David Huron, *Sweet Anticipation: Music and the Psychology of Expectation* (MIT Press, 2006), chapter 9.
2. Temperley & de Clercq, "Statistical analysis of harmony and melody in rock music", *JNMR* (2013). The data is free online as the "Rock Corpus".
3. Carol Krumhansl, *Cognitive Foundations of Musical Pitch* (Oxford, 1990).
4. David Temperley, *Music and Probability* (MIT Press, 2007).
5. Hooktheory's "Trends" pages, for a quick, informal look at pop.

## Question 2
> Great artefact. I'm fascinated by this. Please create a 5 times longer pdf which goes into more detail, especially in terms of '4. genre notes for your ep' and more detail on "2. the key studies". Please include some actual distribution data from the studies.

## Answer 2
Here's the 21-page PDF, about six times longer than the first version, with 10 charts of real distribution data.

- **Studies:** 11 sources, each with its published numbers. New since the first version: rock melodies are about 86% major pentatonic, and on downbeats the 1 overtakes the 5.
- **Genre notes:** Detroit, Autechre and Aphex, funk and hip hop, four degree profiles and a section-by-section arrangement map.
- **Correction:** Marbles' STEPS works clockwise. It learns weights from how often you play each note, so I added 60-note "jam" recipes.

No corpus study covers techno, so that section is analysis by ear. A few rock-paper figures came from a summary and are flagged in the text.

### Full text of the PDF

```text
M U S I C T H E O RY · R E S E A R C H N OT E S F O R T H E E P

Scale Degrees in Practice

What corpus studies and listening experiments say about which
notes of a scale music actually uses, and how to use that knowledge
in Detroit-lineage techno, generative patches and arrangement.
1 The short answer
2 How the counting is done
3 The key studies, with data
4 What the data adds up to
5 Genre notes for your EP

6 Recipes for your rig
7 Exercises
8 Further reading
A Appendix: full data tables

Distribution values come from the published profiles as collected in the AMADS key-profile library (Gotham et al.),
which reproduces them as reported in the original papers. The rock chord-root shares match the published profile; a
few further figures from the rock papers were read in summary and are flagged in the text. Where a number could
not be checked against the original, the text says so.

Scale Degrees in Practice · 1

1. The short answer
Your hunch was that some scale degrees are used much more than others in popular melodies, with the
root and fifth near the top and the 2nd near the bottom. The research supports the first half strongly and
the second half not at all.
1, 3 and 5 are the top three in every corpus that has been measured. That holds for German folk
songs, classical scores and rock vocals. The order inside the top three changes with the style: folk
melodies use 5 most, classical scores use 1 most, and rock melodies in major keys use 1 then 3.
2 is the fourth most common degree almost everywhere. In folk melodies it makes up about 15% of
all notes, more than 4, 6 and 7. In one large classical corpus, counted across all beats, 2 is even
slightly ahead of 3.
What 2 rarely does is land. It is rare on downbeats, at phrase endings and as the final note. The 2nd is
a travelling note, the 1st a destination note. This difference between how often a note is used and how
stable it sounds is what melodic tension is made of.
Popular styles have their own fingerprints. Rock melodies are strongly pentatonic: in major-key rock
songs, 1, 2, 3, 5 and 6 together account for about 86% of all melody notes. In minor-key rock songs,
the minor pentatonic (1, ♭3, 4, 5, ♭7) accounts for about 79%. The ♭7, which is rare in classical music,
is a normal part of rock and folk minor melodies, and the ♭VII chord is the fourth most common
chord in rock.
The rest of this document shows where these claims come from, with the actual distributions plotted,
and then turns them into concrete tools: weighted scales you can teach Marbles, section-by-section
plans for arranging a track, and genre notes on Detroit techno, Autechre and Aphex Twin, funk, hip hop
and reggae.
A note on terms
A scale degree is a note's position relative to the key's home note (the tonic), regardless of the actual pitch. In C
major, C is 1, D is 2, G is 5. In A minor, A is 1, C is ♭3, G is ♭7. The symbols ♭ and ♯ show a degree lowered or
raised by a semitone compared with the major scale. Using degrees instead of note names lets researchers pool
songs in different keys into one count.

2. How the counting is done
Before looking at the numbers it helps to know what kinds of numbers exist, because different methods
give different rankings. Most disagreements between studies are really disagreements about what is
being counted.

Scale Degrees in Practice · 2

2.1 Five ways to measure "how common"
Method

What it counts

Example study

What it is good for

Note counts

Every melody note gets one vote,
whatever its length.

Aarden (2003), Essen folk
songs

The overall colour of a melody
line.

Duration
weighting

Each note's vote is scaled by its length,
so held notes count more.

Used in many key-finding
models

What the ear rests on; long notes
dominate key perception.

Metric
position

Counts split by where in the bar a note
falls.

Prince & Schmuckler
(2014)

Separating anchor notes from
passing notes.

Segment
presence

Whether a degree appears at all in a
short segment, yes or no.

Temperley (2007), Kostka– Robust against repeated notes and
Payne excerpts
ornaments.

Listener
ratings

No counting at all: people rate how well
a tone "fits" after hearing a key.

Krumhansl & Kessler
(1982)

Perceived stability, the felt
hierarchy.

A sixth variant counts the notes of chords instead of melodies (Budge 1943; de Clercq & Temperley 2011).
Chord counts rate 7 and 4 higher than melody counts, because those degrees live inside the V and IV
chords even when the tune avoids them.

2.2 Pitfalls to keep in mind
You need a key before you can count degrees. Every study has to decide the tonic of each song first,
usually by expert judgement. In ambiguous music, such as modal rock or a techno loop that never
cadences, that decision shapes the result. Temperley & de Clercq found that labelling rock songs as
simply major or minor was "quite problematic".
The corpus is a choice. Essen is mainly German folk song collected in the 19th and 20th centuries.
The rock corpus is built from Rolling Stone's "500 Greatest Songs" list, so it reflects critics' tastes,
mostly from the 1950s to the 1990s. Neither represents "music in general", and no published study
gives comparable data for techno or other electronic dance music.
Transcription is interpretation. Blue notes that sit between 6 and ♭7, or between ♭3 and 3, have to be
assigned to one degree or the other. The rock study reports that its two authors disagreed on several
such notes in Aretha Franklin's "Respect".
Ratings are not counts. Probe-tone ratings measure how stable a tone sounds. They correlate closely
with usage counts, which is the central finding of this field, but they are a different quantity and use a
different scale.

3. The key studies, with data
The studies below run roughly in historical order. Each one has a chart; exact numbers for all of them
are in the appendix. In the charts, bold labels on the x-axis are the seven degrees of the major scale, and
grey labels are the chromatic notes.

3.1 Krumhansl & Kessler (1982): the felt hierarchy
What they did. Carol Krumhansl and Edward Kessler, building on work by Krumhansl and Roger
Shepard (1979), developed the probe-tone method. Listeners heard a short passage that firmly set a key,
such as a scale or a cadence, followed by one of the 12 chromatic tones. They rated on a 1–7 scale how
well that tone fitted. Repeating this for all 12 tones in both major and minor contexts gave a profile of
perceived stability.

Scale Degrees in Practice · 3

Krumhansl & Kessler (1982): probe-tone fit ratings
8
6

Major context

6.35 6.33
5.38

)7–1( gnitar

5.19
4.38

4

4.09

3.48 3.52
2.23

2
0

Minor context

1

2.68

♭2

2.33

2

♭3

3.98

3.53

2.60

3

4.75

2.52 2.54

4

♯4

2.39

5

♭6

3.66
2.69

6

3.34
2.29

♭7

3.17

2.88

7

Figure 1. How well each of the 12 tones was rated as fitting a major or minor key context. The higher the bar, the more stable the
tone sounds. Bold labels are degrees of the major scale.

What it shows. In major the order is 1 > 5 > 3 > 4 > 6 > 2 > 7 . The tonic is rated far above everything else
(6.35), the fifth is second (5.19) and the third third (4.38). Then come 4, 6 and 2 in a cluster between 3.5
and 4.1, and finally 7 (2.88), which is barely above the chromatic notes (2.2–2.5). In minor, the order is 1
> ♭3 > 5 > ♭6 > 4 > 2 > ♭7 . Notice that ♭3 rises to second place in minor, above 5, and that ♭6 is rated quite
high.
The ratings form three tiers that correspond to music-theory categories: the tonic triad (1, 3, 5), the rest
of the scale (2, 4, 6, 7), and the chromatic notes. Krumhansl's later book (1990) compared these ratings
with how often each degree appears in classical pieces and found the two closely related. That finding
launched the idea that listeners learn the tonal hierarchy from statistical exposure, which David Huron
later developed in detail (section 3.9).
Why it matters for you. The ratings are the clearest picture of "home versus away". When a generative
patch lands on 1 or 5 on a strong beat, the ear hears rest; when it lands on 7 or 2, the ear hears a
question. Because the two tiers are so far apart, small changes in how often your sequencer hits the top
tier change the mood a lot.
The Krumhansl–Schmuckler key-finding algorithm
These profiles were turned into the first widely used key-finding algorithm. You count how much each pitch class
appears in a piece, then compare (correlate) that count with the 24 major and minor profiles rotated to every key.
The best match is the guessed key. Most of the later profiles in this document were built to improve that
algorithm, which is why they are often called "key profiles".

3.2 Budge (1943) and Bellman (2005): counting chords
Helen Budge's 1943 dissertation counted the chords used by 18th- and 19th-century composers, one of
the earliest statistical studies of harmony. Héctor Bellman later converted those chord counts into a
pitch profile by crediting each chord's notes. Because this counts chord tones, not melody notes, it
shows the harmonic skeleton.

Scale Degrees in Practice · 4

Bellman / Budge (1943): pitch weights derived from chord counts
30

)%( thgiew

20

Major

Minor

20.3 21.1

18.2

16.8

13.3

12.9 13.0

13.5

11.9 11.2

10

7.5
0.9

0

1

1.4

0.7

♭2

2

♭3

3

4

1.8

1.4

1.2

1.1

10.6 10.2

8.0

♯4

5

1.5

♭6

0.6

6

0.9

♭7

7

Figure 3. Profiles derived from Helen Budge's 1943 count of chords in common-practice music. Because these count chord
tones, not melody notes, 7 scores high (it is in the V chord).

The major order is 5 > 1 > 3 > 2 > 4 > 7 > 6 . Two differences from the melody counts stand out. First, 5 is
the top degree (20.3%), because it is part of both the I and V chords. Second, 7 rises to 10.6%, above 6,
because it is the third of the V chord. Melodies avoid 7 much more than harmonies do. In minor, the
leading tone 7 (10.2%) is far ahead of ♭7 (0.9%): classical minor borrows the raised 7 from the harmonic
minor scale for its V chord.

3.3 Aarden (2003): the Essen folk song counts
The corpus. The Essen Folksong Collection, compiled by Helmut Schaffrath (1995), contains several
thousand folk melodies, mostly German, in a computer-readable form. Bret Aarden's 2003 dissertation at
Ohio State University tabulated how often each scale degree occurs across its major-key and minor-key
melodies. His main topic was melodic expectation: he used reaction-time experiments to show that
listeners anticipate notes in line with these statistics.
Aarden (2003): note frequencies in the Essen folk song collection
30

seton fo %

20

Major melodies

Minor melodies

22.1

19.8

17.8 18.3

18.6

16.9

14.9 14.0

14.4
11.4

10

8.2

7.4

4.6

0

0.1

1

0.7

♭2

0.7

0.2

2

♭3

0.7

0.3

3

4

♯4

1.9

0.1

5

♭6

6

5.0
1.8

0.2

♭7

Figure 2. Percentage of all melody notes that fall on each scale degree, Essen Folksong Collection (Schaffrath 1995), as
tabulated by Aarden (2003). Note that 5, not 1, is the most frequent note in major-key melodies.

Major keys. The order is 5 > 3 > 1 > 2 > 4 > 6 > 7 . In percentages:
Degree

5

3

1

2

4

6

7

all chromatic

% of notes

22.1

19.8

17.8

14.9

11.4

8.2

5.0

under 1.0

Scale Degrees in Practice · 5

7

This is the cleanest answer to your original question. The fifth is the single most common melody note,
the root is only third, and the 2nd at almost 15% is comfortably ahead of 4, 6 and 7. All five chromatic
notes together make up under 1% of the notes.
Why would 5 beat 1? Folk melodies tend to sit in the range above and around the tonic, and 5 works as a
stable "turning point" at the top of many phrases. The tonic is the most common final note but is used
less in the middle of phrases. The 2nd is common because melodies move mostly by step. A tune that
walks from 3 to 1, or from 1 to 3, passes through 2 every time.
Minor keys. The order is 5 > 1 > ♭3 > 4 > 2 > ♭7 > ♭6 , with 5 at 18.6%, 1 at 18.3%, ♭3 at 16.9%, 4 at 14.4%, 2
at 14.0%, ♭7 at 7.4%, ♭6 at 4.6%, 6 at 1.9% and 7 at 1.8%. This is the natural minor (Aeolian) scale, with ♭7
four times as common as 7. Classical minor is different, as section 3.4 shows. Folk minor melodies, like
rock and techno minor melodies, do without the leading tone.

3.4 Temperley (2007): classical excerpts from a textbook
David Temperley's book Music and Probability built a probabilistic key-finding model from 46 excerpts of
classical music used in Stefan Kostka and Dorothy Payne's harmony textbook. Instead of counting notes,
the profile measures the chance that a degree appears at least once in a short segment of the music. The
values therefore do not add up to 1.
Temperley (2007): share of segments containing each degree (Kostka–Payne corpus)
0.8

Major

0.75

Minor

0.71

stnemges fo noitroporp

0.62

0.6

0.71

0.67

0.49 0.47

0.75

0.46 0.46

0.40

0.4
0.2
0.06 0.08

0

1

♭2

0.08

2

♭3

0.10 0.10

0.05

3

4

♯4

0.10

5

♭6

0.40

0.37

0.33

0.13
0.07

6

0.06

♭7

7

Figure 4. The probability that a degree appears at least once in a short segment of a piece, from 46 textbook excerpts of
classical music. Minor keys show the harmonic-minor leading tone (7) far ahead of ♭7.

The major order is 1 > 5 > 3 > 2 > 4 > 7 > 6 : 1 appears in 75% of segments, 5 in 72% and 3 in 67%. The
minor profile is the main contrast with folk music: the raised leading tone 7 (0.33) is far more common
than ♭7 (0.13), while ♭6 (0.40) is much more common than 6 (0.07). That is the harmonic minor scale, the
"classical" sound of minor with its augmented second between ♭6 and 7.

3.5 Prince & Schmuckler (2014): where in the bar
Jon Prince and Mark Schmuckler analysed a large corpus of classical scores and split the counts by
metric position. This directly tests the idea that some degrees are anchors and others are passing notes.

Scale Degrees in Practice · 6

Prince & Schmuckler (2014): major keys, all beats vs downbeats
20%

All beats

Downbeats only

19
16

seton fo erahs

13

12

11

10%

12

1

♭2

2

4

3

2

2

♭3

10

9

9

2

0%

19

18

3

4

3

♯4

3

3

5

♭6

9

8

2

6

8

2

♭7

7

Figure 5. Normalised pitch-class distributions in major keys for all notes versus notes falling on downbeats. On downbeats 1
overtakes 5 and the share of 2 drops: direct evidence that 2 is a 'moving' note.

Across all beats, the major order is 5 > 1 > 2 > 3 > 6 > 7 > 4 . Notice that 2 (12.7%) narrowly beats 3
(12.2%). On downbeats only, it becomes 1 > 5 > 3 > 2 > 4 > 6 > 7 . The tonic rises from 16% to 19% and
overtakes 5, while 2 drops from 12.7% to 11.0% and falls behind 3. The minor-key data shows the same
pattern: 1 rises from 15.1% to 18.3% on downbeats.
This is the most direct evidence for the travelling-note idea. The 2nd is used a great deal, but mostly
between the strong beats. If you want a pattern to feel grounded, put 1 and 5 on the downbeats; if you
want it to feel restless, put 2, 4 or 6 there.

3.6 Albrecht & Shanahan (2013) and Quinn & White (2017)
Joshua Albrecht and Daniel Shanahan built new profiles from a large classical corpus, using only the
opening and closing portions of pieces, where the key is most stable. Their major order is 1 > 5 > 3 > 2 >
4 > 7 > 6 , with the tonic far ahead (23.8%), then 5 (21.4%), 3 (13.7%) and 2 (11.1%). This is the classical
"tonic-first" profile used in Figure 10.
Ian Quinn and Christopher White asked a subtler question: is the profile the same in every key? Using a
very large corpus, they found that it is not quite. Distributions for different keys differ slightly, probably
because instruments, tunings and composers' habits favour some keys for some purposes. The
differences are small (in major, the tonic's share only varies between about 16.6% and 17.5% across
keys), so for practical purposes one profile per mode is fine.

3.7 de Clercq & Temperley (2011): chords in rock
The corpus. Trevor de Clercq and David Temperley took the top 20 songs from each decade (1950s to
1990s) of Rolling Stone's 2004 "500 Greatest Songs of All Time" list. Both authors analysed every song by
ear in Roman numerals and agreed on over 90% of chords. Public Enemy's "Bring the Noise" was
dropped for having no triadic harmony, leaving 99 songs.

Scale Degrees in Practice · 7

de Clercq & Temperley (2011): chord roots in rock (99 songs)
40%

Chord roots
33

sdrohc fo erahs

30%
23

20%

16

10%
0%

4

0

I

♭II

II

3

2

♭III

III

7

4

8
0

0

IV

♯IV

V

♭VI

VI

♭VII

VII

Figure 8. Proportion of all chords built on each root, relative to the tonic, in the Rolling Stone 5×20 corpus. ♭VII is the fourth most
common chord, above vi.

The roots in order are I (32.8%), IV (22.6%), V (16.3%), ♭VII (8.1%), VI (7.2%), ♭VI (4.0%), II (3.6%), ♭III
(2.6%) and III (1.9%). The five most common roots (I, IV, V, ♭VII, VI) make up 87% of all chords. Three
findings stand out:
IV, not V, is the main partner of I. In classical harmony V is the most important chord after I. In rock,
IV is more common overall and is the most common chord both directly before and directly after the
tonic.
♭VII is a core rock chord. It is "quite rare in common-practice music" but appears in 37 of the 99
songs and is the third most common chord before the tonic. The ♭VII–IV–I move, the "Mixolydian
cadence" heard in "Hey Jude", is one of the most frequent three-chord endings.
Root motion is symmetrical. Rising and falling fourths are almost equally common (2,266 against
2,220). Classical harmony strongly favours one direction (as in V to I). Rock moves back and forth; a
two-chord vamp such as I–IV–I–IV is a typical result.
The paper also traces change by decade. Below are the shares of the main roots in each decade's 20
songs. These numbers come from the paper's Table 8 as read by a summarising tool; the overall root
shares above match the published profile exactly, but check this table against the original before you
quote it.
Decade

I

IV

V

♭VII

VI

♭VI

1950s

0.423

0.321

0.221

0.007

0.011

0.001

1960s

0.327

0.239

0.146

0.084

0.072

0.003

1970s

0.302

0.226

0.154

0.089

0.063

0.054

1980s

0.332

0.220

0.170

0.108

0.081

0.047

1990s

0.313

0.187

0.153

0.064

0.100

0.062

In the 1950s, I, IV and V make up over 95% of chords: this is the 12-bar blues era. From the 1960s onward
the "flat side" chords (♭VII, ♭VI, ♭III) grow, which the authors connect with hard rock and heavy metal.
The authors warn that 20 songs per decade is a small sample. Among three-chord progressions ending
on the tonic, the most frequent were IV–V–I, V–IV–I, ♭VII–IV–I, VI–IV–I and ♭VII–♭VI–I.

Scale Degrees in Practice · 8

3.8 Temperley & de Clercq (2013): rock melodies
The corpus. The authors expanded the set to 200 songs by adding the next 101 highest-ranked songs
from the same list, and this time also transcribed the vocal melodies by ear, in scale-degree notation,
checking them by MIDI playback. Six songs with mostly spoken vocals (such as "The Message") have no
melody transcription. Where both authors transcribed the same section, they matched on about 89% of
notes exactly and about 98% within a semitone and an eighth of a bar.
Temperley & de Clercq (2013): rock melodies (200 songs)
40%

Major songs

Minor songs

seton ydolem fo erahs

32

30%
22

19

20%

13

9

10%
0%

17

16

16

0

1

2

0

♭2

5

2

7

3

9
5

1

0

♭3

12

10

4

♯4

1

0

5

♭6

4

1

6

♭7

1

7

Figure 6. Scale-degree distribution of transcribed vocal melodies in the Rolling Stone 200 corpus, split by mode. Values as
published in Vuvan & Hughes (2021) from Temperley's data.

Major-key songs. The order is 1 > 3 > 5 > 2 > 6 > 4 > 7 : 1 at 22.3%, 3 at 19.4%, 5 at 16.9%, 2 at 15.8%, 6 at
11.9%, 4 at 7.1% and 7 at only 3.5%. Compared with folk and classical melodies, rock major melodies:
put 3 ahead of 5;
use 6 much more (11.9%, against 8.2% in folk and 8.0% in classical);
use 4 and 7 much less. These are the two degrees that the major pentatonic scale leaves out. Together
the five pentatonic degrees make up 86% of all notes.
Minor-key songs. The order is 1 > ♭3 > 5 > 4 > 2 > ♭7 > 6 . The tonic is dominant here (31.7%), far more
than in any other corpus. ♭3 (15.9%), 5 (13.1%), 4 (9.7%), 2 (9.0%) and ♭7 (8.7%) follow. The leading tone 7
has almost vanished (0.9%), and the major 3rd still appears 4.6% of the time in minor songs: that is the
blues habit of mixing 3 and ♭3. The paper cites the verse of "Satisfaction", which alternates between the
two. The minor pentatonic degrees make up 79% of notes.

Scale Degrees in Practice · 9

Rock, major-key songs: melody notes vs chord tones
30%

Melody

Harmony

23

22

19

20%
erahs

16

15

10%

0%

0

1

♭2

8

7
2

2

13

12

11

9

0

19

17

0

♭3

0

0

3

4

♯4

5

♭6

1

1

0

0

6

♭7

4

7

Figure 7. In major-key rock songs, the melody leans on 2 and 3 more than the chords do, while the chords carry more 4, 5 and 7.
The gap between the two is the 'melodic–harmonic divorce'.

Melody versus harmony. Figure 7 compares the melody notes of major-key songs with the notes of their
chords. The melody uses more 2 and 3, and the chords use more 4, 5, 7 and ♭7. Temperley developed this
in an earlier article on the "melodic–harmonic divorce" in rock: the vocal often follows its own
pentatonic logic and does not track the chord underneath. Over a I–IV–V progression in a major key, a
singer may sing a minor-pentatonic line with ♭3 and ♭7. Classical theory treats that as a clash; in rock,
soul and funk it is the norm.
Major, minor, or something else. Of the 194 songs with melodies, the authors classified 98 as major in
both melody and harmony and 46 as minor in both. The interesting group is 41 songs that were minor in
melody but major in harmony; only 9 showed the reverse. A statistical analysis of the songs found one
main axis of variation running from the major degrees (3, 6, 7) to the flat degrees (♭3, ♭6, ♭7). Songs
spread continuously along that axis instead of falling into two clean groups. In other words, rock does
not really have two modes. It has a sliding scale between bright and dark. (The 98/46/41/9 split and the
axis analysis come from the paper's second half, which I could only read in summary; the melodic
profiles themselves are the published values.)
Key-finding as a test. The authors used these distributions to make computers guess each song's key.
Melody note counts alone guessed right in 76% of songs, rising to 82% when weighted by duration.
Chord roots weighted by duration and metric position reached 91%. Adding the melody to that
harmonic model reached 97%. Harmony, rhythm and melody each carry key information, and the more
of them you have, the clearer the key.

3.9 Vuvan & Hughes (2021): does rock change what we hear?
Dominique Vuvan and Bryn Hughes repeated the probe-tone experiment with two kinds of context:
classical excerpts and rock excerpts. If listeners learn the hierarchy from what they hear, rock contexts
should call up a rock-shaped hierarchy.

Scale Degrees in Practice · 10

Vuvan & Hughes (2021): probe-tone ratings after classical vs rock contexts
6

Classical context

Rock context

5.38 5.34

4.93

5
4
gnitar

3.33

3

3.39

3.73
3.01

2.65

3.39

3.62

3.95

4.54

3.96 3.99
2.90 2.90

2.83 2.82

3.38 3.21

2.91 2.88

3.03

2.71

2
1
0

1

♭2

2

♭3

3

4

♯4

5

♭6

6

♭7

7

Figure 9. Listener fit ratings after hearing classical versus rock excerpts. The rock profile is slightly flatter (standard deviation
0.76 against 0.83) and rates ♭2, 2, ♭3 and 3 higher.

They did. After rock contexts, listeners rated the tonic just as highly (5.34 against 5.38) but rated 5 lower
(4.54 against 4.93) and rated ♭2, 2, ♭3 and 3 higher. The rock profile is flatter overall, with less distance
between the most and least stable tones. That fits the corpus data: rock uses more of the scale more
evenly, and tolerates "out" notes like ♭3 over major chords. The hierarchy is not fixed; it adapts to style.

3.10 Huron (2006): why frequency becomes feeling
David Huron's Sweet Anticipation brought these strands together into a general theory of expectation,
called ITPRA (imagination, tension, prediction, reaction, appraisal). The relevant idea, from chapter 9, is
scale-degree qualia: each scale degree has a typical feel, such as the stable finality of 1, the "leaning"
restlessness of 7, or the floating quality of 6. Huron argues that these feelings come from statistical
learning. We have heard millions of notes, so our brains know which degree usually follows which. A
frequent and predictable note feels stable; a note that usually moves on to a particular neighbour feels
like it is pointing there.
Huron also reviews the evidence on where each degree appears. Phrase endings are dominated by the
tonic, followed by 5 and 3. Some degrees are strongly tied to a successor: 7 usually goes up to 1, 4 often
falls to 3, and 2 goes to either 1 or 3. That is why a melody that stops on 2 or 7 feels unfinished: the
statistics we have absorbed say it should keep going.
Two other findings from the same tradition are worth knowing. Small steps are much more common
than leaps in melodies across cultures, which is why 2, 4 and 6 get so much traffic between the chord
tones. Large leaps tend to be followed by a change of direction, which von Hippel and Huron (2000)
showed is partly a simple consequence of melodies staying within a range.

3.11 Other sources in brief
Youngblood (1958) applied information theory to scale-degree counts in songs by Schubert,
Mendelssohn and Schumann. It was one of the first studies of this kind and introduced the idea of
measuring a style by the entropy of its distribution (section 4.3).
The McGill Billboard corpus (Burgoyne, Wild & Fujinaga 2011) gives expert chord annotations for
about a thousand songs sampled from the Billboard Hot 100 between 1958 and 1991. Studies using it
show the same trends as the Rolling Stone corpus, including the growth of ♭VII and the dominance of
I and IV.

Scale Degrees in Practice · 11

Allan Moore, Walter Everett, Nicole Biamonte and Christopher Doll have written theoretical
accounts of rock's modal and pentatonic harmony. Moore's 1995 article on the "so-called flattened
seventh" argues that ♭7 in rock is not a borrowed or "wrong" note but part of a Mixolydian or Aeolian
system.
Hooktheory's TheoryTab database is a large crowd-sourced collection of pop transcriptions. It is not
peer-reviewed, but its public "trends" pages show the same top three degrees and the same I, V, vi
and IV chord dominance.

3.12 Side by side
Major keys across corpora (normalised)
30%

Essen folk

Classical (A&S)

Rock melody

seton fo erahs

20%

10%

0%

1

♭2

2

♭3

3

4

♯4

5

♭6

6

♭7

7

Figure 10. Three corpora on the same scale: German folk melodies (Aarden), classical scores (Albrecht & Shanahan 2013) and
rock melodies (Temperley & de Clercq). Same top three, different fingerprints: folk favours 5, classical 1, rock 3 and 6.

Corpus (major keys)

Ranking, most to least common

Fingerprint

Listener ratings (K&K)

1>5>3>4>6>2>7

Pure stability: the triad first.

Folk melodies (Essen)

5>3>1>2>4>6>7

5 on top; stepwise, so 2 is high.

Classical (A&S)

1>5>3>2>4>7>6

Tonic far ahead; 7 above 6.

Classical, all beats (P&S)

5>1>2>3>6>7>4

2 above 3 once passing notes count.

Classical, downbeats (P&S)

1>5>3>2>4>6>7

Anchors only: 1 rises, 2 falls.

Rock melodies (T&dC)

1>3>5>2>6>4>7

Pentatonic: 3 and 6 high, 4 and 7 low.

4. What the data adds up to
4.1 Using a note and landing on it are different things
The biggest lesson is that two hierarchies run in parallel. The frequency hierarchy says which notes fill
the melody, and there 2 is a major player. The stability hierarchy says which notes can end a phrase or
sit on a downbeat without sounding unresolved, and there 2 is weak. The tonic sits on top of both. The
2nd, 4th and 6th are frequent but unstable: they are the "connective tissue". The 7th is both infrequent
and unstable, which makes it the most charged note in a major key.
For composing, this means you have two separate controls: which notes a pattern uses, and where it puts
them. A generative sequence that uses all seven degrees but puts 1 and 5 on beat one of each bar will feel
grounded. The same notes with 2 or 6 on beat one will feel like it is floating or searching.

Scale Degrees in Practice · 12

4.2 Each style has a fingerprint
Folk: 5 is the most common note, and natural minor with ♭7.
Classical: the tonic is strongest, 7 is used more than 6 (the leading tone pulls to the tonic), and minor
keys use harmonic minor (7 and ♭6).
Rock: pentatonic melodies, 3 and 6 strong, 4 and 7 weak, ♭7 normal, ♭VII and IV chords strong, and
melodies that ignore the chords.
You can hear a style partly by its distribution, without any rhythm or timbre. That suggests a deliberate
tool: to shift the stylistic colour of a part, shift its distribution.

4.3 How focused or how spread out: entropy
Entropy, from information theory, measures how spread out a distribution is, in "bits". If one note took
100% of the notes, the entropy would be 0. If all 12 notes were equally likely, it would be 3.58 bits. A
more intuitive version is the "effective number of notes": 2 raised to the entropy. It tells you how many
equally used notes would give the same spread.
Distribution

Entropy (bits)

Effective number of notes

Uniform chromatic (all 12 equal)

3.58

12.0

Uniform diatonic (7 equal)

2.81

7.0

Classical, major (Albrecht & Shanahan)

2.92

7.6

Essen folk, minor

2.98

7.9

Rock melody, minor

2.87

7.3

Rock melody, major

2.80

7.0

Essen folk, major

2.75

6.7

Uniform pentatonic (5 equal)

2.32

5.0

Every measured corpus sits close to or below a flat seven-note scale, even though they use more than
seven notes. The weighting acts as if the music used only about six or seven evenly. Folk major is the
most focused, at about 6.7 effective notes. The flat chromatic case (12 effective notes) is what you get
from a random sequence with no quantizer, and it sounds atonal precisely because it has no hierarchy.
This gives you a single number for a key aesthetic decision on the EP. Detroit techno and most dance
music sit at the focused end, often below the pentatonic line, especially in basslines. Autechre-style
material lives further up, with flatter distributions. Moving a part's entropy up or down over the course
of a track is a way to build and release tension that is completely separate from rhythm, filter or
volume.

Scale Degrees in Practice · 13

5. Genre notes for your EP
Honest limits
No published corpus study gives scale-degree statistics for Detroit techno, IDM, or electronic dance music in
general. Research on electronic dance music, such as Mark Butler's Unlocking the Groove (2006), has focused
on rhythm, meter and form, not pitch. So this section combines the measured findings above with analysis by ear
and common practice in the genres. Treat the remarks about specific artists as listening guides to check with
your own ears, not as measured facts.

5.1 Why loop music changes the picture
The studies above all measure music with phrases and cadences: songs that go somewhere and stop. A
techno track built from loops has a different relationship with the hierarchy, for three reasons.
No cadences. In a loop, the "ending" note of a phrase is immediately followed by the start of the next
repeat. The stability hierarchy still works, but at the level of the loop's downbeat instead of the
phrase ending. Whatever note sits on the first beat of the loop functions as the "home" note, whether
or not it is 1.
Repetition teaches fast. Huron's statistical-learning argument implies that a listener learns a loop's
distribution within a few repetitions. After that, the ear's expectations are set by the loop itself, not
by a general sense of major and minor. This is why a small change, such as one note altered in the
fourth bar, has such a strong effect in techno: it violates a freshly learned expectation.
Pedals and drones. Much techno sits over one bass note or one chord for a long time. The bass note
then acts as an unmovable 1, and everything above it is heard as a degree relative to it. Philip Tagg
calls the two-chord back-and-forth common in loop-based music a shuttle. In both cases the
hierarchy is "flattened" into one stable note plus colour.
The practical result: in techno you design the hierarchy, you do not inherit it. The loop's own
distribution, and where its notes land in the bar, tell the listener where home is. This is liberating,
because it means a hierarchy can be built deliberately and then changed during the track.

5.2 Detroit: minor colours, parallel chords and the "unresolved" sound
Minor and Dorian as the default. Detroit techno, from Juan Atkins's Cybotron and Model 500 records to
Derrick May's Rhythim Is Rhythim and Kevin Saunderson's Inner City and Reese work, overwhelmingly
sits in minor-flavoured territory. Listen for whether the 6th is natural (Dorian: brighter, slightly hopeful)
or flat (Aeolian: darker). The rock data suggests the same thing for popular music in general: minor-key
melodies lean on 1, ♭3, 4, 5 and ♭7, with 2 as colour. The difference in Detroit is that the 9th (the 2nd an
octave up) is often heard held inside a chord, not as a passing note.
Extended chords as a cushion. Pads and stabs in the Detroit tradition often use minor 7th, minor 9th
and minor 11th chords. These extended chords contain 1, ♭3, 5, ♭7, 9 (2) and 11 (4) all at once. That
changes the role of the "unstable" degrees. When 2 and 4 are inside a sustained chord, they stop
sounding like passing notes and become part of a soft, unresolved haze. That haze is a large part of the
"melancholy futurism" often heard in Detroit records. In scale-degree terms, the chord includes almost
the entire minor pentatonic plus 2, so no single note feels like it needs to move.

Scale Degrees in Practice · 14

Parallel chord stabs. Several synths of the 1980s offered a chord memory function: you hold a chord, store
it, and from then on each single key plays that whole chord shape transposed. Playing a riff this way
moves one fixed chord shape around in parallel, a technique called planing. It has a strong effect on
scale degrees: each stab carries its own 1–♭3–5–♭7 relative to its own root, so the music constantly steps
outside a single key. The listener's sense of key comes from the bassline or the most frequent stab root.
Many classic Detroit and house stabs, such as the minor 7th or minor 9th stab moved up a fourth and
back, work this way. On your rig the Hapax chord tools or a fixed chord shape played from Metropolix
through a polyphonic voice will do the same job.
Unresolved two-chord shuttles. A large share of Detroit harmony is a slow alternation of two chords,
often a minor chord and a chord a tone or a fourth away, that never cadences. Compare this with the
rock finding that root motion runs equally in both directions: there is no "goal" chord, only movement
and return. The emotional quality of this is suspension, which is the opposite of the "tension then
release" logic of classical harmony.
Basslines as the anchor. In much Detroit techno the bass carries the hierarchy most clearly: it often uses
very few notes, typically 1 and 5 or 1, ♭3 and ♭7 in a syncopated pattern. Kevin Saunderson's detuned
"Reese" bass, first heard on "Just Want Another Chance" (1988), shows a different approach: the bass is a
sustained, slowly moving pitch whose detuning makes its exact pitch blurry. It still anchors the key but
in a smeared way, leaving room for the pads above.
Electro roots. Juan Atkins's early work with Cybotron drew heavily on Kraftwerk. Kraftwerk-style
sequences tend to use short, repeating, mostly stepwise riffs within a pentatonic or minor scale, with
clear rhythmic placement of the root. The robotic sound comes as much from that tight, focused
distribution as from the sounds themselves.
Strings and melodies above. When a lead line or string figure enters in a Detroit track, it often
emphasises 5, ♭7 and 9 instead of the root. That leaves the root to the bass, so the lead floats above. This
is the dance-music equivalent of the rock melody's freedom from the chords.

5.3 Autechre and Aphex Twin: bending and breaking the hierarchy
Aphex Twin. Much of Richard D. James's best-known melodic material, including Selected Ambient Works
85–92 and pieces like "Avril 14th", is plainly tonal and often diatonic, built from simple modal loops over
a pedal or two-chord shuttle. Its strangeness tends to come from timbre, detuning and rhythm, not from
avoiding the hierarchy. In other work, especially fast drill-and-bass tracks, pitch content becomes far
more chromatic and fragmented, and James has also worked with alternative tunings. A useful way to
study him is to pick a melodic track, transcribe the main loop, count its degrees, and compare its
ranking and entropy with the tables above. You will usually find that his "beautiful" tracks are as focused
as folk melodies.
Autechre. Early Autechre albums such as Incunabula, Amber and Tri Repetae contain lush, unresolved
pad harmonies that owe a debt to Detroit, with the same minor and extended chord colours. From the
late 1990s onward, and especially from Confield (2001), the duo increasingly built music from generative
systems, much of it in Max. Pitch becomes less fixed: notes are chosen by rules and processes instead of
a fixed scale, and timbre and rhythm carry more of the musical identity. In terms of this document,
their later music often has a flatter and shifting distribution: more notes in use, fewer clear anchors,
and a centre of gravity that slowly moves.
Self-mutating patterns, in degree terms. The quality you said draws you to Autechre, patterns that seem
to evolve by themselves, can be described in exactly these terms. A pattern can mutate in at least four
ways:

Scale Degrees in Practice · 15

1. Re-weighting: the same set of notes, but the frequency of each changes over time. The pattern seems
to "lean" in a new direction without any new note.
2. Re-anchoring: the note on the downbeat changes, so the felt home note moves while the material
stays the same.
3. Widening or narrowing: the number of allowed notes grows or shrinks, raising or lowering entropy.
This is exactly what Marbles's STEPS knob does (section 6.1).
4. Rotating: the whole distribution shifts relative to the bass, so the same melodic shape takes on a new
modal colour (Dorian, then Aeolian, then Phrygian).
Each of these is a parameter you can automate or modulate. A track that slowly goes through all four
types of change, at different rates, will feel like it is evolving even though no "new part" has been added.

5.4 Funk, soul, hip hop and reggae
You mentioned these styles as influences, and they share several things with Detroit techno.
Funk often sits on one chord for minutes: a dominant 7th or 9th chord, or a minor 7th. The melody
and bass use the minor pentatonic or Dorian scale (1, 2, ♭3, 4, 5, 6, ♭7), with the ♭7 very prominent
and the major 3rd in the chord rubbing against a ♭3 in the riffs. That is the melodic–harmonic divorce
at full strength. Detroit techno inherited the one-chord, many-layers approach directly.
Soul adds gospel harmony: the IV chord, the ♭VII chord and plagal (IV–I) movement are very
common, as in the rock data, and vocal lines move fluidly between 3 and ♭3.
Hip hop mostly loops a sampled phrase. The phrase's own distribution becomes the track's harmonic
world, and a rapped vocal adds rhythm instead of pitch. That is very close to techno's relationship
with loops. Sampled soul and jazz loops often contain extended chords, which is why they suit the
minor 9th world of Detroit.
Reggae and dub often use major keys and simple I–IV or I–V shuttles, but the bass carries the melody,
using pentatonic lines that land on 1 and 5 at the start of the bar. Dub production removes and
reintroduces parts in real time, which is an arrangement technique, discussed in 5.6.

5.5 Four degree profiles for the EP
Below are four "degree profiles" you could use as starting colours for parts on the EP. The first two come
directly from measured data; the second two are my designs, informed by the data and by the genre
notes above. All are expressed relative to the track's root, so they work in any key.
Profile

Main degrees, by weight

Character

Best for

Anchor

1 and 5 dominant, ♭3 or 3
occasionally

Grounded, hypnotic, low
entropy

Basslines, intros, the
"home" state

Rock minor
(measured)

1 > ♭3 > 5 > 4 > 2 > ♭7, with a little 3
and 6

Bluesy, dark, focused

Leads, hooks, vocal-like
riffs

Detroit Dorian
(designed)

1 > ♭3 = 5 > ♭7 > 2 > 4 > 6

Melancholic but open,
hopeful 6

Pads, chord stabs, string
lines

Drift (designed)

Almost flat across all 12, with slightly
more 1, ♭3, 5

Unsettled, Autechre-like,
high entropy

Breakdowns, transitions,
mutations

Scale Degrees in Practice · 16

5.6 Arrangement: using the hierarchy against loop fatigue
Your main sticking point is arrangement: loops start to feel stale, and it is hard to steer a track
somewhere. Most arrangement advice works on layers (adding and removing parts) and on sound
(filters, effects). Scale-degree thinking gives you a third, independent dimension: the stability of the
pitch material. Because the ear learns a loop's distribution quickly, a change to that distribution is heard
as an event even when the parts and sounds stay the same.
Here is a map for a 6–7 minute track, with the pitch state for each section. It assumes a bass, a lead or
sequence, and a pad or stab layer.
Section

Bars

Pitch state

What it does

Intro

1–32

Bass on 1 only; sequence on Anchor (1, 5)

Teaches the listener where home is.

Build 1

33–64

Sequence widens to Rock minor; pad enters with a min7
chord

More notes, still grounded: interest
without tension.

Groove

65–
128

Pad shuttles between i7 and a second chord (for
example IV7 or ♭VII); sequence moves to Detroit Dorian

The track's main state. Harmonic
movement keeps the loop alive.

Breakdown 129–
160

Bass drops out; sequence switches to Drift and its
downbeat notes move to 2, 4 or 6

Removes the anchor, so the ear loses
home. Maximum tension.

Return

161–
224

Bass back on 1; sequence snaps back to Anchor for 8
bars, then Rock minor

Release: the learned hierarchy is
restored, which feels like arriving home.

Outro

225–
end

Gradually narrow back to 1 and 5

Closes the circle.

Three variations on this map:
The slow drift. Instead of switching profiles at section boundaries, modulate the weights
continuously with a very slow LFO, over 64 or 128 bars. Nothing seems to change at any single
moment, but the track at minute 5 has a different character from minute 1. This is the closest to the
Autechre "self-mutating" quality.
The false home. In the groove section, move the bass to ♭6 or 4 for 16 bars while the sequence stays
the same. Every note of the sequence takes on a new degree relative to the bass, so the melody
changes meaning without changing notes. Return the bass to 1 at the drop.
Downbeat swapping. Keep the note set fixed for the whole track and change only which note sits on
beat one. For example, use 1 on beat one in the verse-like sections and 2 or 6 in the build. This is
subtle, but effective because Prince & Schmuckler's data shows how strongly downbeats carry the
stability hierarchy.

6. Recipes for your rig
6.1 Marbles: teaching it a weighted scale
The Marbles manual explains that when you record a custom scale, the module "analyzes your jam to
measure how frequently each note occurs". When the X section's STEPS knob is turned clockwise from
12 o'clock, the output is quantized to the scale, and as you keep turning it, "the least frequently played
notes will be the first to be eliminated", until "the most frequently played note" is left alone at the fully
clockwise position. In other words, Marbles stores a frequency-weighted hierarchy, and STEPS is an
entropy knob. If you play the scale in plain ascending order when recording, it cannot learn weights
and STEPS will not thin the scale.

Scale Degrees in Practice · 17

Recording procedure (from the manual): connect the CV and gate outputs of a keyboard, MIDI interface
or sequencer to the SPREAD and CLOCK inputs. Hold the external processing button [O] for two seconds;
the scale-select LED blinks to show which of the six slots will be overwritten. Play a "jam" of at least 50
notes, then press [O] again. To restore a slot's factory scale, enter recording mode the same way and hold
[O] for two seconds.
The table below turns the profiles into note counts for a 60-note jam. Play each degree the stated number
of times in any order and octave; the Hapax or Metropolix can play the jam into Marbles for you. The last
column shows the order in which STEPS will remove notes as you turn it clockwise.
Recipe

Notes to play in the jam (any order, any octave)

Total Order STEPS removes them (first →
last)

A. Folk major (Essen data) 5 ×13, 3 ×12, 1 ×11, 2 ×9, 4 ×7, 6 ×5, 7 ×3

60

7→6→4→2→1→3→5

B. Rock minor melody
(T&dC data)

1 ×20, ♭3 ×10, 5 ×8, 2 ×6, 4 ×6, ♭7 ×5, 3 ×3, 6 ×3

61

3 → 6 → ♭7 → 2 → 4 → 5 → ♭3 → 1

C. Detroit Dorian
(designed)

1 ×16, ♭3 ×11, 5 ×11, ♭7 ×7, 2 ×6, 4 ×5, 6 ×4

60

6 → 4 → 2 → ♭7 → ♭3 → 5 → 1

D. Drift (designed, nearly
flat)

1 ×7, ♭3 ×7, 5 ×7, 2 ×6, 4 ×6, ♭7 ×6, 6 ×5, 7 ×5, ♭6
×4, ♯4 ×3, ♭2 ×2, 3 ×2

60

♭2 → 3 → ♯4 → ♭6 → 6 → 7 → 2 → 4 →
♭7 → 1 → ♭3 → 5

Recipes A and B use the published distributions, rounded; very rare degrees (under 2%) are left out so
that the quantizer does not include them. C and D are my designs. Recipe D keeps almost every
chromatic note in play, so with STEPS just past 12 o'clock you get a nearly atonal output, and as you turn
it clockwise the output gradually becomes a minor pentatonic and then the root alone. That single knob
turn performs a "chaos to order" transition.
Patching it as an arrangement tool. Put slow CV on STEPS, for example from a Random8 output through
a slew, or from a very slow LFO on Just Friends or Zadar. Set the knob around one o'clock and the CV
amount so the modulation sweeps between "full scale" and "1, ♭3, 5". The sequence then breathes
between focused and open over 16 to 64 bars. For section changes, send a CV from Live (via CV Tools and
the disting NT) to STEPS and draw the automation in the arrangement view: low values for the
breakdown, high for the drop.

6.2 Metropolix and Hapax: controlling where notes land
Downbeat anchors. In Metropolix, put 1 or 5 on the first stage, and give the passing degrees (2, 4, 6) to
stages that fall off the beat. Use stage probability or skip on the anchoring stage to make the anchor
occasionally disappear, which creates a moment of instability each time.
Re-anchoring with transpose. Use a slowly changing transpose CV in scale degrees (not semitones)
so the whole pattern moves within the scale, changing which degree sits on the downbeat. Rotating a
pattern this way is the "rotating" mutation in section 5.3.
Hapax chords. When you take progressions from the chord playground, write down each chord's root
as a degree (i, ♭VII, IV and so on) and compare against the rock data. A progression like i–♭VII–♭VI–
♭VII sits very close to the rock "flat side" corpus. One like i7–iv7, a minor 7th shuttle, sits closer to
Detroit. Try voicing the chords with 9ths and 11ths to blur the hierarchy, as in 5.2.

6.3 DROID: switching hierarchies with a gate
DROID's quantizer circuits let you choose which notes of the scale are allowed. Set up two note sets: a
narrow "Anchor" set (1, 5, maybe ♭3) and a wide "full" set. Switch between them from a P2B8 button, a
gate from Live, or a clock divider. A button press then moves the part from grounded to open, which is a

Scale Degrees in Practice · 18

performance version of the arrangement map. Check the DROID manual for the exact parameter names
of the quantizer circuit in your firmware version before you write the patch.

6.4 Live and VCV Rack
Count your own clips. Export a MIDI clip and count how many times each degree appears, with and
without duration weighting. Compare the result with the tables in this document. You will learn a lot
about your own habits; for example, you may find that you use 1 much more than any corpus does,
which explains part of the loop fatigue.
Weighted randomness in Live. Live's Random MIDI effect followed by a Scale device produces an
even distribution within the scale (high entropy). To get a weighted one, stack duplicates of the
important notes in a chord-memory or arpeggiator setup, or use note probability in Live clips to
make the anchor notes play more reliably than the passing notes.
VCV. Any quantizer whose note set can be switched by CV, fed by a random source with a weighted
distribution, gives you the same tools. Your Francois Markovian plugin works with note transitions,
which corresponds to Huron's "which degree follows which" statistics in section 3.10: try giving it
transitions where 7 goes to 1, 4 goes to 3 and 2 goes to either 1 or 3, and see whether the output
sounds more "tonal".

7. Exercises
1. Count a classic. Choose one Detroit track whose main riff you can transcribe by ear, for example a
stab pattern or a bassline. Count the degrees and work out the ranking. Is it closer to the Essen,
classical or rock profile? What is on the downbeats?
2. One loop, four profiles. Make a 4-bar Marbles sequence and run it through recipes A to D in turn
without changing anything else. Record each. Which feels most like your EP?
3. Downbeat swap. Take a finished loop and change only the first note of each bar to 2, 4 or 6. Listen to
how the energy changes. Then return it to 1 and listen for the release.
4. Entropy arc. Automate Marbles STEPS through a whole 6-minute arrangement, using the map in
section 5.6. Do not add or remove any parts. How much "arrangement" does pitch alone provide?
5. Divorce test. Put a minor pentatonic lead over a major 7th chord pad and listen for the "soul" sound.
Then reverse it with a major pentatonic lead over a minor 7th pad and compare.

8. Further reading
Aarden, B. (2003). Dynamic melodic expectancy. PhD dissertation, Ohio State University.
Albrecht, J. & Shanahan, D. (2013). The use of large corpora to train a new type of key-finding algorithm. Music
Perception.
Budge, H. (1943). A study of chord frequencies. Teachers College, Columbia University.
Burgoyne, J. A., Wild, J. & Fujinaga, I. (2011). An expert ground-truth set for audio chord recognition and music
analysis. Proceedings of ISMIR.
Butler, M. J. (2006). Unlocking the groove: rhythm, meter, and musical design in electronic dance music. Indiana University
Press.
de Clercq, T. & Temperley, D. (2011). A corpus analysis of rock harmony. Popular Music, 30(1), 47–70.
Gotham, M. et al. AMADS key-profile library (pitch-class usage profiles collected from the literature), musiccomputing.github.io/amads.

Scale Degrees in Practice · 19

Huron, D. (2006). Sweet anticipation: music and the psychology of expectation. MIT Press. Chapter 9 on scale-degree
qualia.
Krumhansl, C. L. (1990). Cognitive foundations of musical pitch. Oxford University Press.
Krumhansl, C. L. & Kessler, E. J. (1982). Tracing the dynamic changes in perceived tonal organization in a spatial
representation of musical keys. Psychological Review, 89(4).
Moore, A. (1995). The so-called "flattened seventh" in rock. Popular Music, 14(2).
Prince, J. B. & Schmuckler, M. A. (2014). The tonal-metric hierarchy: a corpus analysis. Music Perception.
Quinn, I. & White, C. W. (2017). Corpus-derived key profiles are not transpositionally equivalent. Music Perception.
Schaffrath, H. (1995). The Essen folksong collection, ed. D. Huron. Center for Computer Assisted Research in the
Humanities.
Tagg, P. (2009). Everyday tonality. Mass Media Music Scholars' Press.
Temperley, D. (2007). Music and probability. MIT Press.
Temperley, D. (2007). The melodic-harmonic 'divorce' in rock. Popular Music, 26(2).
Temperley, D. & de Clercq, T. (2013). Statistical analysis of harmony and melody in rock music. Journal of New Music
Research, 42(3), 187–204.
von Hippel, P. & Huron, D. (2000). Why do skips precede reversals? The effect of tessitura on melodic structure. Music
Perception.
Vuvan, D. T. & Hughes, B. (2021). Probe tone paradigm reveals less differentiated tonal hierarchies in rock music.
Music Perception.
Youngblood, J. E. (1958). Style as information. Journal of Music Theory, 2(1).
Mutable Instruments. Marbles user manual (scale recording and STEPS sections).

Scale Degrees in Practice · 20

Appendix A: full data tables
All values are in the units of the original study: ratings for Krumhansl–Kessler and Vuvan–Hughes,
percentages for Aarden–Essen and Budge–Bellman, segment proportions for Temperley's Kostka–Payne
profile, and proportions summing to 1 for the others. Prince–Schmuckler values are shown normalised.
Columns are scale degrees relative to the tonic. Source: published profiles as collected in the AMADS
library.
Profile

1

♭2

2

♭3

3

4

♯4

5

♭6

6

♭7

7

Krumhansl–Kessler major

6.350 2.230 3.480 2.330 4.380 4.090 2.520

5.190 2.390 3.660 2.290 2.880

Krumhansl–Kessler minor

6.330 2.680

4.750 3.980 2.690 3.340

Budge/Bellman major

16.800 0.860 12.950

Budge/Bellman minor

18.160 0.690 12.990 13.340

Aarden–Essen major

17.766 0.146 14.927

Aarden–Essen minor

18.265 0.738 14.050 16.860

0.702 14.436 0.702

18.616 4.566 1.932 7.376

Temperley K–P major

0.748 0.060 0.488 0.082

0.670 0.460 0.096

0.715 0.104 0.366 0.057 0.400

Temperley K–P minor

0.712 0.084

0.474

0.618 0.049 0.460 0.105

0.747 0.404 0.067 0.133 0.330

Albrecht–Shanahan major

0.238 0.006

0.111 0.006

0.137 0.094 0.016

0.214 0.009 0.080 0.008

Albrecht–Shanahan minor

0.220 0.006

0.104

0.123

0.019

0.103 0.012

0.214 0.062 0.022 0.061 0.052

Prince–Schmuckler all beats maj

0.160 0.020

0.127

0.025

0.122

0.092 0.037

0.175 0.027 0.095 0.025 0.094

Prince–Schmuckler downbeat maj

0.194 0.017

0.110

0.027

0.119

0.093 0.027

0.190 0.030 0.084 0.024 0.083

Prince–Schmuckler all beats min

0.151 0.026

0.110

0.121

0.028

0.108 0.029

0.173 0.083 0.037 0.081 0.052

Prince–Schmuckler downbeat min

0.183 0.023

0.095

0.117

0.032

0.098 0.029

0.181 0.078 0.032 0.079 0.052

Rock melody major

0.223 0.001

0.158

0.015

0.194

0.071 0.002

0.169 0.003

Rock melody minor

0.317 0.001 0.090

0.159 0.046

0.097 0.007

0.131 0.009 0.047 0.087 0.009

Rock harmony major

0.231 0.002

0.091 0.004

0.149

0.111 0.004

0.193 0.004 0.126 0.011 0.076

Rock harmony minor

0.202 0.006

0.102

0.127

0.047

0.113 0.005

0.177 0.046 0.051 0.090 0.034

Vuvan–Hughes classical

5.380 2.650 3.390

3.010

3.620 3.960 2.830

4.930 2.900 3.380 2.910 3.030

Vuvan–Hughes rock

5.340 3.330

3.730 3.390

3.950 3.990 2.820

4.540 2.900 3.210 2.880

3.520

5.380 2.600

3.530 2.540

3.170

1.410 13.490 11.930 1.250 20.280 1.800 8.040 0.620 10.570
1.070

11.150 1.380 21.070 7.490 1.530 0.920 10.210

0.160 19.805 11.359 0.291 22.062 0.146 8.155 0.233

4.951
1.756

0.081

0.119 0.008 0.035

2.710

Rock chord roots
Profile

I

♭II

II

♭III

III

IV

♯IV

V

♭VI

VI

♭VII

VII

Rock chord roots (de Clercq & Temperley 2011) 0.328 0.005 0.036 0.026 0.019 0.226 0.003 0.163 0.040 0.072 0.081 0.004

Scale Degrees in Practice · 21
```

## Sources
- Project files: scale-degree-frequency.md and scale-degrees-in-practice.pdf (text reproduced above)

Related: [[Gamelan scale degrees in pelog and slendro]], [[Note weightings for house, gospel, funk, soul and reggae]]
