// Tapeizer: the whole module's signal path, without Rack. Used by the module and by the offline tests.
//
// IN L/R + TRACKS (poly) -> DESK -> MULTITRACK deck (x BOUNCE) -> mix -> MIX OUT
// MASTER IN (normalled to the mix) -> BUS -> MASTER deck or DAT -> MIX (dry/wet) -> OUTPUT
#pragma once
#include "Tape.hpp"

namespace tapeizer {

enum MasterMachine { MM_DECK = 0, MM_BOOMBOX, MM_QUARTER, MM_DAT, NUM_MM };
static const int MULTI_MACHINES[6] = {M_4TR, M_4TR_HI, M_4TR_C, M_8TR_CASS, M_8TR_75, M_8TR_15};
static const int MASTER_MACHINES[3] = {M_DECK, M_BOOMBOX, M_QUARTER};
static const int MAX_TRACKS_IN = 8;
static const int MAX_BOUNCE = 4;

struct Params {
	// global
	double strength = 0.4, age = 0.2, mix = 1, outputDb = 0;
	// desk
	bool deskOn = true; double trimDb = 0, loDb = 0, loF = 100, hiDb = 0, hiF = 10000;
	// noise reduction (multitrack)
	int nrType = NR_X2; bool nrEnc = true, nrDec = true; double calDb = 0;
	// multitrack
	bool multiOn = true; int multiMachine = 0, multiTape = 1, bounce = 1; double recDb = 0, bias = 0;
	// tape path (both decks)
	double wow = 0.5, flutter = 0.5, dropout = 0.5, hiss = 0.5, azimuth = 0, wear = 0;
	// bus
	bool busOn = false; double thrDb = -8, ratio = 4;
	// master
	bool masterOn = true; int masterMachine = MM_DECK, masterTape = 1; bool masterNr = true; double masterRecDb = 0;
	// menu
	double humHz = 50;
};

struct IO {   // one sample in and out, internal units (1.0 = 0 VU)
	int nTracks = 0; double tracks[MAX_TRACKS_IN] = {};
	bool inOn = false; double inL = 0, inR = 0;
	bool masterInOn = false; double masterL = 0, masterR = 0;
	bool tracksOutOn = false;
	// out
	double tracksOut[MAX_TRACKS_IN] = {};
	double mixL = 0, mixR = 0, outL = 0, outR = 0;
};

struct Engine {
	double fs = 0;
	uint64_t seed = 1;
	Params p;           // targets, set by the user of the engine
	Params cur;         // structural settings in force
	// smoothed continuous values
	double sStrength, sAge, sMix, sOut, sTrim, sLo, sLoF, sHi, sHiF, sCal, sRec, sBias, sWow, sFl, sDo, sHiss, sAz, sWear, sThr, sRatio, sMRec;
	double smA = 0;
	bool smInit = false;

	Desk desk[MAXT];
	Deck multi[MAX_BOUNCE];
	Deck master;
	BusComp bus;
	Dat dat;
	int C = 0;          // tracks on the multitrack tape
	bool inTracks = false;

	// mute ramp for structural changes
	enum { RUN, FADE_OUT, HOLD, FADE_IN } state = RUN;
	double wetGain = 1; int holdLeft = 0; double rampStep = 0;
	bool needInit = true;

	// dry path
	std::vector<double> dryL, dryR; int dryPos = 0, dryMask = 0;
	double latencySamples = 0;

	int ctrl = 0;
	// meters
	double multiVu = 0, masterVu = 0, multiPeak = 0, masterPeak = 0, busGr = 0;

	void init(double sampleRate) {
		fs = sampleRate;
		int sz = 1; while (sz < (int)(0.5 * fs)) sz <<= 1;
		dryL.assign(sz, 0.0); dryR.assign(sz, 0.0); dryMask = sz - 1; dryPos = 0;
		smA = 1 - std::exp(-1 / (0.02 * fs));
		rampStep = 1.0 / (0.005 * fs);
		smInit = false;
		needInit = true;
	}

	static bool structuralDiffers(const Params& a, const Params& b) {
		return a.deskOn != b.deskOn || a.nrType != b.nrType || a.nrEnc != b.nrEnc || a.nrDec != b.nrDec || a.multiOn != b.multiOn
		       || a.multiMachine != b.multiMachine || a.multiTape != b.multiTape || a.bounce != b.bounce || a.busOn != b.busOn
		       || a.masterOn != b.masterOn || a.masterMachine != b.masterMachine || a.masterTape != b.masterTape || a.masterNr != b.masterNr
		       || a.humHz != b.humHz;
	}

	DeckSettings multiSettings() const {
		DeckSettings s;
		s.machine = MULTI_MACHINES[std::max(0, std::min(5, cur.multiMachine))]; s.tape = cur.multiTape;
		s.nrEnc = cur.nrEnc ? cur.nrType : NR_OFF; s.nrDec = cur.nrDec ? cur.nrType : NR_OFF;
		s.humHz = cur.humHz;
		fillCont(s, sRec, sCal);
		return s;
	}
	DeckSettings masterSettings() const {
		DeckSettings s;
		s.machine = MASTER_MACHINES[std::max(0, std::min(2, cur.masterMachine))]; s.tape = cur.masterTape;
		s.nrEnc = s.nrDec = cur.masterNr ? NR_B : NR_OFF;
		s.humHz = cur.humHz;
		fillCont(s, sMRec, 0.0);
		s.bias = 0;
		return s;
	}
	void fillCont(DeckSettings& s, double rec, double cal) const {
		s.recDb = rec; s.calDb = cal; s.bias = sBias; s.age = sAge; s.wear = sWear; s.wow = sWow; s.flutter = sFl;
		s.azimuth = sAz; s.dropout = sDo; s.hiss = sHiss; s.strength = sStrength;
	}

	void snapSmoothers() {
		sStrength = p.strength; sAge = p.age; sMix = p.mix; sOut = p.outputDb; sTrim = p.trimDb; sLo = p.loDb; sLoF = p.loF; sHi = p.hiDb; sHiF = p.hiF;
		sCal = p.calDb; sRec = p.recDb; sBias = p.bias; sWow = p.wow; sFl = p.flutter; sDo = p.dropout; sHiss = p.hiss; sAz = p.azimuth; sWear = p.wear;
		sThr = p.thrDb; sRatio = p.ratio; sMRec = p.masterRecDb;
		smInit = true;
	}

	// Rebuild every stage for the structural settings in `cur` and the track layout.
	void rebuild(int nTracks, bool inOn) {
		C = nTracks + (inOn ? 2 : 0);
		inTracks = inOn;
		for (int c = 0; c < MAXT; c++) desk[c].init(fs);
		int nc = std::max(1, C);
		for (int b = 0; b < MAX_BOUNCE; b++) multi[b].init(fs, nc, seed, seed * 31 + 1000 + b, multiSettings());
		master.init(fs, 2, seed + 100, seed * 31 + 2000, masterSettings());
		bus.init(fs);
		dat.init(fs, seed * 31 + 3000);
		needInit = false;
	}

	double computeLatency() const {
		double L = 0;
		if (cur.deskOn) L += HalfBand::M - 0.5;
		if (cur.multiOn) for (int b = 0; b < cur.bounce; b++) L += multi[b].latency();
		if (cur.masterOn && cur.masterMachine != MM_DAT) L += master.latency();
		return L;
	}

	void process(IO& io) {
		if (!smInit) snapSmoothers();
		int nT = std::max(0, std::min(MAX_TRACKS_IN, io.nTracks));
		int wantC = nT + (io.inOn ? 2 : 0);
		bool structChange = structuralDiffers(p, cur) || wantC != C || io.inOn != inTracks;
		if (needInit) { cur = p; rebuild(nT, io.inOn); state = RUN; wetGain = 1; structChange = false; }
		if (structChange && state == RUN) state = FADE_OUT;
		if (state == FADE_OUT) {
			wetGain -= rampStep;
			if (wetGain <= 0) {
				wetGain = 0; cur = p; rebuild(nT, io.inOn);
				holdLeft = (int)computeLatency() + 2; state = HOLD;
			}
		} else if (state == HOLD) {
			if (--holdLeft <= 0) state = FADE_IN;
		} else if (state == FADE_IN) {
			wetGain += rampStep;
			if (wetGain >= 1) { wetGain = 1; state = RUN; }
			if (structChange) state = FADE_OUT;
		}
		// layout guard: never index past the built layout while waiting for the fade
		const bool layoutOk = (wantC == C && io.inOn == inTracks);

		// smoothing
		const double a = smA;
		sStrength += a * (p.strength - sStrength); sAge += a * (p.age - sAge); sMix += a * (p.mix - sMix); sOut += a * (p.outputDb - sOut);
		sTrim += a * (p.trimDb - sTrim); sLo += a * (p.loDb - sLo); sLoF += a * (p.loF - sLoF); sHi += a * (p.hiDb - sHi); sHiF += a * (p.hiF - sHiF);
		sCal += a * (p.calDb - sCal); sRec += a * (p.recDb - sRec); sBias += a * (p.bias - sBias); sWow += a * (p.wow - sWow); sFl += a * (p.flutter - sFl);
		sDo += a * (p.dropout - sDo); sHiss += a * (p.hiss - sHiss); sAz += a * (p.azimuth - sAz); sWear += a * (p.wear - sWear);
		sThr += a * (p.thrDb - sThr); sRatio += a * (p.ratio - sRatio); sMRec += a * (p.masterRecDb - sMRec);

		if (++ctrl >= 32) {
			ctrl = 0;
			DeckSettings ms = multiSettings(), ma = masterSettings();
			for (int b = 0; b < MAX_BOUNCE; b++) { copyCont(multi[b].st, ms); multi[b].update(); }
			copyCont(master.st, ma); master.update();
			for (int c = 0; c < MAXT; c++) desk[c].set(sLo, sLoF, sHi, sHiF);
			latencySamples = computeLatency();
		}

		// ---- dry path (IN L/R, delayed by the current latency)
		double dl = io.inOn ? io.inL : 0, dr = io.inOn ? io.inR : 0;
		dryPos = (dryPos + 1) & dryMask; dryL[dryPos] = dl; dryR[dryPos] = dr;
		double dd = std::min(latencySamples, (double)dryMask - 2);
		int di = (int)dd; double df = dd - di;
		double dryOutL = (1 - df) * dryL[(dryPos - di) & dryMask] + df * dryL[(dryPos - di - 1) & dryMask];
		double dryOutR = (1 - df) * dryR[(dryPos - di) & dryMask] + df * dryR[(dryPos - di - 1) & dryMask];

		double mixL = 0, mixR = 0;
		if (layoutOk && C > 0) {
			double t[MAXT];
			for (int c = 0; c < nT; c++) t[c] = io.tracks[c];
			if (inTracks) { t[nT] = io.inL; t[nT + 1] = io.inR; }
			if (cur.deskOn) { double tg = db(sTrim); for (int c = 0; c < C; c++) t[c] = desk[c].process(t[c], tg); }
			if (cur.multiOn) {
				for (int b = 0; b < cur.bounce; b++) { double y[MAXT]; multi[b].process(t, y); for (int c = 0; c < C; c++) t[c] = y[c]; }
			}
			for (int c = 0; c < nT; c++) io.tracksOut[c] = t[c] * wetGain;
			if (!io.tracksOutOn) for (int c = 0; c < nT; c++) { mixL += t[c]; mixR += t[c]; }
			if (inTracks) { mixL += t[nT]; mixR += t[nT + 1]; }
		} else {
			for (int c = 0; c < nT; c++) io.tracksOut[c] = 0;
		}
		mixL *= wetGain; mixR *= wetGain;
		io.mixL = mixL; io.mixR = mixR;

		// ---- master section
		double ml = io.masterInOn ? io.masterL : mixL, mr = io.masterInOn ? io.masterR : mixR;
		if (cur.busOn) { bus.process(ml, mr, sThr, std::max(1.0, sRatio)); busGr = bus.gr; } else busGr = 0;
		if (cur.masterOn) {
			if (cur.masterMachine == MM_DAT) { ml = dat.process(0, ml); mr = dat.process(1, mr); }
			else { double x[2] = {ml, mr}, y[2]; master.process(x, y); ml = y[0]; mr = y[1]; }
		}
		ml *= wetGain; mr *= wetGain;
		const double og = db(sOut);
		io.outL = (sMix * ml + (1 - sMix) * dryOutL) * og;
		io.outR = (sMix * mr + (1 - sMix) * dryOutR) * og;

		// meters
		multiVu = cur.multiOn ? multi[0].vu() : 0;
		masterVu = (cur.masterOn && cur.masterMachine != MM_DAT) ? master.vu() : 0;
		if (cur.multiOn) multiPeak = std::max(multiPeak, multi[0].takePeak());
		if (cur.masterOn && cur.masterMachine != MM_DAT) masterPeak = std::max(masterPeak, master.takePeak());
	}
	static void copyCont(DeckSettings& d, const DeckSettings& s) {
		d.recDb = s.recDb; d.calDb = s.calDb; d.bias = s.bias; d.age = s.age; d.wear = s.wear; d.wow = s.wow; d.flutter = s.flutter;
		d.azimuth = s.azimuth; d.dropout = s.dropout; d.hiss = s.hiss; d.strength = s.strength;
	}
	double takeMultiPeak() { double v = multiPeak; multiPeak = 0; return v; }
	double takeMasterPeak() { double v = masterPeak; masterPeak = 0; return v; }
};

}  // namespace tapeizer
