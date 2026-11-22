# Cicada Fable

A VCV Rack 2 module that models singing insects: cicadas, crickets, katydids, grasshoppers and wingbeats.

Design stage. Not built yet.

- `docs/BUILD-INSTRUCTIONS.md`: the build specification.
- `docs/RESEARCH.md`: research, measurements and the proposal.
- `tools/proto/cicada_ref.py`: the reference engine the C++ must match. `presets.py` holds the factory presets, `render_ref.py` renders them and the patched demos (`python3 render_ref.py outdir`, needs numpy, scipy, numba, soundfile).
