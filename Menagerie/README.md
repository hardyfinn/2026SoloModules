# Menagerie

Sixteen sound engines behind one small set of controls, in the spirit of a
"macro oscillator" but with none of the usual algorithms: nothing here comes
from Mutable Instruments Plaits or Braids. 18 HP, polyphonic (up to 16
voices), MIT licence.

## Controls

| Control | What it does |
|---|---|
| MODEL | Picks the engine (1-16). The MODEL input adds 0.5 V per step. |
| FREQ | +-4 octaves around C4. V/OCT adds to it (polyphonic). |
| SHAPE, TEXTURE | The two timbre macros. The screen says what they do in each engine. |
| LIFE | How much the engine changes by itself over time (0 = stays put). |
| DECAY | Envelope decay, or ring time for the struck engines. |
| CV trimmers | Amount (+-) for the SHAPE, TEXTURE and LIFE inputs. |
| TRIG | Patched: triggers or gates play notes through a built-in envelope; gates hold. The struck engines (Bubbles, Spring, Bounce, Tine) are hit instead. Unpatched: the voice drones and struck engines strike on every new note. |
| LEVEL | 0-8 V (normalled to 8 V): volume, and how hard notes are struck. |
| MAIN / AUX | Two outputs; AUX is a different side of each engine. |

## Engines

| # | Engine | SHAPE | TEXTURE | LIFE | AUX |
|---|---|---|---|---|---|
| 1 | Gendy (Xenakis random-walk wave) | number of points | roughness | wander | soft low-passed copy |
| 2 | Herd (8 coupled oscillators) | spread | pull together | stray | the herd as sines |
| 3 | Duffing (driven nonlinear spring) | one well to two wells | drive, into chaos | unrest | spring velocity |
| 4 | Scanned (masses on springs, read at pitch) | rest shape | tension | invisible hand | movement only |
| 5 | Walsh (square-function additive) | formant | brightness | drift | formant alone |
| 6 | PLL (CD4046-style loop) | multiple 1-8 | loop speed | divider slips | XOR comparator |
| 7 | Bytebeat | formula (8) | shift | bit flips | 4-bit |
| 8 | Automaton (rule-based cells) | rule | generation speed | mutate | previous generation |
| 9 | Bubbles | size spread | rise | density | stereo partner |
| 10 | Spring (dispersion chirps) | dispersion | tone | drips, wobble | middle tap |
| 11 | Bounce (dropped ball) | bounciness | ball: rubber to steel | uneven floor | clicks only |
| 12 | Howl (mic feedback) | distance | gain | walking the mic | mic signal |
| 13 | Tine (electric piano) | hammer | pickup: mellow to bell | tremolo | other tremolo side |
| 14 | Tonewheel (organ) | drawbars (8 settings) | dirt: click, leakage, drive | vibrato, then chorus | other scanner side |
| 15 | Pad (PWM + BBD chorus) | saw to thin pulse | chorus | PWM, detune | chorus right |
| 16 | Harmonium (free reed) | stops 8' to 16'+8'+4' | pressure | bellows, celeste | other side |

## Building

    make RACK_DIR=path/to/Rack-SDK dist     # the plugin
    make test                               # engine checks + test/menagerie-demo.wav
    python3 res/make_panel.py               # panel (needs fonttools)
