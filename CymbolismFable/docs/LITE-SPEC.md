# Cymbolism Lite: build spec

A third module in the CymbolismFable plugin. It is Cymbolism with the four costly
control ranges narrowed and a lighter engine, so that Rack's Randomize can never land
on a setting that swamps the CPU. Same engine code, same sound inside the shared range.
Plugin version becomes 2.4.0.

## 1. Why these limits (measured)

120 randomized settings, a strike every 0.5 s for 3 s, percent of one cloud Xeon core
(`tools/cpu-bench/rnd.cpp`, see section 8):

| Version | Mean | 90th pct | Worst |
|---|---|---|---|
| Cymbolism, HIGH, full ranges | 44 | 64 | 83 |
| Lite ranges, HIGH engine | 38 | 57 | 65 |
| **Lite ranges, MID engine (Lite default)** | **31** | **46** | **52** |
| **Lite ranges, ECO engine (Lite option)** | **23** | **34** | **40** |

Cost model fitted to the runs: about 16 points per 1,000 modes, 5 per 1,000 coupling
terms, up to 10 for rivets, 10 to 15 for BOW (continuous contact). Mode count is set by
SIZE (larger = more modes under 20 kHz) and WEIGHT (thinner = more modes). Coupling
terms are set by the quality tier. FLANGE, TENSION and PROFILE limits were tested and
gained 0 to 4 percent, so they stay at full range.

## 2. Identity

- Slug `CymbolismLite`, name `Cymbolism Lite`, tags Drum, Physical modeling, Synth voice.
- Description: "Cymbolism on a CPU budget: the same physical cymbal with size, weight,
  rivets and beaters kept inside a range that stays light however the knobs are set or
  randomized".
- 40 HP, the same layout, jacks and screen as Cymbolism. Panel title reads
  `CYMBOLISM LITE`. Everything else on the panel is identical except the range marks
  listed in section 3. Text labels, Hær logo, as on Cymbolism.

## 3. What differs from Cymbolism

Only these. Every other parameter, input, default, menu item and behaviour is the same.

| Control | Cymbolism | Lite | Notes |
|---|---|---|---|
| SIZE knob | 0.6 to 1.5 | 0.6 to 1.2 | default 1.0. After SIZE CV the engine value clamps to 0.35 to 1.2 (Cymbolism: 0.35 to 2.0) |
| WEIGHT knob | 0.5 to 2.5 | 0.8 to 2.5 | default 1.0. The geometry clamp after CV is also 0.8 to 2.5 |
| SIZZLE knob | 0 to 12 rivets | 0 to 6 | CV range 10 V = 6 rivets; clamp 0 to 6 after CV. The engine still receives rivets / 12 |
| PLAY switch | 6 beaters | 5: Tip, Shoulder, Mallet, Rods, Brush (no Bow) | PLAY CV spreads 0 to 10 V across the five; clamp 0 to 4 |
| Quality menu | ECO, MID, HIGH, ULTRA | "MID (default)", "ECO (lightest, top end to 16 kHz)" | saved codes stay 3 and 0; any other code on load becomes 3 |

Consequences to handle:

- Type defaults already fit: no type has more than 6 rivets or BOW as its default play,
  and `applyTypeDefaults` sets WEIGHT to 1.0. Assert this at start-up in debug builds
  rather than assuming it.
- ROLL still rolls (tip, shoulder, mallet, rods) and sweeps (brush). Tooltips for ROLL,
  RATE, FORCE and PLAY CV must not mention the bow.
- The screen shows at most 6 rivets and never the bow.
- `dispRivets` is still `round(sizzle * 12)`.

## 4. Implementation

Do not copy Cymbolism.cpp. Parameterise it.

- Add to `src/Cymbolism.cpp` a small limits struct and make the module read it
  everywhere a range appears:

  ```cpp
  struct CyLimits {
      float sizeMax, sizeCvMax, weightMin;
      int sizzleMax, numPlays;
      int defaultQuality;      // quality code
      bool lite;
  };
  static const CyLimits FULL = {1.5f, 2.0f, 0.5f, 12, NUM_PLAYS, 1, false};
  static const CyLimits LITE = {1.2f, 1.2f, 0.8f, 6, NUM_PLAYS - 1, 3, true};
  ```

  `Cymbolism` gains `const CyLimits& lim` and a constructor `Cymbolism(const CyLimits&)`;
  the default constructor delegates with `FULL`. `struct CymbolismLite : Cymbolism` has a
  default constructor passing `LITE`. The places that use the limits: `configParam` for
  SIZE, WEIGHT, SIZZLE; `configSwitch` for PLAY (label list cut to `numPlays`);
  `readGeometry` (weight clamp); `readControls` (size clamp, sizzle clamp and CV range);
  `playNow`; `dataFromJson` (quality); the quality submenu; tooltips.
- `CymbolismWidget` takes the panel path the same way; `CymbolismLiteWidget` loads
  `res/CymbolismLite.svg`. Param, input, output and light ids are identical, so the
  widget code is shared.
- Cymbolism itself must behave exactly as before: the existing tests and presets must
  pass unchanged, and `quality` must still default to 1 there.
- `plugin.hpp` / `plugin.cpp`: declare and add `modelCymbolismLite`.
- `res/make_panel.py`: take a variant argument and write `res/CymbolismLite.svg` with
  the new title and the changed range marks, without disturbing `res/Cymbolism.svg`
  (regenerating must leave it byte-identical). Add the SVG to DISTRIBUTABLES if it is
  listed per file. `tools/preview.py` renders `panel-preview-lite.png`.
- No engine changes. `CyEngine.hpp`, `CyModes.cpp`, `CyRunner.hpp` are not edited.

## 5. Presets

20 factory presets in `presets/CymbolismLite/`, from `tools/make_presets.py`:

- Start from the 20 Cymbolism presets. Keep each one whose values already sit inside
  the Lite ranges, clamping SIZE, WEIGHT and SIZZLE where needed.
- Replace any preset that relies on BOW, on a SIZE above 1.2 or on a WEIGHT under 0.8
  for its character with a new one that makes sense in Lite (for example a rod roll, a
  small tight splash, a dry heavy ride). Keep the spread: several rides, crashes,
  chinas, effects, and at least three ROLL presets.
- The generator must assert that every Lite preset is inside the Lite ranges.
- `test/preset_check` renders all 60; each must be finite, audible and not clipping,
  as for the existing 40. Render an audition WAV to
  `/mnt/project-files/cymbal-vcv/presets/presets-lite.wav`.

## 6. Manual

`docs/lite-guide.html` and `docs/Cymbolism-Lite-Manual.pdf`, in the style of the
Cymbolism manual, short: what Lite is and what it leaves out (the table in section 3
in plain words), the CPU table from section 1 with the note that these are cloud-core
figures and a Mac will read lower, a paragraph on randomizing, the control reference
(may point to the Cymbolism manual for the unchanged controls but must list all
controls with ranges), five patch recipes that suit Lite (at least one built around
Randomize, for example a trigger sequencer plus sample-and-hold into the CV inputs),
and the preset table between `<!--PRESETS-->` markers. Add a line about Lite to the
other two manuals' introductions and to README.md.

## 7. Tests

Add `test/lite_check.cpp` and a `make test` entry. It uses the engine directly, with
the Lite limits written as constants shared with the module where practical.

- L1 ranges: 300 random draws mapped through the Lite ranges, with CV at both extremes
  (+10 V and -10 V on SIZE, WEIGHT, SIZZLE, PLAY). Engine size never above 1.2, weight
  never under 0.8, rivets never above 6, play never BOW.
- L2 stability: 100 random Lite settings at MID and at ECO, hard strikes, 3 s each.
  Finite and bounded output.
- L3 mode ceiling: over the same draws, record the largest active mode count. Fail if
  it exceeds 2,600 (measured worst was about 2,230).
- L4 CPU, reported and loosely asserted: the same 60 random draws rendered with full
  ranges at HIGH and with Lite ranges at MID. Print mean and worst for both. Fail only
  if the Lite mean is above 0.85 of the full mean (measured ratio 0.70) so that a noisy
  machine does not fail the build.

All existing tests must still pass.

## 8. Benchmark tool

`tools/cpu-bench/rnd.cpp` is the benchmark behind section 1. Build:

    g++ -O3 -march=nehalem -funsafe-math-optimizations -std=c++17 -pthread -Isrc \
        tools/cpu-bench/rnd.cpp src/CyModes.cpp -o /tmp/rnd

`rnd sweep` gives per-control costs. `rnd random 120 0 quality=3 sizeMax=1.2
weightMin=0.8 sizzleMax=6 playMax=4` reproduces the Lite row (second argument 1 also
randomizes the geometry knobs, as with "Keep geometry when TYPE changes" on). After the
build, rerun the full and Lite rows and put the fresh numbers in the manual.

## 9. Delivery

Branch `claude/project-thread-fjq3bu`, Apple silicon build from CI, preview release.
Copy the manual PDF, the panel preview and the preset WAV to
`/mnt/project-files/cymbal-vcv/`. No model names in the repository. Do not merge to main.
