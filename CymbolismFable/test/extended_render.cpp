// Cymbolism Extended: demo renders through the module's engine code (no Rack).
//   ./test/extended_render <dir> [name]
#include "../src/CyEngine.hpp"
#include <cstdio>
#include <functional>
#include <map>
#include <string>
using namespace cy;

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* fp = fopen(path.c_str(), "wb");
	if (!fp) return;
	int n = (int)L.size(), bytes = n * 6;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, fp); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); w32(36 + bytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); w32(16); w16(1); w16(2); w32(sr); w32(sr * 6); w16(6); w16(24);
	fwrite("data", 1, 4, fp); w32(bytes);
	for (int i = 0; i < n; i++)
		for (float v : {L[i], R[i]}) {
			double s = std::max(-1.0, std::min(1.0, (double)v / 10.0));
			int32_t iv = (int32_t)std::lround(s * 8388607.0);
			unsigned char b[3] = {(unsigned char)(iv & 255), (unsigned char)((iv >> 8) & 255), (unsigned char)((iv >> 16) & 255)};
			fwrite(b, 1, 3, fp);
		}
	fclose(fp);
}
static std::map<std::string, std::shared_ptr<Model>> cacheM;
static std::shared_ptr<Model> model(int type) {
	std::string k = std::to_string(type);
	if (!cacheM.count(k)) {
		GeomParams gp;
		gp.fromType(type);
		gp.sampleRate = 48000;
		cacheM[k] = buildModel(gp);
	}
	return cacheM[k];
}
static Controls defaults(int type) {
	Controls c;
	c.play = CY_TYPES[type].play;
	c.position = CY_TYPES[type].position;
	c.mic = 1.0;
	c.u.width = 0.35;
	return c;
}
struct Take { std::vector<float> L, R; };
typedef std::function<void(Engine&, double)> Script;
static void perform(Take& tk, int type, Controls c, double dur, Script sc, int typeB = -1) {
	Engine E;
	E.ctl = c;
	E.init(model(type), 48000);
	if (typeB >= 0) E.setMorphModel(*model(typeB));
	int n = (int)(dur * 48000);
	size_t o = tk.L.size();
	tk.L.resize(o + n);
	tk.R.resize(o + n);
	for (int s = 0; s < n; s += 32) {
		sc(E, (double)s / 48000);
		Engine::Extra ex;
		ex.R = tk.R.data() + o + s;
		E.process(nullptr, tk.L.data() + o + s, std::min(32, n - s), &ex);
	}
	float pk = 0;
	for (int i = 0; i < n; i++) pk = std::max(pk, std::max(std::fabs(tk.L[o + i]), std::fabs(tk.R[o + i])));
	printf("    %-10s %5.1f s  peak %.2f V  guard %d\n", CY_TYPES[type].name, dur, pk, E.guardEvents);
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	std::string only = argc > 2 ? argv[2] : "all";
	auto want = [&](const char* n) { return only == "all" || only == n; };
	auto save = [&](const char* name, Take& t) { writeWav(dir + "/" + name + ".wav", t.L, t.R, 48000); printf("  %s.wav\n", name); };

	if (want("tuned")) {
		// crash pulled onto D minor 9: TUNE 0, 50, 100%, a hard hit at 100%, then a ride whose chord changes each bar
		Take t;
		for (double amt : {0.0, 0.5, 1.0}) {
			Controls c = defaults(1);
			c.u.tune = amt; c.u.scale = SC_CHORD; c.u.root = 2;
			perform(t, 1, c, 4.0, [](Engine& E, double tt) { if (tt == 0) E.strike(3.0); });
		}
		Controls c = defaults(1);
		c.u.tune = 1; c.u.scale = SC_CHORD; c.u.root = 2;
		perform(t, 1, c, 5.0, [](Engine& E, double tt) { if (tt == 0) E.strike(8.0); });
		Controls r = defaults(4);
		r.u.tune = 1; r.u.scale = SC_CHORD;
		const float ch[3][4] = {{2 / 12.f, 5 / 12.f, 9 / 12.f, 0}, {7 / 12.f, 11 / 12.f, 2 / 12.f, 5 / 12.f}, {0, 4 / 12.f, 7 / 12.f, 11 / 12.f}};   // Dm7, G7, Cmaj7
		int last = -1;
		perform(t, 4, r, 9.0, [&](Engine& E, double tt) {
			int b = std::min(2, (int)(tt / 2.4));
			E.ctl.u.nChord = 4;
			for (int j = 0; j < 4; j++) E.ctl.u.chord[j] = ch[b][j];
			int k = (int)(tt / 0.3);
			if (k != last && tt < 7.2) { last = k; E.strike(k % 4 == 0 ? 3.0 : 1.6); }
		});
		save("ext-tuned", t);
	}
	if (want("warp")) {
		Take t;
		for (double g : {1.0, 0.75, 0.5, 0.0, 1.3, 1.7, 2.5}) {
			Controls c = defaults(1);
			c.u.warp = g;
			perform(t, 1, c, 3.5, [](Engine& E, double tt) { if (tt == 0) E.strike(5.0); });
		}
		save("ext-warp", t);
	}
	if (want("mirror")) {
		// WARP -1 on a ride while PIVOT is swept by hand
		Take t;
		Controls c = defaults(4);
		c.u.warp = -1;
		int last = -1;
		perform(t, 4, c, 10.0, [&](Engine& E, double tt) {
			E.ctl.u.pivot = 400 * std::pow(6.0, 0.5 - 0.5 * std::cos(2 * CY_PI * tt / 8.0));
			int k = (int)(tt / 0.8);
			if (k != last && tt < 8.5) { last = k; E.strike(2.5); }
		});
		save("ext-mirror", t);
	}
	if (want("swell")) {
		// reverse cymbal from a HOLD gate: SWELL slow then fast, COLOUR low then high; CHOKE on the downbeat
		Take t;
		struct S { double swell, colour; };
		for (S s : {S{3, 400}, S{3, 4000}, S{10, 1500}}) {
			Controls c = defaults(1);
			perform(t, 1, c, 4.5, [&](Engine& E, double tt) {
				E.ctl.u.sustain = tt < 2.5 ? 0.8 : 0.0;
				E.ctl.u.swell = s.swell;
				E.ctl.u.colour = s.colour;
				E.ctl.choke = tt > 2.5 && tt < 2.7;
				if (std::fabs(tt - 2.5) < 1e-3) E.strike(6.0);
			});
		}
		save("ext-reverse-swell", t);
	}
	if (want("drone")) {
		// endless crash: SUSTAIN held, COLOUR swept by a slow LFO, BLOOM 1.5
		Take t;
		Controls c = defaults(7);
		c.bloom = 1.5;
		c.u.swell = 4;
		perform(t, 7, c, 20.0, [](Engine& E, double tt) {
			E.ctl.u.sustain = tt < 17 ? 0.7 : 0.0;
			E.ctl.u.colour = 1200 * std::pow(8.0, std::sin(2 * CY_PI * tt / 14.0));
			if (tt == 0) E.strike(2.0);
		});
		save("ext-drone", t);
	}
	if (want("bloom")) {
		// the same hard crash: BLOOM +1, BLOOM -1, slow FLOW, fast FLOW
		Take t;
		struct V { double bloom, flow; };
		for (V v : {V{1, 1}, V{-1, 1}, V{1.5, 0.08}, V{1.5, 8}}) {
			Controls c = defaults(1);
			c.bloom = v.bloom;
			c.u.flow = v.flow;
			perform(t, 1, c, 4.5, [](Engine& E, double tt) { if (tt == 0) E.strike(7.0); });
		}
		save("ext-bloom", t);
	}
	if (want("morph")) {
		// melting cymbal: a mallet roll on a crash while MORPH goes to CHINA and back
		Take t;
		Controls c = defaults(1);
		c.play = MALLET; c.hardness = 0.5; c.rate = 9; c.force = 2.0; c.position = 0.85;
		perform(t, 1, c, 14.0, [](Engine& E, double tt) {
			E.ctl.roll = tt < 12;
			E.ctl.u.morph = tt < 6 ? tt / 6 : std::max(0.0, 1 - (tt - 7) / 5);
		}, 7);
		Controls b = defaults(1);
		perform(t, 1, b, 8.0, [](Engine& E, double tt) {
			E.ctl.u.morph = std::min(1.0, tt / 5);
			if (tt == 0) E.strike(6.0);
		}, 9);
		save("ext-morph", t);
	}
	if (want("load")) {
		// a 300 g weight sliding from the bell to the edge and back while a ride rings
		Take t;
		Controls c = defaults(4);
		c.u.load = 0.3;
		int last = -1;
		perform(t, 4, c, 10.0, [&](Engine& E, double tt) {
			E.ctl.u.loadPos = 0.55 + 0.4 * std::sin(2 * CY_PI * 0.35 * tt);
			int k = (int)(tt / 2.0);
			if (k != last && tt < 8) { last = k; E.strike(3.0); }
		});
		save("ext-load", t);
	}
	if (want("melody")) {
		// acid bell: BELL, HARMONIC, V/OCT from a little sequence, short DECAY
		Take t;
		Controls c = defaults(9);
		c.u.tune = 1; c.u.scale = SC_HARMONIC; c.decay = 0.4;
		const double seq[16] = {0, 0, 12, 0, 3, 0, 15, 7, 0, 0, 12, 10, 3, 5, 15, 7};
		int last = -1;
		perform(t, 9, c, 8.0, [&](Engine& E, double tt) {
			int k = (int)(tt / 0.15);
			if (k != last && tt < 7.2) { last = k; E.ctl.u.voct = seq[k % 16] / 12.0 - 1.0; E.strike(k % 4 == 0 ? 4.0 : 2.5); }
		});
		save("ext-acid-bell", t);
	}
	if (want("tilt")) {
		// TILT -1 (highs outlast lows) against +1 on the same crash
		Take t;
		for (double tl : {-1.0, 0.0, 1.0}) {
			Controls c = defaults(1);
			c.u.tilt = tl;
			perform(t, 1, c, 4.0, [](Engine& E, double tt) { if (tt == 0) E.strike(4.0); });
		}
		save("ext-tilt", t);
	}
	if (want("freeze")) {
		// a crash hit frozen after half a second: the level holds, the colour keeps moving
		Take t;
		Controls c = defaults(1);
		c.bloom = 1.5;
		perform(t, 1, c, 10.0, [](Engine& E, double tt) {
			if (tt == 0) E.strike(7.0);
			E.ctl.u.freeze = tt > 0.5 && tt < 8.0;
		});
		save("ext-freeze", t);
	}
	return 0;
}
