# Groover

Ableton grooves for VCV Rack drums. Groover loads Live groove files (`.agr`,
plain or gzipped) or MIDI files (`.mid`), or uses one of its 14 built-in
grooves, and moves your drum hits onto the groove, the way Live's Groove Pool
does for clips.

20 HP. MIT licence.

## Two ways to patch it

1. **Grooved clock.** Send a straight 16th clock to CLOCK and drive your
   sequencer from the CLOCK output. Each step is pushed or pulled to where the
   groove puts it. Groover predicts the tempo from the incoming clock, so steps
   can land *early* as well as late.
2. **Trigger lanes.** Keep your sequencer on the straight clock (CLOCK still
   needs the same clock) and run up to 8 trigger or gate lines through IN 1-8.
   OUT 1-8 are the same hits moved to the groove. Gate lengths are kept.
   Early hits land on the grid unless you turn on *Lane look-ahead* in the
   right-click menu (which delays every lane by that amount).

ACCENT gives the groove's velocity for the current step (0-10 V) and LOOP
fires at the start of every pass of the groove.

## Controls

| Control | What it does |
|---|---|
| GROOVE | Picks the groove: built-ins first, then loaded files (marked `*`) |
| AMOUNT | Live's Global Amount, 0-130 % |
| TIMING | How strongly hits move to the groove |
| VELOCITY | How much of the groove's loudness reaches ACCENT; negative flips it |
| RANDOM | Per-hit timing wobble, up to a quarter of a step |
| QUANTIZE | Lanes only: pulls loose input hits to the grid before grooving |

Each knob has a CV input (±5 V covers the whole range).

Right-click menu: load a file or a folder, save the current groove as `.agr`
for Live, show the built-in grooves as `.agr` files, copy a groove's own
Groove Pool settings to the knobs, clock PPQN (1-48, default 4 = 16ths),
Base override (1/4 … 1/32), lane look-ahead. You can also drop `.agr` / `.mid`
files on the panel. Loaded grooves are saved inside the patch.

## Building and testing

    make test          # offline engine checks, no Rack needed
    make dist          # needs the Rack SDK (RACK_DIR=...)
    python3 res/make_panel.py   # redraw the panel (needs fonttools)

`grooves/` holds the built-ins as `.agr` files (made by
`GROOVER_EXPORT=grooves ./test/engine_test`).
