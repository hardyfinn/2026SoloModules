"""Cicada reference engine.  This file is the specification for the C++ port.

It is written as a plain per-sample loop (numba only makes it fast) so that it reads like the C++.
One call of voice() renders one individual insect.  Everything a knob does is in here.

Signal flow of one voice
    song (phrase gesture) -> chirp scheduler -> pulse -> mechanism (ribs / teeth / wingbeats = impulses)
    -> two impulse-rung resonators A and B (complex one-pole "phasor" resonators)
    -> body band-pass (abdomen), mixed by BODY, tuned by OPEN -> pan -> L, R
Shared after all voices are summed: distance() (air loss + leaf reflections), then output gain and soft clip.
"""
import numpy as np
from numba import njit

# ---- knob mappings (k is always the knob value 0..1 after CV is added and clamped) -------------------
TYMBAL, FILE, WING = 0, 1, 2
STEADY, SWELL, PULSATE, RISE, SLUR, TICKBUZZ, SINK, TALK = range(8)
SONG_NAMES = ['STEADY', 'SWELL', 'PULSATE', 'RISE', 'SLUR', 'TICK-BUZZ', 'SINK', 'TALK']
REST_FRAC = np.array([0.35, 0.50, 0.50, 0.90, 0.28, 0.15, 0.35, 0.40])     # rest after a phrase, x LENGTH

@njit(cache=True)
def map_pitch(k): return 100.0*160.0**k                    # carrier, 100 Hz .. 16 kHz
@njit(cache=True)
def map_rate(k): return 1000.0**k                          # pulses per second, 1 .. 1000
@njit(cache=True)
def map_q(k): return 2.0*15.0**k                           # PURE: resonator Q 2 .. 30
@njit(cache=True)
def map_lock(k): return min(1.0, max(0.0, (k-0.45)/0.30))  # PURE: tooth lock 0 .. 1
@njit(cache=True)
def map_chirp(k):                                          # pulses per chirp 1..32, 0 = continuous trill
    if k >= 0.96: return 0
    return int(np.floor(32.0**(k/0.95)+0.5))
@njit(cache=True)
def map_gap(k): return 0.02*200.0**k                       # 20 ms .. 4 s
@njit(cache=True)
def map_length(k):                                         # 0.5 .. 30 s, negative = HOLD
    if k >= 0.96: return -1.0
    return 0.5*60.0**(k/0.95)
@njit(cache=True)
def map_temp(k): return (10.0+25.0*k-5.0)/17.5             # speed factor, 1.0 at k = 0.5 (22.5 C)

# inverse helpers for writing presets in real units
def k_pitch(hz): return np.log(hz/100.0)/np.log(160.0)
def k_rate(hz): return np.log(hz)/np.log(1000.0)
def k_q(q): return np.log(q/2.0)/np.log(15.0)
def k_chirp(n): return 1.0 if n == 0 else 0.95*np.log(n)/np.log(32.0)
def k_gap(s): return np.log(s/0.02)/np.log(200.0)
def k_length(s): return 1.0 if s < 0 else 0.95*np.log(s/0.5)/np.log(60.0)
def k_ribs(n): return (n-1)/11.0
def k_duty(d): return (d-0.05)/0.9

# parameter rows of the control array P[row, sample]
(MECH, PITCH, PURE, STROKE, BODY, OPEN, RATE, CHIRP, GAP, SONG, LENGTH, TEMP, LOOSE,
 VOCT, TRACK, GATE, TRIG, CLOCK, NP) = range(19)
# flags[]: gate_patched, trig_patched, clock_patched
CTL = 16                                                    # control rate: every 16 samples

@njit(cache=True)
def _rand(st):                                              # xorshift32 -> uniform 0..1
    x = st[0]; x ^= (x << 13) & 0xFFFFFFFF; x ^= x >> 17; x ^= (x << 5) & 0xFFFFFFFF; st[0] = x
    return x/4294967296.0
@njit(cache=True)
def _gauss(st):                                             # cheap normal: sum of 4 uniforms, variance 1
    return (_rand(st)+_rand(st)+_rand(st)+_rand(st)-2.0)*1.7320508
@njit(cache=True)
def _lerp3(u, a, b, c, ub):                                 # piecewise linear through (0,a) (ub,b) (1,c)
    if u < ub: return a+(b-a)*u/ub
    return b+(c-b)*(min(u, 1.0)-ub)/(1.0-ub)

@njit(cache=True)
def voice(n, sr, P, flags, seed, indiv):
    """Render n samples of one insect.  indiv = (pitch mult, rate mult, gap mult, pan -1..1, start delay s) for poly voices.
    Returns L, R, pulse trigger, chirp gate, and the two raw resonator outputs summed (for tests)."""
    L = np.zeros(n); R = np.zeros(n); pulse_out = np.zeros(n); chirp_out = np.zeros(n)
    st = np.array([seed*2654435761 % 4294967296 or 1], dtype=np.uint64)
    # resonators: z = z*coef + impulse, output = imag(z)
    zA = 0j; zB = 0j; cA = 0j; cB = 0j; fA = 1000.0; fB = 1000.0
    # body filters (TPT state-variable band-pass), one per resonator
    s1A = 0.0; s2A = 0.0; s1B = 0.0; s2B = 0.0
    # song state
    BODYS, REL, REST = 0, 1, 2
    sstate = BODYS if (flags[0] == 0 and indiv[4] <= 0) else REST
    tS = 0.0; restLen = indiv[4];                           # poly voices start late so a chorus is not in step
    gate_prev = 0.0; trig_prev = 0.0; clock_prev = 0.0
    syl = 0; tSyl = 0.0; sylLen = 1.0; relLen = 0.05; pitchAtRel = 1.0
    # chirp scheduler
    pulsesLeft = 0; pulseTimer = 0.0; inGap = False; tChirp = 0.0; chirpDur = 0.1; pulseIdx = 0
    trigChirp = False; waitingClock = False; pauseT = 0.0
    side = 0
    # mechanism state
    ribsLeft = 0; ribTimer = 0.0; ribK = 0; ribN = 1; outTimer = -1.0; pulseAmp = 0.0; ribSide = 0
    toothOn = False; toothTimer = 0.0; tPulse = 0.0; pulseDur = 1.0; upTimer = -1.0
    buckA = 0.0; buckB = 0.0                                # how far each tymbal is buckled, 0..1
    drift = 0.0; driftTarget = 0.0; driftT = 0.0
    # values refreshed at control rate
    f0 = 1000.0; q = 10.0; lock = 1.0; rate = 30.0; nChirp = 0; gap = 0.2; tf = 1.0; loose = 0.0
    ampMul = 1.0; pitchMul = 1.0; rateMul = 1.0; openOfs = 0.0; gapMul = 1.0; nMul = 1.0
    cont = False; active = True; mech = 0; stroke = 0.5; bodyD = 0.0; g_bp = 0.0; R_bp = 0.0; drop = 0.0
    decay = 0.999; norm = 1.0; tremPh = 0.0; panL_A = 0.7; panR_A = 0.7; panL_B = 0.7; panR_B = 0.7
    dt = 1.0/sr; song = 0; hold = True; Ls = 8.0; fc = 1000.0; S = 0.2

    for i in range(n):
        # ================= control rate =================
        if i % CTL == 0:
            mech = int(P[MECH, i]+0.5); loose = P[LOOSE, i]; stroke = P[STROKE, i]
            tf = map_temp(P[TEMP, i]); song = int(P[SONG, i]+0.5)
            q = map_q(P[PURE, i]); lock = map_lock(P[PURE, i])
            Lk = map_length(P[LENGTH, i]); hold = Lk < 0; Ls = 8.0 if hold else Lk
            nChirp = map_chirp(P[CHIRP, i]); gap = map_gap(P[GAP, i])*indiv[2]/tf
            # slow drift of the carrier (a live animal is never perfectly steady)
            driftT -= CTL*dt
            if driftT <= 0: driftTarget = _gauss(st); driftT = 0.4+0.8*_rand(st)
            drift += (driftTarget-drift)*CTL*dt*3.0
            wander = 1.0+drift*loose*(0.03 if mech == WING else 0.006)
            if P[TRACK, i] > 0.5:       # V/OCT plays the pulse rate, RATE knob transposes +-4 octaves
                f0 = map_pitch(P[PITCH, i])
                rate = 261.6256*2.0**(P[VOCT, i]+(P[RATE, i]-0.5)*8.0)
            else:
                f0 = map_pitch(P[PITCH, i])*2.0**P[VOCT, i]
                rate = map_rate(P[RATE, i])
            f0 *= indiv[0]*tf**0.38*wander; rate *= indiv[1]*tf
            rate = min(rate, 2000.0)
            # ---- song: gate / phrase state machine ----
            gate = P[GATE, i] > 1.0
            if flags[0] == 1:
                if gate and gate_prev <= 0.5: sstate = BODYS; tS = 0.0; syl = 0; tSyl = 0.0; pulsesLeft = 0; inGap = False; pulseTimer = 0.0
                if (not gate) and gate_prev > 0.5 and sstate == BODYS: sstate = REL; tS = 0.0; tSyl = 0.0
                gate_prev = 1.0 if gate else 0.0
            S = (max(nChirp, 1)/rate+gap)                    # chirp period = TALK syllable length
            x = min(1.0, tS/Ls) if sstate == BODYS else 1.0
            ampMul = 1.0; pitchMul = 1.0; rateMul = 1.0; openOfs = 0.0; gapMul = 1.0; nMul = 1.0
            cont = nChirp == 0; active = sstate != REST
            relLen = 0.03
            if song == SWELL or song == PULSATE:
                ampMul = 0.03+0.97*min(1.0, x/0.45); relLen = 0.3*Ls
                if song == PULSATE:
                    depth = 0.45*min(1.0, max(0.0, x-0.1)/0.45)
                    tremPh += 6.3*tf*CTL*dt; tr = np.sin(2*np.pi*tremPh)
                    ampMul *= 1.0-depth*(0.5+0.5*tr); openOfs = 0.12*(depth/0.45)*tr
            elif song == RISE:
                ampMul = 0.25+0.75*x**1.5
            elif song == SLUR:
                ampMul = 0.1+0.9*min(1.0, tS/0.4) if sstate == BODYS else 1.0
                relLen = min(1.2, max(0.15, 0.2*Ls))
            elif song == TICKBUZZ:
                relLen = 0.25*Ls
                if x < 0.45 and sstate == BODYS:
                    cont = True; rateMul = 0.06+0.09*x/0.45; ampMul = 0.5+0.5*x/0.45
                else:
                    cont = True; u = (x-0.45)/0.55
                    pitchMul = 0.96+0.08*min(1.0, u/0.4); ampMul = 0.5+0.5*min(1.0, u/0.35)
            elif song == SINK:
                relLen = 0.15*Ls
                gapMul = 1.0/(1.0+1.6*x); nMul = 1.0-0.35*x; pitchMul = 1.0-0.10*x**1.5
                ampMul = _lerp3(x, 0.5, 1.0, 0.8, 0.15) if x < 0.6 else 0.8-0.45*(x-0.6)/0.4
                u = min(1.0, tChirp/max(chirpDur, 1e-3))
                ampMul *= _lerp3(u, 0.6, 1.0, 0.35, 0.15); pitchMul *= 1.0-0.05*u
                openOfs = -0.28*min(1.0, tChirp/(1.6*max(chirpDur, 1e-3)))
            elif song == TALK:
                cont = True; relLen = 5.0*S
                sylLen = (3.3*S if syl == 0 else S) if sstate == BODYS else 5.0*S
                up = min(1.0, tSyl/(0.25*S)); dn = min(1.0, max(0.0, (sylLen-tSyl)/(0.35*S)))
                if sstate == REL:
                    w = up; ampMul = 0.25+0.75*up; pitchMul = 1.0-0.04*tSyl/sylLen
                    ampMul *= min(1.0, (sylLen-tSyl)/(0.3*sylLen))
                else:
                    w = min(up, dn); ampMul = 0.25+0.75*min(up, dn)
                openOfs = -0.43*(1.0-w)
                tSyl += CTL*dt
                if sstate == BODYS and tSyl >= sylLen:
                    tSyl = 0.0; syl += 1
                    if tS >= Ls and not hold and flags[0] == 0: sstate = REL; tS = 0.0
            if sstate == REL:
                y = min(1.0, tS/relLen)
                if song == STEADY or song == RISE or song == SINK: ampMul *= 1.0-y
                elif song == SWELL or song == PULSATE: ampMul *= 1.0-y; pitchMul *= 1.0-0.07*y; rateMul *= 1.0-0.25*y
                elif song == SLUR: ampMul *= 1.0-0.85*y; pitchMul *= 1.0-0.26*y
                elif song == TICKBUZZ: ampMul *= 1.0-0.9*y; pitchMul = 1.04-0.16*y
            # advance the phrase clock
            tS += CTL*dt
            if sstate == BODYS:
                if flags[0] == 0 and (not hold) and tS >= Ls and song != TALK: sstate = REL; tS = 0.0
            elif sstate == REL:
                if tS >= relLen:
                    sstate = REST; tS = 0.0
                    restLen = REST_FRAC[song]*Ls*max(0.2, 1.0+0.3*loose*_gauss(st))
            elif flags[0] == 0 and tS >= restLen:
                sstate = BODYS; tS = 0.0; syl = 0; tSyl = 0.0; pulsesLeft = 0; inGap = False; pulseTimer = 0.0
            # ---- derived mechanism values ----
            fc = min(f0*pitchMul, 0.45*sr)
            bw = fc/q; decay = np.exp(-np.pi*bw*dt)
            drop = max(0.0, 0.12*(1.0-P[PURE, i]/0.75))
            # loudness normalisation so that every setting lands near the same level
            if mech == FILE:
                tr_ = fc*(0.22+0.78*lock)
                norm = lock*(np.pi/q)*1.15+(1.0-lock)*0.5/np.sqrt(tr_/(4*np.pi*bw))
            elif mech == TYMBAL:
                nr = 1+int(stroke*11+0.5); norm = 1.1/np.sqrt(nr)/np.sqrt(max(1.0, rate*rateMul/(np.pi*bw)))
            else:
                norm = 0.9/np.sqrt(max(1.0, rate*rateMul/(np.pi*bw)))
            # body band-pass
            bodyD = 0.9*P[BODY, i]
            o = min(1.0, max(0.0, P[OPEN, i]+openOfs))
            fb = min(fc*2.0**(2.0*(o-0.5)), 0.45*sr)
            g_bp = np.tan(np.pi*fb*dt); R_bp = 1.0/3.5       # R = 1/Q, Q = 3.5
            # pan: the two tymbals sit 0.15 either side of the individual's position
            pc = indiv[3]; sp = 0.15 if mech == TYMBAL else 0.0
            a = (min(1.0, max(-1.0, pc-sp))+1.0)*np.pi/4; panL_A = np.cos(a); panR_A = np.sin(a)
            a = (min(1.0, max(-1.0, pc+sp))+1.0)*np.pi/4; panL_B = np.cos(a); panR_B = np.sin(a)
            # resonator frequencies follow the mechanism state (updated again at every impulse)
            if mech == WING:
                fA = fc; fB = min(fc*1.9, 0.45*sr)
            cA = decay*np.exp(1j*2*np.pi*fA*dt); cB = decay*np.exp(1j*2*np.pi*fB*dt)

        # ================= triggers (sample rate) =================
        tg = P[TRIG, i] > 1.0
        if flags[1] == 1 and tg and trig_prev <= 0.5:
            trigChirp = True; pulsesLeft = max(nChirp, 1); pulseTimer = 0.0; inGap = False; pulseIdx = 0; tChirp = 0.0
        trig_prev = 1.0 if tg else 0.0
        ck = P[CLOCK, i] > 1.0
        if flags[2] == 1 and ck and clock_prev <= 0.5 and nChirp > 0:
            pulsesLeft = max(1, int(nChirp*nMul+0.5)); pulseTimer = 0.0; inGap = False; pulseIdx = 0; tChirp = 0.0
            chirpDur = pulsesLeft/(rate*rateMul)
        clock_prev = 1.0 if ck else 0.0

        # ================= chirp scheduler: decides when a pulse (one muscle twitch) starts =================
        tChirp += dt
        pulseTimer -= 1.0
        while pulseTimer <= 0.0:
            period = sr/(rate*rateMul)
            fire = False; amp = ampMul
            if flags[1] == 1:                               # TRIG patched: only triggered chirps, song ignored
                if pulsesLeft > 0: fire = True; amp = 1.0; pulsesLeft -= 1
                else: pulseTimer = 1e12; break
            elif not active:
                pulseTimer += period; pulsesLeft = 0; inGap = False; continue
            elif cont:
                fire = True
                if pauseT > 0: fire = False; pauseT -= period*dt
                elif nChirp == 0 and song == STEADY and mech != WING and _rand(st) < 0.15*loose*period*dt: pauseT = 0.15+0.25*_rand(st)
            else:
                if pulsesLeft == 0 and flags[2] == 1:       # CLOCK patched: wait for the next clock edge
                    pulseTimer = 1e12; break
                if pulsesLeft == 0:                         # start a new chirp
                    pulsesLeft = max(1, int(nChirp*nMul+0.5)); pulseIdx = 0; tChirp = 0.0
                    chirpDur = pulsesLeft/(rate*rateMul)
                    if pulsesLeft >= 3 and _rand(st) < 0.4*loose**1.5: pulsesLeft -= 1   # a dropped last pulse
                fire = True; pulsesLeft -= 1
            if fire:
                late = -pulseTimer                           # samples since the exact onset (0..1)
                # chirp attack: the first pulses of a chirp are quieter (measured on field crickets)
                if (not cont) and nChirp >= 3 and pulseIdx < 2: amp *= 0.45+0.30*pulseIdx
                amp *= max(0.0, 1.0+0.2*loose*_gauss(st))
                pulse_out[i] = 1.0; pulseIdx += 1
                # ---- start the mechanism ----
                pulseAmp = amp*norm
                if mech == TYMBAL:
                    ribN = 1+int(stroke*11+0.5); ribsLeft = ribN; ribK = 0; ribTimer = -late; ribSide = side
                    outTimer = max(0.48*period, (ribN+0.5)*sr/fc)-late; side = 1-side
                elif mech == FILE:
                    toothOn = True; toothTimer = -late; tPulse = 0.0
                    pulseDur = (0.05+0.9*stroke)*period
                else:
                    zA += pulseAmp*cA**late; upTimer = 0.45*period-late
            pulseTimer += period*max(0.2, 1.0+0.08*loose*_gauss(st))
            if (not cont) and flags[1] == 0 and pulsesLeft == 0 and flags[2] == 0:
                pulseTimer += sr*gap*gapMul*max(0.2, 1.0+0.3*loose*_gauss(st))
        chirp_out[i] = 1.0 if (pulsesLeft > 0 or toothOn or ribsLeft > 0) and not cont else (1.0 if cont and active and flags[1] == 0 else 0.0)

        # ================= mechanism: turns a pulse into impulses =================
        if mech == TYMBAL:
            if ribsLeft > 0:
                ribTimer -= 1.0
                while ribTimer <= 0.0 and ribsLeft > 0:
                    late = -ribTimer
                    fr = fc*(1.0-drop*ribK/max(ribN-1, 1))  # each buckled rib lowers the tymbal's pitch
                    a = pulseAmp*(1.0-0.5*ribK/ribN)*max(0.0, 1.0+0.24*loose*_gauss(st))
                    if ribSide == 0:
                        fA = fr*0.988; cA = decay*np.exp(1j*2*np.pi*fA*dt); zA += a*cA**late; buckA = 1.0
                    else:
                        fB = fr*1.012; cB = decay*np.exp(1j*2*np.pi*fB*dt); zB += a*cB**late; buckB = 1.0
                    ribK += 1; ribsLeft -= 1
                    ribTimer += (sr/fr)*max(0.3, 1.0+0.12*loose*_gauss(st))
            if outTimer > -0.5:
                outTimer -= 1.0
                if outTimer <= 0.0:                          # the tymbal springs back: quieter, higher click
                    late = -outTimer; outTimer = -1.0
                    if ribSide == 0:
                        fA = fc*0.988*1.08; cA = decay*np.exp(1j*2*np.pi*fA*dt); zA += 0.35*pulseAmp*cA**late; buckA = 0.0
                    else:
                        fB = fc*1.012*1.08; cB = decay*np.exp(1j*2*np.pi*fB*dt); zB += 0.35*pulseAmp*cB**late; buckB = 0.0
        elif mech == FILE:
            if toothOn:
                toothTimer -= 1.0; tPulse += 1.0
                while toothTimer <= 0.0 and toothOn:
                    late = -toothTimer; xx = (tPulse-late)/pulseDur
                    if xx >= 1.0: toothOn = False; break
                    ft = fc*(1.0-0.08*lock*(xx-0.5))        # the carrier slides down through each pulse
                    jit = 0.45*(1.0-lock)
                    a = pulseAmp*max(0.02, np.sin(np.pi*max(xx, 0.0))**0.7)*(1.0+jit*_gauss(st))
                    fA = ft; cA = decay*np.exp(1j*2*np.pi*fA*dt); zA += a*cA**late
                    tr = ft*(0.22+0.78*lock)
                    toothTimer += (sr/tr)*max(0.2, 1.0+jit*_gauss(st))
        else:
            if upTimer > -0.5:
                upTimer -= 1.0
                if upTimer <= 0.0: zB += stroke*pulseAmp*cB**(-upTimer); upTimer = -1.0

        # ================= resonators and body =================
        zA *= cA; zB *= cB
        a_ = zA.imag; b_ = zB.imag
        if bodyD > 0.0:
            d1 = 1.0/(1.0+R_bp*g_bp+g_bp*g_bp)
            hp = (a_-(R_bp+g_bp)*s1A-s2A)*d1; bp = g_bp*hp+s1A; s1A = g_bp*hp+bp; s2A = g_bp*bp+s2A+g_bp*bp
            a_ = (1.0-bodyD)*a_+bodyD*bp*R_bp
            hp = (b_-(R_bp+g_bp)*s1B-s2B)*d1; bp = g_bp*hp+s1B; s1B = g_bp*hp+bp; s2B = g_bp*bp+s2B+g_bp*bp
            b_ = (1.0-bodyD)*b_+bodyD*bp*R_bp
        mk = 1.0+bodyD                                       # make-up for what the body filter removes
        L[i] = mk*(a_*panL_A+b_*panL_B); R[i] = mk*(a_*panR_A+b_*panR_B)
    return L, R, pulse_out, chirp_out

# ---- shared stage: air and leaves -----------------------------------------------------------------------
_r = np.random.default_rng(7)
TAPS = [(float(np.round(_r.uniform(0.004, 0.12), 5)), int(_r.choice([-1, 1])), int(_r.integers(0, 2)), int(_r.integers(0, 2))) for _ in range(24)]
# each tap: (delay seconds, sign, destination channel, source channel).  Printed in the build spec as a table.

def distance(L, R, d, sr):
    """DISTANCE 0..1: one-pole low-pass at 18000(1-d)^2+2500 Hz, plus 24 fixed reflections, low-passed at 5 kHz."""
    from scipy import signal
    y = np.stack([L, R], 1)
    if d <= 0: return L, R
    y = signal.sosfilt(signal.butter(1, min(18000*(1-d)**2+2500, 0.45*sr), fs=sr, output='sos'), y, axis=0)
    wet = np.zeros_like(y)
    for (tt, sg, dst, src) in TAPS:
        k = int(tt*sr); wet[k:, dst] += sg*d*0.5*np.exp(-tt/0.05)*y[:-k, src]
    y = y+signal.sosfilt(signal.butter(1, 5000, fs=sr, output='sos'), wet, axis=0)
    return y[:, 0], y[:, 1]

OUT_GAIN = 5.0     # volts for a normalised level of 1.0; final stage is 10 V * tanh(x / 10 V)

def render(preset, seconds, sr=48000, seed=1, voices=1, auto=None, flags=(0, 0, 0)):
    """preset: dict of knob values (rows of P by name).  auto: dict name -> array to automate a row."""
    n = int(seconds*sr); P = np.zeros((NP, n))
    names = dict(MECH=MECH, PITCH=PITCH, PURE=PURE, STROKE=STROKE, BODY=BODY, OPEN=OPEN, RATE=RATE, CHIRP=CHIRP, GAP=GAP,
                 SONG=SONG, LENGTH=LENGTH, TEMP=TEMP, LOOSE=LOOSE, VOCT=VOCT, TRACK=TRACK, GATE=GATE, TRIG=TRIG, CLOCK=CLOCK)
    base = dict(MECH=0, PITCH=0.6, PURE=0.5, STROKE=0.3, BODY=0, OPEN=0.5, RATE=0.5, CHIRP=1, GAP=0.5, SONG=0, LENGTH=1,
                TEMP=0.5, LOOSE=0.4, VOCT=0, TRACK=0, GATE=0, TRIG=0, CLOCK=0)
    base.update({k: v for k, v in preset.items() if k in names})
    for k, v in base.items(): P[names[k]] = v
    for k, v in (auto or {}).items(): P[names[k]] = v
    L = np.zeros(n); R = np.zeros(n)
    for v in range(voices):
        indiv = individual(v, voices)
        l, r, pu, ch = voice(n, float(sr), P, np.array(flags), seed+17*v, indiv)
        L += l; R += r
    L, R = distance(L, R, preset.get('DISTANCE', 0.15), sr)
    y = np.stack([L, R], 1)*OUT_GAIN
    return 10.0*np.tanh(y/10.0), pu, ch

def individual(v, voices):
    """Poly voice v differs from voice 0 by fixed amounts: pitch, rate, gap multipliers and pan position."""
    if v == 0 and voices == 1: return np.array([1.0, 1.0, 1.0, 0.0, 0.0])
    r = np.random.default_rng(1000+v)
    pan = 0.0 if voices == 1 else -0.8+1.6*((v*7) % voices)/(voices-1)
    if v == 0: return np.array([1.0, 1.0, 1.0, pan, 0.0])
    return np.array([2**(r.uniform(-1, 1)*0.035), 1+r.uniform(-1, 1)*0.04, 1+r.uniform(-1, 1)*0.15, pan, r.uniform(0.2, 4.0)])
