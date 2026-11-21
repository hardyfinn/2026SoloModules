// Cymbolism Fable offline renders and checks (no Rack).  make test
#include "../src/CyEngine.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
using namespace cy;

static void writeWav(const std::string& path, const std::vector<float>& x, int sr) {
	FILE* fp = fopen(path.c_str(), "wb");
	if (!fp) return;
	int n = (int)x.size();
	int bytes = n * 3;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, fp); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); w32(36 + bytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); w32(16); w16(1); w16(1); w32(sr); w32(sr * 3); w16(3); w16(24);
	fwrite("data", 1, 4, fp); w32(bytes);
	for (float v : x) {
		double s = std::max(-1.0, std::min(1.0, (double)v / 10.0));
		int32_t iv = (int32_t)std::lround(s * 8388607.0);
		unsigned char b[3] = {(unsigned char)(iv & 255), (unsigned char)((iv >> 8) & 255), (unsigned char)((iv >> 16) & 255)};
		fwrite(b, 1, 3, fp);
	}
	fclose(fp);
}

struct Hit { double t, v; };

static std::vector<float> renderHits(Engine& E, const std::vector<Hit>& hits, double dur, double chokeAt = -1) {
	int sr = (int)E.fsHost;
	int n = (int)(dur * sr);
	std::vector<float> out(n);
	size_t h = 0;
	const int B = 32;
	for (int s = 0; s < n; s += B) {
		double t = (double)s / sr;
		while (h < hits.size() && hits[h].t <= t) { E.strike(hits[h].v); h++; }
		E.ctl.choke = chokeAt >= 0 && t >= chokeAt;
		int m = std::min(B, n - s);
		E.process(nullptr, out.data() + s, m);
	}
	return out;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	std::string outDir = dir + "/test/out";
	std::string mk = "mkdir -p " + outDir;
	(void)system(mk.c_str());
	const int SR = 48000;
	bool quick = argc > 2 && !strcmp(argv[2], "quick");

	// ---- first audio: soft / medium / hard on CRASH, RIDE, CHINA, SPLASH ----
	struct T { int type; int play; double pos; };
	T set[] = {{1, SHOULDER, 0.96}, {4, TIP, 0.55}, {7, SHOULDER, 0.96}, {0, SHOULDER, 0.94}};
	for (auto& tt : set) {
		GeomParams gp;
		gp.fromType(tt.type);
		gp.quality = 1;
		gp.sampleRate = SR;
		auto M = buildModel(gp);
		Engine E;
		E.ctl.play = tt.play;
		E.ctl.position = tt.pos;
		E.ctl.mic = 2.6;
		E.ctl.hardness = 0.5;
		E.init(M, SR);
		E.outGain = 1.0;
		E.outGain = 0.004;
		auto t0 = std::chrono::steady_clock::now();
		double dur = quick ? 3.0 : 10.0;
		std::vector<Hit> hits = {{0.05, 0.7}, {dur / 3 + 0.05, 3.0}, {2 * dur / 3 + 0.05, 9.0}};
		auto out = renderHits(E, hits, dur);
		auto t1 = std::chrono::steady_clock::now();
		double secs = std::chrono::duration<double>(t1 - t0).count();
		float pk = 0;
		for (float v : out) pk = std::max(pk, std::fabs(v));
		printf("%-11s modes %d (active %d) core %d build %.0f ms  render %.1fx real time  peak %.2f V  guard %d\n", CY_TYPES[tt.type].name, E.N, E.NA,
		       E.NC, M->buildMs, dur / secs, pk, E.guardEvents);
		std::string name = std::string(CY_TYPES[tt.type].name);
		for (auto& c : name) c = c == ' ' ? '-' : (char)tolower(c);
		writeWav(outDir + "/first-" + name + ".wav", out, SR);
	}
	return 0;
}
