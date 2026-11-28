// Molten: prototype engine. One metal body that is heated from a cold glassy bar,
// through red-hot, to a liquid drop held by surface tension, and quenched back.
// gcc -O3 -ffast-math -o molten molten.c -lm ; ./molten <scene>  -> raw-<scene>.f32 (stereo, 96 kHz)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 96000.0
#define NF 24          // bar modes per polarisation family
#define NS (2 * NF)    // solid modes
#define NL 12          // liquid (drop) modes, l = 2..13
#define K 16           // thermal zones along the body
#define TICK 32
#define PI 3.14159265358979323846

typedef struct {
  double heat;   // flame temperature target: 0 cold, 1 melting point, 2 white-hot liquid
  double rate;   // how fast the body follows the flame, 1/s
  double xf, wf; // flame position 0..1 and width
  double quench; // 0..1, plunges the body into water
  double sing;   // 0..1 thermal-contact self-oscillation (hot body on a cold anvil)
  double f0;     // cold fundamental, Hz
  double tens;   // liquid fundamental / solid fundamental when fully molten at the melting point
  double ucmax;  // how far the liquid can be supercooled before it freezes by itself
  double ping;   // stress pings amount
  double boil;   // boiling amount
  double latent; // latent heat
  double soft;   // amplitude softening amount
  double mush;   // solid <-> liquid coupling
  double tau;    // contact thermal lag, s
  double cond;   // heat conduction between zones
  double kc;     // contact stiffness as a frequency ratio
} Ctl;
typedef struct { double t, vel, pos, hard; } Hit;

#include "scenes.h"

static unsigned long long rs = 88172645463325252ULL;
static double rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (double)(rs >> 11) / 9007199254740992.0; }
static double rnds(void) { return 2 * rnd() - 1; }

static double beta[NF];
static double shape(int n, double x, int curv) { // free-free bar mode n (0-based), value or curvature/beta^2
  double b = beta[n], xs = x <= 0.5 ? x : 1 - x, sg = (x <= 0.5 || (n % 2 == 0)) ? 1 : -1, v;
  if (n < 4) {
    double sig = (cosh(b) - cos(b)) / (sinh(b) - sin(b));
    v = curv ? cosh(b * xs) - cos(b * xs) - sig * (sinh(b * xs) - sin(b * xs))
             : cosh(b * xs) + cos(b * xs) - sig * (sinh(b * xs) + sin(b * xs));
  } else
    v = curv ? -cos(b * xs) + sin(b * xs) + exp(-b * xs) : cos(b * xs) - sin(b * xs) + exp(-b * xs);
  return sg * v;
}
static double legendre(int l, double x) {
  double p0 = 1, p1 = x;
  for (int k = 2; k <= l; k++) { double p = ((2 * k - 1) * x * p1 - (k - 1) * p0) / k; p0 = p1; p1 = p; }
  return l == 0 ? 1 : p1;
}

int main(int argc, char **argv) {
  double CL = getenv("CL") ? atof(getenv("CL")) : 0.1;  // contact loss
  const char *name = argc > 1 ? argv[1] : "melt";
  int sc = scene_id(name);
  double dur = scene_dur(sc);
  int nh; const Hit *hits = scene_hits(sc, &nh);
  char fn[256]; snprintf(fn, 256, "raw-%s.f32", name);
  FILE *fo = fopen(fn, "wb");
  snprintf(fn, 256, "log-%s.txt", name);
  FILE *fl = fopen(fn, "w");

  // ---------- solid: free-free bar, two polarisations ----------
  static const double b5[5] = {4.730041, 7.853205, 10.995608, 14.137165, 17.278760};
  for (int n = 0; n < NF; n++) beta[n] = n < 5 ? b5[n] : (n + 1.5) * PI;
  double ratio[NS], w[NS][K], fam[NS], q[NS] = {0}, v[NS] = {0}, det[NS];
  for (int n = 0; n < NS; n++) {
    int m = n % NF, f = n / NF;
    ratio[n] = pow(beta[m] / beta[0], 2.0) * (f ? 1.335 : 1.0);
    fam[n] = f ? 0.7 : 1.0;
    det[n] = 1 + 0.002 * rnds();
    double tot = 0;
    for (int j = 0; j < K; j++) {
      double s = 0;
      for (int i = 0; i < 8; i++) { double x = (j + (i + 0.5) / 8) / K, c = shape(m, x, 1); s += c * c; }
      w[n][j] = s; tot += s;
    }
    for (int j = 0; j < K; j++) w[n][j] /= tot;
  }
  // ---------- liquid: Rayleigh modes of a drop ----------
  double lrat[NL], ldmp[NL], a[NL] = {0}, av[NL] = {0}, gs[NL], gp1[NL], gp2[NL], gc[NL], hh[NL];
  for (int i = 0; i < NL; i++) {
    int l = i + 2;
    lrat[i] = sqrt(l * (l - 1.0) * (l + 2.0) / 8.0);
    ldmp[i] = pow((l - 1.0) * (2 * l + 1.0) / 5.0, 0.45); // viscous law, flattened so the drop keeps some top
    gs[i] = legendre(l, 0.35); gp1[i] = legendre(l, 1.0) * 0.8; gp2[i] = legendre(l, -0.45) * 1.3;
    gc[i] = 1.6 * legendre(l, -1.0); hh[i] = legendre(l, 0.8) / (1 + 0.3 * i);
  }
  // ---------- thermal state ----------
  Ctl c; scene_ctl(sc, 0, &c);
  double h[K], T[K], phi[K], Tprev[K], phiprev[K]; int liq[K], nuc[K];
  for (int j = 0; j < K; j++) {
    double T0 = c.heat; h[j] = T0 < 1 ? T0 : T0 + c.latent; liq[j] = T0 > 1; nuc[j] = -1;
    T[j] = Tprev[j] = T0; phi[j] = phiprev[j] = T0 > 1;
  }
  // mode coefficients
  double om[NS], cc[NS], ss[NS], ws[NS], ee[NS], lom[NL], lcc[NL], lss[NL], lws[NL], lee[NL];
  double hitw[NS], pk1[NS], pk2[NS], cs[NS];
  double xp1 = 0.07, xp2 = 0.31, xc = 0.224 * 0.5; // contact near (not at) a node of the fundamental
  for (int n = 0; n < NS; n++) {
    pk1[n] = shape(n % NF, xp1, 0) * fam[n]; pk2[n] = shape(n % NF, xp2, 0) * fam[n] * (n / NF ? -1 : 1);
    cs[n] = shape(n % NF, xc, 0) * fam[n];
  }
  double M = 0, Tsol = 0, Tliq = 1, U0s = 100, U0l = 1, mm = 0, wL = 1;
  double compz = 0, bump = 0, lp1 = 0, lp2 = 0, hiss = 0, hissx = 0.5, dt = 1 / SR;
  double ping_next_amp = 0; (void)ping_next_amp;
  long N = (long)(dur * SR); int ih = 0, active[NS]; for (int n = 0; n < NS; n++) { om[n] = 1; active[n] = 0; }
  double peak = 0; long guard = 0;
  double dcl = 0, dcr = 0, gUs = 0, gUl = 0;

  for (long i = 0; i < N; i++) {
    double t = i / SR;
    if (i % TICK == 0) {
      scene_ctl(sc, t, &c);
      double dtc = TICK / SR;
      // ----- heat flow -----
      double lapl[K];
      for (int j = 0; j < K; j++) {
        double tl = T[j > 0 ? j - 1 : j], tr = T[j < K - 1 ? j + 1 : j];
        lapl[j] = tl - 2 * T[j] + tr;
      }
      for (int j = 0; j < K; j++) {
        double x = (j + 0.5) / K, g = exp(-pow((x - c.xf) / c.wf, 2));
        double dh = c.heat > T[j] ? c.rate * g * (c.heat - T[j]) : 0.6 * c.rate * (c.heat - T[j]);
        dh += c.cond * lapl[j];
        dh += c.quench * 2.5 * (0.5 + 1.5 * fabs(2 * x - 1)) * (0 - T[j]);
        h[j] += dh * dtc;
        if (h[j] < 0) h[j] = 0;
      }
      // ----- phase: enthalpy with a latent plateau, plus supercooling and a freezing front -----
      for (int j = 0; j < K; j++) {
        if (h[j] >= 1 + c.latent) liq[j] = 1;
        if (liq[j] && h[j] < 1 + c.latent) { // supercooled
          int seed = (h[j] - c.latent) < 1 - c.ucmax;
          int nb = (j > 0 && !liq[j - 1] && phi[j - 1] < 0.999) || (j < K - 1 && !liq[j + 1] && phi[j + 1] < 0.999);
          if (seed) { liq[j] = 0; nuc[j] = -1; }
          else if (nb) { // the front arrives: faster the deeper the undercooling
            if (nuc[j] < 0) nuc[j] = (int)((0.004 + 0.03 * fmax(0, 1 - 3 * (1 - (h[j] - c.latent)))) / dtc);
            else if (--nuc[j] <= 0) { liq[j] = 0; nuc[j] = -1; }
          }
        } else nuc[j] = -1;
        Tprev[j] = T[j]; phiprev[j] = phi[j];
        if (liq[j]) { T[j] = h[j] - c.latent; phi[j] = 1; }
        else if (h[j] < 1) { T[j] = h[j]; phi[j] = 0; }
        else { T[j] = 1; phi[j] = (h[j] - 1) / c.latent; }
      }
      // ----- stress pings: thermal shock in the solid, and each zone as it freezes -----
      for (int j = 0; j < K; j++) {
        double p = c.ping * (1.2 * fabs(T[j] - Tprev[j]) * (1 - phi[j]) + 2.5 * fmax(0, phiprev[j] - phi[j]));
        if (rnd() < p) {
          double x = (j + rnd()) / K, amp = (0.15 + 0.6 * rnd() * rnd()) * (rnd() < 0.5 ? -1 : 1);
          for (int n = 0; n < NS; n++) {
            double f = om[n] / (2 * PI);
            v[n] += amp * shape(n % NF, x, 0) * fam[n] * f * f / (f * f + 2500.0 * 2500.0);
          }
        }
      }
      // ----- aggregate state -----
      M = 0; Tsol = 0; Tliq = 0; double ms = 0;
      for (int j = 0; j < K; j++) { M += phi[j] / K; Tsol += T[j] * (1 - phi[j]); ms += 1 - phi[j]; Tliq += T[j] * phi[j]; }
      Tsol = ms > 1e-6 ? Tsol / ms : 1; Tliq = M > 1e-6 ? Tliq / (M * K) : 1;
      mm = 4 * M * (1 - M);
      U0s = 0.4 * exp(6.0 * (1 - Tsol)) / (c.soft + 1e-9);
      U0l = 0.35 / (c.soft + 1e-9);
      // ----- solid modes: each mode's stiffness and loss follow the zones where it bends -----
      for (int n = 0; n < NS; n++) {
        double f00 = c.f0 * ratio[n] * det[n];
        active[n] = f00 < 20000;
        double st = 0, dm = 0;
        for (int j = 0; j < K; j++) {
          double Tj = fmin(T[j], 1.0);
          st += w[n][j] * (1 - phi[j]) * (1 - 0.6 * Tj * Tj);
          dm += w[n][j] * (1 + 6 * Tj * Tj * Tj + 15 * phi[j]);
        }
        double f = f00 * sqrt(st + 1e-4);
        double t60 = 9.0 / (1 + f00 / 1800.0) / dm, ga = 6.908 / t60;
        om[n] = 2 * PI * f; ee[n] = exp(-ga * dt);
        cc[n] = cos(om[n] * dt); ss[n] = sin(om[n] * dt) / om[n]; ws[n] = sin(om[n] * dt) * om[n];
      }
      // ----- drop modes: pitch from surface tension and size, loss from viscosity -----
      double sig = fmax(0.2, 1 - 0.5 * (Tliq - 1)), visc = exp(-2.2 * (Tliq - 1));
      wL = 2 * PI * c.f0 * c.tens * sqrt(sig / fmax(M, 0.05));
      for (int l = 0; l < NL; l++) {
        double f = fmin(wL * lrat[l] / (2 * PI), 20000);
        double ga = 6.908 / 1.4 * ldmp[l] * visc * (1 + 3 * (1 - M));
        lom[l] = 2 * PI * f; lee[l] = exp(-ga * dt);
        lcc[l] = cos(lom[l] * dt); lss[l] = sin(lom[l] * dt) / lom[l]; lws[l] = sin(lom[l] * dt) * lom[l];
      }
      if (i % (TICK * 30) == 0)
        fprintf(fl, "%.3f %.3f %.3f %.3f %.3f %.2f %.2f %.4f %.3g %.3g %ld\n", t, c.heat, M, Tsol, Tliq, om[0] / (2 * PI), lom[0] / (2 * PI), bump, gUs, gUl, guard);
    }
    // ----- strikes -----
    while (ih < nh && hits[ih].t <= t) {
      const Hit *H = &hits[ih++];
      double fh = 700 * pow(12.0, H->hard);
      for (int n = 0; n < NS; n++)
        if (active[n]) { double f = c.f0 * ratio[n]; v[n] += H->vel * shape(n % NF, H->pos, 0) * fam[n] * exp(-f * f / (fh * fh) * 0.02) * 0.6; }
      for (int l = 0; l < NL; l++) av[l] += H->vel * gs[l] * sqrt(M) * 0.8;
      if (H->vel > 0.8) { int j = (int)(H->pos * K); if (j >= K) j = K - 1; if (liq[j] && h[j] < 1 + c.latent) liq[j] = 0; } // a hard strike seeds freezing
    }
    // ----- energies and collective coordinates -----
    double Us = 0, Ul = 0, ha = 0, uc = 0;
    for (int n = 0; n < NS; n++) if (active[n]) { Us += om[n] * om[n] * q[n] * q[n]; uc += cs[n] * q[n]; }
    for (int l = 0; l < NL; l++) { Ul += lom[l] * lom[l] * a[l] * a[l]; ha += hh[l] * a[l]; }
    Us *= 0.5; Ul *= 0.5; gUs = Us; gUl = Ul;
    double sM = sqrt(M), s1M = sqrt(1 - M);
    double ucl = 0; for (int l = 0; l < NL; l++) ucl += gc[l] * a[l];
    double u = s1M * uc + sM * ucl;
    double X = tanh(wL * ha), bm = c.mush * mm;
    double sfs = 0.25 + 0.75 / (1 + Us * (1 + bm * X) / U0s); // never softer than an octave down
    double ks = (1 + bm * X) * sfs; // solid stiffness factor: softened by level, swung by the drop
    double softl = 0.75 * (1 - 1 / (1 + Ul / U0l)), kl = bm * Us * wL * (1 - X * X) * sfs;
    // ----- thermal contact: the anvil bump grows where the hot body presses, with a lag -----
    double Fc = 0;
    if (c.sing > 0) {
      double dT = 0.25 + 0.75 * fmin(1.5, M > 0.5 ? Tliq : Tsol);
      double d0 = 6e-5 * c.sing, gfb = 0.9 * fmin(1, c.sing * dT * 1.4);
      double kc = pow(2 * PI * c.f0 * c.kc, 2);
      double comp = bump + d0 - u; if (comp < 0) comp = 0;
      double bmax = 4 * d0 * (0.4 + 1.2 * dT);
      bump += dt / c.tau * (bmax * tanh(gfb * comp / bmax) - bump); // the bump can only grow so far
      Fc = kc * (comp - d0) + (comp > 0 ? CL * (1 - 0.75 * fmin(1, dT)) * sqrt(kc) * (comp - compz) * SR : 0); // lossy contact
      compz = comp;
    } else bump *= 0.9995;
    // ----- quench hiss and boiling -----
    double Fn = 0, Fb = 0;
    if (c.quench > 0) {
      double hot = 0; for (int j = 0; j < K; j++) hot += T[j] / K;
      hiss += 0.3 * (rnds() - hiss);
      Fn = c.quench * hot * hiss * 2.0e3;
    }
    if (c.boil > 0 && Tliq > 1.3 && M > 0.3 && rnd() < c.boil * (Tliq - 1.3) * 60 * dt) {
      double amp = 0.5 + rnd();
      for (int l = 0; l < NL; l++) av[l] += amp * 0.5 * legendre(l + 2, rnds());
    }
    // ----- step -----
    double o1 = 0, o2 = 0;
    for (int n = 0; n < NS; n++) {
      if (!active[n]) continue;
      double k2 = om[n] * om[n] * q[n];
      double F = k2 * (1 - ks) + (Fc + Fn) * s1M * cs[n];
      double qn = ee[n] * (q[n] * cc[n] + v[n] * ss[n]), vn = ee[n] * (v[n] * cc[n] - q[n] * ws[n]);
      q[n] = qn; v[n] = vn + F * dt;
      o1 += pk1[n] * v[n]; o2 += pk2[n] * v[n];
    }
    double l1 = 0, l2 = 0;
    for (int l = 0; l < NL; l++) {
      double k2 = lom[l] * lom[l] * a[l];
      double F = k2 * softl - kl * hh[l] + (Fc + Fn * 0.3 + Fb) * sM * gc[l];
      double an = lee[l] * (a[l] * lcc[l] + av[l] * lss[l]), vn = lee[l] * (av[l] * lcc[l] - a[l] * lws[l]);
      a[l] = an; av[l] = vn + F * dt;
      double nl = 1 + 0.6 * tanh(wL * a[l] * lrat[l]); // the pickup sees the shape, not just the speed
      l1 += gp1[l] * av[l] * nl; l2 += gp2[l] * av[l] * nl;
    }
    double e = Us + Ul;
    if (!(e < 1e6)) { guard++; for (int n = 0; n < NS; n++) { q[n] *= 0.5; v[n] *= 0.5; } for (int l = 0; l < NL; l++) { a[l] *= 0.5; av[l] *= 0.5; } }
    double yl = 0.5 * o1 + 1.6 * sM * l1, yr = 0.5 * o2 + 1.6 * sM * l2;
    dcl += 0.0002 * (yl - dcl); dcr += 0.0002 * (yr - dcr); yl -= dcl; yr -= dcr;
    lp1 = yl; lp2 = yr;
    if (fabs(yl) > peak) peak = fabs(yl);
    float out[2] = {(float)lp1, (float)lp2};
    fwrite(out, sizeof(float), 2, fo);
  }
  fprintf(stderr, "%s: peak %.3f guard %ld\n", name, peak, guard);
  fclose(fo); fclose(fl);
  return 0;
}
