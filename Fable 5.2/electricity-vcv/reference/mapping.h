// Live Wire: knob (0..1) to physical mappings, and their inverses (used by the scenes so that
// every sketch is reachable from the panel). The C++ Mapping.hpp must match these exactly.
#ifndef LW_MAPPING_H
#define LW_MAPPING_H
#include <math.h>
static inline double lw_clamp(double x,double a,double b){ return x<a?a:(x>b?b:x); }
static inline double lw_exp(double k,double lo,double hi){ return lo*pow(hi/lo,lw_clamp(k,0,1)); }
static inline double lw_iexp(double v,double lo,double hi){ return log(v/lo)/log(hi/lo); }
// name            forward                                         inverse
#define M_VOLTS(k)   (4.0*(k))                                      // E, strike-volt units
#define I_VOLTS(v)   ((v)/4.0)
#define M_FREQ(k)    lw_exp(k,20.0,500.0)                           // mains Hz (default 50: k=0.2847)
#define I_FREQ(v)    lw_iexp(v,20.0,500.0)
#define M_CUR(k)     lw_exp(k,0.01,4.0)                             // 1/Rs
#define I_CUR(v)     lw_iexp(v,0.01,4.0)
#define M_CHOKE(k)   lw_exp(k,5e-5,2e-2)                            // Ls/Rs seconds
#define I_CHOKE(v)   lw_iexp(v,5e-5,2e-2)
#define M_GAP(k)     (0.01+4.0*(k)*(k))                             // electrode spacing
#define I_GAP(v)     sqrt(((v)-0.01)/4.0)
#define M_HEAT(k)    lw_exp(k,2e-4,0.6)                             // P0, arc power per unit length
#define I_HEAT(v)    lw_iexp(v,2e-4,0.6)
#define M_COOL(k)    lw_exp(k,8e-6,3e-3)                            // tau, seconds (at C4 when TRACK is on)
#define I_COOL(v)    lw_iexp(v,8e-6,3e-3)
#define M_QUENCH(k)  lw_exp(k,2e-3,0.5)                             // conductance where the arc goes out
#define I_QUENCH(v)  lw_iexp(v,2e-3,0.5)
#define M_MEM(k)     lw_exp(k,1e-3,0.2)                             // ionisation memory, seconds
#define I_MEM(v)     lw_iexp(v,1e-3,0.2)
#define M_RESTR(k)   (0.95*(k))
#define I_RESTR(v)   ((v)/0.95)
#define M_JIT(k)     (0.6*(k))
#define I_JIT(v)     ((v)/0.6)
#define M_WIND(k)    (0.15*(k)*(k))
#define I_WIND(v)    sqrt((v)/0.15)
#define M_CLIMB(k)   (40.0*(k)*(k)*(k))
#define I_CLIMB(v)   cbrt((v)/40.0)
#define M_TUNE(k)    (261.6256*pow(2.0,10.0*(k)-4.0))               // tank Hz, C0..C10 (clamped below)
#define I_TUNE(v)    ((log2((v)/261.6256)+4.0)/10.0)
#define M_RING(k)    lw_exp(k,0.7,60.0)                             // tank Q
#define I_RING(v)    lw_iexp(v,0.7,60.0)
#define M_LOAD(k)    lw_exp(k,8.0,0.15)                             // tank impedance: more LOAD = lower Z = bigger currents
#define I_LOAD(v)    lw_iexp(v,8.0,0.15)
#define M_IRON(k)    (3.0*(k)*(k))
#define I_IRON(v)    sqrt((v)/3.0)
#define M_CORONA(k)  (3.0*(k))
#define I_CORONA(v)  ((v)/3.0)
#define M_HUM(k)     (0.3*(k))
#define I_HUM(v)     ((v)/0.3)
#define M_FB(k)      (1.6*(k))
#define I_FB(v)      ((v)/1.6)
#define M_DIST(k)    lw_exp(k,3e-4,2e-2)                            // feedback delay, seconds
#define I_DIST(v)    lw_iexp(v,3e-4,2e-2)
#define M_ARC(k)     (8.0*(k)*(k)*(k))                              // output mix levels
#define I_ARC(v)     cbrt((v)/8.0)
#define M_FIELD(k)   (2.0*(k)*(k))
#define I_FIELD(v)   sqrt((v)/2.0)
#define M_DRAW(k)    lw_exp(k,0.05,10.0)                            // seconds to pull the electrodes apart after a touch
#define I_DRAW(v)    lw_iexp(v,0.05,10.0)
#define M_OUT(k)     (4.0*(k)*(k))                                  // output gain before the soft clip (default k=0.5: 1.0)
#define I_OUT(v)     sqrt((v)/4.0)
#define M_IN(k)      (2.0*(k)*(k))                                  // audio in to source volts (5 V in = 1 unit at 1.0)
#endif
