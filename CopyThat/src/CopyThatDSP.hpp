// Copy That: an FM radio link that loses the signal.
// Behavioural model inspired by the Fairfield Circuitry Roger That. Nothing here comes
// from Fairfield: the pedal's circuit is not published. MIT licence (see LICENSE.txt).
//
// How it works, in plain words:
//   your audio -> DRIVE preamp -> FM transmitter (pre-emphasis, broadcast clipper)
//   -> the air (noise, hum, crackle, phone buzz, a neighbouring station)
//   -> FM receiver tuned by TUNE (IF filter + automatic frequency control set by SHIFT)
//   -> FM discriminator -> de-emphasis -> back to audio.
// The radio is simulated in "complex baseband", the way software radios do it: each
// station is a spinning phasor whose speed is its frequency relative to the receiver.
// Nothing is faked: the clicks, hiss, distortion and squeals all come out of the
// receiver struggling with a weak or mistuned signal, as on a real FM radio.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace copythat {

static const float PI_F = 3.14159265358979f;
static const float TWO_PI = 6.28318530717959f;

// ---- small building blocks -------------------------------------------------------

struct Rng {
	uint32_t s = 0x9E3779B9u;
	void seed(uint32_t v) { s = v ? v : 0x1234567u; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }   // 0..1
	float bi() { return uni() * 2.f - 1.f; }                      // -1..1
	float gauss() { // sum of 4 uniforms, unit variance, cheap
		return (uni() + uni() + uni() + uni() - 2.f) * 1.7320508f;
	}
};

struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void lowpass(float fc, float fs, float q) {
		float w = TWO_PI * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		float a0 = 1.f + al;
		b0 = (1.f - c) * 0.5f / a0; b1 = (1.f - c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	void highpass(float fc, float fs, float q) {
		float w = TWO_PI * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		float a0 = 1.f + al;
		b0 = (1.f + c) * 0.5f / a0; b1 = -(1.f + c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	void reset() { z1 = z2 = 0; }
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
};

// First-order shelf (1 + s*t1) / (1 + s*t2), bilinear. Swap t1/t2 for the exact inverse.
struct Shelf1 {
	float b0 = 1, b1 = 0, a1 = 0, x1 = 0, y1 = 0;
	void set(float t1, float t2, float fs) {
		float k = 2.f * fs;
		float a0 = 1.f + k * t2;
		b0 = (1.f + k * t1) / a0; b1 = (1.f - k * t1) / a0; a1 = (1.f - k * t2) / a0;
	}
	void reset() { x1 = y1 = 0; }
	float process(float x) { float y = b0 * x + b1 * x1 - a1 * y1; x1 = x; y1 = y; return y; }
};

// 8th-order Butterworth lowpass (4 biquads): the decimation filter.
struct Lp8 {
	Biquad s[4];
	void set(float fc, float fs) {
		static const float q[4] = {0.50979558f, 0.60134489f, 0.89997622f, 2.5629154f};
		for (int i = 0; i < 4; i++) s[i].lowpass(fc, fs, q[i]);
	}
	void reset() { for (auto& b : s) b.reset(); }
	float process(float x) { for (auto& b : s) x = b.process(x); return x; }
};

// One transmitter: preamp output in, phase increment out.
struct Transmitter {
	Shelf1 pre;
	void set(float fsi) { pre.set(50e-6f, 7.5e-6f, fsi); }   // 50 us broadcast pre-emphasis
	void reset() { pre.reset(); }
	// returns the modulation m (about -1..1)
	float modulate(float p) {
		float e = pre.process(p);
		// broadcast clipper: transparent below ~0.7, then rounds into a hard ceiling at 1
		float a = std::fabs(e);
		if (a > 0.7f) e = std::copysign(0.7f + 0.3f * std::tanh((a - 0.7f) / 0.3f), e);
		return e;
	}
};

// ---- settings the module hands over each sample ------------------------------------

struct Settings {
	float tune = 0.25f;   // 0..1 (knob + CV)
	float shift = 0.3f;   // 0..1
	float drive = 0.3f;   // 0..1
	float fade = 1.f;     // 0..1 signal strength
	bool extended = false; // RANGE 2
	float mainsHz = 50.f;
};

struct Frame {
	float wet = 0.f;     // volts, in phase with the dry (the module inverts it)
	float dry = 0.f;     // volts, the driven input, time-aligned with the wet
	float signal = 0.f;  // 0..1 receiver signal strength
	bool lock = false;
};

// Nominal FM deviation for full modulation (Hz, in our scaled-down radio).
static const float DEV_HZ = 15000.f;
static const float NEIGH_HZ = 50000.f;   // neighbouring station, RANGE 2
static const float NEIGH_DELAY = 0.130f; // its programme: your input 130 ms late
static const float NOISE_SIGMA = 0.047f; // per-sample noise at the internal rate
static const float LEAK = 0.06f;         // receiver LO leakage: the tuning whistle

// Receiver mistuning in Hz from the TUNE position (0..1).
// RANGE 1: on station at 0, fine at first then coarse (0 .. 32 kHz off).
// RANGE 2: from 15 kHz below the station, through it (t ~ 0.36), up past the
// neighbouring station (t ~ 0.85) to 70 kHz.
inline float tuneToHz(float t, bool extended) {
	t = std::min(std::max(t, 0.f), 1.f);
	if (!extended) return 32000.f * t * t;
	return -15000.f + 85000.f * std::pow(t, 1.7f);
}

// ---- one channel of the whole link ------------------------------------------------

struct Channel {
	// rates
	float fs = 48000.f, fsi = 192000.f;
	int os = 4;
	// input side
	float pPrev = 0.f, nPrev = 0.f;
	std::vector<float> delay; int dpos = 0, dlen = 1;
	Transmitter tx, txN;
	double phS = 0.0, phN = 0.0;     // station / neighbour phase
	// channel
	Rng rng;
	float humPh = 0.f;
	float crackle = 0.f, crackleRe = 0.f, crackleIm = 0.f;
	float gsmTime = 0.f, gsmPh = 0.f;
	float drift = 0.f, driftT = 0.f, driftTarget = 0.f;
	// receiver
	double loPh = 0.0;
	Biquad ifI[2], ifQ[2];
	float lastCut = -1.f;
	float zPrevI = 1.f, zPrevQ = 0.f;
	float afcC = 0.f, afcErr = 0.f;
	float pwr = 0.f;
	Shelf1 de;
	Lp8 decW, decD;
	float dcW = 0.f, dcD = 0.f;
	float sig = 0.f, muteS = 1.f; bool locked = false;
	Biquad voiceHp, voiceLp;

	void setRate(float sampleRate, uint32_t seed) {
		fs = sampleRate;
		os = std::max(1, std::min(8, (int) std::ceil(176000.f / fs)));
		fsi = fs * os;
		tx.set(fsi); txN.set(fsi);
		de.set(7.5e-6f, 50e-6f, fsi);                     // exact inverse of the pre-emphasis
		float fc = std::min(15000.f, 0.42f * fs);
		decW.set(fc, fsi); decD.set(fc, fsi);
		voiceHp.highpass(300.f, fs, 0.707f); voiceLp.lowpass(3000.f, fs, 0.707f);
		dlen = std::max(2, (int) (NEIGH_DELAY * fs));
		delay.assign(dlen, 0.f); dpos = 0;
		lastCut = -1.f;
		rng.seed(seed);
		reset();
	}
	void reset() {
		pPrev = nPrev = 0.f; std::fill(delay.begin(), delay.end(), 0.f);
		tx.reset(); txN.reset(); phS = phN = loPh = 0.0;
		for (int i = 0; i < 2; i++) { ifI[i].reset(); ifQ[i].reset(); }
		zPrevI = 1.f; zPrevQ = 0.f; afcC = afcErr = 0.f; pwr = 1.f;
		de.reset(); decW.reset(); decD.reset(); voiceHp.reset(); voiceLp.reset();
		crackle = 0.f; gsmTime = 0.f; drift = driftT = 0.f; sig = 1.f; muteS = 1.f; dcW = dcD = 0.f; locked = true;
	}

	// DRIVE: 0 .. +30 dB into an asymmetric JFET-style curve. Returns about -1..1.
	static float preamp(float x, float drive) {
		float g = std::pow(10.f, drive * 1.5f);           // 1 .. 31.6
		float u = x * 0.2f * g;                            // 5 V -> 1
		const float bias = 0.25f;
		float y = std::tanh(u + bias) - std::tanh(bias);
		return y / (1.f - std::tanh(bias) * std::tanh(bias)); // unity slope at zero
	}

	Frame process(float in, const Settings& st, bool voice) {
		// ---- transmitter side (base rate)
		float p = preamp(in, st.drive);
		float pN = delay[dpos];
		delay[dpos] = p; if (++dpos >= dlen) dpos = 0;

		// ---- receiver settings for this sample
		float cut = 16000.f * std::pow(3000.f / 16000.f, st.shift);   // IF bandwidth
		if (std::fabs(cut - lastCut) > 0.5f) {
			static const float q[2] = {0.5411961f, 1.3065630f};      // 4th-order Butterworth
			for (int i = 0; i < 2; i++) { ifI[i].lowpass(cut, fsi, q[i]); ifQ[i].lowpass(cut, fsi, q[i]); }
			lastCut = cut;
		}
		float ifOffset = 0.35f * st.shift * cut;                       // off-centre IF: asymmetric
		// AFC (automatic frequency control) pulls the receiver toward what it hears.
		// It is an integrator behind a smoothing filter: a second-order loop with natural
		// frequency wn and damping zeta. Low SHIFT = quicker and well damped (accurate);
		// high SHIFT = slow and under-damped, so it overshoots and hunts (warble, pumping).
		float wn = TWO_PI * 4.f * std::pow(0.5f / 4.f, st.shift);
		float zeta = 0.9f - 0.78f * st.shift;
		float afcLp = 2.f * zeta * wn;          // smoothing of the error, rad/s
		float afcK = wn * wn / afcLp;           // integrator gain
		float pull = 3000.f * (1.f - 0.5f * st.shift);  // how far it can pull, Hz

		// slow oscillator drift, more as you go off station
		driftT -= 1.f / fs;
		if (driftT <= 0.f) { driftT = 0.3f + 1.5f * rng.uni(); driftTarget = rng.bi() * 150.f * (0.3f + st.tune); }
		drift += (driftTarget - drift) * (1.5f / fs);

		float detune = tuneToHz(st.tune, st.extended) + drift;
		float bad = 1.f - sig;                                         // how lost we are
		float A = st.fade;
		float nA = st.extended ? 0.7f : 0.f;

		// GSM-style phone buzz: occasional episodes when the signal is poor
		if (gsmTime <= 0.f) {
			if (bad > 0.5f && rng.uni() < (bad - 0.5f) * 0.3f / fs) gsmTime = 0.4f + 1.2f * rng.uni();
		} else gsmTime -= 1.f / fs;

		float wSum = 0.f, dSum = 0.f;
		for (int k = 0; k < os; k++) {
			float fr = (k + 1) / (float) os;
			float pi_ = pPrev + (p - pPrev) * fr;
			float pni = nPrev + (pN - nPrev) * fr;

			// station phasor
			float m = tx.modulate(pi_);
			float fLo = detune + afcC;             // receiver offset from the station
			float fS = DEV_HZ * m - fLo + ifOffset;
			phS += fS / fsi; phS -= std::floor(phS);
			float hum = 1.f + 0.35f * bad * std::sin(humPh) + 0.12f * bad * std::sin(2.f * humPh);
			humPh += TWO_PI * st.mainsHz / fsi; if (humPh > TWO_PI) humPh -= TWO_PI;
			float aS = A * hum;
			float zi = aS * std::cos(TWO_PI * (float) phS), zq = aS * std::sin(TWO_PI * (float) phS);
			// neighbouring station (RANGE 2)
			if (nA > 0.f) {
				float mN = txN.modulate(pni);
				phN += (DEV_HZ * mN + NEIGH_HZ - fLo + ifOffset) / fsi; phN -= std::floor(phN);
				zi += nA * std::cos(TWO_PI * (float) phN); zq += nA * std::sin(TWO_PI * (float) phN);
			}
			// receiver LO leakage at the receiver's own centre: the tuning whistle
			zi += LEAK * std::cos(TWO_PI * (float) loPh); zq += LEAK * std::sin(TWO_PI * (float) loPh);
			loPh += ifOffset / fsi; loPh -= std::floor(loPh);
			// thermal noise
			zi += NOISE_SIGMA * rng.gauss(); zq += NOISE_SIGMA * rng.gauss();
			// crackle and pops: sparse impulses, denser when the signal is poor
			if (crackle > 0.01f) { zi += crackle * crackleRe; zq += crackle * crackleIm; crackle *= 0.6f; }
			else if (rng.uni() < (0.5f + 40.f * bad * bad) / fsi) {
				crackle = 0.4f + 2.5f * rng.uni() * rng.uni();
				float a = TWO_PI * rng.uni(); crackleRe = std::cos(a); crackleIm = std::sin(a);
			}
			// phone buzz: 217 Hz frames of 0.577 ms bursts
			if (gsmTime > 0.f) {
				gsmPh += 217.f / fsi; if (gsmPh >= 1.f) gsmPh -= 1.f;
				if (gsmPh < 0.577e-3f * 217.f) { zi += 1.2f * rng.gauss(); zq += 1.2f * rng.gauss(); }
			}

			// IF filter (complex: same lowpass on I and Q)
			for (int i = 0; i < 2; i++) { zi = ifI[i].process(zi); zq = ifQ[i].process(zq); }
			pwr += (zi * zi + zq * zq - pwr) * (30.f / fsi);

			// discriminator: angle between this sample and the last = instantaneous frequency
			float re = zi * zPrevI + zq * zPrevQ;
			float im = zq * zPrevI - zi * zPrevQ;
			zPrevI = zi; zPrevQ = zq;
			float fHz = std::atan2(im, re) * fsi / TWO_PI - ifOffset;

			// AFC: the station sits at -fLo on average; nudge the LO until that is zero
			afcErr += (std::min(std::max(fHz, -60000.f), 60000.f) - afcErr) * (afcLp / fsi);
			afcC += afcK * afcErr / fsi;
			afcC = std::min(std::max(afcC, -pull), pull);

			float audio = de.process(fHz / DEV_HZ);
			wSum = decW.process(audio);
			dSum = decD.process(pi_);
		}
		pPrev = p; nPrev = pN;

		// signal meter: IF power above the floor (noise + leakage), in dB
		float floorP = NOISE_SIGMA * NOISE_SIGMA * 2.f * (2.f * lastCut / fsi) + LEAK * LEAK;
		float cnr = 10.f * std::log10(std::max(pwr / floorP - 1.f, 1e-4f));
		float target = std::min(std::max(cnr / 25.f, 0.f), 1.f);
		sig += (target - sig) * (30.f / fs);
		if (locked && cnr < 9.f) locked = false;
		else if (!locked && cnr > 13.f) locked = true;

		// soft mute, like a real FM tuner: as the signal weakens the receiver turns itself
		// down a little, so between stations the hiss does not swamp everything
		float mute = 0.3f + 0.7f * std::min(1.f, sig * 1.6f);
		muteS += (mute - muteS) * (20.f / fs);
		Frame f;
		// DC block (a mistuned receiver reads the offset as DC), same filter on the dry so
		// the two stay matched for cancellation
		float wb = wSum - dcW; dcW += wb * (TWO_PI * 8.f / fs);
		float db = dSum - dcD; dcD += db * (TWO_PI * 8.f / fs);
		// a narrow IF squeezes the deviation and the level drops: make some of it up
		float makeup = 1.f + 1.2f * st.shift * st.shift;
		float w = 5.f * wb * muteS * makeup;
		float aw = std::fabs(w);                     // soft ceiling around 10 V
		if (aw > 6.f) w = std::copysign(6.f + 4.f * std::tanh((aw - 6.f) / 4.f), w);
		if (voice) w = voiceLp.process(voiceHp.process(w));
		f.wet = w;
		f.dry = 5.f * db;
		f.signal = sig;
		f.lock = locked;
		return f;
	}

};

} // namespace copythat
