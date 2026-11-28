# Tiers

A gate pattern generator for VCV Rack. 30 HP, plugin 2.0.0, MIT. Untested in Rack so far.

You decide **how often each level of the bar plays**, and Tiers builds the pattern.

| Knob | Steps it covers (1/16 grid, one bar) |
|---|---|
| **1/2** | 1 and 9 (the downbeat and the half bar) |
| **1/4** | 5 and 13 (beats 2 and 4) |
| **1/8** | 3, 7, 11, 15 (the "and"s) |
| **1/16** | every even step (the "e" and "a" 16ths) |
| **1/32** | on the 1/16 grid, the chance that a hit doubles into two 32nds; on the 1/32 grid, the odd 32nd steps |

Every step keeps its own dice roll. A step plays when its roll is under its level's knob, so
turning a knob up adds hits on that level one by one, and turning it down takes the same hits away.
**GEN** throws new dice.

## Controls

- **HITS** (COUNT mode): exactly this many hits, placed where the weights say. **MODE** switches CHANCE / COUNT.
- **ACCENT**: how many hits are accented. Strong positions get accents first. Presets sit at 50%.
- **ROTATE**: slides the pattern along the grid.
- **MUTATE**: at the end of each loop, re-rolls some steps (up to a quarter of them at 100%). The weights still apply, so it wanders in style.
- **LOCK** (button latches, or a high gate): mutation stops. **BACK**: return to the last generated or loaded pattern.
- **PRESET** + **LOAD**: pick a figure and load it. The weight knobs, HITS and STEPS move to match, so GEN and MUTATE carry on in that style. Also in the right-click menu by genre.
- **STEPS** 16 / 32, **STEP** 1/16 / 1/32 grid, **SWING** 50-75% (delays every second step).
- Screen: click a step to toggle it, Shift-click for an accent, Ctrl-click for a 32nd double. Hand-placed steps ignore the knobs and MUTATE (right-click > Clear hand edits).
- Right-click > Gate length: 10, 25, 50 (default) or 90% of a step.

CV inputs under the top row: the five weights, HITS, ACCENT and ROTATE (+-5 V = +-50%, +-16 hits; ROTATE 0-10 V = whole length).
Inputs: CLOCK (one pulse per step), RESET, GEN, MUTATE CV, LOCK gate, BACK, PRESET CV (0-10 V = whole list), LOAD.

Outputs: **GATE**, **TRIG** (1 ms per hit and per 32nd double), **INV** (gates on the rests),
**ACC**, **LEVEL** (held CV of the level the last hit came from: 1/2 = 10 V, 1/4 = 8, 1/8 = 6, 1/16 = 4, 1/32 = 2),
**EOC** (end of cycle).

## Patch ideas

- Kick, clap and hats from three Tiers on the same clock: the kick with 1/2 and 1/4 at 100%, the clap with only 1/4, the hats with 1/8 and some 1/16. Add MUTATE on the hats only and LOCK when a bar is good.
- LEVEL into a VCA or a filter cutoff for velocity that follows the metre.
- INV into an open hat so it fills only the gaps.

## Presets

Common single-lane figures, written on a 16th grid (X accent, x hit, r/R with a 32nd double, - rest).
They are generic figures, not transcriptions of records.

**House**

| Preset | Steps |
|---|---|
| Four to the floor kick | `X---X---X---X---` |
| Offbeat open hat | `--X---X---X---X-` |
| Clap on 2 and 4 | `----X-------X---` |
| 16th hats, offbeat lift | `x-Xxx-Xxx-Xxx-Xx` |
| Shuffle hat | `--xx--xx--xx--Xx` |
| Chicago jack kick | `X---X---X-x-X---` |
| Deep house clave | `X--x--x---x-x---` |
| Shaker push | `x-xX-xx-x-xX-xxx` |
| Organ stab push | `---X--x---X--x--` |
| Ghost snare groove | `----X--x----X-xx` |
| Clap + bar-2 pickup | `----X-------X-------X-------X-xx` |

**Techno**

| Preset | Steps |
|---|---|
| Rumble kick | `X-xxX-xxX-xxX-xx` |
| Detroit 16th hat | `xxXxxxXxxxXxxxXx` |
| Rolling bass (no downbeat) | `-xxx-xXx-xxx-xXx` |
| 8th ride | `X-x-X-x-X-x-X-x-` |
| Tresillo 3-3-2 | `X--x--x-X--x--x-` |
| 3-3-3-3-4 stab | `X--x--x--x--x---` |
| Electro kick | `X-----x---x-----` |
| Dotted 8ths over 2 bars | `X--x--x--x--x--x--x--x--x--x--x-` |
| Kick with bar-2 drop | `X---X---X---X---X---X---X-------` |
| Clap on 4 only | `------------X---` |
| Ratchet hat | `--X---R---X---Rr` |
| Minimal perc | `---x--x----x-x--` |

**Funk**

| Preset | Steps |
|---|---|
| Breakbeat kick A | `X-x-------x--x--` |
| Breakbeat kick B | `X-x-------xx----` |
| Breakbeat snare + ghosts | `----X--x-x--X--x` |
| Funk 16th hat | `XxxxXxxxXxxxXxxx` |
| Bass on the one | `X------xx-x--x--` |
| Guitar scratch 16ths | `-x-xx-x--x-xx-x-` |
| Son clave 3-2 | `X-----x-----x-------x---x-------` |
| Rumba clave 3-2 | `X-----x-------x-----x---x-------` |
| Bossa clave | `X-----x-----x-------x-----x-----` |
| Cowbell funk | `X-x-x-X-x-xxx-x-` |

**Disco**

| Preset | Steps |
|---|---|
| Disco kick | `X---X---X---X---` |
| Disco hat, open offbeats | `x-X-x-X-x-X-x-X-` |
| Disco snare + pickup | `----X-------X-xx` |
| Tambourine 16ths | `xxxxXxxxxxxxXxxx` |
| Octave bass | `x-X-x-X-x-X-x-X-` |
| Philly string stab | `---X---X---X--X-` |
| Disco fill, bar 2 | `X---X---X---X---X---X---XxxxXxXx` |
| Conga run | `---x-xx----x-xxX` |

## Building

`make test` checks the engine offline (every preset reproduces exactly, weights only add hits as they rise,
COUNT gives exact counts). `python3 res/make_panel.py` redraws the panel (needs fonttools),
`python3 tools/preview.py out.png` renders a picture with Rack's component graphics.
