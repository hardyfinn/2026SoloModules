// Scene scripts for the Molten prototype: controls as functions of time, and strike lists.
static double ramp(double t, double t0, double t1, double a, double b) {
  if (t <= t0) return a; if (t >= t1) return b; double x = (t - t0) / (t1 - t0); return a + (b - a) * x;
}
static int scene_id(const char *n) {
  if (!strcmp(n, "melt")) return 0; if (!strcmp(n, "quench")) return 1;
  if (!strcmp(n, "sing")) return 2; if (!strcmp(n, "pushed")) return 3; return 0;
}
static double scene_dur(int s) { static const double d[] = {34, 25, 34, 30}; return d[s]; }

static const Hit H0[] = {
  {0.3, 1.0, 0.13, 0.9}, {1.6, 0.7, 0.37, 0.8}, {2.7, 1.0, 0.05, 1.0}, {4.0, 0.9, 0.22, 0.9}, {5.2, 0.8, 0.41, 0.8},
  {6.3, 1.0, 0.09, 0.9}, {7.5, 0.9, 0.3, 0.9}, {8.6, 1.0, 0.15, 0.9}, {9.8, 0.8, 0.45, 0.8}, {10.9, 1.1, 0.07, 0.9},
  {12.0, 1.0, 0.26, 0.9}, {13.2, 1.1, 0.12, 0.9}, {14.3, 1.0, 0.38, 0.9}, {15.5, 1.2, 0.08, 0.9}, {16.6, 1.1, 0.2, 0.9},
  {17.8, 1.2, 0.33, 0.9}, {18.9, 1.2, 0.1, 0.9}, {20.1, 1.3, 0.27, 0.9}, {21.2, 1.3, 0.14, 0.9}, {22.4, 1.4, 0.4, 0.9},
  {23.5, 1.5, 0.06, 0.9}, {24.7, 1.3, 0.21, 0.9}, {25.8, 1.8, 0.35, 0.9}, {27.0, 1.2, 0.11, 0.9}, {28.1, 2.2, 0.3, 0.9},
  {29.6, 1.0, 0.16, 0.9}, {30.4, 2.6, 0.42, 0.9}};
static const Hit H1[] = {
  {0.4, 1.4, 0.3, 0.9}, {1.9, 1.0, 0.12, 0.9}, {3.3, 1.8, 0.4, 0.9}, {5.5, 0.7, 0.2, 0.9}, {6.3, 0.7, 0.35, 0.9},
  {7.2, 0.7, 0.1, 0.9}, {9.0, 1.0, 0.13, 1.0}, {10.0, 0.8, 0.37, 1.0},
  {11.5, 1.0, 0.3, 0.9}, {12.3, 1.0, 0.15, 0.9}, {13.2, 1.1, 0.25, 0.9}, {14.2, 1.3, 0.4, 0.9}, {16.6, 0.5, 0.25, 0.9},
  {17.6, 0.6, 0.4, 0.9}, {19.0, 1.6, 0.06, 1.0}, {21.6, 0.9, 0.3, 1.0}};
static const Hit H2[] = {{0.05, 0.3, 0.2, 0.5}};
static const Hit H3[] = {
  {0.2, 3.0, 0.2, 0.9}, {2.0, 4.0, 0.35, 0.9}, {4.1, 3.0, 0.1, 0.9}, {6.0, 5.0, 0.3, 0.9}, {8.2, 4.0, 0.15, 0.9},
  {10.1, 6.0, 0.4, 0.9}, {12.0, 4.0, 0.22, 0.9}, {13.5, 5.0, 0.08, 0.9}, {15.0, 6.0, 0.33, 0.9}, {16.2, 5.0, 0.18, 0.9},
  {17.3, 6.0, 0.41, 0.9}, {18.2, 5.0, 0.12, 0.9}, {19.0, 7.0, 0.27, 0.9}, {19.7, 6.0, 0.36, 0.9}, {20.3, 7.0, 0.1, 0.9},
  {20.9, 6.0, 0.3, 0.9}, {21.4, 8.0, 0.2, 0.9}, {24.0, 6.0, 0.3, 0.9}};
#define NH(a) ((int)(sizeof(a) / sizeof(a[0])))
static const Hit *scene_hits(int s, int *n) {
  switch (s) {
  case 0: *n = NH(H0); return H0; case 1: *n = NH(H1); return H1;
  case 2: *n = NH(H2); return H2; default: *n = NH(H3); return H3; }
}
static void scene_ctl(int s, double t, Ctl *c) {
  c->heat = 0; c->rate = 2.5; c->xf = 0.3; c->wf = 0.35; c->quench = 0; c->sing = 0; c->f0 = 196; c->tens = 0.6; c->kc = 2.5; c->cond = 6;
  c->ucmax = 0; c->ping = 0.3; c->boil = 0; c->latent = 0.5; c->soft = 1; c->mush = 0.5; c->tau = 1.0 / (2 * PI * 200);
  switch (s) {
  case 0: // MELT: strike a cold bar while the flame comes up, through red-hot, into a drop
    c->heat = t < 4 ? 0 : t < 15 ? ramp(t, 4, 15, 0, 1.0) : t < 24 ? ramp(t, 15, 24, 1.0, 1.75) : 1.85;
    c->rate = 3; c->ping = 0.15; c->wf = 0.5; c->cond = 2.5; c->xf = 0.25;
    break;
  case 1: // QUENCH: a ringing drop is plunged into water; later supercooled and frozen by one hard strike
    c->heat = t < 5 ? 1.8 : t < 11 ? 0 : t < 15 ? 1.9 : t < 19.4 ? 0.72 : 0;
    c->rate = t < 5 ? 3 : t < 11 ? 0.4 : t < 15 ? 1.3 : t < 19.4 ? 2.0 : 3;
    c->quench = (t > 5 && t < 9) ? 0.25 * ramp(t, 5, 5.3, 0, 1) : (t > 19.4 && t < 22) ? 0.6 : 0;
    c->ucmax = t > 11 ? 0.5 : 0; c->ping = 0.7; c->wf = 2;
    break;
  case 2: // SING: the hot body sings on a cold anvil; heat is swept through the melt and it is quenched
    c->f0 = 147; c->tens = 1.0; c->sing = ramp(t, 0, 2, 0.4, 1);
    c->heat = t < 6 ? 0.25 : t < 15 ? ramp(t, 6, 15, 0.25, 1.0) : t < 25 ? ramp(t, 15, 25, 1.0, 1.7) : t < 29 ? 1.8 : 0;
    c->quench = t > 29 ? ramp(t, 29, 29.2, 0, 1) * ramp(t, 31, 32, 1, 0) : 0;
    c->rate = 3; c->ping = 0.5; c->xf = 0.7; c->wf = 0.3; c->mush = 0.6;
    break;
  case 3: { // PUSHED: the flame swings across the melting point faster and faster, with heavy strikes
    double ph = 0.15 * t + 0.045 * t * t; // cycles
    c->heat = 1.05 + 0.75 * sin(2 * PI * ph);
    if (t > 22) c->heat = t < 26 ? 1.95 : 0;
    c->rate = 6 + t * 0.6; c->latent = 0.25; c->ucmax = 0.25; c->ping = 1.0; c->soft = 2.5; c->mush = 0.6;
    c->boil = t > 22 ? 1 : 0.3; c->sing = t > 9 ? ramp(t, 9, 14, 0, 1) : 0; c->f0 = 147; c->tens = 1.0; c->wf = 2;
    c->quench = t > 26 ? ramp(t, 26, 26.1, 0, 1) * ramp(t, 28, 29, 1, 0) : 0;
  } break;
  }
}
