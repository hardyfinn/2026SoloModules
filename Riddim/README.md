# Riddim

Dancehall riddim drum machine for VCV Rack 2 (30 HP). Pick one of 24 riddims and it plays
with that riddim's machine flavour: Casio, Roland 808, Roland PCM, Linn/Oberheim, Simmons,
sampler, workstation or live kit. All ten voices are synthesised, with no samples.

Send it a 16th-note clock. RIDDIM picks the riddim, VAR picks the A or B version, FILL plays
a fill on the last beat of the bar, MUTATE adds ghost notes and drops soft ones.

See `docs/Riddim-Guide.pdf` for the full list and every control. The riddim names, producers
and years are checked where marked in the guide; the machines are shown with a `?` (red on
the screen) wherever no source names the box. Patterns are by-ear reconstructions of each
riddim's feel, not transcriptions.

Build: `make dist RACK_DIR=...`. Engine checks and demo WAVs without Rack: `make test`.
MIT licence.
