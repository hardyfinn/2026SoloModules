# Weighted (VCV Rack, Apple Silicon Mac)

A quantizer that picks notes by weight: random CV in, notes out, with strong
scale degrees chosen more often. Profiles come from scale-degree research
(folk, classical, rock, listening tests) and from gamelan pathet roles, with
Focus, accents, Step, Learn, and slendro/pelog tunings (Scala loading).

- Install: drop `Weighted-2.4.0-mac-arm64.vcvplugin` into
  `~/Library/Application Support/Rack2/plugins-mac-arm64/` and restart Rack. (2.4.0 is 20 HP with labels twice the size, and merges the Root, Scale, Rel, Profile and Focus inputs into two assignable inputs, Mod A and Mod B. 2.3.0 added the Paper panel, an Octave knob, the Relative switch and gate, a circle-of-fifths Root knob and an Auto CV range, and fixes Step so 7 can step up to 1 and notes keep their weights. 2.2.0 added drawing weights, phrase ending, drift and chord output; 2.1.0 added house, gospel, funk, soul and reggae. Keep only the newest file.)
- Guide: `Weighted-Guide.pdf` (or `Weighted-Guide.html`).
- Source and automatic builds: https://github.com/hardyfinn/Weighted
  (newest build always at Releases > latest).
- Background: `research/scale-degrees-in-practice.pdf` and
  `research/gamelan-weights-and-seleh-quantizer.pdf`.
