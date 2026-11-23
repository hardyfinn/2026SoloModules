// Birdie reference engine. Authoritative for the DSP and the phrase generator.
// Plain C99, no dependencies. Port to C++ keeping the arithmetic and RNG draw order identical.
#ifndef BIRDIE_REF_H
#define BIRDIE_REF_H
#include <stdint.h>
#include <math.h>
#include <string.h>

#define BD_MAXOS 16
#define BD_FIRQ 8                 // decimator taps = OS * BD_FIRQ
#define BD_DELAY 16384            // trachea buffer (enough for 192 kHz)
#define BD_ALPHA_IDLE (-0.02)
#define BD_GAMMA0 24000.0
#define BD_EDGE_N 256             // edge table: alpha 0..1
#define BD_ST_PER_OCT 4.68        // tension: d = 2^(st/4.68), so pitch moves about one semitone per st
#define BD_F_COEF 0.1771          // pitch estimate: f = BD_F_COEF * G * d^0.39

enum { SH_LINE, SH_FALL, SH_SWOOP, SH_ARCH, SH_DOUBLE, SH_COUNT };
enum { PT_REPEAT, PT_PAIR, PT_MOTIF, PT_TAIL, PT_RUN, PT_CALL, PT_SHUFFLE, PT_COUNT };

typedef struct {            // all values are after knob mapping (see spec table)
  // voice
  double size;              // 0.5 .. 64      bird scale. pitch ~ 1/size
  double tension;           // -36 .. +7 semitones above the reference pitch (4250 Hz / size)
  double pressure;          // 0.05 .. 0.6    peak alpha
  double breath;            // 0 .. 1
  double rough;             // 0 .. 1
  double v2mix;             // 0 .. 1
  double v2int;             // -12 .. +12 st
  double v2slide;           // -1 .. +1  (second side's slide relative to first)
  // note
  double length;            // 0.01 .. 2 s
  double slide;             // -24 .. +24 st
  int    shape;             // SH_*
  double attack, release;   // 0.02 .. 0.6 fraction of note
  double buzzRate;          // 5 .. 400 Hz
  double buzz;              // 0 .. 0.5  tension wobble depth
  double chop;              // 0 .. 1    pressure chop depth
  // tract
  double trachea;           // 0.005 .. 0.10 m (at size 1)
  double tube;              // 0 .. 0.9  reflection
  double throat;            // 0 .. 1
  double tune;              // 0 .. 1
  int    follow;            // 1 = throat follows pitch
  double beak;              // 500 .. 16000 Hz (at size 1; scales with 1/sqrt(size))
  // phrase
  int    notes;             // 1 .. 32
  double rate;              // 1 .. 40 notes per second (free)
  int    pattern;           // PT_*
  double vary;              // 0 .. 1
  double drift;             // -12 .. +12 st over the phrase
  double accel;             // -1.5 .. +1.5 octaves of rate over the phrase
  double swell;             // -1 .. +1
  double time;              // 1 .. 16
  int    hold;              // 0/1
  double clockPeriod;       // seconds per note when locked to clock, 0 = free
} BdParams;

typedef struct {
  double dT, dSlide, lenF, pressF, attF, relF;
  int shape;                // -1 = use panel
  int buzzOn; double buzzRate, buzz, chop;
  int v2On;  double v2mix, v2int, v2slide;
} BdType;

typedef struct { double x, y; } BdOsc;

typedef struct {
  double sr; int os; double dt;
  BdOsc o1, o2;
  double edge[BD_EDGE_N+1];
  // decimator
  double fir[BD_MAXOS*BD_FIRQ]; double hist[BD_MAXOS*BD_FIRQ]; int hpos; int ntaps;
  // tract
  double dl[BD_DELAY]; int wp; double dSm;
  double ic1, ic2; double lpA, lpB; double dcX, dcY;
  // noise
  uint32_t nrng; double nzFast, nzSlow;
  // gesture
  int noteOn; double nt, ndur; BdType cur; double curDrift, curSwell;
  double buzzPhase; double envSm, holdSm;
  // phrase
  int phOn; int phIdx, phN; double phNext, phPeriod; uint32_t seed, shufRng; BdType types[4];
  double phDrift, phAccel, phSwell; int phPattern;
  // monitor outputs
  double outPress, outPitch; int outGate, outEnd;
} Birdie;

void bd_init(Birdie* b, double sr);
void bd_note(Birdie* b, const BdParams* p);                 // NOTE trigger
void bd_phrase(Birdie* b, const BdParams* p, uint32_t seed); // PHRASE trigger
double bd_process(Birdie* b, const BdParams* p);            // one sample, returns volts (about +-5)
#endif
