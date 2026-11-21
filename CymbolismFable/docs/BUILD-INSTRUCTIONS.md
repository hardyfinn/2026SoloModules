# Cymbolism Fable: build instructions

A single VCV Rack module that physically models one cymbal at a time (crash, ride, china, splash, flat ride, bell and more; no hi-hats) for the most realistic sound we can get on a CPU. Written on 2026-11-21 by Opus 5.5 (this thread ran on Opus throughout, not Fable) for the build.

Hardy's decisions (2026-11-21):
- One module. Mono output. Monophonic (one cymbal ringing at a time).
- Best possible sound; high CPU cost is acceptable.
- As many controls with a large effect on the sound as possible.
- A choice of how the cymbal is played (sticks, mallets, brushes, rods, bow).
- An external exciter input.
- Name: **Cymbolism Fable**. It lives in `hardyfinn/2026SoloModules` in its own folder.

House conventions (from the repo README and Hardy's other plugins):
- Folder `CymbolismFable/` with its own `plugin.json`, `Makefile`, `src/`, `res/`, `docs/`, `test/`, `tools/`, laid out like `Hangover/` on branch `claude/project-thread-u8dufv` (read that folder first and copy its structure, panel generator, Hær logo, guide and PDF tooling).
- Plugin slug `CymbolismFable`, plugin name `Cymbolism Fable`, brand `Haer`, version `2.0.0`, GPL-3.0-or-later. Module slug `Cymbolism`, module name `Cymbolism Fable`, tags `Drum`, `Physical modeling`, `Synth voice`.
- Text labels on the panel (Rack can't draw SVG text, so `make_panel.py` turns labels into paths). Control labels 3.2 mm, jack labels 2.9 mm, Hær logo on the bottom rail.
- Apple silicon Mac build only; the repo CI already builds every folder with a `plugin.json`.
- A guide at `docs/guide.html` with patch recipes, printed to `docs/Cymbolism-Fable-Manual.pdf` with `tools/pdf.js`.
- `make test` builds and runs `test/render.cpp`, which runs the engine offline (no Rack) and writes the checks and demo WAVs below.
- Add a row for Cymbolism Fable to the repo `README.md` table.
- Branch: this design thread's branch is `claude/project-thread-fjq3bu`. The repo has no `main` yet; every other module lives on its own branch. Build on `claude/project-thread-fjq3bu` unless Hardy says otherwise.

---

## 1. The idea in one paragraph

A cymbal is a thin bronze shell with a domed bell. Struck softly it rings like a dense bank of inharmonic resonators. Struck hard, its large-amplitude bending stretches the shell, and that stretching couples every mode to every other: energy pours from the low modes into the high ones over 20 to 200 ms, the "bloom" or "wash" of a crash, and curvature makes pitches glide. Cymbolism Fable computes the cymbal's real vibration modes from its geometry (size, thickness, taper, bow profile, bell, flange), runs the low and middle modes as a **fully coupled nonlinear shell** (geometrically exact for a shallow shell, energy-conserving time stepping), runs the remaining modes up to 20 kHz as a large linear modal bank fed by a physics-based **energy cascade**, and strikes it all with a **physical contact model** for each beater (stick tip, shoulder, mallet, brush, rods, bow). Rivets, choke, tape, the mount and the swing of the cymbal on its stand are physical too. Nothing is sampled.

Why one module and not several: every part (beater, shell, rivets, mount, pickup) shares the same mode set and the same time step, and the beater and rivets push back on the shell. Splitting into modules would mean passing hundreds of modal states between modules a sample apart, which Rack's cables can't do. One module gives the better model.

---

## 2. Physics model

### 2.1 Geometry

The cymbal is a shell of revolution, described in polar coordinates (r, θ), outer radius `a`, centre hole radius `r0` (6.5 mm).

- **Profile** `z(r)`: height of the mid-surface above the edge plane.
  - Bell, `r0 ≤ r ≤ rb`: a steep dome, `z = Hb_total - hb*(r/rb)^2` style curve that joins the bow smoothly (C1).
  - Bow, `rb ≤ r ≤ a`: a shallow spherical cap with rise `Hbow` at `rb` falling to 0 at `a`.
  - China flange, `rf ≤ r ≤ a` (china type only): the profile turns upward again (upturned edge), with flange angle `phi_f` (default 15 degrees upward from the bow tangent) and width `a - rf` (default 14% of `a`). Smooth (C1) join.
  - Use a smooth monotone spline (cubic Hermite through control points) so all types share one code path.
- **Thickness** `h(r)`: edge thickness `he`, rising toward the centre by the **taper** ratio `T` (`h(rb) = T*he`, linear or smooth in r), and in the bell `h = Tb*he` (bell thickness factor, default 1.15*T).
- **Material**: Young's modulus E, density ρ, Poisson ν, internal loss factor η(f). See ALLOY in section 4.

Mount: the centre hole sits on a felt washer and sleeve. Model the inner edge `r = r0` as an elastic, lossy support: transverse spring `k_m` and rotational spring `c_m` in parallel with dampers, set by the MOUNT control. Outer edge free.

### 2.2 Shallow shell equations (Marguerre / Donnell, nonlinear)

Displacements: transverse `w`, in-plane `u_r`, `u_θ`. Strains of the mid-surface (shallow shell, z depends on r only, primes are d/dr):

```
e_rr = u_r,r + z' w,r            + 1/2 w,r^2
e_tt = u_r/r + u_t,t/r                     + 1/2 (w,t/r)^2
g_rt = u_t,r - u_t/r + u_r,t/r + z' w,t/r  + w,r w,t/r
```

Bending curvatures:

```
k_rr = -w,rr
k_tt = -(w,r/r + w,tt/r^2)
k_rt = -2 (w,t/r),r
```

Energies (per unit area, integrated over r dr dθ):

```
Membrane U_m = Eh/(2(1-ν^2)) * [e_rr^2 + e_tt^2 + 2ν e_rr e_tt + (1-ν)/2 g_rt^2]
Bending  U_b = D/2 * [k_rr^2 + k_tt^2 + 2ν k_rr k_tt + (1-ν)/2 k_rt^2],   D(r) = E h(r)^3 / (12(1-ν^2))
Kinetic  T   = ρ h(r)/2 * w_dot^2      (in-plane inertia neglected: static condensation)
```

This one model gives everything that matters: curvature stiffening (the `z' w` terms), quadratic nonlinearity (softening, pitch glide, the china's trashiness), cubic nonlinearity (von Kármán stretching, the crash bloom), with arbitrary profile and thickness.

### 2.3 Discretisation into modes (the geometry solver)

Angular Fourier decomposition: every field is expanded in `cos(mθ)` and `sin(mθ)`. For each angular order `m = 0..M`:

1. **Bending modes.** Radial finite elements on `[r0, a]`: C1 cubic Hermite elements for `w` (DOFs `w`, `w,r` per node), 60 to 120 elements graded finer near the edge and bell join. Assemble bending stiffness `K_b(m)` (from `U_b`, plus mount springs at r0) and mass `M(m)` (from `T`), with Gauss quadrature (4 points per element). Solve `K_b φ = ω^2 M φ` and keep the M-normalised eigenvectors. For m > 0 each eigenvector is a degenerate pair (cos and sin).
   - **The modal basis is these bending-only modes.** All membrane effects, linear and nonlinear, go into the nonlinear potential of section 2.4. This keeps the linear part diagonal and puts curvature stiffening in the energy-conserving scheme, which is exact in the limit and second-order accurate at audio rate.
2. **Membrane condensation.** For each angular order `m_s` of the in-plane field, radial elements for `u_r`, `u_t` (quadratic Lagrange elements are fine). The membrane strain is `e = B_u u + e_w(w)` where `e_w` collects the `z' w` and `1/2 w^2` terms. Minimising `U_m` over `u` (static condensation) leaves `U_m*(w) = 1/2 e_w^T P e_w`, where `P = C - C B_u (B_u^T C B_u)^-1 B_u^T C` at the quadrature points, `C` the membrane constitutive matrix times quadrature weights. Eigen-decompose `P = Σ λ_n v_n v_n^T` per angular order and keep the `Nψ` largest-λ components overall (these are the "stress modes", the discrete Airy functions).
3. **Coupling coefficients.** Write the shell displacement as `w = Σ_p q_p φ_p(r) trig(m_p θ)`. Then each stress mode amplitude is

   ```
   η_n(q) = Σ_p A_n,p q_p + Σ_{p,q} B_n,pq q_p q_q
   A_n,p  = sqrt(λ_n) v_n^T e_lin(φ_p)        (curvature term z' w; only nonzero when the shell is curved)
   B_n,pq = sqrt(λ_n) v_n^T e_quad(φ_p, φ_q) / 2
   ```

   The θ integrals are done analytically: `B_n,pq` is nonzero only when the angular orders satisfy `m_n = |m_p ± m_q|` with matching cos/sin parity, and `A_n,p` only when `m_n = m_p`. Store `A` and `B` sparse (CSR per n), symmetric in (p, q).
4. **Nonlinear potential** (all membrane energy, always ≥ 0):

   ```
   V(q) = 1/2 Σ_n η_n(q)^2
   ∇V(q) = Σ_n η_n(q) (A_n + 2 B_n q)
   ```

   Cost per evaluation: for each stress mode, one sparse mat-vec `B_n q` plus two dot products. This is the expensive inner loop; make it SIMD friendly (pack per-n nonzeros contiguously, float32 with double accumulation only where tests show it is needed).

Validation of the solver is mandatory before any sound work (section 8, tests T1 to T4). Write it first as a Python reference (`tools/shell_ref.py`, numpy/scipy) and then port it to C++ (Eigen, vendored header-only under `dep/eigen`), and check the C++ results against the Python ones.

### 2.4 Three tiers of modes

Sort the bending modes by frequency. With an 18" medium crash there are roughly 1,500 modes (counting pairs) below 20 kHz, so:

| Tier | What | Default (HIGH) | ECO | ULTRA |
|---|---|---|---|---|
| **Core** | Fully coupled nonlinear modes (section 2.3) | `Nc = 128` modes, `Nψ = 64` stress modes | 64 / 32 | 224 / 112, run at 2x oversampling |
| **Upper** | Linear modes above the core, up to 20 kHz, exact FEM modes up to angular order `M` | up to 1,800 modes | up to 900 | up to 2,400 |
| **Cascade** | Energy transfer from core to upper (section 2.6) | on | on | on |

The angular order `M` for the FEM must reach the 20 kHz modes (M around 60 to 90 for big cymbals). If solving all `M` exact problems is too slow on the background thread (target under 400 ms on M1 for HIGH), compute exact FEM modes up to the core plus 400 modes, and generate the rest with the asymptotic formula: local-thickness plate wavenumber `k(r) = (ρ h ω^2 / D)^(1/4)`, mode frequencies from the WKB condition `∫ sqrt(k(r)^2 - m^2/r^2) dr = (n + 1/2)π` (between turning points), shapes from WKB amplitude. Document which you used.

Profile before tuning: report the measured CPU of each quality level at 48 kHz in the README. Target for HIGH: under one performance core on an M1 (the engine runs on Rack's engine thread). If HIGH is over budget, lower `Nc` before `Nψ` and say so.

### 2.5 Time stepping (energy-conserving, explicit)

Use the **scalar auxiliary variable (SAV)** method (Bilbao, Ducceschi and Zama, "Explicit exactly energy-conserving methods for Hamiltonian systems", J. Comput. Phys. 2023; read it). It is explicit, unconditionally stable for a nonnegative potential, and needs only a rank-one solve per nonlinearity.

State: modal displacements `q` (core and upper together; upper modes simply have zero coupling), the beater and rivet DOFs (section 2.7, 2.8), and one auxiliary scalar `ρ_j` per nonlinear potential `V_j` (membrane, each beater contact, each rivet).

Per sample, with step `k = 1/fs` (or `1/(2fs)` for ULTRA's core):

```
g_j   = ∇V_j(q^n) / sqrt(2 V_j(q^n) + ε)            ε = 1e-12 (scaled to the model's energy units)
lin   = exact-frequency leapfrog per mode:  k^2 ω~_i^2 = 2 - 2cos(ω_i k)
loss  = centred:  σ_i (q^{n+1} - q^{n-1}) / k
(1 + σ_i k) q^{n+1}_i = 2 q^n_i - (1 - σ_i k) q^{n-1}_i - k^2 ω~_i^2 q^n_i
                       - k^2 Σ_j g_j,i (ρ_j^{n+1} + ρ_j^n)/2 + k^2 f_i^n
ρ_j^{n+1} = ρ_j^n + 1/2 g_j · (q^{n+1} - q^{n-1})
```

Substitute the second line into the first: `q^{n+1}` is the solution of `D q + (k^2/4) G G^T q = rhs` with `D` diagonal and `G` a few columns (one per active nonlinearity). Solve with Sherman-Morrison (one column) or Woodbury (a small dense solve, size = number of active contacts + 1). Skip inactive contacts (no penetration and zero potential) to keep the system tiny.

Leapfrog with the exact-frequency `ω~` keeps every linear mode at its true frequency. Modes with `ω_i k` above about 2.8 rad (fs = 44.1k: above ~19.6 kHz) are dropped.

Every few hundred samples, re-anchor each `ρ_j` toward `sqrt(2V_j + ε)` by a tiny amount (the papers discuss drift control) so that `ρ_j` cannot wander from the true potential over long rings; check this against test T5.

Denormals: flush to zero on the engine thread. Keep the state in float32 where accurate; use double for `ρ_j` and the dot products feeding them.

### 2.6 The cascade (core to upper modes)

The core modes carry the exact nonlinear dynamics, but in a real cymbal they also hand energy directly to the thousands of modes above them. The cheap, physics-based stand-in, energy conserving:

1. **Outflow from the core.** Track the energy `E_top` held in the top 20% of the core (by frequency). Add a nonlinear extra damping to those modes, `σ_out = κ * BLOOM * (E_top / E_ref)`, where `E_ref` is the energy scale at which the nonlinearity becomes strong for this geometry (compute it from the geometry: the energy at which the membrane potential equals the bending energy for the lowest axisymmetric mode, with amplitude about one thickness). Energy removed per sample, `ΔE_out`, goes to step 2.
2. **Band energies.** Split the upper tier into 1/3-octave bands from the core's top frequency to 20 kHz. Band `b` holds the energy of its modes, `E_b` (sum of modal energies, computed every 32 samples). Energy moves from band `b` to `b+1` at rate `T_b = Γ * BLOOM * E_b^2 / E_ref` (four-wave interaction scaling for plates; Düring, Josserand and Rica 2006). Band 0 receives `ΔE_out` from the core.
3. **Applying it to real modes.** A band that loses energy gets extra damping on its modes, `σ_b = T_b / (2 E_b)`. A band that gains energy receives it as a random force on each of its modes (independent white noise per mode, variance set so the injected power equals the incoming `T`), so the energy arrives as ringing, inharmonic modes, not as noise.
4. **High-mode shimmer.** The low stress modes also modulate the stiffness of the upper modes. Use the axisymmetric (m = 0) stress amplitudes `η_0..η_3` from the core to frequency-modulate upper modes: `δω_i/ω_i = Σ_s c_s η_s` with `c_s` from the same membrane integrals evaluated for a representative subset of upper modes and interpolated by frequency. Cheap and gives the shimmering pitch wobble in the highs after a hard crash.

Calibrate `κ` and `Γ` once (constants in code, not knobs) so that a hard crash shows the high-frequency peak 20 to 80 ms after the strike and a soft tap shows none (test T7). `BLOOM` (section 4) scales the whole nonlinearity.

At soft levels the cascade is effectively off and the upper modes ring only from the direct strike, as a linear cymbal does.

### 2.7 Beaters (how it gets played)

All beaters touch the shell at the strike point `x_s = (r_s, θ_s)` (θ_s fixed at 0; r_s from POSITION). The shell displacement there is `w_s = Σ_i φ_i(x_s) q_i`, using all modes (core and upper).

**Contact model** (TIP, SHOULDER, MALLET, RODS): the beater is a mass `M_b` with vertical position `y`, launched toward the shell at speed `v0` (from FORCE / VEL). Penetration `δ = y - w_s` (positive when pressing in). Contact potential `V_c = K/(α+1) [δ]_+^(α+1)` goes into the SAV system as its own potential; loss is Hunt-Crossley, `F_loss = λ K [δ]_+^α δ_dot`, applied centred. After release the beater flies away (no double hits unless the physics makes one, which a slack mallet sometimes should).

The contact is spread over a small patch (radius `rc`): average φ over 3 to 7 points inside the patch (precomputed per strike position), which naturally rolls off very short-wavelength modes, as a real broad contact does.

| PLAY | M_b | K | α | λ | rc | Notes |
|---|---|---|---|---|---|---|
| TIP | 8 g (effective) | 3e7 to 3e8 N/m^α (HARDNESS) | 1.5 | 0.1 to 0.3 s/m | 1.5 mm | wood or nylon tip; ride ping |
| SHOULDER | 25 g | 1e7 to 1e8 | 1.5 | 0.3 | 5 mm | shank on the edge, crash; POSITION defaults toward the edge |
| MALLET | 30 g | 1e4 to 1e6 | 2.8 | 0.6 to 1.2 | 12 mm | felt or yarn; soft swells |
| RODS | 7 x 2 g | 1e7 | 1.5 | 0.3 | 1 mm each | 7 dowels spaced over 8 mm, arrival times spread 0.1 to 1.5 ms (random per hit) |
| BRUSH | grains | | | | | see below |
| BOW | friction | | | | | see below |

Effective masses and stiffnesses are starting values; tune by ear against test T6 (contact time 0.3 to 1.5 ms for a stick, 3 to 10 ms for a mallet).

**BRUSH**: 30 to 60 wire contacts per stroke, each a half-sine force pulse 40 to 150 µs long, random amplitude, random position inside a 25 mm patch around `x_s`, spread over 5 to 25 ms (FORCE sets energy, HARDNESS sets wire stiffness = pulse length and count). While the ROLL gate is high the brush **sweeps**: a continuous grain stream (rate from RATE) whose position moves around the cymbal in a slow arc. For grains, project onto modes using a precomputed table of φ on a 48 (radius) x 32 (angle) grid.

**BOW**: a bass bow on the edge (r = 0.99 a, regardless of POSITION; POSITION moves it from the edge inwards a little). Friction force `F = F_N φ_f(v_rel)` with the smooth friction curve `φ_f(v) = sqrt(2β) v exp(-β v^2 + 1/2)` (Bilbao, Numerical Sound Synthesis, ch. 7), `v_rel = v_bow - w_dot_s`. Solve the scalar implicit equation for `v_rel` per sample with Newton (2 to 4 iterations) using the scheme's effective mobility at the bow point (Sherman-Morrison gives it). Bow pressure `F_N` from FORCE (or VEL), bow speed from RATE, bowing while the ROLL gate is high. A good bowed cymbal gives a sustained, slowly shifting chord of 1 to 3 strong modes with occasional jumps between them.

**ROLL** (gate) for the contact beaters: repeated strikes at RATE (4 to 30 per second) with alternating "hands" (two slightly different positions and ±15% force), humanised timing (±8% jitter). With MALLET this makes the classic cymbal swell; ramp the VEL input for a crescendo.

**EXCITE input** (external exciter): audio voltage applied as a force at the strike point, `f_i += φ_i(x_s) * g_exc * V_in`, with EXC level knob. It goes through the whole nonlinear model, so a loud drum loop blooms the cymbal like a hard hit, and a quiet sine barely rings it. Use ±5 V nominal; FORCE also scales it.

### 2.8 Rivets and other contacts

**SIZZLE**: N rivets (0 to 12) in holes at r = 0.82 a, spread evenly in θ with small random offsets. Each rivet is a mass `m_r` (0.4 g) resting on the shell under gravity, free to bounce up, one-sided contact with the shell: `V_r = K_r/2.5 [w(x_r) - y_r]_+^2.5`, plus gravity force on `y_r`. Each rivet is an SAV potential like the beater. SIZZLE also sets looseness (how much the rivet's contact stiffness and loss vary per rivet). A rivet head striking the shell excites all modes at its point: the real sizzle buzz, which grows and dies with the cymbal's own motion.

**CHOKE** (gate and button): a hand grabbing the edge. Extra modal damping `σ_choke,i = S * e_i` where `e_i` is mode i's mean squared shape over the outer 15% of the radius (precomputed), ramped in over 15 ms, plus a few soft contact pulses at the start (fingers landing). While the gate stays high the cymbal stays held. A short gate (under 40 ms) is a "grab and release" that leaves a quiet tail.

**DAMP** (tape or a gel pad): a pad at r = 0.85 a. Constant extra damping `σ_damp,i = DAMP * d_i` with `d_i` mode i's squared shape averaged over a 30 mm patch. Large effect: dries out the wash and shortens the tail, as gaffer tape does.

### 2.9 Mount and swing

MOUNT sets the felt and wing nut: tight = stiff, lossy support at the centre (low modes damped, a shorter, drier sound) up to loose = soft support with little loss (longer low ring). MOUNT also sets how freely the cymbal **swings** on the stand. Model swing as a damped rigid-body pendulum tilt `ψ(t)` (natural frequency 1.5 to 4 Hz depending on size; damping from MOUNT), kicked by each strike in proportion to the strike impulse and how far out the strike is. The tilt changes the pickup (section 2.10) by rotating the mic's view around the cymbal, so a hard crash on a loose stand wobbles in tone as it swings, like a real one.

### 2.10 Pickup (mono output)

Sound pressure at the microphone is the sum of modal velocities weighted by pickup weights, then shaped by radiation:

```
out = Σ_i W_i(mic, ψ) * q_dot_i
W_i = R(f_i) * Σ_j a_j φ_i(x_j)
```

- `x_j` are pickup points on the shell surface: for a close mic, 3 points inside a 40 mm patch below the mic; for a far mic, 24 points spread over the whole surface with weights `a_j` that approximate the far-field directivity (area-weighted, with a sign convention that makes low-order modes partly cancel, as a dipole does).
- `R(f)`: radiation efficiency of a thin plate below coincidence, `R(f) = sqrt((f/f_co) / (1 + f/f_co))` times a low-frequency edge-cancellation high-pass (second order, at `c / (2πa)`), where `f_co = c^2/(2π) * sqrt(ρh/D)` at the edge thickness, `c` = 343 m/s.
- MIC (one knob) morphs: BELL (close, above the bell) → BOW (close, mid-bow) → EDGE (close, over the edge, bright and trashy) → OVERHEAD (far, 1 m above) → ROOM (far, 3 m, with gentle air absorption: 1-pole lowpass at 9 kHz to 14 kHz depending on distance). Interpolate weights between neighbouring settings; recompute weights at control rate (every 32 samples) and smooth.
- Swing tilt ψ shifts the close-mic patch along θ and r (small) and modulates the far weights.

Output: ±5 V nominal at a medium hit, with a soft limiter at ±10 V (tanh-style, only kicks in above 8 V). OUT LEVEL is a context-menu trim (−12 to +12 dB), not a knob.

---

## 3. Cymbal types (TYPE)

TYPE selects the geometry family. When TYPE changes, the geometry knobs (SIZE, WEIGHT, TAPER, PROFILE, BELL) jump to the type's defaults, unless the context-menu option "Keep geometry when TYPE changes" is ticked. These numbers are starting points; tune them by ear against reference recordings and note any changes in the README.

| TYPE | Size | Edge thickness | Taper (centre/edge) | Bow rise (% of radius) | Bell diameter (% of size) | Bell height | Defaults for play |
|---|---|---|---|---|---|---|---|
| SPLASH | 10" | 0.55 mm | 1.6 | 6% | 30% | 12 mm | SHOULDER, edge |
| CRASH | 18" | 0.85 mm | 1.7 | 7% | 25% | 18 mm | SHOULDER, edge |
| THIN CRASH | 17" | 0.65 mm | 1.5 | 6% | 24% | 16 mm | SHOULDER, edge |
| CRASH RIDE | 20" | 1.10 mm | 1.8 | 8% | 23% | 20 mm | TIP, bow |
| RIDE | 21" | 1.40 mm | 1.9 | 8% | 22% | 22 mm | TIP, bow |
| HEAVY RIDE | 22" | 1.90 mm | 1.8 | 9% | 26% | 26 mm | TIP, bow |
| FLAT RIDE | 20" | 1.10 mm | 1.4 | 1.5% | none | none | TIP, bow |
| CHINA | 18" | 0.75 mm | 1.5 | 5% plus upturned flange | 22% (straight-sided) | 22 mm | SHOULDER, edge |
| SWISH | 20" | 0.90 mm | 1.5 | 5% plus flange | 20% | 18 mm | TIP, bow; 6 rivets |
| BELL | 8" | 3.0 mm | 1.2 | 25% | 60% | 25 mm | TIP, bell |

Changing TYPE does not change PLAY, POSITION or SIZZLE by itself; the "Defaults for play" column is applied only from the context-menu item "Load type's playing defaults".

---

## 4. Controls

Every knob has a CV input unless noted (10 V = full range, added to the knob). Geometry controls rebuild the cymbal (section 5).

### Cymbal (geometry; rebuild the model)

| Control | Range | Effect |
|---|---|---|
| **TYPE** | 10 types (snap knob, name on display) | geometry family |
| **SIZE** | 6" to 24" | overall pitch and modal density; display shows inches |
| **WEIGHT** | 0.4 mm to 2.6 mm edge thickness | thin = low, washy, very nonlinear, quick bloom; heavy = higher, pingy, glassy |
| **TAPER** | 1.0 to 2.5 | thin edge vs thick centre: crashy edge with a solid ride centre |
| **PROFILE** | 0% to 14% bow rise | flat = dry, controlled, little glide; high = brighter, more pitch glide and a stiffer low end |
| **BELL** | 0 to 2 (x type's bell size) | bell size and height: from no bell to a big, loud, pitched bell |
| **FLANGE** | −20 to +25 degrees | edge turned down (negative) or up (positive, china). Large effect on trashiness. No CV |
| **ALLOY** | B20, B12, B8, BRASS, NICKEL SILVER, STEEL (switch) | E, ρ, ν and internal loss: B20 dark and long, B8 bright and short, brass and nickel silver cheaper-sounding, steel harsh and long. No CV |
| **HAMMER** | 0 to 1 | hammering irregularity: splits degenerate pairs (up to 2.5%), jitters frequencies (up to ±3%), varies per-mode damping, adds small symmetry-breaking couplings in B (random, seeded). 0 = a perfect machine-spun disc; 1 = a hand-hammered, complex, darker cymbal |
| **NEW CYMBAL** (button) | | re-rolls the hammering pattern seed (saved with the patch); the same settings give a different but similar cymbal, as two cymbals of one model do |

### Sound and damping (no rebuild)

| Control | Range | Effect |
|---|---|---|
| **DECAY** | 0.25x to 4x | scales internal loss; very long rides to short trashy splashes |
| **BLOOM** | 0 to 2 | scales the nonlinearity (membrane coupling in the core and cascade rates). 0 = a linear cymbal (glassy, bell-like); 1 = physical; 2 = exaggerated, roaring wash |
| **DAMP** | 0 to 1 | tape or gel pad |
| **MOUNT** | tight to loose | felt and wing nut: centre damping, low-mode support, how much it swings |
| **SIZZLE** | 0 to 12 rivets (stepped), looseness rises with count | rivets |
| **MIC** | BELL, BOW, EDGE, OVERHEAD, ROOM (continuous morph) | where the listener is |

### Playing

| Control | Range | Effect |
|---|---|---|
| **PLAY** | TIP, SHOULDER, MALLET, RODS, BRUSH, BOW (switch) | beater; no CV (use the context menu or a future expander) |
| **POSITION** | bell centre (0) to edge (1) | which modes are excited and how hard the nonlinearity is driven; bell = clear bell tone, edge = full crash |
| **FORCE** | 0.2 to 12 m/s (log) | strike speed when VEL is unpatched; with VEL patched, sets the speed at 10 V. Bow: bow pressure. EXCITE: input gain |
| **HARDNESS** | 0 to 1 | beater stiffness within its type: soft wood to nylon; soft yarn to hard felt; brush wire stiffness |
| **RATE** | 2 to 30 Hz | roll rate; brush sweep speed; bow speed |
| **EXC** | 0 to 1 | external exciter level |
| **STRIKE** (button) | | strike now |
| **CHOKE** (button) | | grab the cymbal while held |

### Inputs and outputs

Inputs: **STRIKE** (trigger), **VEL** (0 to 10 V, sampled at the trigger), **CHOKE** (gate), **ROLL** (gate: roll, brush sweep or bow), **EXCITE** (audio), and CV for SIZE, WEIGHT, TAPER, PROFILE, BELL, HAMMER, DECAY, BLOOM, DAMP, MOUNT, SIZZLE, MIC, POSITION, FORCE, HARDNESS, RATE.

Output: **OUT** (mono audio).

Context menu: QUALITY (ECO, HIGH, ULTRA; default HIGH), Keep geometry when TYPE changes, Load type's playing defaults, OUT trim, "Rebuild on CV changes" (on by default; when off, geometry CV is read only at strikes).

### Display

A small screen (about 60 x 28 mm) showing: TYPE, size in inches, edge thickness in mm, alloy, PLAY; a live picture of the cymbal from above with the strike point, the rivets and the mic; a small spectrum of the output (log frequency, 60 dB range) which makes the bloom visible; CPU % of the engine and a BUILDING light while the model rebuilds.

---

## 5. Rebuilding the cymbal when geometry changes

- Geometry changes (TYPE, SIZE, WEIGHT, TAPER, PROFILE, BELL, FLANGE, ALLOY, HAMMER, NEW CYMBAL, QUALITY, sample rate) trigger the geometry solver on a **background thread**. Never block the audio thread and never allocate on it.
- Debounce: start a rebuild 80 ms after the last change; if a rebuild is running, queue at most one more with the latest values.
- When the new model is ready, it is swapped in **at the next strike**. The old cymbal keeps ringing until then; at the swap it fades out over 30 ms while the new one is struck (both run during the fade). If no strike comes within 2 s, swap silently (old one faded over 300 ms).
- Keep an LRU cache of the last 8 built models (keyed by quantised geometry), so a sequencer alternating between a few settings swaps instantly.
- On patch load, build synchronously in the background before the first strike; strikes before the model exists are ignored (the BUILDING light is on).
- Swap pointers with a lock-free handoff (atomic pointer, old model freed on the background thread).

---

## 6. Panel

About 34 HP (172.7 mm), house style from Hangover's `make_panel.py`, own colour scheme: dark patinated bronze background (`#2A2418`-ish), warm gold ink, a lighter bronze for section headers. Sections, top to bottom:

1. Display (left) with TYPE knob and NEW CYMBAL button beside it.
2. CYMBAL row: SIZE, WEIGHT, TAPER, PROFILE, BELL, FLANGE, HAMMER, ALLOY switch.
3. SOUND row: DECAY, BLOOM, DAMP, MOUNT, SIZZLE, MIC.
4. PLAY row: PLAY switch, POSITION (large knob), FORCE (large knob), HARDNESS, RATE, EXC, STRIKE and CHOKE buttons.
5. Jack field: gates and audio (STRIKE, VEL, CHOKE, ROLL, EXCITE, OUT), then the CV jacks, each labelled with its control name.

Large knobs for POSITION and FORCE, medium for the rest, stepped knobs with the value on the display for TYPE and SIZZLE. Generate `panel-preview.png` like Hangover does.

---

## 7. Code layout

```
CymbolismFable/
  plugin.json  Makefile  README.md  LICENSE.txt  panel-preview.png
  src/plugin.cpp  src/plugin.hpp
  src/Cymbolism.cpp           Rack module + widget + display
  src/CyGeometry.hpp/.cpp     profile, thickness, materials, type table
  src/CyModes.hpp/.cpp        geometry solver (FEM per angular order, condensation, A/B tensors, asymptotic upper modes)
  src/CyEngine.hpp/.cpp       SAV time stepper, core and upper tiers, cascade, damping, choke, damp, mount, swing
  src/CyExciters.hpp/.cpp     contact beaters, rods, brush grains, bow, rivets, EXCITE input, roll
  src/CyPickup.hpp/.cpp       mic weights, radiation, output limiter
  dep/eigen/                  vendored Eigen (header-only, MPL2), used only in the geometry solver
  tools/shell_ref.py          Python reference of the solver (numpy, scipy)
  tools/pdf.js  tools/preview.py
  res/make_panel.py  res/haer_logo.py  res/Cymbolism.svg
  test/render.cpp             offline checks + demo WAVs (no Rack)
  docs/guide.html  docs/Cymbolism-Fable-Manual.pdf
```

The engine and solver are pure C++17 with no Rack dependency, so `test/render.cpp` can run them. Use `-O3 -ffast-math` only on files where tests still pass with it (the SAV energy test is sensitive).

---

## 8. Tests (`make test`)

Each test prints PASS or FAIL with numbers. Do not tune constants to pass a test by breaking another.

- **T1 Flat plate.** Profile 0, no bell, uniform thickness, free edge, tiny hole, ν = 0.33: frequency parameters λ² for (m, n) = (2,0), (0,1), (3,0), (1,1), (4,0) within 2% of 5.253, 9.084, 12.23, 20.52, 21.6 (Leissa, free circular plate).
- **T2 Python vs C++.** Frequencies, A and B tensors for CRASH and CHINA agree to 1e-4 relative.
- **T3 Curvature.** For a uniform spherical cap, the lowest axisymmetric frequency rises with curvature as published by Thomas, Touzé and Chaigne (2005) for free-edge shells; reproduce their trend and say how close the numbers are.
- **T4 Backbone signs.** A flat plate's first axisymmetric mode hardens with amplitude (frequency up); a curved cap's softens at moderate amplitude. Measure by free decay from large initial amplitude.
- **T5 Energy.** Lossless, unforced, CRASH at HIGH: total discrete energy (modal kinetic + potential + ½Σρ_j²) constant to 1e-6 relative over 10 s from a large initial displacement; with loss on, energy never increases.
- **T6 Contact time.** TIP at 3 m/s on RIDE mid-bow: contact 0.3 to 1.5 ms. MALLET at 1 m/s: 3 to 10 ms.
- **T7 Bloom.** CRASH, SHOULDER on the edge: at 10 m/s the 6 to 12 kHz band peaks 20 to 80 ms after the strike; at 0.5 m/s it peaks within 5 ms and the spectrum shape stays roughly constant (linear).
- **T8 Choke.** A 1 s ring choked at 500 ms drops 40 dB within 120 ms.
- **T9 Stability sweep.** Every TYPE x PLAY x extreme knob setting (FORCE max, BLOOM 2, WEIGHT min, SIZE max) for 5 s: no NaN or Inf, output under ±12 V.
- **T10 CPU.** Real-time factor per quality level at 48 kHz on the build machine, printed (not pass/fail; put the numbers in the README).

Demo WAVs (48 kHz, 24-bit) in `test/out/`: a gallery of all 10 types struck soft then hard; a ride pattern with bell accents; a crash then choke; a mallet swell (roll with VEL ramp); brush sweep on a crash ride; bowed china and bowed BELL; rods on a splash; a SIZZLE ride; a BLOOM sweep 0 to 2 on the same crash hit; a drum loop through EXCITE (synthesised kick and snare in the test so nothing external is needed). Copy the gallery and two or three highlights to `/mnt/project-files/cymbal-vcv/` for Hardy.

---

## 9. Guide and manual

`docs/guide.html` in the Hangover style, printed to PDF. Contents: what the module is and how the physics works in plain words (no equations beyond one or two); every control; the TYPE table; the quality levels with measured CPU; and patch recipes:

1. **Jazz ride**: RIDE, TIP mid-bow, a swing pattern from any sequencer into STRIKE and VEL, accents to the bell via POSITION CV.
2. **Big crash and choke**: CRASH, SHOULDER on the edge, FORCE high; a second trigger into CHOKE.
3. **Orchestral swell**: MALLET, ROLL gate held, an envelope into VEL for the crescendo.
4. **Brushes**: CRASH RIDE, BRUSH, ROLL gate for sweeps and STRIKE for taps.
5. **Bowed metal**: BELL or CHINA, BOW, slow LFO into RATE and FORCE.
6. **One cymbal, many cymbals**: sequenced SIZE and WEIGHT CV per hit (uses the model cache).
7. **Drum loop bloom**: a drum loop into EXCITE, BLOOM up.
8. **Sizzle and tape**: SWISH with SIZZLE, DAMP to taste.
9. **Impossible cymbals**: BLOOM 2, STEEL, 24" with 0.4 mm weight; or a 6" HEAVY RIDE.

---

## 10. Build order

1. Python reference solver (`tools/shell_ref.py`) and T1, T3.
2. C++ solver, T2.
3. SAV engine with core only and a test impulse, T4, T5.
4. Upper tier and cascade, T7.
5. Beaters (contact, rods, brush, bow), T6; then rivets, choke, damp, mount, swing, pickup, T8.
6. Background rebuild, cache, swap.
7. Rack module, panel, display; T9, T10; demo WAVs.
8. Guide and PDF, README rows, push, check CI is green.

Ask Hardy (via the thread) before cutting any feature for CPU or time; say what it would cost to keep it.

---

## 11. References

- M. Ducceschi, C. Touzé, "Modal approach for nonlinear vibrations of damped impacted plates: application to sound synthesis of gongs and cymbals", J. Sound Vib. 2015.
- M. Ducceschi, C. Touzé, "Simulations of nonlinear plate dynamics: an accurate and efficient modal algorithm", DAFx 2015.
- S. Bilbao, M. Ducceschi, F. Zama, "Explicit exactly energy-conserving methods for Hamiltonian systems", J. Comput. Phys. 2023 (SAV).
- O. Thomas, C. Touzé, A. Chaigne, "Non-linear vibrations of free-edge thin spherical shells: modal interaction rules and 1:1:2 internal resonance", Int. J. Solids Struct. 2005.
- Q. Bao, C. Touzé, A. Chaigne, tapered cymbals, JASA 2019: https://perso.ensta-paris.fr/~touze/PDF/2019_BaoCT_TaperedCymbals_JASA.pdf
- S. Bilbao, "Numerical Sound Synthesis", Wiley 2009 (plates, shells, bow friction, contact).
- S. Bilbao, nonlinear shell vibration (cymbals): https://www2.ph.ed.ac.uk/~sbilbao/cympage.html ; NESS cymbals and gongs: https://www.ness.music.ed.ac.uk/target-systems/cymbals-and-gongs
- G. Düring, C. Josserand, S. Rica, "Weak turbulence for a vibrating plate: can one hear a Kolmogorov spectrum?", PRL 2006.
- A. Leissa, "Vibration of Plates", NASA SP-160, 1969 (free circular plate table).
- R. Perrin et al., "The normal modes of cymbals": https://www.ioa.org.uk/system/files/proceedings/r_perrin_gm_swallowe_sa_zietlow_tr_moore_the_normal_modes_of_cymbals.pdf
- Real-time modal synthesis of crash cymbals with nonlinear approximations, using a GPU (DAFx): https://dafx.de/paper-archive/details/qe8SzTmE_uIgihtLswWFFA

Citations from memory (author, title, year) should be checked when you read them; if a formula here disagrees with a paper, follow the paper and note it in the README.
