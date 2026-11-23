// Offline checks and demo renders for Tapeizer.  No Rack needed:  make test
//   ./test/render OUTDIR [STEMDIR DEMODIR]
// Every check prints PASS or FAIL; the program exits non-zero if any fails.
// Check numbers follow section 9 of docs/BUILD-INSTRUCTIONS.md. Expected values come from
// tools/proto/tape_ref.py through tools/proto/export_ref.py (test/ref_data.h).
#include "../src/dsp/Engine.hpp"
#include "ref_data.h"
#include <cstdio>
#include <string>
#include <vector>
#include <complex>
#include <chrono>
#include <functional>

using namespace tapeizer;

static int failures = 0;
static void check(bool ok, const char* what, const std::string& detail) {
	printf("%s  %-64s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
	fflush(stdout);
	if (!ok) failures++;
}
template <typename... A> static std::string fmt(const char* f, A... a) { char b[1024]; snprintf(b, sizeof b, f, a...); return b; }

// Hann-windowed DFT amplitude at f (lvl() in check_ref.py), over x[from:]
static double lvl(const std::vector<double>& x, double f, double fs, size_t from) {
	size_t n = x.size() - from; std::complex<double> s(0); double ws = 0;
	for (size_t i = 0; i < n; i++) {
		double w = 0.5 - 0.5 * std::cos(2 * PI * i / (n - 1));
		s += x[from + i] * w * std::polar(1.0, -2 * PI * f * i / fs); ws += w;
	}
	return 2 * std::abs(s) / ws;
}

static const char* MNAME[9] = {"4TR", "4TR HI", "4TR C", "8TR CASS", "8TR 7.5", "8TR 15", "DECK", "BOOMBOX", "1/4 INCH"};
static const double FREQS[6] = {40, 100, 1000, 5000, 10000, 14000};
static const double LEVELS[3] = {-20, 0, 6};

// One deck, one track, a sine at amplitude db(L): gain in dB at f.
static double deckGain(double fs, int machine, double L, double f, std::function<void(DeckSettings&)> mod = nullptr, double ff = -1) {
	DeckSettings s; s.machine = machine; s.quiet = true; s.age = 0; s.strength = 1;
	if (mod) mod(s);
	Deck d; d.init(fs, 1, 1, 1, s);
	int n = (int)(2 * fs); std::vector<double> y(n);
	double A = db(L);
	for (int i = 0; i < n; i++) { double x = A * std::sin(2 * PI * f * i / fs), o; d.process(&x, &o); y[i] = o; }
	return 20 * std::log10(lvl(y, ff > 0 ? ff : f, fs, (size_t)(fs / 2)) / A);
}

static const double* refArr(const char* name, double fs) {
	if (!strcmp(name, "RESP")) return fs == 44100 ? RESP_44100 : fs == 48000 ? RESP_48000 : RESP_96000;
	if (!strcmp(name, "S05")) return fs == 44100 ? RESP_S05_44100 : fs == 48000 ? RESP_S05_48000 : RESP_S05_96000;
	if (!strcmp(name, "S0")) return fs == 44100 ? RESP_S0_44100 : fs == 48000 ? RESP_S0_48000 : RESP_S0_96000;
	if (!strcmp(name, "HD3")) return fs == 44100 ? HD3_44100 : fs == 48000 ? HD3_48000 : HD3_96000;
	if (!strcmp(name, "BIAS")) return fs == 44100 ? BIAS_44100 : fs == 48000 ? BIAS_48000 : BIAS_96000;
	if (!strcmp(name, "AZ")) return fs == 44100 ? AZ_44100 : fs == 48000 ? AZ_48000 : AZ_96000;
	if (!strcmp(name, "AGE")) return fs == 44100 ? AGE_44100 : fs == 48000 ? AGE_48000 : AGE_96000;
	return fs == 44100 ? NRENC_44100 : fs == 48000 ? NRENC_48000 : NRENC_96000;
}

static void checkResponse(double fs) {
	const double* R = refArr("RESP", fs);
	double worst = 0, worst14 = 0; std::string where;
	for (int mi = 0; mi < 9; mi++) for (int li = 0; li < 3; li++) for (int fi = 0; fi < 6; fi++) {
		double gdb = deckGain(fs, mi, LEVELS[li], FREQS[fi]);
		double e = std::fabs(gdb - R[(mi * 3 + li) * 6 + fi]);
		if (fi == 5) worst14 = std::max(worst14, e);
		else if (e > worst) { worst = e; where = fmt("%s %+.0f dB %.0f Hz: %.2f vs %.2f", MNAME[mi], LEVELS[li], FREQS[fi], gdb, R[(mi * 3 + li) * 6 + fi]); }
	}
	check(worst <= 0.4 && worst14 <= 0.8, fmt("2. quiet response, 9 machines x 3 levels, %.0f Hz", fs).c_str(),
	      fmt("worst %.2f dB (%s), worst at 14 kHz %.2f dB", worst, where.c_str(), worst14));
}

static void checkHD3(double fs) {
	const double* R = refArr("HD3", fs);
	struct { int m, t; } cases[4] = {{M_4TR, 0}, {M_4TR, 1}, {M_4TR, 2}, {M_8TR_15, 1}};
	bool ok = true; std::string det;
	int k = 0;
	for (auto& cs : cases) {
		const TapeDef& td = (MACHINES[cs.m].reel ? TAPES_REEL : TAPES_CASS)[cs.t];
		for (double dl : {0.0, -10.0}) {
			DeckSettings s; s.machine = cs.m; s.tape = cs.t; s.quiet = true; s.age = 0;
			Deck d; d.init(fs, 1, 1, 1, s);
			int n = (int)(2 * fs); std::vector<double> y(n); double A = db(td.mol + dl);
			for (int i = 0; i < n; i++) { double x = A * std::sin(2 * PI * 315 * i / fs), o; d.process(&x, &o); y[i] = o; }
			double h = 100 * lvl(y, 945, fs, (size_t)(fs / 2)) / lvl(y, 315, fs, (size_t)(fs / 2));
			bool good = dl == 0 ? (h >= 2.3 && h <= 2.7) : (h >= 0.2 && h <= 0.35);
			ok = ok && good;
			det += fmt("%s/%s%+.0f: %.2f%% (ref %.2f)  ", MNAME[cs.m], cs.t == 0 ? "I" : cs.t == 1 ? "II" : "IV", dl, h, R[k]);
			k++;
		}
	}
	check(ok, fmt("4. third harmonic at MOL and MOL-10, %.0f Hz", fs).c_str(), det);
}

// ---------------------------------------------------------------- WAV I/O
static bool readWav(const std::string& path, std::vector<std::vector<double>>& ch, double& sr) {
	FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
	char id[4]; uint32_t sz; fread(id, 1, 4, f); fread(&sz, 4, 1, f); fread(id, 1, 4, f);
	int fmtTag = 0, nch = 0, bits = 0; uint32_t rate = 0; std::vector<char> data;
	while (fread(id, 1, 4, f) == 4 && fread(&sz, 4, 1, f) == 1) {
		std::vector<char> b(sz); fread(b.data(), 1, sz, f); if (sz & 1) fgetc(f);
		if (!memcmp(id, "fmt ", 4)) { fmtTag = *(uint16_t*)&b[0]; nch = *(uint16_t*)&b[2]; rate = *(uint32_t*)&b[4]; bits = *(uint16_t*)&b[14]; if (fmtTag == 0xFFFE) fmtTag = *(uint16_t*)&b[24]; }
		else if (!memcmp(id, "data", 4)) data = b;
	}
	fclose(f);
	if (!nch) return false;
	sr = rate; size_t n = data.size() / (bits / 8) / nch; ch.assign(nch, std::vector<double>(n));
	for (size_t i = 0; i < n; i++) for (int c = 0; c < nch; c++) {
		size_t k = i * nch + c;
		if (fmtTag == 3 && bits == 32) ch[c][i] = ((float*)data.data())[k];
		else if (bits == 16) ch[c][i] = ((int16_t*)data.data())[k] / 32768.0;
	}
	return true;
}
static void writeWav(const std::string& path, const std::vector<double>& L, const std::vector<double>& R, double sr) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) { printf("cannot write %s\n", path.c_str()); return; }
	uint32_t n = L.size(), dataBytes = n * 4, v; uint16_t s;
	fwrite("RIFF", 1, 4, f); v = 36 + dataBytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	v = 16; fwrite(&v, 4, 1, f); s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f); v = (uint32_t)sr; fwrite(&v, 4, 1, f);
	v = (uint32_t)sr * 4; fwrite(&v, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f);
	for (uint32_t i = 0; i < n; i++) for (double x : {L[i], R[i]}) { int16_t q = (int16_t)std::lround(clampd(x, -1, 1) * 32767); fwrite(&q, 2, 1, f); }
	fclose(f);
}

// ---------------------------------------------------------------- presets (same as presets/*.vcvm, section 10)
struct Preset { const char* name; std::function<void(Params&)> set; };
static std::vector<Preset> presets() {
	return {
		{"Four-Track To Cassette", [](Params&) {}},
		{"Two Decks 1985", [](Params& p) { p.strength = 0.5; p.multiMachine = 0; p.multiTape = 0; p.nrType = NR_OFF; p.recDb = 3; p.masterMachine = MM_DECK; p.masterTape = 0; p.masterNr = false; p.masterRecDb = 3; }},
		{"Clean Four-Track", [](Params& p) { p.strength = 0.25; p.multiMachine = 1; p.nrType = NR_X2; p.masterMachine = MM_DAT; }},
		{"In The Red", [](Params& p) { p.strength = 0.5; p.trimDb = 8; p.recDb = 9; p.masterRecDb = 6; }},
		{"260 Session", [](Params& p) { p.strength = 0.45; p.multiMachine = 2; p.nrType = NR_C; p.recDb = 4; p.loDb = 2; p.loF = 100; p.hiDb = 2; p.hiF = 8000; p.masterMachine = MM_DAT; }},
		{"Eight On A Cassette", [](Params& p) { p.strength = 0.5; p.multiMachine = 3; p.nrType = NR_X2; p.recDb = 2; }},
		{"Studio Eight", [](Params& p) { p.strength = 0.55; p.multiMachine = 4; p.nrType = NR_X2; p.recDb = 6; p.masterMachine = MM_QUARTER; p.masterNr = false; p.masterRecDb = 5; }},
		{"Budget Reel", [](Params& p) { p.strength = 0.5; p.multiMachine = 5; p.nrType = NR_C; p.recDb = 6; p.masterMachine = MM_QUARTER; p.masterNr = false; p.masterRecDb = 4; }},
		{"Third Bounce", [](Params& p) { p.strength = 0.45; p.multiMachine = 1; p.bounce = 3; p.recDb = 3; }},
		{"Boombox Dub", [](Params& p) { p.strength = 0.5; p.multiOn = false; p.masterMachine = MM_BOOMBOX; p.masterTape = 0; p.masterNr = false; p.masterRecDb = 5; }},
		{"Bright Trick", [](Params& p) { p.strength = 0.4; p.nrType = NR_B; p.nrEnc = true; p.nrDec = false; p.masterNr = false; }},
		{"Neglected", [](Params& p) { p.strength = 0.7; p.age = 1.0; p.wear = 0.5; }},
	};
}

// Render stereo through the engine (IN L/R), internal units = volts * 0.2828; input files peak at 1.0 = 5 V.
static void renderStereo(const Params& p, const std::vector<double>& L, const std::vector<double>& R, double fs, std::vector<double>& oL, std::vector<double>& oR, uint64_t seed = 7) {
	Engine e; e.seed = seed; e.init(fs); e.p = p;
	size_t n = L.size(); oL.assign(n, 0); oR.assign(n, 0);
	IO io; io.inOn = true;
	for (size_t i = 0; i < n; i++) {
		io.inL = L[i] * 5 * 0.2828; io.inR = R[i] * 5 * 0.2828;
		e.process(io);
		oL[i] = io.outL / 0.2828 / 5; oR[i] = io.outR / 0.2828 / 5;
	}
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	std::string stemDir = argc > 2 ? argv[2] : "", demoDir = argc > 3 ? argv[3] : "";
	const double fs = 48000;

	// 1. unit numbers
	{
		Unit u1 = makeUnit(1), u2 = makeUnit(12345); double worst = 0;
		for (int c = 0; c < 10; c++) {
			worst = std::max({worst, std::fabs(u1.sens[c] - UNIT_1[c]), std::fabs(u1.cal[c] - UNIT_1[10 + c]), std::fabs(u2.sens[c] - UNIT_12345[c]), std::fabs(u2.cal[c] - UNIT_12345[10 + c])});
		}
		worst = std::max({worst, std::fabs(u1.az - UNIT_1[20]), std::fabs(u1.bias - UNIT_1[21]), std::fabs(u2.az - UNIT_12345[20]), std::fabs(u2.bias - UNIT_12345[21])});
		check(worst < 1e-12, "1. unit numbers match the reference (seeds 1, 12345)", fmt("worst difference %.2e", worst));
	}
	// 2. quiet response
	checkResponse(fs);
	// 3. STRENGTH 0 and 0.5
	{
		double worst = 0;
		for (int mi = 0; mi < 9; mi++) for (int fi = 0; fi < 6; fi++) worst = std::max(worst, std::fabs(deckGain(fs, mi, 6, FREQS[fi], [](DeckSettings& s) { s.strength = 0; })));
		check(worst <= 0.1, "3. STRENGTH 0: every machine flat 40 Hz to 14 kHz at +6 dB", fmt("worst %.3f dB", worst));
		const double* R = refArr("S05", fs); double w2 = 0;
		for (int li = 0; li < 3; li++) for (int fi = 0; fi < 6; fi++)
			w2 = std::max(w2, std::fabs(deckGain(fs, M_4TR, LEVELS[li], FREQS[fi], [](DeckSettings& s) { s.strength = 0.5; }) - R[li * 6 + fi]));
		check(w2 <= 0.4, "3. STRENGTH 0.5 on 4TR matches the reference", fmt("worst %.2f dB", w2));
	}
	// 4. third harmonic
	checkHD3(fs);
	// 5. bias
	{
		const double* R = refArr("BIAS", fs); std::string det; bool ok = true; int k = 0;
		for (double b : {-1.0, 0.0, 1.0}) {
			double g315 = deckGain(fs, M_4TR, -30, 315, [=](DeckSettings& s) { s.bias = b; });
			double g10k = deckGain(fs, M_4TR, -20, 10000, [=](DeckSettings& s) { s.bias = b; });
			ok = ok && std::fabs(g315 - R[k]) <= 0.5 && std::fabs(g10k - R[k + 1]) <= 0.5;
			det += fmt("bias %+.0f: %.2f/%.2f (ref %.2f/%.2f)  ", b, g315, g10k, R[k], R[k + 1]); k += 2;
		}
		check(ok, "5. BIAS: 315 Hz at -30 dB and 10 kHz at -20 dB", det);
	}
	// 6. azimuth
	{
		const double* R = refArr("AZ", fs); double worst = 0; std::string det; bool ok = true; int k = 0;
		for (double az : {0.0, 0.5, 1.0}) for (double f : {5000.0, 10000.0, 14000.0}) {
			double gdb = deckGain(fs, M_4TR, -20, f, [=](DeckSettings& s) { s.azimuth = az; });
			double e = std::fabs(gdb - R[k]); double tol = R[k] < -20 ? 1.5 : 0.5;
			ok = ok && e <= tol; worst = std::max(worst, e); det += fmt("%.1f/%.0fk %.2f(%.2f) ", az, f / 1000, gdb, R[k]); k++;
		}
		check(ok, "6. AZIMUTH 0, 0.5, 1 on 4TR", det);
	}
	// 7. noise reduction
	{
		double worstRt = -999;
		for (int t : {NR_B, NR_C, NR_X2}) {
			NoiseReduction e, d; e.init(t, true, fs); d.init(t, false, fs);
			int n = (int)(2 * fs); double se = 0, sx = 0;
			for (int i = 0; i < n; i++) {
				double tt = i / fs;
				double x = 0.1 * std::sin(2 * PI * 1000 * tt) * (std::sin(2 * PI * 2 * tt) > 0 ? 1 : 0) + 0.003 * std::sin(2 * PI * 6000 * tt);
				double y = d.process(e.process(x)); se += (y - x) * (y - x); sx += x * x;
			}
			worstRt = std::max(worstRt, 10 * std::log10(se / sx + 1e-30));
		}
		check(worstRt < -60, "7. NR encode then decode returns the input (B, C, 2:1)", fmt("worst error %.1f dB", worstRt));
		const double* R = refArr("NR", fs); double worst = 0; int k = 0; std::string where;
		for (int t : {NR_X2, NR_B, NR_C}) for (double L : {-60.0, -40.0, -20.0, 0.0}) for (double f : {200.0, 1000.0, 5000.0}) {
			NoiseReduction e; e.init(t, true, fs);
			int n = (int)(2 * fs); double so = 0, si = 0;
			for (int i = 0; i < n; i++) { double x = db(L) * std::sin(2 * PI * f * i / fs), y = e.process(x); if (i >= fs) { so += y * y; si += x * x; } }
			double gdb = 10 * std::log10(so / si), er = std::fabs(gdb - R[k]);
			if (er > worst) { worst = er; where = fmt("type %d %.0f dB %.0f Hz %.2f vs %.2f", t, L, f, gdb, R[k]); }
			k++;
		}
		check(worst <= 0.3, "7. NR encode gains match the reference", fmt("worst %.2f dB (%s)", worst, where.c_str()));
	}
	// 8. latency
	{
		DeckSettings s; s.quiet = true; s.age = 0; Deck d; d.init(fs, 1, 1, 1, s);
		int n = (int)fs; int pk = 0; double best = 0;
		for (int i = 0; i < n; i++) { double x = i == 100 ? 1e-3 : 0, o; d.process(&x, &o); if (std::fabs(o) > best) { best = std::fabs(o); pk = i - 100; } }
		double L = d.latency();
		check(std::fabs(pk - L) <= 1.0, "8. latency of a quiet pass equals the documented figure", fmt("measured %d, documented %.1f samples", pk, L));
		// dry path alignment through the engine with MIX at 0.5, everything quiet-ish: compare a click's dry and wet peaks
		Engine e; e.seed = 1; e.init(fs); e.p.mix = 0.0; e.p.strength = 0; e.p.hiss = 0; e.p.wow = 0; e.p.flutter = 0; e.p.dropout = 0;
		IO io; io.inOn = true; int pkd = -1; double bd = 0;
		for (int i = 0; i < n; i++) { io.inL = io.inR = (i == 4000 ? 1.0 : 0.0); e.process(io); if (std::fabs(io.outL) > bd) { bd = std::fabs(io.outL); pkd = i - 4000; } }
		Engine e2; e2.seed = 1; e2.init(fs); e2.p = e.p; e2.p.mix = 1.0; e2.p.nrType = NR_OFF; e2.p.masterNr = false; e2.p.deskOn = true;
		int pkw = -1; double bw = 0;
		for (int i = 0; i < n; i++) { io.inL = io.inR = (i == 4000 ? 1.0 : 0.0); e2.process(io); if (std::fabs(io.outL) > bw) { bw = std::fabs(io.outL); pkw = i - 4000; } }
		check(std::abs(pkd - pkw) <= 1, "8. MIX dry path is aligned with the wet path", fmt("dry peak at %d, wet peak at %d samples", pkd, pkw));
	}
	// 9. statistics with the random sources on
	{
		DeckSettings s; s.machine = M_4TR; s.age = 0.25; s.strength = 1;
		Deck d; d.init(fs, 1, 1, 5, s);
		int n = (int)(10 * fs); double ss = 0;
		for (int i = 0; i < n; i++) { double x = 0, o; d.process(&x, &o); if (i > fs) ss += o * o; }
		double hissDb = 10 * std::log10(ss / (n - fs));
		double expect = MACHINES[M_4TR].hiss + TAPES_CASS[1].hiss + 4 * 0.25;
		// hum and the low cut change the measured level a little; the tolerance is 1 dB
		check(std::fabs(hissDb - expect) <= 1.0, "9. hiss level with no input", fmt("%.2f dB, expected %.2f", hissDb, expect));
		// speed deviation: track the phase of a 3150 Hz tone
		Deck d2; d2.init(fs, 1, 1, 9, s); d2.st.hiss = 0; d2.st.dropout = 0; d2.update(true);
		std::vector<double> y(n);
		for (int i = 0; i < n; i++) { double x = 0.3 * std::sin(2 * PI * 3150 * i / fs), o; d2.process(&x, &o); y[i] = o; }
		// zero-crossing intervals -> instantaneous frequency, weighted crudely by a 0.5-200 Hz band (RMS of deviation)
		std::vector<double> tz; for (int i = (int)fs; i + 1 < n; i++) if (y[i] <= 0 && y[i + 1] > 0) tz.push_back(i + y[i] / (y[i] - y[i + 1]));
		std::vector<double> fr; for (size_t i = 1; i < tz.size(); i++) fr.push_back(fs / (tz[i] - tz[i - 1]));
		double mean = 0; for (double v : fr) mean += v; mean /= fr.size();
		// smooth over 8 cycles (removes the scrape band and zero-crossing jitter)
		double sd = 0; int cnt = 0;
		for (size_t i = 8; i < fr.size(); i++) { double m8 = 0; for (int k = 0; k < 8; k++) m8 += fr[i - k]; m8 /= 8; sd += (m8 - mean) * (m8 - mean); cnt++; }
		double wrms = 100 * std::sqrt(sd / cnt) / mean;
		double nominal = MACHINES[M_4TR].wow * (1 + 5 * std::pow(0.25, 1.5));
		check(wrms >= 0.5 * nominal && wrms <= 2 * nominal, "9. speed deviation (RMS) within half to twice the machine's figure", fmt("%.3f %%, machine %.3f %%", wrms, nominal));
		// drop-outs: count events directly
		Deck d3; d3.init(fs, 1, 1, 11, s); d3.st.wear = 0.4; d3.update(true);
		int events = 0, prevLeft = 0;
		for (int i = 0; i < n; i++) { double x = 0, o; d3.process(&x, &o); if (d3.doLeft[0] > 0 && prevLeft == 0) events++; prevLeft = d3.doLeft[0]; }
		double expectN = d3.rate * 1.6 * 10;   // one track is an edge track
		check(events >= expectN / 2 && events <= expectN * 2, "9. drop-out count within a factor of two of the rate", fmt("%d events, expected %.1f", events, expectN));
	}
	// 10. other sample rates
	for (double r : {44100.0, 96000.0}) { checkResponse(r); checkHD3(r); }
	// 11. crosstalk
	{
		bool ok = true; std::string det;
		for (int mi : {M_4TR, M_8TR_CASS, M_8TR_75}) {
			DeckSettings s; s.machine = mi; s.quiet = true; s.age = 0;
			Deck d; d.init(fs, 4, 1, 1, s);
			int n = (int)(2 * fs); std::vector<double> y1(n), y2(n), y0(n);
			for (int i = 0; i < n; i++) { double x[4] = {0.3 * std::sin(2 * PI * 1000 * i / fs), 0, 0, 0}, o[4]; d.process(x, o); y0[i] = o[0]; y1[i] = o[1]; y2[i] = o[2]; }
			double a0 = lvl(y0, 1000, fs, n / 4), a1 = lvl(y1, 1000, fs, n / 4), a2 = lvl(y2, 1000, fs, n / 4);
			double xdb = 20 * std::log10(a1 / a0), x2 = 20 * std::log10(a2 / a0 + 1e-12);
			// the leak path is y + 4 * lowpass(200 Hz): at 1 kHz that adds about +1.6 dB to the machine's figure
			double expect = MACHINES[mi].xt + 20 * std::log10(std::abs(1.0 + 4.0 / std::complex<double>(1, 1000.0 / 200.0)));
			ok = ok && std::fabs(xdb - expect) <= 1.5 && x2 < -100;
			det += fmt("%s %.1f dB (exp %.1f), two away %.0f dB  ", MNAME[mi], xdb, expect, x2);
		}
		check(ok, "11. crosstalk to the neighbour, none two tracks away", det);
	}
	// 12. speed
	{
		auto bench = [&](int nTracks, bool in, int bounce) {
			Engine e; e.seed = 3; e.init(fs); e.p.bounce = bounce;
			IO io; io.inOn = in; io.nTracks = nTracks;
			int n = (int)(5 * fs);
			auto t0 = std::chrono::steady_clock::now();
			for (int i = 0; i < n; i++) {
				double v = 0.3 * std::sin(i * 0.05);
				io.inL = io.inR = v; for (int c = 0; c < nTracks; c++) io.tracks[c] = v;
				e.process(io);
			}
			double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			return 5.0 / secs;
		};
		double s1 = bench(0, true, 1), s2 = bench(8, false, 4);
		check(s1 >= 10, "12. stereo, both decks, faster than real time x10", fmt("x%.1f", s1));
		check(s2 >= 1, "12. eight TRACKS at BOUNCE 4 faster than real time", fmt("x%.1f", s2));
	}
	// 13. extremes and switching
	{
		bool finite = true; double worstJump = 0, sigMax = 0;
		Engine e; e.seed = 5; e.init(fs);
		IO io; io.inOn = true; io.nTracks = 3; io.masterInOn = false;
		int n = (int)(fs * 0.25);
		std::vector<std::function<void(Params&)>> ext = {
			[](Params& p) { p.strength = 1; p.age = 1; p.wear = 1; p.wow = 1; p.flutter = 1; p.dropout = 1; p.hiss = 1; p.azimuth = 1; p.bias = -1; p.recDb = 12; p.trimDb = 24; p.loDb = 15; p.hiDb = 15; p.calDb = 6; p.masterRecDb = 12; p.busOn = true; p.thrDb = -24; p.ratio = 20; p.bounce = 4; },
			[](Params& p) { p.strength = 0; p.age = 0; p.recDb = -12; p.bias = 1; p.azimuth = -1; p.loDb = -15; p.hiDb = -15; p.calDb = -6; p.masterRecDb = -12; p.loF = 1200; p.hiF = 700; },
			[](Params& p) { p.nrType = NR_C; p.nrEnc = false; p.multiMachine = 5; p.masterMachine = MM_DAT; },
			[](Params& p) { p.nrType = NR_X2; p.nrDec = false; p.multiMachine = 3; p.masterMachine = MM_BOOMBOX; p.masterTape = 2; },
			[](Params& p) { p.multiOn = false; p.deskOn = false; p.masterOn = false; },
			[](Params& p) { *&p = Params(); },
		};
		for (auto& f : ext) {
			f(e.p);
			for (int i = 0; i < n; i++) {
				double v = (i % 2 ? 12.0 : -12.0) * 0.2828 * (i < n / 2 ? 1 : 0.1);
				io.inL = io.inR = v; for (int c = 0; c < 3; c++) io.tracks[c] = v;
				e.process(io);
				if (!std::isfinite(io.outL) || !std::isfinite(io.outR) || !std::isfinite(io.mixL)) finite = false;
			}
		}
		check(finite, "13. no NaN or infinity at the extremes and +-12 V inputs", finite ? "all finite" : "non-finite output");
		// switching: machines and bypasses during a 220 Hz tone
		Engine s; s.seed = 6; s.init(fs); s.p.hiss = 0; s.p.dropout = 0;
		IO io2; io2.inOn = true; double prev = 0;
		for (int i = 0; i < (int)(6 * fs); i++) {
			int step = i / (int)(0.5 * fs);
			if (i % (int)(0.5 * fs) == 0) {
				s.p.multiMachine = step % 6; s.p.masterMachine = step % 4; s.p.deskOn = step % 3 != 1; s.p.multiOn = step % 5 != 2; s.p.bounce = 1 + step % 4; s.p.nrType = step % 4;
			}
			double x = 0.5 * std::sin(2 * PI * 220 * i / fs);
			io2.inL = io2.inR = x; s.process(io2);
			if (i > 2000) { worstJump = std::max(worstJump, std::fabs(io2.outL - prev)); sigMax = std::max(sigMax, std::fabs(io2.outL)); }
			prev = io2.outL;
		}
		// a 220 Hz sine of amplitude A moves at most 2 pi 220 A / fs per sample; allow ten times that
		double limit = 10 * 2 * PI * 220 * sigMax / fs;
		check(worstJump <= std::max(limit, 0.05), "13. switching machines and bypasses does not click", fmt("largest step %.4f, limit %.4f", worstJump, limit));
	}

	// ---------------------------------------------------------------- demo renders
	if (!stemDir.empty() && !demoDir.empty()) {
		for (const char* song : {"house", "hiphop", "techno"}) {
			std::vector<std::vector<double>> mix; double sr;
			if (!readWav(stemDir + "/" + song + "-mix.wav", mix, sr)) { printf("no stems for %s\n", song); continue; }
			std::vector<Preset> ps = presets();
			std::vector<double> L, R;
			for (size_t k = 0; k < ps.size(); k++) {
				if (std::string(song) != "house" && k != 0 && k != 2 && k != 4 && k != 6) continue;
				Params p; ps[k].set(p);
				renderStereo(p, mix[0], mix[1], sr, L, R);
				std::string name = fmt("%s/%s-%02zu-%s.wav", demoDir.c_str(), song, k + 1, ps[k].name);
				for (auto& ch : name) if (ch == ' ') ch = '-';
				writeWav(name, L, R, sr);
			}
			// STRENGTH steps on the default preset, four bars each
			if (std::string(song) == "house") {
				std::vector<double> SL, SR;
				size_t bars4 = (size_t)(4 * 4 * 60.0 / 120 * sr);
				for (double st : {0.0, 0.2, 0.4, 0.6, 0.8, 1.0}) {
					Params p; p.strength = st; renderStereo(p, mix[0], mix[1], sr, L, R);
					for (size_t i = 0; i < bars4 && i < L.size(); i++) { SL.push_back(L[i]); SR.push_back(R[i]); }
					for (int i = 0; i < (int)(0.35 * sr); i++) { SL.push_back(0); SR.push_back(0); }
				}
				writeWav(demoDir + "/house-strength-0-20-40-60-80-100.wav", SL, SR, sr);
			}
			// the TRACKS workflow: drums, bass, keys L, keys R on four tracks, mixed outside and returned to MASTER IN
			std::vector<std::vector<double>> stems;
			if (readWav(stemDir + "/" + std::string(song) + "-stems.wav", stems, sr) && stems.size() == 4) {
				Engine e; e.seed = 7; e.init(sr);
				IO io; io.nTracks = 4; io.tracksOutOn = true; io.masterInOn = true;
				size_t n = stems[0].size(); std::vector<double> oL(n), oR(n);
				double mL = 0, mR = 0;
				for (size_t i = 0; i < n; i++) {
					for (int c = 0; c < 4; c++) io.tracks[c] = stems[c][i] * 5 * 0.2828;
					io.masterL = mL; io.masterR = mR;   // the outside mixer, one sample late as in Rack
					e.process(io);
					mL = io.tracksOut[0] + io.tracksOut[1] + io.tracksOut[2];
					mR = io.tracksOut[0] + io.tracksOut[1] + io.tracksOut[3];
					oL[i] = io.outL / 0.2828 / 5; oR[i] = io.outR / 0.2828 / 5;
				}
				writeWav(demoDir + "/" + std::string(song) + "-four-tracks-mixed-in-rack.wav", oL, oR, sr);
			}
		}
		printf("demos written to %s\n", demoDir.c_str());
	}
	printf("\n%s: %d check(s) failed\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
