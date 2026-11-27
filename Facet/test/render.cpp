// Offline checks for the Facet engine + demo WAVs. No Rack needed.
//   make test   (prints the checks, writes test/out/*.wav)
#include "../src/FacetEngine.hpp"
#include "../src/FacetPresets.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>

using namespace facet;
static float SR = 48000.f;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); fails++; } else std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, float gain) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	int n = L.size(), sr = (int) SR, bytes = n * 4;
	auto w32 = [&](int v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](short v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) {
		float l = std::max(-1.f, std::min(1.f, L[i] * gain)), r = std::max(-1.f, std::min(1.f, R[i] * gain));
		w16((short) (l * 32767)); w16((short) (r * 32767));
	}
	std::fclose(f);
}

struct Ev { float t, dur; std::vector<float> notes; float vel; };

// plays events, returns peak; appends audio
static float play(Engine& e, const std::vector<Ev>& evs, float secs, std::vector<float>& L, std::vector<float>& R, bool& finite) {
	In in;
	in.velConnected = true;
	int n = (int) (secs * SR);
	float pk = 0.f;
	finite = true;
	for (int i = 0; i < n; i++) {
		float t = i / SR;
		int ch = 0;
		for (auto& ev : evs)
			for (float nt : ev.notes) {
				if (ch >= MAXV) break;
				bool on = t >= ev.t && t < ev.t + ev.dur;
				in.voct[ch] = (nt - 60.f) / 12.f;
				in.gate[ch] = on ? 10.f : 0.f;
				in.vel[ch] = ev.vel * 10.f;
				ch++;
			}
		in.channels = ch;
		e.process(in);
		if (!std::isfinite(e.outL) || !std::isfinite(e.outR)) finite = false;
		pk = std::max(pk, std::max(std::fabs(e.outL), std::fabs(e.outR)));
		L.push_back(e.outL);
		R.push_back(e.outR);
	}
	return pk;
}

static std::vector<Ev> phraseFor(const std::string& cat) {
	if (cat == "Basses") return {{0.0f, 0.2f, {36}, 0.9f}, {0.25f, 0.2f, {36}, 0.6f}, {0.5f, 0.2f, {48}, 0.9f}, {0.75f, 0.2f, {39}, 0.7f},
	                             {1.0f, 0.2f, {36}, 0.9f}, {1.25f, 0.2f, {43}, 0.6f}, {1.5f, 0.4f, {41}, 0.9f}};
	if (cat == "Leads") return {{0.0f, 0.45f, {72}, 0.9f}, {0.5f, 0.45f, {75}, 0.8f}, {1.0f, 0.45f, {77}, 0.9f}, {1.5f, 1.2f, {79}, 0.8f}};
	if (cat == "Stabs") return {{0.0f, 0.15f, {60, 63, 67, 70}, 0.9f}, {0.375f, 0.15f, {60, 63, 67, 70}, 0.7f}, {0.75f, 0.15f, {58, 62, 65, 69}, 0.9f},
	                            {1.5f, 0.15f, {56, 60, 63, 67}, 0.9f}};
	if (cat == "Bells" || cat == "Keys" || cat == "FX")
		return {{0.0f, 0.4f, {60}, 0.9f}, {0.4f, 0.4f, {67}, 0.7f}, {0.8f, 0.4f, {72}, 0.8f}, {1.2f, 1.2f, {64, 71, 76}, 0.8f}};
	return {{0.0f, 2.6f, {48, 55, 60, 63, 67}, 0.8f}};
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : "test/out";
	auto t0 = std::chrono::steady_clock::now();
	const Rom& rom = Rom::get();
	double romMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	CHECK(romMs < 2000, "wave ROM builds in %.0f ms (%d waves)", romMs, rom.size());
	const auto& pres = factoryPresets();
	CHECK(pres.size() == 96, "%d factory presets", (int) pres.size());

	// 1. every preset sounds, stays finite and sensible in level
	int quiet = 0, loud = 0;
	std::vector<float> suggest;
	for (size_t p = 0; p < pres.size(); p++) {
		Engine e;
		e.setSampleRate(SR);
		std::copy(pres[p].v.begin(), pres[p].v.end(), e.v);
		std::vector<float> L, R;
		bool fin;
		float pk = play(e, phraseFor(pres[p].cat), 4.f, L, R, fin);
		bool ok = fin && pk > 0.05f && pk < 3.f;
		if (pk <= 0.05f) quiet++;
		if (pk >= 3.f) loud++;
		if (!ok) std::printf("FAIL: preset %d %s peak %.3f finite %d\n", (int) p + 1, pres[p].name.c_str(), pk, fin), fails++;
		else std::printf("      %2d %-18s %-12s peak %.2f level %.0f\n", (int) p + 1, pres[p].name.c_str(), pres[p].cat.c_str(), pk, pres[p].v[cp(C_LEVEL)]);
		suggest.push_back(std::max(30.f, std::min(127.f, std::round(pres[p].v[cp(C_LEVEL)] * std::sqrt((std::string("Basses Leads").find(pres[p].cat) != std::string::npos ? 0.9f : 1.1f) / pk)))));
	}
	std::printf("SUGGEST {");
	for (float x : suggest) std::printf("%.0f,", x);
	std::printf("}\n");
	CHECK(quiet == 0 && loud == 0, "all presets between -26 dBFS and +10 dB peak (quiet %d, loud %d)", quiet, loud);

	// 2. RAND keeps patches playable: 300 rolls of everything at full amount
	Rng r(99);
	int bad = 0;
	for (int k = 0; k < 300; k++) {
		Engine e;
		e.setSampleRate(SR);
		std::copy(pres[k % 96].v.begin(), pres[k % 96].v.end(), e.v);
		randomizePatch(e.v, k % 3 == 0 ? 1.f : 0.3f, SC_ALL, k % 10 == 0, r);
		std::vector<float> L, R;
		bool fin;
		float pk = play(e, {{0.f, 1.0f, {48, 60, 64, 67}, 0.8f}}, 1.5f, L, R, fin);
		if (!fin || pk > 4.f) bad++;
		// protected values unchanged
		if (e.v[cp(C_LEVEL)] != pres[k % 96].v[cp(C_LEVEL)] || e.v[tp(0, T_PKF)] != pres[k % 96].v[tp(0, T_PKF)]) bad++;
		for (int t = 0; t < 4 && k % 10; t++) {
			float c = e.v[tp(t, T_COARSE)];
			float orig = pres[k % 96].v[tp(t, T_COARSE)];
			bool musical = c == orig || c == 0 || c == 12 || c == -12 || c == 7 || c == 19 || c == 24;
			if (!musical) bad++;
		}
	}
	CHECK(bad == 0, "300 random rolls: finite, bounded, level/key-follow untouched, coarse musical (%d problems)", bad);

	// 3. MUTATE walk stays bounded
	{
		float base[NPV], off[NPV] = {};
		std::copy(pres[0].v.begin(), pres[0].v.end(), base);
		for (int k = 0; k < 5000; k++) mutateStep(base, off, 1.f, SC_ALL, r);
		float worst = 0.f;
		for (int i = 0; i < NPV; i++) {
			Def d = defOf(i);
			if (d.choices >= 0 || (isTone(i) && i % NT == T_WAVE)) {
				if (std::fabs(off[i]) > 2.f) worst = 1.f;
				continue;
			}
			worst = std::max(worst, std::fabs(off[i]) / (d.max - d.min + 1e-6f));
		}
		CHECK(worst <= 0.26f, "mutation offsets stay within 25%% of range after 5000 steps (worst %.2f)", worst);
	}

	// 4. CPU: 16 voices x 4 tones of the heaviest pad
	{
		Engine e;
		e.setSampleRate(SR);
		std::copy(pres[0].v.begin(), pres[0].v.end(), e.v);
		for (int t = 0; t < 4; t++) e.v[tp(t, T_ON)] = 1;
		std::vector<float> L, R;
		bool fin;
		std::vector<float> notes;
		for (int i = 0; i < 16; i++) notes.push_back(36 + i * 3);
		auto a = std::chrono::steady_clock::now();
		play(e, {{0.f, 5.f, notes, 0.8f}}, 5.f, L, R, fin);
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
		CHECK(s / 5.0 < 0.5, "64 tones at once use %.1f%% of one core in real time", 100.0 * s / 5.0);
	}

	// 5. demos
	const int demo[] = {0, 1, 5, 8, 9, 16, 24, 31, 32, 38, 40, 43, 49, 56, 58, 64, 65, 69, 72, 73, 80, 85, 86, 88};
	for (int d : demo) {
		Engine e;
		e.setSampleRate(SR);
		std::copy(pres[d].v.begin(), pres[d].v.end(), e.v);
		std::vector<float> L, R;
		bool fin;
		play(e, phraseFor(pres[d].cat), 6.f, L, R, fin);
		char name[128];
		std::snprintf(name, sizeof(name), "%s/%02d-%s.wav", dir.c_str(), d + 1, pres[d].name.c_str());
		for (char* c = name + dir.size() + 1; *c; c++)
			if (*c == ' ') *c = '-';
		writeWav(name, L, R, 0.2f);
	}
	// 6. "preset, then a small RAND" demo: the way Hardy expects to use it
	{
		std::vector<float> L, R;
		Rng rr(7);
		for (int k = 0; k < 4; k++) {
			Engine e;
			e.setSampleRate(SR);
			std::copy(pres[0].v.begin(), pres[0].v.end(), e.v);
			if (k) randomizePatch(e.v, 0.2f, SC_ALL, false, rr);
			bool fin;
			play(e, {{0.f, 2.5f, {48, 55, 60, 63, 67}, 0.8f}}, 3.5f, L, R, fin);
		}
		writeWav(dir + "/rand-0.2-on-glass-cathedral.wav", L, R, 0.2f);
	}
	std::printf("%s (%d failed)\n", fails ? "FAILED" : "all checks passed", fails);
	return fails ? 1 : 0;
}
