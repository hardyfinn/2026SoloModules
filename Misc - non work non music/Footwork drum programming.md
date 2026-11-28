---
title: Footwork drum programming
date: 2026-11-26
source: Misc - non work non music · thread "Write footwork drum programming guide"
topic: Music
tags: [footwork, juke, drum-programming, rhythm, vcv-rack]
summary: |
  Footwork sits at about 155–165 BPM (160 standard), mixing a half-time backbone with double-time hats, rolls and chops.
  The core is the 3-3-2 grid (kicks on steps 1, 4, 7, 9, 12, 15 of 16) pushing against a steady quarter pulse; keep 808 kicks sparse.
---

# Footwork drum programming

## Summary
Footwork sits at about 155–165 BPM, mixing a half-time backbone with double-time hats, rolls and chops.
The core is the 3-3-2 grid (accents on steps 1, 4, 7, 9, 12 and 15) pushing against a steady quarter pulse.

## Question
> Make an HTML guide on programming drum patterns in the style of footwork.

## Answer
The footwork drum programming guide is done and published as a private artifact: https://claude.ai/artifact/CTChbKofEiQki757ZweXtG. It covers tempo and feel, the 3-3-2 grid, kicks, snares and rolls, hats, classic sounds, swing, fills, arrangement and mistakes. It has four 16-step grids (the 3-3-2 map, basic, intermediate, roll/fill) and a VCV Rack setup section. It works on phones and in dark mode. I didn't render or preview the page, so layout is unchecked. The VCV section is generic, so it doesn't name specific modules. Only you can open it until you share it from the page's Share menu.

### Guide text (from project file guides/footwork-drum-programming.html)

*The guide's 16-step grids are flattened to text here.*

### Tempo and feel

Footwork sits at roughly **155–165 BPM**, and 160 is the standard starting point. The dancers footwork to the fast count, while the track often feels like a slow 80 BPM groove underneath. Both readings are valid, and the best patterns let you hear either.

Feel | What it means at 160 | Typical hits |

Half-time (80 BPM) | Clap or snare lands once per two beats, so the track feels spacious and heavy. | Clap on step 9 only |

Double-time (160 BPM) | Claps and hats run at the full pace, so the track feels frantic. | Clap on steps 5 and 13 |

Most tracks mix them: a half-time backbone with double-time hats, rolls and chops running over it. At 160 BPM a 16th-note step lasts about 94 ms, which is why programming accurately and keeping hits sparse both matter.

### The 3-3-2 grid

Work on a 16-step bar of 16th notes. Instead of splitting it into four groups of four, think of it as **3 + 3 + 2 + 3 + 3 + 2**. Accents land on steps 1, 4, 7, 9, 12 and 15. Against a steady quarter-note pulse on 1, 5, 9 and 13, those accents drift in and out of alignment, and that tension is the footwork feel.

The same 16 steps felt two ways: quarter pulse against 3-3-2 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
3-3-2 accents | ● |  |  | ● |  |  | ● |  | ● |  |  | ● |  |  | ● |  |
Quarter pulse | ● |  |  |  | ● |  |  |  | ● |  |  |  | ● |  |  |  |

● hit · ghost (soft, about 30–50% velocity)

The shaded columns mark each beat. Notice that the 3-3-2 accents hit the beat only on steps 1 and 9. The rest fall in between, so they sound like they are pushing against the clap.

### Kick patterns

- **Start with 3-3-2:** steps 1, 4, 7, 9, 12, 15. This is the signature pattern and the safest one.

- **Thin it out:** drop steps 12 and 15 for a heavier, half-time feel.

- **Move one hit:** shift a single kick by a step (for example 12 to 11) to create variation without breaking the pulse.

- **Keep step 1 on the downbeat** in most bars so dancers and listeners can find where the bar starts.

- Kicks are long, boomy 808s, so leave a gap after each one. Dense kicks turn to mush.

### Snares, claps and rolls

- **Half-time:** one clap on step 9.

- **Double-time:** claps on steps 5 and 13. A snare layered under the clap adds snap.

- **Offbeat snare hits** on steps such as 8 or 16 add the "stumbling" feel. Keep them quieter than the main clap.

- **Rolls** are fast snare or clap repeats, usually 16ths at the end of a bar or phrase. Build velocity upward through the roll so it feels like it is accelerating.

### Hats and percussion

- **Closed hat:** 8th notes (odd steps) as a base. Add 16ths for energy, and lower the velocity of the in-between hits.

- **Open hat:** sparingly, on steps 8 or 16, to open up the end of a half-bar. Choke it with the next closed hat.

- **Cowbell and woodblock:** place them on 3-3-2 steps (4, 10) so they reinforce the polyrhythm.

- **Shakers and rim clicks:** keep them quiet and fill the gaps between hats.

### Classic sounds

Sound | Role | Tip |

808 kick | Low-end anchor | Short decay for dense patterns, longer for sparse. Tune it to the key of the track. |

Toms (808-style) | Fills and accents | Pitch them down in steps across a fill. Do not leave them at the same pitch. |

Cowbell | Top-line hook | Use one clean sample and keep it dry. |

Clap / snare | Backbone | Layer a clap over a short snare for body. |

Vocal chops | Melodic hook | Short stabs and cut-up phrases, often on off-beats. Sample a few words and loop one syllable. |

Hats | Drive | Bright and short, with the in-between hits quieter. |

### Swing and ghost notes

Footwork is mostly **straight and robotic** on the grid. The groove comes from placement, not swing, so keep global swing low or off. Where you want looseness, add it by hand:

- **Ghost notes** are very quiet extra hits (30–50% velocity) on snare, hat or percussion, placed just before or after a main hit.

- Try a light swing of around 52–55% on hats only, leaving kicks and claps straight.

- Vary velocities by hand. Identical velocities across a bar sound mechanical in a bad way.

### Fills and stutters

- Fill the **last half bar of a 4 or 8-bar phrase**, and leave the other bars alone.

- Drop the kick during the fill to create space, then bring it back on step 1 of the next bar.

- Run a snare or clap roll into the bar line, with rising velocity.

- Stutter a vocal chop by retriggering it on four consecutive 16ths. Pitch it up or down a step each time for extra movement.

- Use toms for a descending run: for example steps 13, 14, 15 and 16 at falling pitches.

### Example patterns

Each grid is one bar of 16 steps at 160 BPM. Treat them as starting points and change one or two hits at a time.

#### 1. Basic

Basic: kick on the 3-3-2 pulse, clap on 5 and 13 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
Kick | ● |  |  | ● |  |  | ● |  | ● |  |  | ● |  |  | ● |  |
Clap |  |  |  |  | ● |  |  |  |  |  |  |  | ● |  |  |  |
Closed hat | ● |  | ● |  | ● |  | ● |  | ● |  | ● |  | ● |  | ● |  |
Cowbell |  |  |  | ● |  |  |  |  |  | ● |  |  |  |  |  | · |

● hit · ghost (soft, about 30–50% velocity)

#### 2. Intermediate

Intermediate: syncopated kick, ghost snares, tom and a chop answering the clap | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
Kick | ● |  |  | ● |  |  | ● |  |  | · | ● | ● |  |  | ● |  |
Clap / snare |  |  |  |  | ● |  |  | · |  |  |  |  | ● |  |  | · |
Closed hat | ● | · | ● | ● | ● | · | ● |  | ● | · | ● | ● | ● | · | ● |  |
Open hat |  |  |  |  |  |  |  | ● |  |  |  |  |  |  |  | ● |
Tom |  |  |  |  |  |  |  |  |  |  |  |  |  | ● |  |  |
Cowbell |  |  |  | ● |  |  |  |  |  | ● |  |  |  |  |  |  |
Vocal chop |  |  | ● |  |  |  |  |  |  |  | ● |  |  |  |  |  |

● hit · ghost (soft, about 30–50% velocity)

#### 3. Roll / fill

Roll / fill (last bar of a phrase): kick drops out, snare roll builds into step 16 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
Kick | ● |  |  | ● |  |  | ● |  |  |  |  |  |  |  |  |  |
Snare roll |  |  |  |  |  |  |  |  | ● | · | ● | · | ● | ● | ● | ● |
Tom |  |  |  |  |  |  |  |  |  | ● |  | ● |  |  |  |  |
Hat (16ths) | ● | ● | ● | ● | ● | ● | ● | ● |  |  |  |  |  |  |  |  |
Vocal stutter |  |  |  |  |  |  |  |  |  |  |  |  | ● | ● | ● | ● |

● hit · ghost (soft, about 30–50% velocity)

**Use them together.** Play the basic or intermediate bar for three or seven bars, then drop in the roll bar to close the phrase.

### Programming it in VCV Rack

Any 16-step gate sequencer works. The setup is the same in every module.

- **Clock:** set a clock module to **160 BPM** and take a **16th-note output** (4 times the beat clock, which is 640 pulses per minute). Some clocks have a ratio or multiplier setting for this.

- **Sequencer length:** set the sequencer to **16 steps** and wire the 16th clock into its clock input. Wire the clock's reset into the sequencer reset so every bar starts on step 1.

- **One gate row per voice:** use one 16-step gate row or one sequencer instance per drum voice (kick, clap, hat, perc). Wire each gate output to that drum module's trigger input.

- **Enter the hits:** copy the steps from the grids above. A lit step is a gate.

- **Velocity and ghosts:** if the sequencer has per-step values or probability, use low values on the ghost-note steps and feed them to the drum module's level or velocity input. If it does not, use a separate quieter hat or snare voice for ghosts.

- **Fills:** run a second sequencer holding the roll pattern, and use a sequential switch or a counter that selects it on bar 4 or 8. Alternatively, mute rows by hand using a mute or gate-mixer module.

- **Chokes:** send the closed-hat gate to the open hat's choke input, if the drum module has one.

**Check by ear.** Solo kick and clap first. The kick pulse should feel like it is pushing against the clap before you add anything else.

### Arrangement tips

- Footwork tracks are loop-based and typically **repeat in 4-bar and 8-bar phrases**. Change one element every 4 bars.

- Open with just hats and one chop, then bring in the kick and clap.

- Strip drums out completely for a bar before a drop. Silence emphasizes the next kick.

- Keep the sub-bass and kick from fighting: duck the bass on kick hits, or give it its own steps.

- Build variation using the vocal chop and top percussion, and leave the kick and clap pattern consistent for dancers.

### Common mistakes

- **Too many kicks.** The 808 tail smears. Space them out or shorten the decay.

- **Making it swing like house or hip-hop.** Keep the grid tight.

- **Constant rolls.** Rolls lose impact when they happen every bar. Save them for phrase ends.

- **Flat velocity.** Vary hat and ghost levels.

- **Losing step 1.** If nothing marks the downbeat, the bar feels lost.

- **Clap on every beat.** That turns it into a straight four-on-the-floor feel. Choose half-time or double-time and stay with it.

- **Same pitch toms.** Descending or ascending pitch makes a fill feel intentional.

Patterns are starting points, not strict rules. Footwork thrives on bending them.

## Sources
- <https://claude.ai/artifact/CTChbKofEiQki757ZweXtG>
- Project file: guides/footwork-drum-programming.html
