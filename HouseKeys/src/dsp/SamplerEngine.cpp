#include "SamplerEngine.hpp"
#include "../generated/Sounds.hpp"
#include <chrono>

namespace hk {

// ---------------------------------------------------------------- generators

static inline double polyBlepD(double t, double dt) {
	if (t < dt) { double x = t / dt; return x + x - x * x - 1.0; }
	if (t > 1.0 - dt) { double x = (t - 1.0) / dt; return x * x + x + x + 1.0; }
	return 0.0;
}

// band-limited saw rendered at 2x and decimated; freq(t) in Hz
template <typename F>
static std::vector<double> sawVoice(int N, double sr, double startPh, F freqAt) {
	std::vector<double> y(N);
	Decimator2 dec;
	double ph = startPh, fs2 = 2 * sr;
	for (int i = 0; i < N; i++) {
		float s2[2];
		for (int k = 0; k < 2; k++) {
			double t = (2 * i + k) / fs2;
			double dt = std::min(freqAt(t) / fs2, 0.45);
			ph += dt; if (ph >= 1.0) ph -= 1.0;
			s2[k] = (float) (2 * ph - 1 - polyBlepD(ph, dt));
		}
		y[i] = dec.process(s2[0], s2[1]);
	}
	return y;
}

static void applyBiquad(std::vector<double>& x, Biquad f) { for (auto& v : x) v = f.process((float) v); }

static std::vector<double> genStrings(double f0, int N, double sr, int players, double bright, double marcato, bool cello, uint32_t seed) {
	std::vector<double> x(N, 0.0);
	for (int p = 0; p < players; p++) {
		double det = hashBi(seed, p, 1) * 9.0;
		double vr = 5.0 + 1.2 * hash01(seed, p, 2), vd = 7.0 + 7.0 * hash01(seed, p, 3), vph = hash01(seed, p, 4);
		double atk = 0.02 + 0.05 * hash01(seed, p, 5);
		auto fr = [&](double t) {
			double fade = std::min(1.0, t / 0.25);
			double c = det + vd * fade * std::sin(2 * PI_D * (vr * t + vph));
			return f0 * std::exp2(c / 1200.0);
		};
		auto y = sawVoice(N, sr, hash01(seed, p, 6), fr);
		for (int i = 0; i < N; i++) { double t = i / sr; x[i] += y[i] * std::min(1.0, t / atk); }
	}
	for (auto& v : x) v /= std::sqrt((double) players);
	Biquad lp; lp.lowpass((float) std::min(2000.0 + 6000.0 * bright, 0.45 * sr), 0.7071f, (float) sr);
	applyBiquad(x, lp);
	const double vio[5] = {280, 460, 1000, 2700, 3400}, cel[5] = {95, 180, 250, 520, 1500};
	for (int k = 0; k < 5; k++) {
		double fc = cello ? cel[k] : vio[k];
		if (fc > 0.45 * sr) continue;
		Biquad pk; pk.peak((float) fc, 4.f, (k % 2 == 0) ? 6.f : -6.f, (float) sr);
		applyBiquad(x, pk);
	}
	if (marcato > 0) {
		Noise nz; nz.seed(seed + 99);
		Biquad bp; bp.bandpass((float) std::min(2740.0, 0.4 * sr), 0.8f, (float) sr);
		double bump = std::pow(10.0, 6.0 * marcato / 20.0) - 1.0;
		double rms = 1e-9; for (int i = 0; i < N; i++) rms += x[i] * x[i]; rms = std::sqrt(rms / N);
		for (int i = 0; i < N; i++) {
			double t = i / sr;
			double n = bp.process(nz.next()) * (t < 0.06 ? 1.0 - t / 0.06 : 0.0);
			x[i] = x[i] * (1.0 + bump * std::exp(-t / 0.06)) + n * rms * 2.0 * marcato;
		}
	}
	return x;
}

static std::vector<double> genPiano(double f0, int N, double sr, double hardness, int strings, uint32_t seed) {
	std::vector<double> x(N, 0.0);
	double B = 0.0004 * std::pow(f0 / 261.6, 1.3);
	double base = 2.5 * std::pow(261.6 / f0, 0.6);
	for (int s = 0; s < strings; s++) {
		double cents = strings == 1 ? 0.0 : (-1.2 + 2.4 * s / (strings - 1));
		double fs = f0 * std::exp2(cents / 1200.0);
		for (int n = 1; n < 400; n++) {
			double fn = n * fs * std::sqrt(1 + B * n * n);
			if (fn > 0.45 * sr) break;
			double a = std::fabs(std::sin(n * PI_D / 8)) * std::pow(n, -(1.9 - hardness)) * std::exp(-n / (8 + 30 * hardness));
			if (a < 1e-5) continue;
			double tau = base / (1 + 0.02 * std::pow(n, 1.7));
			double w = 2 * PI_D * fn / sr;
			double c = std::cos(w), sn = std::sin(w);
			double re = std::cos(hash01(seed, s, n) * 2 * PI_D), im = std::sin(hash01(seed, s, n) * 2 * PI_D);
			double d1 = std::exp(-1.0 / (tau * sr)), d2 = std::exp(-1.0 / (4 * tau * sr));
			double e1 = 0.7, e2 = 0.3;
			for (int i = 0; i < N; i++) {
				x[i] += a * im * (e1 + e2);
				double nr = re * c - im * sn; im = re * sn + im * c; re = nr;
				e1 *= d1; e2 *= d2;
			}
		}
	}
	Noise nz; nz.seed(seed + 7);
	Biquad lp; lp.lowpass(600.f, 0.7f, (float) sr);
	double pk = 1e-9; for (double v : x) pk = std::max(pk, std::fabs(v));
	double th = pk * std::pow(10.0, -14.0 / 20.0) * 3.0;
	for (int i = 0; i < N; i++) x[i] += lp.process(nz.next()) * th * std::exp(-(i / sr) / 0.025);
	return x;
}

static void formants(std::vector<double>& x, double sr, const double* F, const double* Bw, const double* G, int k) {
	std::vector<double> y(x.size(), 0.0);
	for (int j = 0; j < k; j++) {
		if (F[j] > 0.45 * sr) continue;
		Biquad bp; bp.bandpass((float) F[j], (float) (F[j] / Bw[j]), (float) sr);
		for (size_t i = 0; i < x.size(); i++) y[i] += G[j] * bp.process((float) x[i]);
	}
	x = y;
}

static std::vector<double> genChoir(double f0, int N, double sr, int singers, uint32_t seed) {
	std::vector<double> x(N, 0.0);
	for (int p = 0; p < singers; p++) {
		double det = hashBi(seed, p, 1) * 12.0, vph = hash01(seed, p, 2), vr = 4.6 + 0.8 * hash01(seed, p, 3);
		auto fr = [&](double t) { return f0 * std::exp2((det + 15.0 * std::min(1.0, t / 0.3) * std::sin(2 * PI_D * (vr * t + vph))) / 1200.0); };
		auto y = sawVoice(N, sr, hash01(seed, p, 4), fr);
		OnePole lp; lp.setLP((float) std::min(3 * f0, 0.4 * sr), (float) sr);
		for (int i = 0; i < N; i++) x[i] += lp.lp((float) y[i]);
	}
	Noise nz; nz.seed(seed + 5);
	double rms = 1e-9; for (double v : x) rms += v * v; rms = std::sqrt(rms / N);
	for (auto& v : x) v += nz.next() * rms * std::pow(10.0, -26.0 / 20.0) * 4;
	const double F[4] = {700, 1150, 2800, 3500}, Bw[4] = {110, 120, 170, 200}, G[4] = {1.0, 0.7, 0.35, 0.25};
	formants(x, sr, F, Bw, G, 4);
	for (int i = 0; i < N; i++) x[i] *= std::min(1.0, (i / sr) / 0.08);
	return x;
}

static std::vector<double> genBrass(double f0, int N, double sr, int players, double bright, uint32_t seed) {
	std::vector<double> x(N, 0.0);
	for (int p = 0; p < players; p++) {
		double det = hashBi(seed, p, 1) * 6.0;
		auto fr = [&](double t) { double scoop = t < 0.03 ? -25.0 * (1 - t / 0.03) : 0.0; return f0 * std::exp2((det + scoop) / 1200.0); };
		auto y = sawVoice(N, sr, hash01(seed, p, 2), fr);
		for (int i = 0; i < N; i++) x[i] += y[i];
	}
	double top = 1500 + 3500 * bright;
	Biquad lp;
	for (int i = 0; i < N; i++) {
		if (i % 16 == 0) {
			double t = i / sr;
			double fc = t < 0.05 ? 500 + (top - 500) * (t / 0.05) : top * (0.7 + 0.3 * std::exp(-(t - 0.05) / 0.08));
			lp.lowpass((float) std::min(fc, 0.45 * sr), 0.8f, (float) sr);
		}
		x[i] = lp.process((float) x[i]) * std::min(1.0, (i / sr) / 0.02);
	}
	Biquad pk; pk.peak(1200.f, 2.f, 6.f, (float) sr);
	applyBiquad(x, pk);
	return x;
}

static void normalize(std::vector<double>& x, double peak = 1.0) {
	double p = 1e-12; for (double v : x) p = std::max(p, std::fabs(v));
	for (auto& v : x) v *= peak / p;
}

static std::vector<double> genOrchHit(double f0, int N, double sr, uint32_t seed) {
	auto a = genStrings(f0, N, sr, 16, 0.8, 1.0, false, seed);
	auto b = genStrings(f0 * 2, N, sr, 16, 0.8, 1.0, false, seed + 1);
	auto c = genStrings(f0 / 2, N, sr, 16, 0.8, 1.0, false, seed + 2);
	auto d = genBrass(f0, N, sr, 4, 0.8, seed + 3);
	auto e = genBrass(f0 * 1.4983, N, sr, 4, 0.8, seed + 4);
	for (auto* v : {&a, &b, &c, &d, &e}) normalize(*v);
	std::vector<double> x(N);
	double ft = f0 / 2;
	for (int i = 0; i < N; i++) {
		double t = i / sr;
		double timp = 0;
		const double R[4] = {1, 1.5, 2, 2.5};
		for (int k = 0; k < 4; k++) timp += std::sin(2 * PI_D * ft * R[k] * t) * std::exp(-t / 0.4 * (1 + k * 0.5)) / (k + 1);
		x[i] = (a[i] + b[i] + c[i]) / 3.0 + 0.8 * (d[i] + e[i]) / 2.0 + 0.6 * timp;
		x[i] *= std::exp(-t * 4.6 / 0.9);
	}
	return x;
}

static std::vector<double> genUpright(double f0, int N, double sr, uint32_t seed) {
	std::vector<double> x(N, 0.0);
	for (int n = 1; n < 200; n++) {
		double fn = n * f0;
		if (fn > 0.45 * sr) break;
		double a = std::pow(n, -1.6) * std::fabs(std::sin(n * PI_D / 6));
		if (a < 1e-5) continue;
		double tau = 1.8 / (1 + 0.15 * n * n);
		double w = 2 * PI_D * fn / sr, d = std::exp(-1.0 / (tau * sr)), e = 1.0;
		for (int i = 0; i < N; i++) { x[i] += a * e * std::sin(w * i); e *= d; }
	}
	Noise nz; nz.seed(seed);
	Biquad lp; lp.lowpass(300.f, 0.7f, (float) sr);
	for (int i = 0; i < N; i++) x[i] += 0.6 * lp.process(nz.next()) * std::exp(-(i / sr) / 0.015) * 4;
	Biquad p1, p2; p1.peak(110.f, 3.f, 5.f, (float) sr); p2.peak(220.f, 3.f, 5.f, (float) sr);
	applyBiquad(x, p1); applyBiquad(x, p2);
	return x;
}

static std::vector<double> genVibes(double f0, int N, double sr, double motor, uint32_t seed) {
	std::vector<double> x(N, 0.0);
	const double R[3] = {1, 4.0, 10.1}, A[3] = {1, .25, .08}, T[3] = {3.5, .7, .15};
	double sc = std::sqrt(261.6 / f0);
	for (int k = 0; k < 3; k++) {
		if (R[k] * f0 > 0.45 * sr) continue;
		for (int i = 0; i < N; i++) { double t = i / sr; x[i] += A[k] * std::sin(2 * PI_D * R[k] * f0 * t) * std::exp(-t / (T[k] * sc)); }
	}
	Noise nz; nz.seed(seed);
	Biquad hp; hp.highpass(2000.f, 0.7f, (float) sr);
	for (int i = 0; i < N; i++) {
		double t = i / sr;
		x[i] += 0.3 * hp.process(nz.next()) * std::exp(-t / 0.003);
		x[i] *= 1.0 - motor * (0.5 + 0.5 * std::sin(2 * PI_D * 5.0 * t));
	}
	return x;
}

static std::vector<double> genChord(double f0, int N, double sr, const SmpSound& s, uint32_t seed) {
	std::vector<double> x(N, 0.0);
	for (int j = 0; j < s.nnotes; j++) {
		double fn = f0 * std::exp2(s.notes[j] / 12.0);
		auto a = sawVoice(N, sr, hash01(seed, j, 1), [&](double) { return fn / 2 * std::exp2(-8.0 / 1200.0); });
		auto b = sawVoice(N, sr, hash01(seed, j, 2), [&](double) { return fn * std::exp2(8.0 / 1200.0); });
		for (int i = 0; i < N; i++) x[i] += a[i] + b[i];
	}
	Ladder lad;
	double cut = s.prm[1], res = s.prm[2];
	for (int i = 0; i < N; i++) {
		double t = i / sr;
		if (i % 8 == 0) lad.setCutoff((float) std::min(cut * std::exp2(2.0 * std::exp(-t / 0.06)), 0.45 * sr), (float) sr);
		x[i] = lad.process((float) (x[i] * 0.15), (float) (4.1 * std::pow(res, 1.2))) * std::exp(-t * 4.6 / 0.6);
	}
	return x;
}

std::unique_ptr<SmpMem> renderSmpSound(int idx) {
	auto t0 = std::chrono::steady_clock::now();
	const SmpSound& s = SMP_SOUNDS[idx];
	std::unique_ptr<SmpMem> m(new SmpMem());
	m->sound = idx;
	m->nz = s.nz;
	for (int zi = 0; zi < s.nz; zi++) {
		SmpZone& z = m->z[zi];
		z.root = s.zones[zi]; z.sr = s.sr;
		int N = (int) std::lround(s.len * s.sr);
		z.len = N;
		double f0 = 440.0 * std::exp2((z.root - 69) / 12.0);
		uint32_t seed = 7000 + idx * 31 + zi;
		std::vector<double> x;
		switch (s.gen) {
			case GEN_STRINGS: x = genStrings(f0, N, s.sr, (int) s.prm[0], s.prm[1], s.prm[2], s.prm[3] > 0.5f, seed); break;
			case GEN_PIANO: x = genPiano(f0, N, s.sr, s.prm[0], (int) s.prm[1], seed); break;
			case GEN_CHOIR: x = genChoir(f0, N, s.sr, (int) s.prm[0], seed); break;
			case GEN_BRASS: x = genBrass(f0, N, s.sr, (int) s.prm[0], s.prm[1], seed); break;
			case GEN_ORCHHIT: x = genOrchHit(f0, N, s.sr, seed); break;
			case GEN_UPRIGHT: x = genUpright(f0, N, s.sr, seed); break;
			case GEN_VIBES: x = genVibes(f0, N, s.sr, s.prm[0], seed); break;
			default: x = genChord(f0, N, s.sr, s, seed); break;
		}
		normalize(x, 120.0 / 128.0);
		z.loop = s.loop;
		if (s.loop) {
			z.loopStart = std::max(1, (int) std::lround(s.loopp[0] * s.sr));
			z.loopEnd = std::min(N, z.loopStart + (int) std::lround(s.loopp[1] * s.sr));
			int xf = std::min((int) std::lround(s.loopp[2] * s.sr), z.loopStart);
			xf = std::min(xf, z.loopEnd - z.loopStart);
			// equal-power crossfade so the end of the loop runs into its start
			std::vector<double> y = x;
			for (int i = 0; i < xf; i++) {
				double a = (i + 0.5) / xf;
				int e = z.loopEnd - xf + i, b = z.loopStart - xf + i;
				y[e] = x[e] * std::cos(a * PI_D / 2) + x[b] * std::sin(a * PI_D / 2);
			}
			x = y;
		}
		z.f16.resize(N); z.q8.resize(N); z.q6.resize(N); z.c8.resize(N + 1); z.c6.resize(N + 1);
		z.c8[0] = z.c6[0] = 0;
		for (int i = 0; i < N; i++) {
			double v = clampf((float) (x[i] * 128.0), -127.f, 127.f);
			z.f16[i] = (float) (std::round(v / 128.0 * 32767.0) / 32767.0);
			z.q8[i] = (int8_t) std::lround(v);
		}
		for (int i = 0; i < N; i++) {
			int q = std::max(-124, std::min(124, (int) std::lround(z.q8[i & ~1] / 4.0) * 4));
			z.q6[i] = (int8_t) q;
			z.c8[i + 1] = z.c8[i] + z.q8[i];
			z.c6[i + 1] = z.c6[i] + q;
		}
		m->totalSamples += N;
	}
	m->renderSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	return m;
}

// ---------------------------------------------------------------- engine

SamplerEngine::SamplerEngine() {}

SamplerEngine::~SamplerEngine() {
	quit = true;
	if (worker.joinable()) worker.join();
}

void SamplerEngine::init(float sampleRate) {
	fs = sampleRate; fs2 = 2.f * fs;
	s = &SMP_SOUNDS[0];
	if (!synchronous && !worker.joinable()) worker = std::thread([this] { workerLoop(); });
}

SmpMem* SamplerEngine::obtain(int idx) {
	std::lock_guard<std::mutex> lock(cacheMutex);
	for (int i = 0; i < 4; i++)
		if (cache[i] && cache[i]->sound == idx) { cacheAge[i] = ++ageCounter; return cache[i].get(); }
	std::unique_ptr<SmpMem> mem = renderSmpSound(idx);
	// evict the oldest entry that is not playing
	int slot = -1, oldest = 1 << 30;
	SmpMem* act = active.load();
	for (int i = 0; i < 4; i++) {
		if (!cache[i]) { slot = i; break; }
		if (cache[i].get() == act) continue;
		if (cacheAge[i] < oldest) { oldest = cacheAge[i]; slot = i; }
	}
	cache[slot] = std::move(mem);
	cacheAge[slot] = ++ageCounter;
	return cache[slot].get();
}

void SamplerEngine::workerLoop() {
	while (!quit) {
		int r = requested.exchange(-1);
		if (r >= 0) {
			SmpMem* m = obtain(r);
			if (requested.load() < 0 || requested.load() == r) active.store(m);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(4));
	}
}

void SamplerEngine::setSound(int idx) {
	soundIdx = std::max(0, std::min(11, idx));
	s = &SMP_SOUNDS[soundIdx];
	if (synchronous) active.store(obtain(soundIdx));
	else requested.store(soundIdx);
}

bool SamplerEngine::ready() const {
	SmpMem* m = active.load();
	return m && m->sound == soundIdx;
}

void SamplerEngine::allOff() {
	for (auto& vc : v) { vc.gate = false; vc.aenv.stage = 0; vc.aenv.v = 0.f; vc.fenv.stage = 0; vc.lp.reset(); }
	decL.reset(); decR.reset();
}

void SamplerEngine::noteOn(int slot, float note, float vel, float pan) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	const SmpMem* m = active.load();
	if (!m || m->sound != soundIdx) return; // still loading: notes are dropped
	Voice& vc = v[slot];
	vc.mem = m;
	vc.note = note; vc.vel = vel; vc.pan = pan; vc.gate = true;
	// zone by midpoints between roots
	int z = 0;
	for (int i = 1; i < m->nz; i++) if (note >= 0.5f * (m->z[i - 1].root + m->z[i].root)) z = i;
	vc.zone = z;
	vc.pos[0] = vc.pos[1] = 0.0;
	vc.done[0] = vc.done[1] = false;
	vc.incNote = -1000.f;
	vc.v8[0] = vc.v8[1] = vc.v6[0] = vc.v6[1] = false;
	float as = curM.attackScale(), ds = curM.decayScale();
	vc.fenv.set(s->fenv, fs2, as, ds); vc.aenv.set(s->aenv, fs2, as, ds);
	vc.fenv.on(); vc.aenv.on();
	vc.lp.reset();
	vc.filtCounter = 0;
}

void SamplerEngine::setPitch(int slot, float note) { if (slot >= 0 && slot < MAX_SLOTS) v[slot].note = note; }

void SamplerEngine::noteOff(int slot) {
	if (slot < 0 || slot >= MAX_SLOTS) return;
	v[slot].gate = false;
	v[slot].fenv.off(); v[slot].aenv.off();
}

// integral of a zone's staircase from 0 to x, following the loop
static inline double zoneInteg(const SmpZone& z, const std::vector<int8_t>& q, const std::vector<int32_t>& c, double x) {
	if (!z.loop || x <= z.loopEnd) {
		if (x >= z.len) return (double) c[z.len];
		int i = (int) x; return c[i] + (x - i) * q[i];
	}
	double L = z.loopEnd - z.loopStart;
	double over = x - z.loopEnd;
	double k = std::floor(over / L);
	double r = z.loopStart + (over - k * L);
	int i = (int) r;
	double loopSum = (double) (c[z.loopEnd] - c[z.loopStart]);
	return c[z.loopEnd] + k * loopSum + (c[i] - c[z.loopStart]) + (r - i) * q[i];
}

// one output sample: box-integrated staircase (8 or 6 bit) or linear 16-bit, blended by GRIT.
// i8/i6 carry the integral at pos from the previous sample (valid flags), so each read costs one integral.
static inline float zoneRead(const SmpZone& z, double pos, double inc, float grit, double& i8, bool& v8, double& i6, bool& v6) {
	auto lin = [&]() {
		int i = (int) pos; double f = pos - i;
		if (i >= z.len - 1) return z.f16[z.len - 1];
		return (float) (z.f16[i] + f * (z.f16[i + 1] - z.f16[i]));
	};
	auto stair = [&](const std::vector<int8_t>& q, const std::vector<int32_t>& c, double& cache, bool& valid) {
		double a = valid ? cache : zoneInteg(z, q, c, pos);
		double b = zoneInteg(z, q, c, pos + inc);
		cache = b; valid = true;
		return (float) ((b - a) / inc * (1.0 / 128.0));
	};
	if (grit <= 0.f) { v8 = v6 = false; return lin(); }
	if (grit < 0.5f) { v6 = false; float a = lin(), b = stair(z.q8, z.c8, i8, v8); return a + (b - a) * grit * 2.f; }
	if (grit == 0.5f) { v6 = false; return stair(z.q8, z.c8, i8, v8); }
	float a = stair(z.q8, z.c8, i8, v8), b = stair(z.q6, z.c6, i6, v6);
	return a + (b - a) * (grit - 0.5f) * 2.f;
}

float SamplerEngine::renderVoice(Voice& vc) {
	const SmpSound& p = *s;
	const SmpMem* m = vc.mem;
	if (!m) return 0.f;
	const SmpZone& z = m->z[vc.zone];
	float out = 0.f;
	if (vc.note != vc.incNote) {
		vc.incNote = vc.note;
		for (int k = 0; k < 2; k++) {
			float cents = k == 1 ? p.det : 0.f;
			vc.inc[k] = z.sr * std::exp2((vc.note - z.root + cents / 100.0) / 12.0) / fs2;
			vc.v8[k] = vc.v6[k] = false; // the increment changed: the cached integral no longer lines up
		}
	}
	for (int k = 0; k < 2; k++) {
		if (k == 1 && p.mix <= 0.f) break;
		if (vc.done[k]) continue;
		double inc = vc.inc[k];
		float y = zoneRead(z, vc.pos[k], inc, curM.grit, vc.i8[k], vc.v8[k], vc.i6[k], vc.v6[k]);
		vc.pos[k] += inc;
		if (z.loop) {
			if (vc.pos[k] >= z.loopEnd) {
				vc.pos[k] -= std::floor((vc.pos[k] - z.loopStart) / (z.loopEnd - z.loopStart)) * (z.loopEnd - z.loopStart);
				vc.v8[k] = vc.v6[k] = false;
			}
		}
		else if (vc.pos[k] >= z.len - 1) vc.done[k] = true;
		out += (k == 0 ? 1.f : p.mix) * y;
	}
	float fe = vc.fenv.next(), ae = vc.aenv.next();
	float velN = vc.vel / 127.f;
	if (vc.filtCounter-- <= 0) {
		vc.filtCounter = 3;
		float oct = p.env * fe + p.kbd * (vc.note - 60.f) / 12.f + p.velcut * (velN - 0.5f) + curM.tone * 3.f;
		vc.lp.setCutoff(clampf(p.cut * std::exp2(oct), 16.f, std::min(20000.f, 0.45f * fs2)), fs2);
	}
	float y = vc.lp.process(out * 0.7f, 3.8f * p.res);
	if (vc.done[0] && (p.mix <= 0.f || vc.done[1])) vc.aenv.stage = 0;
	return y * ae * (1.f - p.vel * (1.f - velN));
}

void SamplerEngine::process(float* L, float* R, int n, const Macros& m) {
	curM = m;
	float gl[MAX_SLOTS], gr[MAX_SLOTS];
	for (int i = 0; i < MAX_SLOTS; i++) panGains(v[i].pan, gl[i], gr[i]);
	for (int i = 0; i < n; i++) {
		float os[2][2];
		for (int k = 0; k < 2; k++) {
			float sl = 0.f, sr = 0.f;
			for (int vi = 0; vi < MAX_SLOTS; vi++) {
				Voice& vc = v[vi];
				if (vc.aenv.stage == 0) continue;
				float y = renderVoice(vc);
				sl += y * gl[vi]; sr += y * gr[vi];
			}
			os[k][0] = sl; os[k][1] = sr;
		}
		L[i] += decL.process(os[0][0], os[1][0]);
		R[i] += decR.process(os[0][1], os[1][1]);
	}
}

} // namespace hk
