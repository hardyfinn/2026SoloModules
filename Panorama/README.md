# Panorama

Steel pan voice for VCV Rack (Hær). Tenor, double second or triple cello.
Patch a poly GATE + V/OCT (Peal works), or click a note on the screen.

The engine (2.2) is fitted to recordings of a real Trinidad tenor pan. Every
note area has eight harmonics (1 2 3 4 5 6 8 10). The stick mostly excites the
fundamental; the upper harmonics are fed through the note's own shell
nonlinearity (fifteen sum resonances, each note voiced differently), so soft
hits are pure, hard hits are bright, and the brightness swells in after the
hit. Hard hits start sharp and settle (GLIDE), every note rings about 1.3 s,
and part of each hit goes into the neighbouring notes through the steel
(NEIGHBOURS). The stick is a rubber tip on a stiffening spring landing on the
moving note.

Real pans are never perfectly in tune: DETUNE and PARTIALS scale each note's
own fixed random error, RE-ROLL gives a different pan, and FUND / OCT / 5TH
hand-tune the note picked on the screen. Set both to 0 for exact tuning.

* Engine: `src/PanoramaDSP.hpp` (no Rack dependency)
* `make test` renders `panorama-demo.wav` and runs a stability check
* Panel: `python3 res/make_panel.py` (needs fonttools); preview: `python3 tools/preview.py out.png`
* Guide: `docs/guide.html`
