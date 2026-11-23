// Cymbolism Extended: a ~54 s stereo demo piece in D minor, every sound from the module's engine.
//   ./test/extended_gallery <out.wav>
// Voices are rendered on their own threads, levelled, panned and mixed; 16-bit with TPDF dither.
#include "../src/CyEngine.hpp"
#include <cstdio>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <thread>
using namespace cy;

static const int SR = 48000;
static const double LEN = 54.0;

static std::map<int, std::shared_ptr<Model>> models;
static std::shared_ptr<Model> model(int type) {
	if (!models.count(type)) {
		GeomParams gp;
		gp.fromType(type);
		gp.sampleRate = SR;
		models[type] = buildModel(gp);
	}
	return models[type];
}
static Controls defaults(int type) {
	Controls c;
	c.play = CY_TYPES[type].play;
	c.position = CY_TYPES[type].position;
	c.mic = 1.0;
	c.u.width = 0.45;
	return c;
}
typedef std::function<void(Engine&, double)> Script;
struct Voice {
	const char* name;
	int type, typeB;
	Controls c;
	double start, dur, levelDb, pan, fadeOut;
	Script sc;
	std::vector<float> L, R;
	int guard = 0;
};

static void render(Voice& v) {
	Engine E;
	E.ctl = v.c;
	E.init(model(v.type), SR);
	if (v.typeB >= 0) E.setMorphModel(*model(v.typeB));
	int n = (int)(v.dur * SR);
	v.L.assign(n, 0.f);
	v.R.assign(n, 0.f);
	for (int s = 0; s < n; s += 32) {
		v.sc(E, (double)s / SR);
		Engine::Extra ex;
		ex.R = v.R.data() + s;
		E.process(nullptr, v.L.data() + s, std::min(32, n - s), &ex);
	}
	v.guard = E.guardEvents;
}

static void writeWav16(const std::string& path, const std::vector<double>& L, const std::vector<double>& R) {
	FILE* fp = fopen(path.c_str(), "wb");
	if (!fp) { perror(path.c_str()); return; }
	int n = (int)L.size();
	uint32_t bytes = n * 4;
	auto w32 = [&](uint32_t x) { fwrite(&x, 4, 1, fp); };
	auto w16 = [&](uint16_t x) { fwrite(&x, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); w32(36 + bytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); w32(16); w16(1); w16(2); w32(SR); w32(SR * 4); w16(4); w16(16);
	fwrite("data", 1, 4, fp); w32(bytes);
	std::mt19937 rng(7);
	std::uniform_real_distribution<double> u(-0.5, 0.5);
	for (int i = 0; i < n; i++)
		for (double x : {L[i], R[i]}) {
			double s = std::max(-32768.0, std::min(32767.0, std::round(x * 32767.0 + u(rng) + u(rng))));
			int16_t q = (int16_t)s;
			fwrite(&q, 2, 1, fp);
		}
	fclose(fp);
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "ext-gallery.wav";
	enum { CRASH = 1, RIDE = 4, CHINA = 7, BELLT = 9, GONG = 10, SHEET = 11, BOWL = 12, PLATE = 13, RING = 14, DOME = 15 };
	std::vector<Voice> V;
	auto add = [&](const char* name, int type, Controls c, double start, double dur, double db, double pan, Script sc, int typeB = -1, double fade = 1.5) {
		Voice v{name, type, typeB, c, start, dur, db, pan, fade, sc, {}, {}, 0};
		V.push_back(v);
	};
	const double beat = 0.5;   // 120 bpm

	// 1. a reverse swell out of silence, tuned to D minor 9, cut by a hard tuned crash on the downbeat
	{
		Controls c = defaults(CRASH);
		c.u.tune = 0.85; c.u.scale = SC_CHORD; c.u.root = 2; c.bloom = 1.3;
		c.u.swell = 4.5; c.u.colour = 2500;
		auto hit = std::make_shared<bool>(false);
		add("reverse swell", CRASH, c, 0.0, 14.0, -2, 0.0, [=](Engine& E, double t) {
			E.ctl.u.sustain = t < 4.0 ? 0.85 : 0.0;
			E.ctl.choke = t > 4.0 && t < 4.12;
			if (t >= 4.12 && !*hit) { *hit = true; E.strike(8.0); }
		});
	}
	// 2. acid bell: BELL type on the harmonic series, V/OCT from a 16-step line, from the downbeat
	{
		Controls c = defaults(BELLT);
		c.u.tune = 1; c.u.scale = SC_HARMONIC; c.u.root = 2; c.decay = 0.35;
		const double seq[16] = {0, 0, 12, 0, 3, 0, 15, 7, 0, 0, 12, 10, 3, 5, 15, 7};
		auto last = std::make_shared<int>(-1);
		add("acid bell", BELLT, c, 4.0, 18.0, -9, -0.35, [=](Engine& E, double t) {
			int k = (int)(t / (beat / 4));
			if (k != *last && t < 16.0) {
				*last = k;
				E.ctl.u.voct = seq[k % 16] / 12.0 - 1.0;
				E.ctl.decay = 0.3 + 0.25 * (0.5 - 0.5 * std::cos(2 * CY_PI * t / 16.0));
				E.strike(k % 4 == 0 ? 2.2 : (k % 2 ? 1.1 : 1.6));
			}
		});
	}
	// 3. tuned ride on eighths, the chord on the CHORD input changing each bar: Dm9, Bbmaj7, Gm9, A7
	{
		Controls c = defaults(RIDE);
		c.u.tune = 1; c.u.scale = SC_CHORD;
		const float ch[4][5] = {{2 / 12.f, 5 / 12.f, 9 / 12.f, 0, 4 / 12.f}, {10 / 12.f, 2 / 12.f, 5 / 12.f, 9 / 12.f, 0},
		                        {7 / 12.f, 10 / 12.f, 2 / 12.f, 5 / 12.f, 9 / 12.f}, {9 / 12.f, 1 / 12.f, 4 / 12.f, 7 / 12.f, 9 / 12.f}};
		auto last = std::make_shared<int>(-1);
		add("tuned ride", RIDE, c, 4.0, 20.0, -11, 0.4, [=](Engine& E, double t) {
			int b = (int)(t / (4 * beat)) % 4;
			E.ctl.u.nChord = 5;
			for (int j = 0; j < 5; j++) E.ctl.u.chord[j] = ch[b][j];
			int k = (int)(t / (beat / 2));
			if (k != *last && t < 16.0) { *last = k; E.strike(k % 2 == 0 ? 2.4 : 1.3); }
		});
	}
	// 4. melting cymbal: a mallet roll on a crash, MORPH to CHINA and back
	{
		Controls c = defaults(CRASH);
		c.play = MALLET; c.hardness = 0.45; c.rate = 11; c.force = 1.6; c.position = 0.85;
		add("melting roll", CRASH, c, 12.0, 17.0, -6, -0.15, [](Engine& E, double t) {
			E.ctl.roll = t > 0.0 && t < 13.0;
			E.ctl.force = 0.6 + 2.2 * std::sin(CY_PI * std::min(1.0, t / 13.0));
			E.ctl.u.morph = t < 6 ? t / 6 : std::max(0.0, 1 - (t - 7) / 6);
		}, CHINA);
	}
	// 5. endless china: SUSTAIN on, COLOUR on a slow LFO, BLOOM 1.5: a bed under the second half
	{
		Controls c = defaults(CHINA);
		c.bloom = 1.5; c.u.swell = 6;
		add("endless china", CHINA, c, 20.0, 30.0, -13, 0.0, [](Engine& E, double t) {
			E.ctl.u.sustain = t < 24 ? 0.55 : 0.0;
			E.ctl.u.colour = 1500 * std::pow(6.0, std::sin(2 * CY_PI * t / 12.0));
			if (t == 0) E.strike(1.5);
		}, -1, 3.0);
	}
	// 6. mirror metal: WARP -1 on a ride, PIVOT swept, a hit every bar and a half
	{
		Controls c = defaults(RIDE);
		c.u.warp = -1;
		auto last = std::make_shared<int>(-1);
		add("mirror metal", RIDE, c, 22.0, 14.0, -7, 0.55, [=](Engine& E, double t) {
			E.ctl.u.pivot = 350 * std::pow(7.0, 0.5 - 0.5 * std::cos(2 * CY_PI * t / 10.0));
			int k = (int)(t / (3 * beat));
			if (k != *last && t < 11.0) { *last = k; E.strike(k % 2 ? 1.1 : 1.8); }
		});
	}
	// 7. rain on a thunder sheet, density swelling and thinning
	{
		Controls c = defaults(SHEET);
		c.play = RAIN; c.hardness = 0.55; c.force = 1.2;
		add("rain on sheet", SHEET, c, 27.0, 13.0, -8, -0.5, [](Engine& E, double t) {
			E.ctl.roll = t < 10.0;
			E.ctl.rate = 2 + 22 * std::sin(CY_PI * std::min(1.0, t / 10.0));
		});
	}
	// 8. bowed singing bowl, long bow strokes
	{
		Controls c = defaults(BOWL);
		c.play = BOW; c.rate = 6; c.force = 2.5;
		add("bowed bowl", BOWL, c, 33.0, 14.0, -6, 0.3, [](Engine& E, double t) {
			E.ctl.roll = (t > 0.0 && t < 4.5) || (t > 5.5 && t < 10.0);
		});
	}
	// 9. dub siren plate: SUSTAIN on, V/OCT from a slow triangle
	{
		Controls c = defaults(PLATE);
		c.u.swell = 3; c.u.colour = 900;
		add("siren plate", PLATE, c, 38.0, 12.0, -12, -0.3, [](Engine& E, double t) {
			E.ctl.u.sustain = t < 8.5 ? 0.3 : 0.0;
			double ph = std::fmod(t / 2.5, 1.0);
			E.ctl.u.voct = -0.5 + 1.0 * (ph < 0.5 ? 2 * ph : 2 - 2 * ph);
			if (t == 0) E.strike(1.2);
		});
	}
	// 10. a ring gong with a bouncing stick, ricochets
	{
		Controls c = defaults(RING);
		c.play = TIP; c.u.bounce = 0.6;
		auto last = std::make_shared<int>(-1);
		add("bouncing ring", RING, c, 30.0, 10.0, -12, 0.7, [=](Engine& E, double t) {
			int k = (int)(t / (2 * beat));
			if (k != *last && t < 6.0 && k % 2 == 0) { *last = k; E.strike(2.5); }
		});
	}
	// 11. the end: a slow-motion GONG, FLOW at minimum, hard mallet, frozen for a moment then let go
	{
		Controls c = defaults(GONG);
		c.play = MALLET; c.u.flow = 0.06; c.bloom = 1.6; c.u.tilt = -0.3;
		add("slow gong", GONG, c, 44.0, 10.0, -3, 0.0, [](Engine& E, double t) {
			if (t == 0) E.strike(7.0);
			E.ctl.u.freeze = t > 4.0 && t < 6.5;
		}, -1, 3.5);
	}

	if (getenv("SOLO")) {
		std::string so = getenv("SOLO");
		std::vector<Voice> keep;
		for (auto& v : V) if (so == v.name) keep.push_back(v);
		V = keep;
	}
	printf("building models...\n");
	for (auto& v : V) { model(v.type); if (v.typeB >= 0) model(v.typeB); }
	printf("rendering %zu voices...\n", V.size());
	std::vector<std::thread> th;
	for (auto& v : V) th.emplace_back([&v] { render(v); });
	for (auto& t : th) t.join();

	int N = (int)(LEN * SR);
	std::vector<double> L(N, 0.0), R(N, 0.0);
	for (auto& v : V) {
		// level each voice by its loudest second of RMS, then apply the mix level
		int n = (int)v.L.size(), win = SR;
		double best = 1e-9, acc = 0;
		for (int i = 0; i < n; i++) {
			acc += 0.5 * ((double)v.L[i] * v.L[i] + (double)v.R[i] * v.R[i]);
			if (i >= win) acc -= 0.5 * ((double)v.L[i - win] * v.L[i - win] + (double)v.R[i - win] * v.R[i - win]);
			if (i >= win - 1 || i == n - 1) best = std::max(best, acc / std::min(i + 1, win));
		}
		double g = std::pow(10.0, v.levelDb / 20.0) * 0.25 / std::sqrt(best);
		double a = (v.pan + 1) * CY_PI / 4, gl = std::cos(a) * std::sqrt(2.0), gr = std::sin(a) * std::sqrt(2.0);
		int o = (int)(v.start * SR), fade = (int)(v.fadeOut * SR);
		double pk = 0; int hot = 0;
		for (int i = 0; i < n && o + i < N; i++) {
			if (std::fabs(v.L[i]) > 8 || std::fabs(v.R[i]) > 8) hot++;
			double f = std::min(1.0, (double)(n - i) / fade);
			f = std::min(f, (double)(N - o - i) / (2.0 * SR));
			f = std::max(0.0, f);
			L[o + i] += g * f * v.L[i] * gl;
			R[o + i] += g * f * v.R[i] * gr;
			pk = std::max(pk, (double)std::max(std::fabs(v.L[i]), std::fabs(v.R[i])));
		}
		if (getenv("SOLO")) {
			for (int sec = 0; sec * SR < n; sec++) {
				double e = 0; int m = 0;
				for (int i = sec * SR; i < std::min(n, (sec + 1) * SR); i++, m++) e += (double)v.L[i] * v.L[i];
				printf("%.0f ", 10 * std::log10(e / m + 1e-30));
			}
			printf("\n");
		}
		printf("  %-15s %5.1f s at %4.1f s  peak %.2f V  >8V %d smp  gain %.3f  guard %d\n", v.name, v.dur, v.start, pk, hot, g, v.guard);
	}
	// gentle soft clip then normalise to -1 dBFS
	double pk = 0;
	for (int i = 0; i < N; i++) pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])));
	double pre = 1.4 / pk;
	pk = 0;
	for (int i = 0; i < N; i++) {
		L[i] = std::tanh(L[i] * pre);
		R[i] = std::tanh(R[i] * pre);
		pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])));
	}
	double post = std::pow(10.0, -1.0 / 20.0) / pk;
	for (int i = 0; i < N; i++) { L[i] *= post; R[i] *= post; }
	writeWav16(out, L, R);
	printf("wrote %s (%.1f s)\n", out.c_str(), LEN);
	return 0;
}
