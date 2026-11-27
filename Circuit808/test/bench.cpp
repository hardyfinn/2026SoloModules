// Offline checks for the 808 Circuit Bench kit (no Rack needed): make test
#include "../src/BenchVoices.hpp"
#include <vector>
#include <string>
using namespace bench;
static const float FS = 48000.f;
static int fails = 0;
static void check(bool ok, const std::string& w) { printf("  [%s] %s\n", ok ? "ok" : "FAIL", w.c_str()); if (!ok) fails++; }
static float peakOf(Kit& kit, int v, float acc, float sec) {
	float p = 0.f; bool fin = true;
	kit.hit(v, acc);
	for (int i = 0; i < sec * FS; i++) { kit.v[v]->process(); float o = kit.v[v]->out; if (!std::isfinite(o)) fin = false; p = std::max(p, std::fabs(o)); }
	return fin ? p : -1.f;
}
static const char* PAT[NV] = {
	"x.....x...x.....|x.....x...x..x..",   // BD
	"....x.......x...|....x.......x.x.",   // SD
	"................|.............x..",   // LT
	"................|..............x.",   // MT
	"................|...............x",   // HT
	"...x......x.....|...x............",   // RS
	"..x..x..........|..x..x..........",   // CL
	"............x...|............x...",   // CP
	"x.xxx.xxx.xxx.xx|x.xxx.xxx.xxx.xx",   // MA
	".......x...x....|.......x........",   // CB
	"x...............|................",   // CY
	"..............x.|..............x.",   // OH
	"x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.",   // CH
};
// bars of the pattern at 120 BPM, accents on the beat, mixed as the module's L/R (before -6 dB)
static void playBars(Kit& kd, int bars, std::vector<float>& L, std::vector<float>& R) {
	int step = (int)(60.f / 120.f / 4.f * FS);
	for (int bar = 0; bar < bars; bar++) for (int st = 0; st < 16; st++) {
		for (int v = 0; v < NV; v++) if (PAT[v][(bar % 2) * 17 + st] == 'x') kd.hit(v, (st % 4 == 0) ? 1.f : 0.f);
		for (int i = 0; i < step; i++) {
			float l = 0, r = 0;
			for (int v = 0; v < NV; v++) {
				kd.v[v]->process(); float o = kd.v[v]->out;
				if (v == MA) o *= 0.5f;
				l += o * std::min(1.f, 2.f * (1.f - kd.v[v]->pan)); r += o * std::min(1.f, 2.f * kd.v[v]->pan);
			}
			L.push_back(l); R.push_back(r);
		}
	}
}
static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = fopen(path, "wb");
	if (!f) return;
	uint32_t n = L.size(), bytes = n * 4;
	auto w32 = [&](uint32_t x) { fwrite(&x, 4, 1, f); };
	auto w16 = [&](uint16_t x) { fwrite(&x, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32((uint32_t)FS); w32((uint32_t)FS * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (uint32_t i = 0; i < n; i++) for (float x : {L[i], R[i]}) { float c = std::max(-1.f, std::min(1.f, x * 0.5f / 10.f)); w16((uint16_t)(int16_t)(c * 32767)); }
	fclose(f);
}
// a voice's character for comparing kits: brightness (zero crossings per second, weighted
// by level), length (time to fall 30 dB under its peak) and loudness (RMS over the hit)
struct Feat { float zcr, len, rms; };
static Feat feat(Kit& k, int v) {
	k.v[v]->reset();
	k.hit(v, 0.5f);
	int n = (int)(3.f * FS); float pk = 0.f; std::vector<float> x(n);
	for (int i = 0; i < n; i++) { k.v[v]->process(); x[i] = k.v[v]->out; pk = std::max(pk, std::fabs(x[i])); }
	double zc = 0, w = 0, e = 0; int last = 0;
	for (int i = 1; i < n; i++) {
		float a = std::fabs(x[i]);
		if (a > pk * 0.0316f) last = i;
		if ((x[i] > 0) != (x[i - 1] > 0)) zc += a;
		w += a; e += x[i] * x[i];
	}
	Feat f; f.zcr = w > 0 ? (float)(zc / w * FS) : 0.f; f.len = last / FS; f.rms = (float)std::sqrt(e / std::max(1, last));
	return f;
}
int main() {
	printf("808 Circuit Bench\n");
	Kit kit; kit.setSampleRate(FS); kit.setSerial(808);
	bool lv = true;
	for (int v = 0; v < NV; v++) {
		float u = peakOf(kit, v, 0.f, 1.f), a = peakOf(kit, v, 1.f, 1.f);
		printf("  %s %-11s peak %.2f V, accented %.2f V\n", code(v), longName(v), u, a);
		if (!(u > 1.f && u < 11.f && a > u && a < 12.f)) lv = false;
	}
	check(lv, "every voice: stock level 1-11 V, accent louder, finite");
	// toms keep their range whatever is loaded
	kit.v[LT]->toStock(); kit.v[HT]->set(c808::tom::RANGE, 0.f);
	check(kit.v[LT]->k[c808::tom::RANGE] == 0.f && kit.v[HT]->k[c808::tom::RANGE] == 2.f, "LT/MT/HT hold their range");
	// closed hat chokes the open hat
	{
		Kit k2; k2.setSampleRate(FS);
		k2.hit(OH, 0.f);
		for (int i = 0; i < 0.05f * FS; i++) k2.v[OH]->process();
		k2.hit(CH, 0.f);
		float p = 0.f; for (int i = 0; i < 0.2f * FS; i++) { k2.v[OH]->process(); if (i > 0.1f * FS) p = std::max(p, std::fabs(k2.v[OH]->out)); }
		printf("  OH 100-200 ms after a CH: %.3f V\n", p);
		check(p < 0.05f, "CH chokes OH");
	}
	// random settings stay finite and bounded
	{
		c808::Rng r; r.seed(5); bool ok = true;
		for (int t = 0; t < 20; t++) for (int v = 0; v < NV; v++) {
			Kit k3; k3.setSampleRate(t % 2 ? 96000.f : 44100.f);
			for (int i = 0; i < k3.v[v]->nK(); i++) k3.v[v]->set(i, t == 0 ? 0.f : t == 1 ? 1.f : r.uni());
			k3.hit(v, t % 3 ? 1.f : 0.f);
			for (int i = 0; i < 0.3f * 48000; i++) { k3.v[v]->process(); float o = k3.v[v]->out; if (!std::isfinite(o) || std::fabs(o) > (v >= CY ? 25.f : 13.f)) { if (ok) printf("  %s trial %d: %g\n", code(v), t, o); ok = false; } }
		}
		check(ok, "20 random/extreme settings per voice at 44.1/96 kHz: finite, bounded (808 Metal as on its own module)");
	}
	// silent voices sleep
	{
		Kit k4; k4.setSampleRate(FS); bool all = true;
		for (int v = 0; v < NV; v++) {
			k4.hit(v, 0.f); int n = 0;
			while (!k4.v[v]->sleeping() && n < 12 * FS) { k4.v[v]->process(); n++; }
			if (!k4.v[v]->sleeping()) { all = false; printf("  %s never sleeps\n", code(v)); }
		}
		check(all, "every voice sleeps after its sound ends");
	}
	// every parameter of every voice sits on exactly one of the 16 knobs
	{
		bool ok = true;
		for (int v = 0; v < NV; v++) {
			const Slots& S = slots(kindOf(v));
			if (S.n > NSLOT) ok = false;
			std::vector<int> seen(MAXK, 0); int pans = 0;
			for (int s = 0; s < S.n; s++) {
				int p = S.p[s];
				if (p == PAN) { pans++; continue; }
				if (p < 0 || p >= kit.v[v]->nK()) ok = false; else seen[p]++;
			}
			if (pans != 1) ok = false;
			for (int i = 0; i < kit.v[v]->nK(); i++) {
				bool hidden = kindOf(v) == K_TOM && i == c808::tom::RANGE;
				if (seen[i] != (hidden ? 0 : 1)) { ok = false; printf("  %s param %s appears %d times\n", code(v), kit.v[v]->def(i).id, seen[i]); }
			}
		}
		check(ok, "the 16 knobs show every parameter of every voice exactly once");
	}
	// RAND: LEVEL and PAN never move, values stay 0..1, a nudge stays nearer home than wild
	{
		Kit kr; Rng r; bool keep = true, range = true;
		double dn = 0, dw = 0; int cnt = 0;
		for (int t = 0; t < 400; t++) for (int v = 0; v < NV; v++) {
			Voice* vo = kr.v[v];
			vo->toStock();
			if (t % 2) vo->set(0, 0.9f);   // a loaded kit away from stock
			float home[MAXK + 1];
			for (int i = 0; i < MAXK; i++) home[i] = vo->k[i];
			home[MAXK] = vo->pan = 0.3f;
			bool wild = t % 4 >= 2;
			roll(vo, home, wild, r);
			for (int i = 0; i < vo->nK(); i++) {
				if (vo->k[i] < 0.f || vo->k[i] > (vo->def(i).kind == 1 ? 2.f : 1.f)) range = false;
				if (!rollable(vo, i) && vo->k[i] != home[i]) keep = false;
				if (rollable(vo, i) && vo->def(i).kind == 0) { (wild ? dw : dn) += std::fabs(vo->k[i] - home[i]); if (!wild) cnt++; }
			}
			if (vo->pan != 0.3f) keep = false;
			if (kindOf(v) == K_TOM && vo->k[c808::tom::RANGE] != (float)(v - LT)) keep = false;
		}
		printf("  mean move: nudge %.3f, wild %.3f (of the knob's travel)\n", dn / cnt, dw / cnt);
		check(keep, "RAND never moves LEVEL, PAN or a tom's range");
		check(range, "RAND keeps every knob in range");
		check(dn / cnt < 0.13 && dw / cnt > 0.25, "RAND nudge stays near the loaded kit, wild goes anywhere");
	}
	// factory kits: every name they use exists on the voices they name
	{
		bool ok = true;
		Kit kf;
		for (auto& fk : factoryKits()) {
			std::string s(fk.text);
			size_t pos = 0;
			while (pos < s.size()) {
				size_t end = s.find(';', pos); if (end == std::string::npos) end = s.size();
				std::string part = s.substr(pos, end - pos); pos = end + 1;
				size_t c = part.find(':'); if (c == std::string::npos) continue;
				std::string who = " " + part.substr(0, c) + " ", sets = part.substr(c + 1);
				for (int v = 0; v < NV; v++) {
					if (who.find(std::string(" ") + code(v) + " ") == std::string::npos) continue;
					size_t i = 0;
					while (i < sets.size()) {
						while (i < sets.size() && sets[i] == ' ') i++;
						size_t j = sets.find('=', i); if (j == std::string::npos) break;
						std::string id = sets.substr(i, j - i);
						if (paramByIdString(kf.v[v], id) < 0) { ok = false; printf("  %s: %s has no %s\n", fk.name, code(v), id.c_str()); }
						i = sets.find(' ', j); if (i == std::string::npos) break;
					}
				}
			}
		}
		check(ok, "factory kits only use parameters their voices have");
	}
	// demo: four bars of a kit pattern through the stereo mix
	{
		Kit kd; kd.setSampleRate(FS); kd.setSerial(808);
		kd.v[CY]->pan = 0.7f; kd.v[OH]->pan = 0.35f; kd.v[CH]->pan = 0.35f; kd.v[CB]->pan = 0.65f; kd.v[CL]->pan = 0.7f;
		kd.v[LT]->pan = 0.3f; kd.v[HT]->pan = 0.7f; kd.v[MA]->pan = 0.6f;
		std::vector<float> L, R;
		playBars(kd, 4, L, R);
		writeWav("test/out/bench-01-kit.wav", L, R);
	}
	// the factory kits, two bars each, in menu order; each within -6/+3.5 dB of the stock kit's mix peak
	{
		std::vector<float> L, R; bool ok = true; float stockPk = 0.f;
		for (auto& fk : factoryKits()) {
			Kit kd; kd.setSampleRate(FS); kd.setSerial(808);
			applyKit(kd, fk.text);
			size_t from = L.size();
			playBars(kd, 2, L, R);
			float pk = 0.f;
			for (size_t i = from; i < L.size(); i++) pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])) * 0.5f);
			printf("  kit %-28s mix peak %.2f V\n", fk.name, pk);
			if (stockPk == 0.f) stockPk = pk;
			if (!(pk > 0.5f * stockPk && pk < 1.5f * stockPk)) ok = false;
		}
		writeWav("test/out/bench-02-factory-kits.wav", L, R);
		check(ok, "every factory kit plays within -6/+3.5 dB of the stock kit");
	}

	// how far each factory kit moves from the stock kit: a voice counts as changed when its
	// brightness or length moves by a third or more (or its loudness by 4 dB)
	{
		Kit ks; ks.setSampleRate(FS); ks.setSerial(808);
		Feat st[NV]; for (int v = 0; v < NV; v++) st[v] = feat(ks, v);
		bool ok = true;
		const auto& fk = factoryKits();
		std::vector<std::vector<Feat>> all(fk.size(), std::vector<Feat>(st, st + NV));
		for (size_t n = 1; n < fk.size(); n++) {
			Kit kk; kk.setSampleRate(FS); kk.setSerial(808); applyKit(kk, fk[n].text);
			int changed = 0; double score = 0; std::string which;
			for (int v = 0; v < NV; v++) {
				Feat f = feat(kk, v); all[n][v] = f;
				double db = std::fabs(std::log(f.zcr / st[v].zcr)), dl = std::fabs(std::log((f.len + 0.01) / (st[v].len + 0.01))),
					dr = std::fabs(20.0 * std::log10((f.rms + 1e-6) / (st[v].rms + 1e-6)));
				score += db + dl;
				if (db > 0.29 || dl > 0.29 || dr > 4.0) { changed++; which += std::string(" ") + code(v); }
			}
			printf("  %-28s %2d/13 voices changed, distance %.1f:%s\n", fk[n].name, changed, score, which.c_str());
			if (changed < 9) ok = false;
		}
		check(ok, "every factory kit changes at least 9 of the 13 voices audibly");
		// and the kits differ from each other, not only from stock
		double worst = 1e9; std::string pair;
		for (size_t a = 0; a < fk.size(); a++) for (size_t b = a + 1; b < fk.size(); b++) {
			double d = 0;
			for (int v = 0; v < NV; v++)
				d += std::fabs(std::log(all[a][v].zcr / all[b][v].zcr)) + std::fabs(std::log((all[a][v].len + 0.01) / (all[b][v].len + 0.01)));
			if (d < worst) { worst = d; pair = std::string(fk[a].name) + " / " + fk[b].name; }
		}
		double quiet = 0; std::string qv;
		for (size_t a = 1; a < fk.size(); a++) for (int v = 0; v < NV; v++) {
			double r = 20.0 * std::log10((all[a][v].rms + 1e-6) / (st[v].rms + 1e-6));
			if (r < quiet) { quiet = r; qv = std::string(fk[a].name) + " " + code(v); }
		}
		printf("  quietest voice against stock: %s (%.1f dB)\n", qv.c_str(), quiet);
		check(quiet > -17.0, "no factory kit leaves a voice near silent (RMS within 17 dB of stock)");
		printf("  closest pair of kits: %s (distance %.1f)\n", pair.c_str(), worst);
		check(worst > 8.0, "no two factory kits sound alike (distance > 8)");
	}

	printf(fails ? "%d FAILED\n" : "all bench checks passed\n", fails);
	return fails ? 1 : 0;
}
