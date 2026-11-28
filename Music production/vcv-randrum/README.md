# Randrum (VCV Rack, Apple Silicon Mac)

A drum sample player that rolls dice on every hit. Load a folder of WAV or
AIFF files (subfolders included); each trigger picks the sample, pitch, decay,
start, level, pan, direction and whether it plays at all. LOOP and MUTATE keep
the rolls as a 1-16 step pattern that repeats and slowly rewrites itself.

- Install: drop `Randrum-2.3.0-mac-arm64.vcvplugin` into
  `~/Library/Application Support/Rack2/plugins-mac-arm64/` and restart Rack.
  (2.3.0 adds pattern memory A-D, morph and ratchets, and is 28 HP wide; 2.2.0 added VARIETY; 2.1.0 fixed folder loading and added subfolders and AIFF. Keep only the newest file in your plugins folder.)
- Guide: `Randrum-Guide.html`.
- Source and automatic builds: https://github.com/hardyfinn/randrum
  (newest build always at Releases > latest).
