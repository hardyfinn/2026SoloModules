# Panorama

Steel pan voice for VCV Rack (Hær). Tenor, double second or triple cello.
Every note area is a set of tuned modes (fundamental, octave, fifth above the
octave, plus untuned pings) that rings into the areas around it. Patch a poly
GATE + V/OCT (Peal works), or click a note on the screen.

Real pans are never perfectly in tune: DETUNE and PARTIALS scale each note's
own fixed random error, RE-ROLL gives a different pan, and FUND / OCT / 5TH
hand-tune the note picked on the screen. Set both to 0 for exact tuning.

* Engine: `src/PanoramaDSP.hpp` (no Rack dependency)
* `make test` renders `panorama-demo.wav` and runs a stability check
* Panel: `python3 res/make_panel.py` (needs fonttools); preview: `python3 tools/preview.py out.png`
* Guide: `docs/guide.html`
