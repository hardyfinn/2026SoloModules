// Offline checks for the bronze instruments + a demo driven by the Irama engine.
//   make test-instruments   (writes test/instruments-demo-dry.wav)
#include "../src/Instruments.hpp"
#include "../src/GamelanCore.hpp"
#include <cstdio>
#include <vector>
#include <string>
using namespace bronze;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } else { printf("ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static float hzOf(float v) { return 261.6256f * std::pow(2.f, v); }

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = fopen(path, "wb");
	if (!f) return;
	int n = (int)L.size();
	float pk = 1e-6f;
	for (int i = 0; i < n; i++) pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])));
	float g = 0.89f / pk;
	int bytes = n * 4;
	auto w32 = [&](int v) { fwrite(&v, 4, 1, f); };
	auto w16 = [&](short v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) { w16((short)(L[i] * g * 32767.f)); w16((short)(R[i] * g * 32767.f)); }
	fclose(f);
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	const float SR = 16000.f;

	// 1. everything decays, nothing runs away (worst-case corners)
	{
		AgengEngine a; a.setSampleRate(SR); AgengParams q; q.size = 1.f; q.bloom = 1.f; q.mallet = 1.f; q.bright = 1.f; q.ombak = 6.f;
		a.strikeGong(0, 1.f, q); a.strikeGong(1, 1.f, q); a.strikeKempul(110.f, 1.f, q);
		float L, R, S, pk = 0.f, late = 0.f;
		for (int i = 0; i < (int)(SR * 240); i++) { a.process(L, R, S); pk = std::max(pk, std::fabs(L)); if (i > SR * 230) late = std::max(late, std::fabs(L)); }
		CHECK(pk < 10.f && late < 0.01f, "Ageng decays (peak %.2f V, after 230 s %.5f V)", pk, late);
	}
	{
		GangsaEngine g; g.setSampleRate(SR); GangsaParams q; q.damp = 0.f; q.ring = 6.f; q.bright = 1.f; q.tube = 1.f; q.kantilan = 1.f; q.mallet = 1.f;
		for (int k = 0; k < 8; k++) g.strike(k % 2, 300.f * std::pow(2.f, k / 5.f), 0.f, 1.f, q);
		float L, R, P, S, pk = 0.f, late = 0.f;
		for (int i = 0; i < (int)(SR * 80); i++) { g.process(L, R, P, S); pk = std::max(pk, std::fabs(L)); if (i > SR * 75) late = std::max(late, std::fabs(L)); }
		CHECK(pk < 10.f && late < 0.01f, "Gangsa decays (peak %.2f V, after 75 s %.5f V)", pk, late);
	}
	{
		ReyongEngine r; r.setSampleRate(SR); ReyongParams q; q.ring = 5.f; q.sympathy = 1.f; q.kenongRing = 6.f; q.mallet = 1.f;
		for (int k = 0; k < 16; k++) r.strike(k % 2, 200.f * std::pow(2.f, k / 7.f), 1.f, q, false);
		r.kenongHit(110.f, 1.f, q); r.kethukHit(1.f, q); r.cekHit(1.f, q);
		float L, R, C, pk = 0.f, late = 0.f;
		for (int i = 0; i < (int)(SR * 100); i++) { r.process(L, R, C); pk = std::max(pk, std::fabs(L)); if (i > SR * 95) late = std::max(late, std::fabs(L)); }
		CHECK(pk < 10.f && late < 0.01f, "Reyong decays (peak %.2f V, after 95 s %.5f V)", pk, late);
	}
	// 2. Gangsa beat rate = OMBAK at C4 and C6
	for (int oc = 0; oc < 2; oc++) {
		const float sr = 48000.f;
		GangsaEngine g; g.setSampleRate(sr); GangsaParams q; q.kantilan = 0.f; q.bright = 0.f; q.tube = 0.f; q.set = 0.f; q.ring = 6.f; q.ombak = 6.5f; q.mallet = 0.f;
		float hz = oc ? 1046.5f : 261.63f;
		g.strike(0, hz, 0.f, 0.5f, q);
		int win = (int)(sr * 0.004f), nw = (int)(2.f * sr) / win;
		std::vector<float> env(nw, 0.f);
		float L, R, P, S;
		for (int w = 0; w < nw; w++) { float e = 0; for (int i = 0; i < win; i++) { g.process(L, R, P, S); e += P * P; } env[w] = std::sqrt(e / win); }
		// remove the slow decay, then autocorrelate
		for (int w = 0; w < nw; w++) env[w] /= std::exp(-(w * 0.004f) / 6.f);
		float mean = 0; for (float e : env) mean += e; mean /= nw;
		int best = 0; float bv = -1e9f;
		for (int lag = (int)(0.08f / 0.004f); lag < (int)(0.4f / 0.004f); lag++) {
			float c = 0; for (int w = 0; w + lag < nw; w++) c += (env[w] - mean) * (env[w + lag] - mean);
			if (c > bv) { bv = c; best = lag; }
		}
		float rate = 1.f / (best * 0.004f);
		CHECK(std::fabs(rate - 6.5f) < 0.4f, "Gangsa beat at %.0f Hz = %.2f Hz (OMBAK 6.5)", hz, rate);
	}
	// 3. Gangsa damping: the previous note is choked by the next (NEXT mode)
	for (int d = 0; d < 2; d++) {
		const float sr = 48000.f;
		GangsaEngine g; g.setSampleRate(sr); GangsaParams q; q.damp = d ? 1.f : 0.f; q.ring = 3.f;
		g.strike(0, 400.f, 0.f, 0.8f, q);
		float L, R, P, S;
		for (int i = 0; i < (int)(sr * 0.3f); i++) g.process(L, R, P, S);
		int first = g.last[0];
		float before = g.v[0][first].level();
		g.strike(0, 500.f, 0.f, 0.8f, q);
		for (int i = 0; i < (int)(sr * 0.06f); i++) g.process(L, R, P, S);
		float after = g.v[0][first].level();
		float db = 20.f * std::log10(std::max(after, 1e-9f) / before);
		if (d) CHECK(db < -30.f, "Gangsa DAMP 1: previous note down %.1f dB after 60 ms", -db);
		else CHECK(db > -6.f, "Gangsa DAMP 0: previous note down only %.1f dB after 60 ms", -db);
	}
	// 4. Ageng bloom: the fundamental peaks late; decay grows with SIZE
	{
		const float sr = 16000.f;
		float tPeak[2], tail[2];
		for (int k = 0; k < 2; k++) {
			AgengEngine a; a.setSampleRate(sr); AgengParams q; q.bloom = 0.6f; q.size = k ? 1.f : 0.f;
			a.strikeGong(0, 0.8f, q);
			float L, R, S, pk = 0; int at = 0; float e = 0; tail[k] = 0;
			for (int i = 0; i < (int)(sr * 12); i++) {
				a.process(L, R, S);
				e = std::max(std::fabs(S), e * 0.9995f);
				if (e > pk) { pk = e; at = i; }
				if (i > sr * 10) tail[k] = std::max(tail[k], std::fabs(S) / pk);
			}
			tPeak[k] = at / sr;
		}
		float tb = 0.02f + 1.18f * 0.6f;
		CHECK(tPeak[0] > 0.5f * tb, "Ageng fundamental peaks at %.2f s (bloom %.2f s)", tPeak[0], tb);
		CHECK(tail[1] > tail[0] * 2.f, "Ageng SIZE lengthens decay (tail %.4f vs %.4f)", tail[1], tail[0]);
	}
	// 5. Reyong: shoulder strike lowers the fundamental's share
	{
		ReyongEngine r; Partial p[Voice::MAXR];
		float share[2];
		for (int k = 0; k < 2; k++) {
			int n = r.kettle(p, 220.f, 1.f, 1.f, 0.0005f, k ? 1.f : 0.f);
			float tot = 0; for (int i = 0; i < n; i++) tot += std::fabs(p[i].a);
			share[k] = std::fabs(p[0].a) / tot;
		}
		CHECK(share[1] < share[0] * 0.7f, "Reyong shoulder: fundamental share %.2f -> %.2f", share[0], share[1]);
	}

	// ---------------------------------------------------------------- demo
	const int sr = 48000;
	std::vector<float> DL, DR;
	struct Scene { int style, form, mode, level, kot; bool fixed; float bpm16, secs, root; bool bali; };
	Scene scenes[2] = {
		{irama::BALI, 0, 6, 2, irama::K_TELU, true, 520.f, 40.f, 0.f, true},
		{irama::JAVA, 2, 2, 1, irama::K_IMBAL, false, 300.f, 48.f, -0.17f, false},
	};
	for (int sc = 0; sc < 2; sc++) {
		const Scene& S = scenes[sc];
		irama::IramaCore c; c.style = S.style; c.form = S.form; c.mode = S.mode; c.levelTarget = S.level;
		c.kotekan = S.kot; c.tempoFixed = S.fixed; c.mutate = 0.5f; c.sampleRate = sr; c.rng.seed(41 + sc);
		if (S.bali) { c.kebyar = true; c.prevKebyar = false; }
		AgengEngine ag; ag.setSampleRate(sr); AgengParams aq; aq.tuneHz = S.bali ? 52.f : 46.f; aq.interval = 2.f; aq.bloom = S.bali ? 0.25f : 0.5f;
		GangsaEngine ga; ga.setSampleRate(sr); GangsaParams gq;
		ReyongEngine re; re.setSampleRate(sr); ReyongParams rq; rq.bali = S.bali;
		if (!S.bali) { gq.damp = 0.15f; gq.ring = 3.f; gq.ombak = 0.f; gq.kantilan = 0.f; gq.mallet = 0.35f; gq.bright = 0.4f; rq.damp = 0.4f; }
		else { gq.damp = 0.8f; gq.ombak = 6.5f; }
		double per = sr * 60.0 / S.bpm16, next = 0;
		long n = (long)(S.secs * sr);
		float saronHz = 300.f;
		for (long i = 0; i < n; i++) {
			double t = i / (double)sr;
			if (S.bali) {
				if (t > 4.0 && t < 4.01) c.kebyar = false;
				if (t > 14.0 && t < 14.01) c.kotekan = irama::K_NYOGCAG;
				if (t > 20.0 && t < 20.01) c.triggerAngsel();
				if (t > 24.0 && t < 24.01) c.kotekan = irama::K_EMPAT;
				if (t > 31.0 && t < 31.01) c.triggerAngsel();
				if (t > 35.5 && t < 35.51) c.kebyar = true;
				if (t > 38.0 && t < 38.01) c.kebyar = false;
			}
			else {
				if (t > 14.0 && t < 14.01) c.levelTarget = 2;
				if (t > 36.0 && t < 36.01) c.triggerSuwuk();
			}
			bool rise = false;
			if (i >= (long)next) { rise = true; next += per; }
			irama::IramaFrame f; c.tick(rise, f);
			if (f.fire[irama::V_SARON]) saronHz = hzOf(f.volts[irama::V_SARON] + S.root);
			if (f.fire[irama::V_POLOS]) ga.strike(0, hzOf(f.volts[irama::V_POLOS] + S.root), 0.f, 0.75f, gq);
			if (f.fire[irama::V_SANGSIH]) ga.strike(1, hzOf(f.volts[irama::V_SANGSIH] + S.root), 0.f, 0.75f, gq);
			if (f.fire[irama::V_BONANG]) re.strike(0, hzOf(f.volts[irama::V_BONANG] + S.root), 0.7f, rq, false);
			if (!S.bali && f.fire[irama::V_PEKING]) re.strike(1, hzOf(f.volts[irama::V_PEKING] + S.root), 0.45f, rq, false);
			if (f.trig[irama::T_GONG]) ag.strikeGong(S.bali ? (int)(t * 7) % 2 : 0, 0.9f, aq);
			if (f.trig[irama::T_KEMPUL]) ag.strikeKempul(saronHz / 4.f, 0.7f, aq);
			if (f.trig[irama::T_KENONG]) re.kenongHit(saronHz / 2.f, 0.6f, rq);
			if (f.trig[irama::T_KETHUK]) re.kethukHit(S.bali ? 0.3f : 0.5f, rq);
			if (S.bali && f.trig[irama::T_KENDANG]) re.cekHit(0.6f, rq);
			float aL, aR, aS, gL, gR, gP, gS, rL, rR, rC;
			ag.process(aL, aR, aS); ga.process(gL, gR, gP, gS); re.process(rL, rR, rC);
			DL.push_back(0.8f * aL + 0.55f * gL + 0.5f * rL);
			DR.push_back(0.8f * aR + 0.55f * gR + 0.5f * rR);
		}
		for (int i = 0; i < sr * 4; i++) {   // let the gongs ring out
			float aL, aR, aS, gL, gR, gP, gS, rL, rR, rC;
			ag.process(aL, aR, aS); ga.process(gL, gR, gP, gS); re.process(rL, rR, rC);
			DL.push_back(0.8f * aL + 0.55f * gL + 0.5f * rL);
			DR.push_back(0.8f * aR + 0.55f * gR + 0.5f * rR);
		}
	}
	writeWav((dir + "/test/instruments-demo-dry.wav").c_str(), DL, DR, sr);
	printf(fails ? "%d checks FAILED\n" : "all instrument checks passed\n", fails);
	return fails ? 1 : 0;
}
