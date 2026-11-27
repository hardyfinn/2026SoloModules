// Facet factory presets: 96 original patches in 12 groups of 8, written with a small builder.
// Names, data and sounds are our own; the styles follow the early-90s digital-synth palette.
#pragma once
#include "FacetParams.hpp"
#include <vector>

namespace facet {

struct Preset {
	std::string name, cat;
	std::vector<float> v;
};

struct PB {
	float v[NPV];
	PB() {
		patchDefaults(v);
		v[cp(C_CH_ON)] = 0.f;
		v[cp(C_RV_ON)] = 0.f;
	}
	static int W(const char* name) {
		const Rom& rom = Rom::get();
		for (int i = 0; i < rom.size(); i++)
			if (rom.waves[i].name == name) return i;
		return 0;
	}
	void s(int t, int p, float x) { v[tp(t, p)] = clampTo(tp(t, p), x); }
	void c(int p, float x) { v[cp(p)] = clampTo(cp(p), x); }
	// a tone on, with its wave and level
	void on(int t, const char* wave, float level = 100.f, float coarse = 0.f, float fine = 0.f) {
		s(t, T_ON, 1);
		s(t, T_WAVE, (float) W(wave));
		s(t, T_LEVEL, level);
		s(t, T_COARSE, coarse);
		s(t, T_FINE, fine);
	}
	// amp envelope as attack / decay / sustain / release
	void amp(int t, float a, float d, float sus, float r) {
		s(t, T_AE_T1, a);
		s(t, T_AE_L1, 100);
		s(t, T_AE_T2, d);
		s(t, T_AE_L2, sus);
		s(t, T_AE_T3, 0);
		s(t, T_AE_SUS, sus);
		s(t, T_AE_T4, r);
	}
	// swell: rises in two stages (JD pads often bloom after the attack)
	void swell(int t, float a, float bloom, float sus, float r) {
		s(t, T_AE_T1, a);
		s(t, T_AE_L1, 70);
		s(t, T_AE_T2, bloom);
		s(t, T_AE_L2, 100);
		s(t, T_AE_T3, 70);
		s(t, T_AE_SUS, sus);
		s(t, T_AE_T4, r);
	}
	void filt(int t, int mode, float cut, float res, float env = 0.f, float kf = 50.f) {
		s(t, T_F_MODE, mode);
		s(t, T_F_CUT, cut);
		s(t, T_F_RES, res);
		s(t, T_F_ENV, env);
		s(t, T_F_KF, kf);
	}
	void fenv(int t, float a, float d, float sus, float r, float peak = 50.f) {
		s(t, T_FE_T1, a);
		s(t, T_FE_L1, peak);
		s(t, T_FE_T2, d);
		s(t, T_FE_L2, sus);
		s(t, T_FE_T3, 0);
		s(t, T_FE_SUS, sus);
		s(t, T_FE_T4, r);
		s(t, T_FE_L4, 0);
	}
	void penv(int t, float depth, float l0, float t1, float l1 = 0.f, float t2 = 0.f) {
		s(t, T_PE_DEPTH, depth);
		s(t, T_PE_L0, l0);
		s(t, T_PE_T1, t1);
		s(t, T_PE_L1, l1);
		s(t, T_PE_T2, t2);
	}
	void lfo(int t, int k, float rate, int wave, float pitch, float tvf = 0.f, float tva = 0.f, float delay = 0.f, float fade = 0.f) {
		int b = T_L1_RATE + k * LFO_STRIDE;
		s(t, b, rate);
		s(t, b + 1, delay);
		s(t, b + 2, fade);
		s(t, b + 3, wave);
		s(t, b + 6, pitch);
		s(t, b + 7, tvf);
		s(t, b + 8, tva);
	}
	void velo(int t, float sens, float fsens = 0.f) {
		s(t, T_VSENS, sens);
		s(t, T_FE_VELO, fsens);
	}
	void pan(int t, float p) { s(t, T_PAN, p); }
	void keys(int t, float lo, float hi) {
		s(t, T_KEYLO, lo);
		s(t, T_KEYHI, hi);
	}
	void rev(int type, float time, float level, float pre = 10.f, float hfd = 12.f) {
		c(C_RV_ON, 1);
		c(C_RV_TYPE, type);
		c(C_RV_TIME, time);
		c(C_RV_LEVEL, level);
		c(C_RV_PRE, pre);
		c(C_RV_HFD, hfd);
	}
	void cho(float rate, float depth, float level, float delay = 70.f, float fb = 0.f) {
		c(C_CH_ON, 1);
		c(C_CH_RATE, rate);
		c(C_CH_DEPTH, depth);
		c(C_CH_LEVEL, level);
		c(C_CH_DELAY, delay);
		c(C_CH_FB, fb);
	}
	void dly(float cms, float cl, float lms, float ll, float rms, float rl, float fb) {
		c(C_DL_ON, 1);
		c(C_DL_CTIME, cms);
		c(C_DL_CLEV, cl);
		c(C_DL_LTIME, lms);
		c(C_DL_LLEV, ll);
		c(C_DL_RTIME, rms);
		c(C_DL_RLEV, rl);
		c(C_DL_FB, fb);
	}
	void dist(int type, float drive, float level) {
		c(C_DS_ON, 1);
		c(C_DS_TYPE, type);
		c(C_DS_DRIVE, drive);
		c(C_DS_LEVEL, level);
	}
	void phs(float man, float rate, float depth, float reso, float mix = 50.f) {
		c(C_PH_ON, 1);
		c(C_PH_MAN, man);
		c(C_PH_RATE, rate);
		c(C_PH_DEPTH, depth);
		c(C_PH_RESO, reso);
		c(C_PH_MIX, mix);
	}
	void spec(float b1, float b2, float b3, float b4, float b5, float b6, float width = 3.f) {
		c(C_SP_ON, 1);
		float b[6] = {b1, b2, b3, b4, b5, b6};
		for (int i = 0; i < 6; i++) c(C_SP_B1 + i, b[i]);
		c(C_SP_WIDTH, width);
	}
	void enh(float sens, float mix) {
		c(C_EH_ON, 1);
		c(C_EH_SENS, sens);
		c(C_EH_MIX, mix);
	}
	void eq(float lo, float mid, float hi, float midf = 50.f) {
		c(C_EQ_LOG, lo);
		c(C_EQ_MIDG, mid);
		c(C_EQ_HIG, hi);
		c(C_EQ_MIDF, midf);
	}
	void mono(float porta = 0.f) {
		c(C_SOLO, 1);
		c(C_LEGATO, 1);
		if (porta > 0.f) {
			c(C_PORTA, 1);
			c(C_PTIME, porta);
		}
	}
};

typedef void (*Build)(PB&);
struct Recipe {
	const char* name;
	const char* cat;
	Build b;
};

// ======================================================================= 1 GLASS PADS
static void p_glassCathedral(PB& p) {
	p.on(0, "Glass Pad");
	p.swell(0, 55, 62, 90, 70);
	p.filt(0, 0, 78, 10, 10);
	p.on(1, "Breath Chiff", 70, 12);
	p.amp(1, 0, 55, 0, 40);
	p.on(2, "Crystal", 55, 12, 6);
	p.swell(2, 65, 70, 70, 72);
	p.lfo(2, 0, 35, 0, 0, 10, 15);
	p.pan(0, -20);
	p.pan(2, 25);
	p.cho(22, 45, 50);
	p.rev(4, 70, 45, 30);
}
static void p_iceField(PB& p) {
	p.on(0, "Glass", 90);
	p.amp(0, 45, 60, 85, 70);
	p.filt(0, 0, 70, 20, 15);
	p.fenv(0, 55, 70, 20, 60);
	p.on(1, "Glass", 80, 0, 9);
	p.amp(1, 50, 60, 85, 72);
	p.filt(1, 0, 72, 20, 12);
	p.pan(0, -30);
	p.pan(1, 30);
	p.cho(18, 50, 55);
	p.rev(5, 72, 40, 20);
}
static void p_crystalHalo(PB& p) {
	p.on(0, "Crystal", 85);
	p.swell(0, 40, 65, 80, 70);
	p.on(1, "Shimmer", 80, 12);
	p.swell(1, 60, 70, 85, 75);
	p.filt(1, 0, 85, 0);
	p.on(2, "Glass Ping", 60, 24);
	p.amp(2, 0, 70, 0, 60);
	p.cho(25, 40, 50);
	p.dly(375, 25, 250, 15, 500, 15, 35);
	p.rev(4, 75, 45, 25);
}
static void p_prismPad(PB& p) {
	p.on(0, "Spectrum Swp", 100);
	p.swell(0, 50, 70, 90, 68);
	p.filt(0, 0, 75, 25, 10);
	p.on(1, "Glass Pad", 70, 7);
	p.swell(1, 60, 72, 80, 70);
	p.pan(0, -25);
	p.pan(1, 25);
	p.phs(45, 18, 60, 40, 45);
	p.cho(20, 40, 45);
	p.rev(3, 68, 40);
}
static void p_frozenLake(PB& p) {
	p.on(0, "Glass Pad", 95, -12);
	p.swell(0, 65, 75, 95, 78);
	p.filt(0, 0, 62, 15, 15);
	p.fenv(0, 75, 80, 30, 70);
	p.on(1, "Wind", 35, 0);
	p.amp(1, 70, 0, 100, 75);
	p.filt(1, 1, 70, 40, 0);
	p.lfo(1, 0, 25, 4, 0, 20, 0);
	p.on(2, "Crystal", 55, 12);
	p.swell(2, 70, 75, 70, 78);
	p.rev(5, 82, 50, 40, 10);
}
static void p_angelGlass(PB& p) {
	p.on(0, "Choir Pad", 85);
	p.swell(0, 55, 65, 90, 70);
	p.on(1, "Glass", 75, 12);
	p.amp(1, 40, 60, 75, 70);
	p.filt(1, 0, 80, 10, 0);
	p.lfo(0, 0, 40, 0, 4, 0, 0, 40, 30);
	p.pan(0, -20);
	p.pan(1, 20);
	p.cho(22, 45, 50);
	p.rev(4, 74, 45, 25);
}
static void p_digitalDawn(PB& p) {
	p.on(0, "Digital Pad", 90);
	p.swell(0, 60, 72, 90, 70);
	p.filt(0, 0, 55, 25, 30);
	p.fenv(0, 72, 80, 40, 70);
	p.on(1, "Glass Pad", 70, 12);
	p.swell(1, 70, 75, 80, 74);
	p.pan(0, -15);
	p.pan(1, 15);
	p.cho(18, 50, 50);
	p.rev(3, 70, 40, 20);
	p.c(C_DAC90, 1);
}
static void p_quartzBloom(PB& p) {
	p.on(0, "Bell Tone", 80);
	p.amp(0, 0, 70, 40, 70);
	p.on(1, "Glass Pad", 85);
	p.swell(1, 62, 70, 90, 72);
	p.on(2, "Hollow Bell", 60, 12, -7);
	p.amp(2, 0, 72, 30, 72);
	p.pan(1, -20);
	p.pan(2, 25);
	p.cho(20, 40, 45);
	p.rev(4, 75, 42, 20);
}

// ======================================================================= 2 DIGITAL CHOIRS
static void p_cathedralVox(PB& p) {
	p.on(0, "Choir Pad");
	p.swell(0, 55, 65, 90, 70);
	p.filt(0, 0, 72, 5, 0);
	p.on(1, "Vox Ah", 75, 12, 5);
	p.swell(1, 62, 70, 80, 72);
	p.lfo(1, 0, 42, 0, 5, 0, 0, 45, 30);
	p.pan(0, -25);
	p.pan(1, 25);
	p.cho(20, 50, 55);
	p.rev(5, 76, 45, 25);
}
static void p_breathingChoir(PB& p) {
	p.on(0, "Vox Morph", 90);
	p.swell(0, 60, 70, 90, 70);
	p.on(1, "Breath Noise", 35);
	p.amp(1, 55, 0, 100, 65);
	p.filt(1, 1, 75, 30, 0);
	p.on(2, "Choir Pad", 75, -12);
	p.swell(2, 65, 72, 85, 72);
	p.cho(18, 45, 50);
	p.rev(4, 74, 45, 30);
}
static void p_robotMonks(PB& p) {
	p.on(0, "Vox Oh", 90, -12);
	p.amp(0, 40, 0, 100, 60);
	p.on(1, "Vox Oo", 80);
	p.amp(1, 45, 0, 100, 62);
	p.lfo(0, 0, 30, 0, 3);
	p.c(C_DAC90, 1);
	p.rev(2, 65, 40);
	p.cho(15, 30, 40);
}
static void p_kaChant(PB& p) {
	p.on(0, "Vox Ka", 90);
	p.amp(0, 0, 60, 0, 40);
	p.on(1, "Vox Ah", 85);
	p.amp(1, 30, 60, 75, 55);
	p.filt(1, 0, 70, 10, 15);
	p.fenv(1, 20, 55, 10, 50);
	p.cho(25, 35, 40);
	p.rev(3, 60, 35);
}
static void p_airVoices(PB& p) {
	p.on(0, "Vox Ee", 70, 12);
	p.swell(0, 60, 70, 85, 70);
	p.on(1, "Breath Pad", 90);
	p.swell(1, 55, 65, 90, 70);
	p.on(2, "Hiss", 25);
	p.amp(2, 50, 0, 100, 65);
	p.filt(2, 2, 75, 0, 0);
	p.lfo(0, 0, 35, 0, 3, 0, 15);
	p.cho(22, 45, 50);
	p.rev(5, 80, 50, 35);
}
static void p_vocoderish(PB& p) {
	p.on(0, "Nasal", 90);
	p.amp(0, 25, 0, 100, 45);
	p.filt(0, 1, 62, 45, 20);
	p.lfo(0, 0, 55, 3, 0, 15);
	p.on(1, "Vox Morph", 70);
	p.amp(1, 25, 0, 100, 45);
	p.c(C_DAC90, 1);
	p.cho(20, 40, 40);
	p.rev(2, 55, 30);
}
static void p_lowChoir(PB& p) {
	p.on(0, "Choir Pad", 95, -12);
	p.swell(0, 60, 70, 90, 72);
	p.filt(0, 0, 60, 5, 0);
	p.on(1, "Vox Oo", 70, -12, 6);
	p.swell(1, 60, 70, 85, 72);
	p.pan(0, -20);
	p.pan(1, 20);
	p.cho(15, 45, 50);
	p.rev(5, 80, 45, 30, 9);
}
static void p_choirStab(PB& p) {
	p.on(0, "Choir Pad");
	p.amp(0, 0, 55, 0, 50);
	p.on(1, "Vox Ka", 70, 12);
	p.amp(1, 0, 50, 0, 40);
	p.on(2, "Orch Stab", 60);
	p.amp(2, 0, 55, 0, 45);
	p.rev(3, 60, 40, 15);
}

// ======================================================================= 3 STRINGS
static void p_jdStrings(PB& p) {
	p.on(0, "String Ens");
	p.amp(0, 50, 0, 100, 62);
	p.filt(0, 0, 78, 5, 0);
	p.on(1, "Bow Scrape", 45);
	p.amp(1, 30, 55, 0, 40);
	p.on(2, "Fat Saw", 70, 0, 8);
	p.amp(2, 52, 0, 100, 62);
	p.filt(2, 0, 66, 0, 0);
	p.pan(0, -25);
	p.pan(2, 25);
	p.cho(22, 50, 55);
	p.rev(4, 68, 38, 20);
}
static void p_digiStrings(PB& p) {
	p.on(0, "Digi String");
	p.amp(0, 45, 0, 100, 60);
	p.lfo(0, 0, 50, 0, 3, 0, 0, 50, 30);
	p.on(1, "Digi String", 85, 0, -9);
	p.amp(1, 48, 0, 100, 60);
	p.pan(0, -35);
	p.pan(1, 35);
	p.cho(20, 45, 45);
	p.rev(3, 65, 35);
}
static void p_slowStrings(PB& p) {
	p.on(0, "String Ens");
	p.amp(0, 68, 0, 100, 72);
	p.filt(0, 0, 60, 10, 20);
	p.fenv(0, 72, 80, 35, 70);
	p.on(1, "String Ens", 75, 12, 5);
	p.amp(1, 72, 0, 100, 72);
	p.filt(1, 0, 62, 5, 0);
	p.cho(18, 45, 50);
	p.rev(5, 78, 42, 30);
}
static void p_pizzEnsemble(PB& p) {
	p.on(0, "Pizz");
	p.amp(0, 0, 60, 0, 45);
	p.on(1, "String Ens", 55);
	p.amp(1, 0, 55, 0, 45);
	p.filt(1, 0, 60, 0, 25);
	p.fenv(1, 0, 50, 0, 40);
	p.rev(3, 55, 35);
}
static void p_syntheticOrchestra(PB& p) {
	p.on(0, "String Ens");
	p.amp(0, 40, 0, 100, 60);
	p.on(1, "Brass Swell", 70);
	p.amp(1, 45, 0, 100, 58);
	p.filt(1, 0, 65, 10, 20);
	p.fenv(1, 45, 65, 25, 55);
	p.on(2, "Timpani", 55, -12);
	p.amp(2, 0, 65, 0, 55);
	p.keys(2, 0, 59);
	p.cho(20, 40, 45);
	p.rev(4, 70, 40, 20);
}
static void p_tremoloStrings(PB& p) {
	p.on(0, "String Ens");
	p.amp(0, 35, 0, 100, 58);
	p.lfo(0, 0, 66, 0, 0, 0, 35);
	p.on(1, "Digi String", 70, 12);
	p.amp(1, 40, 0, 100, 58);
	p.lfo(1, 0, 66, 0, 0, 0, 35);
	p.cho(20, 40, 40);
	p.rev(3, 62, 35);
}
static void p_cinemaStrings(PB& p) {
	p.on(0, "String Ens", 100, -12);
	p.swell(0, 66, 74, 95, 76);
	p.on(1, "String Ens", 90, 0, 7);
	p.swell(1, 66, 74, 95, 76);
	p.on(2, "Choir Pad", 50, 12);
	p.swell(2, 72, 78, 80, 78);
	p.pan(0, -30);
	p.pan(1, 30);
	p.cho(18, 45, 50);
	p.rev(5, 80, 45, 30);
}
static void p_bowedGlass(PB& p) {
	p.on(0, "Bow Scrape", 60);
	p.amp(0, 30, 60, 0, 40);
	p.on(1, "Glass Pad");
	p.amp(1, 55, 0, 100, 68);
	p.on(2, "String Ens", 70);
	p.amp(2, 58, 0, 100, 68);
	p.filt(2, 0, 64, 10);
	p.cho(22, 45, 50);
	p.rev(4, 74, 42, 25);
}

// ======================================================================= 4 SPECTRAL BRASS
static void p_spectralBrass(PB& p) {
	p.on(0, "Brass Blat");
	p.amp(0, 0, 55, 0, 40);
	p.on(1, "Digi Brass");
	p.amp(1, 28, 0, 100, 45);
	p.filt(1, 0, 52, 15, 30);
	p.fenv(1, 30, 60, 15, 45);
	p.velo(1, 25, 30);
	p.on(2, "Brass Swell", 80, 0, 7);
	p.amp(2, 30, 0, 100, 45);
	p.filt(2, 0, 55, 10, 25);
	p.fenv(2, 32, 62, 15, 45);
	p.cho(20, 35, 40);
	p.rev(3, 60, 30, 15);
}
static void p_fanfare(PB& p) {
	p.on(0, "Digi Brass");
	p.amp(0, 20, 0, 100, 40);
	p.filt(0, 0, 50, 20, 35);
	p.fenv(0, 25, 55, 20, 40);
	p.penv(0, 1, -50, 30);
	p.on(1, "Digi Brass", 85, 12, 6);
	p.amp(1, 22, 0, 100, 40);
	p.filt(1, 0, 55, 15, 30);
	p.fenv(1, 25, 55, 20, 40);
	p.cho(20, 35, 40);
	p.rev(4, 62, 32, 15);
}
static void p_softHorns(PB& p) {
	p.on(0, "Brass Swell");
	p.amp(0, 45, 0, 100, 55);
	p.filt(0, 0, 50, 5, 20);
	p.fenv(0, 50, 70, 30, 55);
	p.on(1, "Soft Saw", 70, -12);
	p.amp(1, 45, 0, 100, 55);
	p.filt(1, 0, 45, 0, 0);
	p.cho(18, 40, 45);
	p.rev(4, 66, 35, 20);
}
static void p_brassStab90(PB& p) {
	p.on(0, "Orch Stab");
	p.amp(0, 0, 55, 0, 40);
	p.on(1, "Digi Brass");
	p.amp(1, 10, 55, 30, 40);
	p.filt(1, 0, 45, 25, 40);
	p.fenv(1, 15, 52, 10, 40);
	p.c(C_DAC90, 1);
	p.rev(3, 55, 30);
}
static void p_synthBrassFat(PB& p) {
	p.on(0, "Fat Saw");
	p.amp(0, 25, 0, 100, 45);
	p.filt(0, 0, 45, 20, 38);
	p.fenv(0, 28, 60, 15, 45);
	p.on(1, "Fat Saw", 90, 0, -10);
	p.amp(1, 25, 0, 100, 45);
	p.filt(1, 0, 45, 20, 38);
	p.fenv(1, 30, 60, 15, 45);
	p.pan(0, -30);
	p.pan(1, 30);
	p.cho(20, 40, 40);
	p.rev(3, 55, 28);
}
static void p_tubaGlass(PB& p) {
	p.on(0, "Brass Swell", 100, -12);
	p.amp(0, 35, 0, 100, 50);
	p.filt(0, 0, 45, 10, 25);
	p.fenv(0, 40, 60, 20, 50);
	p.on(1, "Glass", 50, 12);
	p.amp(1, 40, 0, 100, 55);
	p.cho(18, 35, 40);
	p.rev(4, 64, 32);
}
static void p_wahBrass(PB& p) {
	p.on(0, "Digi Brass");
	p.amp(0, 15, 0, 100, 40);
	p.filt(0, 1, 50, 50, 30);
	p.fenv(0, 35, 60, 10, 40);
	p.lfo(0, 0, 45, 0, 0, 12, 0, 40, 30);
	p.on(1, "Brass Blat", 70);
	p.amp(1, 0, 50, 0, 35);
	p.rev(3, 55, 30);
}
static void p_brassSection(PB& p) {
	p.on(0, "Brass Blat");
	p.amp(0, 0, 55, 0, 40);
	p.on(1, "Brass Swell");
	p.amp(1, 30, 0, 100, 45);
	p.filt(1, 0, 52, 10, 30);
	p.fenv(1, 30, 60, 15, 45);
	p.lfo(1, 0, 52, 0, 4, 0, 0, 50, 30);
	p.on(2, "Digi Brass", 70, 12, 5);
	p.amp(2, 32, 0, 100, 45);
	p.filt(2, 0, 50, 10, 25);
	p.on(3, "Digi Brass", 60, -12, -5);
	p.amp(3, 30, 0, 100, 45);
	p.filt(3, 0, 45, 10, 20);
	p.cho(20, 35, 40);
	p.rev(4, 64, 32, 15);
}

// ======================================================================= 5 BELLS AND MALLETS
static void p_crystalBells(PB& p) {
	p.on(0, "Bell Strike");
	p.amp(0, 0, 78, 0, 72);
	p.on(1, "Glass", 60, 12);
	p.amp(1, 0, 72, 20, 70);
	p.lfo(1, 0, 55, 0, 0, 0, 10);
	p.cho(22, 40, 40);
	p.rev(4, 72, 40, 20);
}
static void p_tubularDigi(PB& p) {
	p.on(0, "Digi Bell");
	p.amp(0, 0, 80, 0, 75);
	p.on(1, "Metal Hit", 60);
	p.amp(1, 0, 70, 0, 65);
	p.on(2, "Hollow Bell", 60, -12);
	p.amp(2, 0, 82, 0, 76);
	p.rev(5, 75, 40, 20);
}
static void p_marimbaGlass(PB& p) {
	p.on(0, "Mallet");
	p.amp(0, 0, 65, 0, 55);
	p.velo(0, 40);
	p.on(1, "Glass Ping", 50, 12);
	p.amp(1, 0, 60, 0, 50);
	p.rev(2, 55, 30);
}
static void p_vibraphone(PB& p) {
	p.on(0, "Vibe");
	p.amp(0, 0, 80, 0, 70);
	p.lfo(0, 0, 62, 0, 0, 0, 30);
	p.on(1, "Sine", 40, 12);
	p.amp(1, 0, 75, 0, 70);
	p.lfo(1, 0, 62, 0, 0, 0, 30);
	p.cho(18, 30, 30);
	p.rev(3, 62, 35);
}
static void p_kalimbaDream(PB& p) {
	p.on(0, "Kalimba");
	p.amp(0, 0, 70, 0, 60);
	p.velo(0, 40);
	p.on(1, "Glass Pad", 40, 12);
	p.amp(1, 30, 72, 30, 70);
	p.dly(375, 25, 250, 20, 500, 20, 35);
	p.rev(4, 70, 38, 20);
}
static void p_steelSun(PB& p) {
	p.on(0, "Steel Drum");
	p.amp(0, 0, 72, 0, 62);
	p.velo(0, 40);
	p.on(1, "Harm 3 5", 40);
	p.amp(1, 0, 60, 0, 55);
	p.filt(1, 0, 70, 20, 20);
	p.fenv(1, 0, 50, 0, 40);
	p.rev(2, 55, 30);
}
static void p_fmBells(PB& p) {
	p.on(0, "FM Clang");
	p.amp(0, 0, 76, 0, 70);
	p.on(1, "FM Tine", 70, 12);
	p.amp(1, 0, 68, 0, 64);
	p.on(2, "Bell Tone", 50, -12, 4);
	p.amp(2, 0, 82, 0, 74);
	p.cho(20, 35, 35);
	p.rev(4, 70, 38, 15);
}
static void p_glockenSpark(PB& p) {
	p.on(0, "Glass Ping", 100, 24);
	p.amp(0, 0, 70, 0, 62);
	p.on(1, "Spark", 40, 12);
	p.amp(1, 0, 40, 0, 30);
	p.on(2, "Crystal", 45, 12);
	p.amp(2, 0, 65, 0, 60);
	p.rev(4, 68, 40);
}

// ======================================================================= 6 PLUCKS AND KEYS
static void p_digitalEP(PB& p) {
	p.on(0, "EP Atk");
	p.amp(0, 0, 75, 0, 55);
	p.velo(0, 45);
	p.on(1, "FM Tine", 60);
	p.amp(1, 0, 72, 25, 55);
	p.velo(1, 35);
	p.lfo(1, 0, 55, 0, 0, 0, 20);
	p.cho(22, 40, 45);
	p.rev(2, 55, 28);
}
static void p_glassHarp(PB& p) {
	p.on(0, "Harp");
	p.amp(0, 0, 75, 0, 60);
	p.velo(0, 40);
	p.on(1, "Glass", 50, 12);
	p.amp(1, 0, 65, 0, 60);
	p.rev(4, 68, 40, 20);
}
static void p_pluckedSpectrum(PB& p) {
	p.on(0, "Pluck");
	p.amp(0, 0, 65, 0, 45);
	p.on(1, "Spectrum 1", 70);
	p.amp(1, 0, 55, 0, 45);
	p.filt(1, 0, 45, 30, 40);
	p.fenv(1, 0, 48, 0, 40);
	p.velo(1, 30, 30);
	p.dly(375, 25, 250, 15, 500, 15, 30);
	p.rev(2, 55, 30);
}
static void p_houseKeys(PB& p) {
	p.on(0, "Organ 8 4");
	p.amp(0, 0, 55, 0, 35);
	p.on(1, "Piano Atk", 70);
	p.amp(1, 0, 60, 0, 40);
	p.on(2, "FM Bright", 45);
	p.amp(2, 0, 50, 0, 35);
	p.filt(2, 0, 60, 10, 20);
	p.fenv(2, 0, 45, 0, 30);
	p.rev(2, 50, 25);
	p.cho(25, 30, 30);
}
static void p_clavinette(PB& p) {
	p.on(0, "Clav Atk");
	p.amp(0, 0, 55, 0, 30);
	p.velo(0, 45);
	p.on(1, "Pulse 12", 60);
	p.amp(1, 0, 50, 20, 30);
	p.filt(1, 0, 45, 35, 35);
	p.fenv(1, 0, 45, 0, 30);
	p.velo(1, 35, 40);
	p.phs(40, 30, 40, 30, 40);
}
static void p_pianoGlass(PB& p) {
	p.on(0, "Piano Atk");
	p.amp(0, 0, 75, 0, 55);
	p.velo(0, 45);
	p.on(1, "Hammer", 50);
	p.amp(1, 0, 40, 0, 30);
	p.on(2, "Glass", 35, 12);
	p.amp(2, 0, 70, 10, 55);
	p.rev(3, 60, 30);
}
static void p_kotoDigi(PB& p) {
	p.on(0, "Pluck", 100, 12);
	p.amp(0, 0, 60, 0, 40);
	p.penv(0, 1, 50, 30);
	p.on(1, "Nasal", 60, 12);
	p.amp(1, 0, 55, 0, 40);
	p.filt(1, 0, 55, 20, 30);
	p.fenv(1, 0, 45, 0, 35);
	p.dly(250, 25, 375, 15, 500, 15, 30);
	p.rev(2, 55, 28);
}
static void p_muteGuitar(PB& p) {
	p.on(0, "Pizz");
	p.amp(0, 0, 50, 0, 30);
	p.on(1, "Saw Square", 60);
	p.amp(1, 0, 45, 0, 30);
	p.filt(1, 0, 42, 25, 30);
	p.fenv(1, 0, 40, 0, 30);
	p.dist(0, 30, 55);
	p.cho(25, 35, 35);
	p.rev(1, 45, 25);
}

// ======================================================================= 7 ORGANS
static void p_digiOrgan(PB& p) {
	p.on(0, "Organ Full");
	p.amp(0, 0, 0, 100, 25);
	p.on(1, "Wood Knock", 40, 24);
	p.amp(1, 0, 35, 0, 25);
	p.cho(55, 35, 40, 70);
	p.rev(2, 50, 25);
}
static void p_leslieDrawbars(PB& p) {
	p.on(0, "Organ Leslie");
	p.amp(0, 0, 0, 100, 28);
	p.on(1, "Organ 8 4", 60, 12);
	p.amp(1, 0, 0, 100, 28);
	p.lfo(1, 0, 70, 0, 2, 0, 15);
	p.cho(68, 40, 45, 75);
	p.rev(2, 55, 28);
}
static void p_houseOrgan(PB& p) {
	p.on(0, "Organ 8 4");
	p.amp(0, 0, 55, 30, 30);
	p.filt(0, 0, 70, 10, 15);
	p.fenv(0, 0, 45, 0, 30);
	p.on(1, "Even Comb", 70, 12);
	p.amp(1, 0, 50, 20, 30);
	p.on(2, "Wood Knock", 45, 12);
	p.amp(2, 0, 30, 0, 20);
	p.rev(2, 50, 25);
}
static void p_churchSine(PB& p) {
	p.on(0, "Sine");
	p.amp(0, 30, 0, 100, 60);
	p.on(1, "Organ Full", 60, 12);
	p.amp(1, 35, 0, 100, 60);
	p.on(2, "Harm 3 5", 40, 19);
	p.amp(2, 35, 0, 100, 60);
	p.rev(5, 80, 45, 30);
}
static void p_reedOrgan(PB& p) {
	p.on(0, "Reed");
	p.amp(0, 15, 0, 100, 30);
	p.filt(0, 0, 66, 5, 0);
	p.lfo(0, 0, 58, 0, 0, 0, 12);
	p.on(1, "Breath Chiff", 50);
	p.amp(1, 0, 45, 0, 30);
	p.rev(3, 58, 30);
}
static void p_percOrgan(PB& p) {
	p.on(0, "Organ 8 4");
	p.amp(0, 0, 0, 100, 25);
	p.on(1, "Harm 3 5", 70, 12);
	p.amp(1, 0, 55, 0, 30);
	p.cho(60, 35, 40, 75);
	p.rev(2, 50, 25);
}
static void p_digitalPipe(PB& p) {
	p.on(0, "Ocarina");
	p.amp(0, 30, 0, 100, 55);
	p.on(1, "Flute Chiff", 50);
	p.amp(1, 0, 50, 0, 40);
	p.on(2, "Organ Full", 60, -12);
	p.amp(2, 35, 0, 100, 55);
	p.rev(5, 78, 45, 30);
}
static void p_dirtyOrgan(PB& p) {
	p.on(0, "Organ Leslie");
	p.amp(0, 0, 0, 100, 25);
	p.dist(1, 40, 55);
	p.cho(62, 40, 45, 75);
	p.rev(1, 45, 25);
}

// ======================================================================= 8 BASSES
static void p_digiBass(PB& p) {
	p.on(0, "FM Bass", 100, -12);
	p.amp(0, 0, 60, 40, 25);
	p.filt(0, 0, 50, 15, 25);
	p.fenv(0, 0, 45, 0, 25);
	p.on(1, "Slap", 60, -12);
	p.amp(1, 0, 45, 0, 25);
	p.velo(0, 35, 30);
}
static void p_subSine(PB& p) {
	p.on(0, "Sine", 100, -12);
	p.amp(0, 0, 0, 100, 25);
	p.on(1, "Wood Knock", 40);
	p.amp(1, 0, 30, 0, 20);
	p.mono(0);
}
static void p_acidish(PB& p) {
	p.on(0, "Syn Saw", 100, -12);
	p.amp(0, 0, 55, 60, 22);
	p.filt(0, 0, 35, 70, 35);
	p.fenv(0, 0, 45, 0, 25);
	p.velo(0, 30, 45);
	p.mono(25);
	p.dist(0, 25, 60);
}
static void p_fatSquareBass(PB& p) {
	p.on(0, "Fat Square", 100, -12);
	p.amp(0, 0, 0, 100, 25);
	p.filt(0, 0, 45, 25, 25);
	p.fenv(0, 0, 50, 10, 25);
	p.on(1, "Sine", 60, -24);
	p.amp(1, 0, 0, 100, 25);
}
static void p_slapDigi(PB& p) {
	p.on(0, "Slap", 100, -12);
	p.amp(0, 0, 55, 0, 30);
	p.on(1, "Pulse 25", 70, -12);
	p.amp(1, 0, 55, 30, 28);
	p.filt(1, 0, 48, 30, 30);
	p.fenv(1, 0, 42, 0, 25);
	p.velo(1, 40, 40);
}
static void p_reeseDrift(PB& p) {
	p.on(0, "Syn Saw", 100, -12, -9);
	p.amp(0, 0, 0, 100, 25);
	p.filt(0, 0, 45, 20, 10);
	p.on(1, "Syn Saw", 100, -12, 9);
	p.amp(1, 0, 0, 100, 25);
	p.filt(1, 0, 45, 20, 10);
	p.lfo(0, 0, 20, 0, 0, 12);
	p.pan(0, -15);
	p.pan(1, 15);
	p.mono(15);
}
static void p_woodBass(PB& p) {
	p.on(0, "Pluck", 100, -12);
	p.amp(0, 0, 65, 0, 30);
	p.filt(0, 0, 55, 5, 10);
	p.on(1, "Triangle", 70, -12);
	p.amp(1, 0, 60, 30, 28);
}
static void p_houseBass(PB& p) {
	p.on(0, "Organ 8 4", 100, -12);
	p.amp(0, 0, 50, 20, 25);
	p.filt(0, 0, 50, 15, 20);
	p.fenv(0, 0, 40, 0, 20);
	p.on(1, "Sine", 80, -24);
	p.amp(1, 0, 55, 30, 25);
	p.dist(0, 15, 65);
}

// ======================================================================= 9 HOUSE AND TECHNO STABS
static void p_detroitStab(PB& p) {
	p.on(0, "Fat Saw");
	p.amp(0, 0, 52, 0, 40);
	p.filt(0, 0, 50, 30, 30);
	p.fenv(0, 0, 48, 0, 35);
	p.on(1, "Digi String", 70, 12);
	p.amp(1, 0, 50, 0, 40);
	p.velo(0, 30, 30);
	p.cho(20, 40, 45);
	p.dly(375, 25, 250, 15, 500, 15, 30);
	p.rev(3, 58, 30);
}
static void p_chordMemory(PB& p) {
	p.on(0, "Syn Square");
	p.amp(0, 0, 50, 0, 38);
	p.filt(0, 0, 45, 40, 30);
	p.fenv(0, 0, 45, 0, 30);
	p.on(1, "Syn Saw", 85, 7);
	p.amp(1, 0, 50, 0, 38);
	p.filt(1, 0, 45, 40, 30);
	p.fenv(1, 0, 45, 0, 30);
	p.on(2, "Syn Saw", 70, 12);
	p.amp(2, 0, 50, 0, 38);
	p.filt(2, 0, 45, 40, 30);
	p.fenv(2, 0, 45, 0, 30);
	p.dly(375, 30, 250, 20, 500, 20, 35);
	p.rev(2, 55, 25);
}
static void p_organStab(PB& p) {
	p.on(0, "Organ Full");
	p.amp(0, 0, 45, 0, 30);
	p.on(1, "Organ 8 4", 70, 12);
	p.amp(1, 0, 45, 0, 30);
	p.filt(1, 0, 70, 20, 20);
	p.fenv(1, 0, 40, 0, 25);
	p.rev(2, 50, 28);
}
static void p_rave92(PB& p) {
	p.on(0, "Orch Stab");
	p.amp(0, 0, 55, 0, 40);
	p.on(1, "Fat Saw", 80, 12);
	p.amp(1, 0, 50, 0, 38);
	p.c(C_DAC90, 1);
	p.rev(3, 55, 30);
}
static void p_dubChord(PB& p) {
	p.on(0, "Soft Saw");
	p.amp(0, 0, 48, 0, 35);
	p.filt(0, 0, 40, 30, 25);
	p.fenv(0, 0, 42, 0, 30);
	p.on(1, "Pulse 25", 70);
	p.amp(1, 0, 48, 0, 35);
	p.filt(1, 1, 55, 30, 20);
	p.dly(375, 45, 250, 25, 500, 25, 55);
	p.rev(3, 66, 35);
}
static void p_hooverish(PB& p) {
	p.on(0, "PWM");
	p.amp(0, 0, 0, 100, 40);
	p.penv(0, 2, -50, 40);
	p.lfo(0, 0, 50, 0, 6);
	p.on(1, "PWM", 90, -12, 10);
	p.amp(1, 0, 0, 100, 40);
	p.penv(1, 2, -50, 42);
	p.cho(30, 60, 55);
	p.c(C_DAC90, 1);
	p.rev(3, 55, 30);
}
static void p_pianoHouse(PB& p) {
	p.on(0, "Piano Atk");
	p.amp(0, 0, 66, 0, 45);
	p.velo(0, 40);
	p.on(1, "Bell Tone", 50, 12);
	p.amp(1, 0, 60, 0, 45);
	p.on(2, "Double Saw", 40);
	p.amp(2, 0, 50, 0, 40);
	p.filt(2, 0, 55, 10, 20);
	p.fenv(2, 0, 45, 0, 30);
	p.rev(3, 55, 30);
	p.cho(22, 30, 30);
}
static void p_stringStab(PB& p) {
	p.on(0, "String Ens");
	p.amp(0, 0, 52, 0, 45);
	p.on(1, "Bow Scrape", 50);
	p.amp(1, 0, 40, 0, 30);
	p.on(2, "Fat Saw", 70, 12);
	p.amp(2, 0, 50, 0, 40);
	p.filt(2, 0, 55, 20, 25);
	p.fenv(2, 0, 45, 0, 30);
	p.dly(375, 25, 250, 15, 500, 15, 30);
	p.rev(4, 62, 32);
}

// ======================================================================= 10 LEADS
static void p_glassLead(PB& p) {
	p.on(0, "Glass");
	p.amp(0, 10, 0, 100, 40);
	p.lfo(0, 0, 58, 0, 5, 0, 0, 45, 30);
	p.on(1, "Syn Square", 60, 12);
	p.amp(1, 10, 0, 100, 40);
	p.filt(1, 0, 60, 20);
	p.mono(15);
	p.dly(375, 25, 250, 20, 500, 20, 30);
	p.rev(3, 58, 30);
}
static void p_syncLead(PB& p) {
	p.on(0, "Sync Sweep");
	p.amp(0, 0, 0, 100, 35);
	p.on(1, "Sync Hi", 70, 0, 5);
	p.amp(1, 0, 0, 100, 35);
	p.filt(1, 0, 70, 20, 15);
	p.mono(10);
	p.dist(1, 20, 55);
	p.dly(250, 20, 375, 15, 500, 15, 25);
	p.rev(2, 50, 25);
}
static void p_flutish(PB& p) {
	p.on(0, "Ocarina");
	p.amp(0, 25, 0, 100, 40);
	p.lfo(0, 0, 58, 0, 5, 0, 0, 50, 35);
	p.on(1, "Flute Chiff", 60);
	p.amp(1, 0, 50, 0, 35);
	p.on(2, "Breath Noise", 25);
	p.amp(2, 25, 0, 100, 40);
	p.filt(2, 1, 78, 20, 0);
	p.mono(8);
	p.rev(4, 62, 35);
}
static void p_spectrumLead(PB& p) {
	p.on(0, "Spectrum 2");
	p.amp(0, 5, 0, 100, 38);
	p.filt(0, 0, 60, 35, 25);
	p.fenv(0, 15, 55, 15, 40);
	p.lfo(0, 0, 58, 0, 4, 0, 0, 50, 30);
	p.mono(12);
	p.dly(375, 30, 250, 20, 500, 20, 35);
	p.rev(3, 58, 30);
}
static void p_digiSolo(PB& p) {
	p.on(0, "Wire");
	p.amp(0, 5, 0, 100, 35);
	p.filt(0, 0, 58, 40, 25);
	p.fenv(0, 10, 50, 20, 35);
	p.on(1, "Fat Square", 70, -12);
	p.amp(1, 5, 0, 100, 35);
	p.filt(1, 0, 50, 20, 20);
	p.mono(18);
	p.dist(0, 25, 55);
	p.rev(2, 50, 25);
}
static void p_vocalLead(PB& p) {
	p.on(0, "Vox Ah");
	p.amp(0, 20, 0, 100, 40);
	p.lfo(0, 0, 56, 0, 5, 0, 0, 50, 30);
	p.on(1, "Vox Ee", 60, 12);
	p.amp(1, 25, 0, 100, 40);
	p.mono(15);
	p.cho(22, 35, 40);
	p.rev(4, 62, 35);
}
static void p_squareChip(PB& p) {
	p.on(0, "Bitwave 2");
	p.amp(0, 0, 0, 100, 30);
	p.on(1, "Pulse 12", 60, 12);
	p.amp(1, 0, 50, 40, 30);
	p.lfo(0, 0, 62, 2, 6, 0, 0, 40);
	p.c(C_DAC90, 1);
	p.dly(250, 25, 375, 20, 500, 20, 30);
}
static void p_resoLead(PB& p) {
	p.on(0, "Fat Saw");
	p.amp(0, 0, 0, 100, 35);
	p.filt(0, 0, 50, 75, 25);
	p.fenv(0, 20, 60, 10, 35);
	p.lfo(0, 1, 30, 0, 0, 15);
	p.mono(20);
	p.phs(50, 20, 50, 40, 40);
	p.rev(3, 55, 28);
}

// ======================================================================= 11 ATMOSPHERES
static void p_nebula(PB& p) {
	p.on(0, "Metal Sweep");
	p.swell(0, 70, 80, 90, 80);
	p.filt(0, 0, 65, 20, 10);
	p.on(1, "Wind", 40);
	p.amp(1, 70, 0, 100, 80);
	p.filt(1, 1, 60, 40, 0);
	p.lfo(1, 0, 22, 4, 0, 25);
	p.on(2, "Shimmer", 60, 12);
	p.swell(2, 75, 82, 80, 82);
	p.rev(5, 88, 50, 40, 10);
	p.dly(500, 25, 375, 20, 750, 20, 45);
}
static void p_deepSpace(PB& p) {
	p.on(0, "Breath Pad", 100, -12);
	p.swell(0, 72, 82, 95, 82);
	p.on(1, "Spectrum Swp", 60);
	p.swell(1, 78, 84, 85, 82);
	p.lfo(1, 0, 15, 0, 0, 20);
	p.rev(5, 92, 55, 50, 8);
}
static void p_metallicDrift(PB& p) {
	p.on(0, "Metal Noise", 40);
	p.amp(0, 72, 0, 100, 80);
	p.filt(0, 1, 70, 50, 0);
	p.lfo(0, 0, 18, 0, 0, 25);
	p.on(1, "Metal Sweep", 70, -12);
	p.swell(1, 75, 80, 90, 80);
	p.phs(40, 10, 70, 50, 50);
	p.rev(5, 85, 50, 40);
}
static void p_slowSweep(PB& p) {
	p.on(0, "Fat Saw");
	p.swell(0, 70, 78, 90, 78);
	p.filt(0, 0, 40, 45, 35);
	p.fenv(0, 82, 86, 30, 78);
	p.on(1, "Fat Saw", 85, 12, 7);
	p.swell(1, 72, 78, 90, 78);
	p.filt(1, 0, 40, 40, 35);
	p.fenv(1, 84, 86, 30, 78);
	p.cho(18, 45, 50);
	p.rev(5, 80, 45, 30);
}
static void p_underwater(PB& p) {
	p.on(0, "Sine", 100, -12);
	p.amp(0, 60, 0, 100, 75);
	p.lfo(0, 0, 30, 4, 6);
	p.on(1, "Band Noise", 50);
	p.amp(1, 60, 0, 100, 75);
	p.filt(1, 0, 40, 60, 0);
	p.lfo(1, 0, 25, 4, 0, 30);
	p.phs(30, 12, 80, 55, 50);
	p.rev(5, 85, 50, 30, 6);
}
static void p_cosmicHorizon(PB& p) {
	p.on(0, "Glass Pad", 100);
	p.swell(0, 70, 80, 90, 80);
	p.on(1, "Choir Pad", 70, 7);
	p.swell(1, 74, 82, 85, 80);
	p.on(2, "Spectrum Swp", 50, 12);
	p.swell(2, 76, 84, 80, 82);
	p.dly(500, 25, 375, 20, 750, 20, 45);
	p.rev(5, 90, 50, 40);
}
static void p_reversedDream(PB& p) {
	p.on(0, "Breath Pad");
	p.swell(0, 60, 74, 90, 72);
	p.on(1, "Glass Ping", 60, 12);
	p.amp(1, 0, 70, 0, 65);
	p.rev(7, 85, 55, 10);
}
static void p_flyingGlass(PB& p) {
	p.on(0, "Glass");
	p.amp(0, 30, 70, 50, 68);
	p.on(1, "Crystal", 60, 12);
	p.amp(1, 35, 70, 40, 68);
	p.rev(8, 70, 50, 10);
}

// ======================================================================= 12 FX AND TEXTURES
static void p_digitalRain(PB& p) {
	p.on(0, "Spark");
	p.amp(0, 0, 45, 0, 40);
	p.s(0, T_PRAND, 40);
	p.on(1, "Glass Ping", 50, 24);
	p.amp(1, 0, 60, 0, 55);
	p.s(1, T_PRAND, 30);
	p.dly(250, 30, 375, 25, 500, 25, 45);
	p.rev(8, 70, 45);
}
static void p_zapper(PB& p) {
	p.on(0, "Digital Zap");
	p.amp(0, 0, 50, 0, 40);
	p.penv(0, 24, 50, 45);
	p.on(1, "Bitwave 1", 50);
	p.amp(1, 0, 45, 0, 35);
	p.penv(1, 12, 50, 40);
	p.c(C_DAC90, 1);
	p.dly(250, 30, 375, 20, 500, 20, 40);
}
static void p_gatedHit(PB& p) {
	p.on(0, "Metal Hit");
	p.amp(0, 0, 60, 0, 50);
	p.on(1, "Timpani", 70, -12);
	p.amp(1, 0, 60, 0, 50);
	p.rev(6, 55, 60);
}
static void p_crackleTape(PB& p) {
	p.on(0, "Crackle", 60);
	p.amp(0, 0, 0, 100, 50);
	p.on(1, "Soft Saw", 70);
	p.amp(1, 40, 0, 100, 60);
	p.filt(1, 0, 50, 10, 0);
	p.lfo(1, 0, 25, 4, 6);
	p.c(C_DAC90, 1);
	p.rev(1, 50, 30);
}
static void p_sciFiBeacon(PB& p) {
	p.on(0, "FM Hollow");
	p.amp(0, 0, 0, 100, 60);
	p.lfo(0, 0, 66, 2, 0, 0, 45);
	p.on(1, "Sync Lo", 50, 12);
	p.amp(1, 0, 0, 100, 60);
	p.lfo(1, 1, 30, 1, 30);
	p.dly(375, 30, 250, 20, 500, 20, 50);
	p.rev(5, 75, 40);
}
static void p_machineBreath(PB& p) {
	p.on(0, "Breath Noise", 70);
	p.amp(0, 45, 0, 100, 55);
	p.filt(0, 1, 55, 55, 20);
	p.fenv(0, 50, 70, 10, 50);
	p.on(1, "Vox Morph", 50, -12);
	p.amp(1, 50, 0, 100, 55);
	p.c(C_DAC90, 1);
	p.rev(4, 70, 40);
}
static void p_windChimes(PB& p) {
	p.on(0, "Glass Ping", 100, 24);
	p.amp(0, 0, 72, 0, 65);
	p.s(0, T_PRAND, 60);
	p.on(1, "Bell Strike", 50, 12);
	p.amp(1, 0, 72, 0, 65);
	p.s(1, T_PRAND, 60);
	p.on(2, "Wind", 30);
	p.amp(2, 60, 0, 100, 70);
	p.filt(2, 1, 72, 35);
	p.rev(5, 82, 45, 30);
}
static void p_industrial(PB& p) {
	p.on(0, "Metal Noise", 70);
	p.amp(0, 0, 50, 0, 40);
	p.filt(0, 1, 60, 50, 25);
	p.fenv(0, 0, 40, 0, 30);
	p.on(1, "FM Clang", 70, -12);
	p.amp(1, 0, 55, 0, 45);
	p.dist(5, 45, 55);
	p.rev(6, 50, 45);
}

static const Recipe RECIPES[] = {
	{"Glass Cathedral", "Glass Pads", p_glassCathedral}, {"Ice Field", "Glass Pads", p_iceField},
	{"Crystal Halo", "Glass Pads", p_crystalHalo}, {"Prism Pad", "Glass Pads", p_prismPad},
	{"Frozen Lake", "Glass Pads", p_frozenLake}, {"Angel Glass", "Glass Pads", p_angelGlass},
	{"Digital Dawn", "Glass Pads", p_digitalDawn}, {"Quartz Bloom", "Glass Pads", p_quartzBloom},
	{"Cathedral Vox", "Choirs", p_cathedralVox}, {"Breathing Choir", "Choirs", p_breathingChoir},
	{"Robot Monks", "Choirs", p_robotMonks}, {"Ka Chant", "Choirs", p_kaChant},
	{"Air Voices", "Choirs", p_airVoices}, {"Vocoderish", "Choirs", p_vocoderish},
	{"Low Choir", "Choirs", p_lowChoir}, {"Choir Stab", "Choirs", p_choirStab},
	{"Facet Strings", "Strings", p_jdStrings}, {"Digi Strings", "Strings", p_digiStrings},
	{"Slow Strings", "Strings", p_slowStrings}, {"Pizz Ensemble", "Strings", p_pizzEnsemble},
	{"Synth Orchestra", "Strings", p_syntheticOrchestra}, {"Tremolo Strings", "Strings", p_tremoloStrings},
	{"Cinema Strings", "Strings", p_cinemaStrings}, {"Bowed Glass", "Strings", p_bowedGlass},
	{"Spectral Brass", "Brass", p_spectralBrass}, {"Fanfare", "Brass", p_fanfare},
	{"Soft Horns", "Brass", p_softHorns}, {"Brass Stab 90", "Brass", p_brassStab90},
	{"Fat Synth Brass", "Brass", p_synthBrassFat}, {"Tuba Glass", "Brass", p_tubaGlass},
	{"Wah Brass", "Brass", p_wahBrass}, {"Brass Section", "Brass", p_brassSection},
	{"Crystal Bells", "Bells", p_crystalBells}, {"Tubular Digi", "Bells", p_tubularDigi},
	{"Marimba Glass", "Bells", p_marimbaGlass}, {"Vibraphone", "Bells", p_vibraphone},
	{"Kalimba Dream", "Bells", p_kalimbaDream}, {"Steel Sun", "Bells", p_steelSun},
	{"FM Bells", "Bells", p_fmBells}, {"Glocken Spark", "Bells", p_glockenSpark},
	{"Digital EP", "Keys", p_digitalEP}, {"Glass Harp", "Keys", p_glassHarp},
	{"Plucked Spectrum", "Keys", p_pluckedSpectrum}, {"House Keys", "Keys", p_houseKeys},
	{"Clavinette", "Keys", p_clavinette}, {"Piano Glass", "Keys", p_pianoGlass},
	{"Koto Digi", "Keys", p_kotoDigi}, {"Mute Guitar", "Keys", p_muteGuitar},
	{"Digi Organ", "Organs", p_digiOrgan}, {"Leslie Drawbars", "Organs", p_leslieDrawbars},
	{"House Organ", "Organs", p_houseOrgan}, {"Church Sine", "Organs", p_churchSine},
	{"Reed Organ", "Organs", p_reedOrgan}, {"Perc Organ", "Organs", p_percOrgan},
	{"Digital Pipe", "Organs", p_digitalPipe}, {"Dirty Organ", "Organs", p_dirtyOrgan},
	{"Digi Bass", "Basses", p_digiBass}, {"Sub Sine", "Basses", p_subSine},
	{"Acidish", "Basses", p_acidish}, {"Fat Square Bass", "Basses", p_fatSquareBass},
	{"Slap Digi", "Basses", p_slapDigi}, {"Reese Drift", "Basses", p_reeseDrift},
	{"Wood Bass", "Basses", p_woodBass}, {"House Bass", "Basses", p_houseBass},
	{"Detroit Stab", "Stabs", p_detroitStab}, {"Chord Memory", "Stabs", p_chordMemory},
	{"Organ Stab", "Stabs", p_organStab}, {"Rave 92", "Stabs", p_rave92},
	{"Dub Chord", "Stabs", p_dubChord}, {"Hooverish", "Stabs", p_hooverish},
	{"Piano House", "Stabs", p_pianoHouse}, {"String Stab", "Stabs", p_stringStab},
	{"Glass Lead", "Leads", p_glassLead}, {"Sync Lead", "Leads", p_syncLead},
	{"Flutish", "Leads", p_flutish}, {"Spectrum Lead", "Leads", p_spectrumLead},
	{"Digi Solo", "Leads", p_digiSolo}, {"Vocal Lead", "Leads", p_vocalLead},
	{"Square Chip", "Leads", p_squareChip}, {"Reso Lead", "Leads", p_resoLead},
	{"Nebula", "Atmospheres", p_nebula}, {"Deep Space", "Atmospheres", p_deepSpace},
	{"Metallic Drift", "Atmospheres", p_metallicDrift}, {"Slow Sweep", "Atmospheres", p_slowSweep},
	{"Underwater", "Atmospheres", p_underwater}, {"Cosmic Horizon", "Atmospheres", p_cosmicHorizon},
	{"Reversed Dream", "Atmospheres", p_reversedDream}, {"Flying Glass", "Atmospheres", p_flyingGlass},
	{"Digital Rain", "FX", p_digitalRain}, {"Zapper", "FX", p_zapper},
	{"Gated Hit", "FX", p_gatedHit}, {"Crackle Tape", "FX", p_crackleTape},
	{"Sci-Fi Beacon", "FX", p_sciFiBeacon}, {"Machine Breath", "FX", p_machineBreath},
	{"Wind Chimes", "FX", p_windChimes}, {"Industrial", "FX", p_industrial},
};

// output LEVEL per preset, measured with test/render.cpp so they all sit at a similar loudness
static const float PRESET_LEVEL[96] = {59,65,55,81,80,64,83,71,59,56,73,57,65,77,62,76,52,52,58,86,51,46,56,53,48,46,60,90,53,66,58,49,62,71,79,65,77,69,75,90,62,101,79,70,65,77,89,123,58,52,69,67,78,62,62,100,118,127,118,121,102,98,101,127,66,58,76,94,83,71,66,78,111,121,115,109,125,102,119,104,95,97,127,70,78,74,75,75,127,107,83,127,65,127,90,121};

inline const std::vector<Preset>& factoryPresets() {
	static std::vector<Preset> list;
	if (list.empty()) {
		int idx = 0;
		for (const Recipe& r : RECIPES) {
			PB p;
			r.b(p);
			p.c(C_LEVEL, PRESET_LEVEL[idx++ % 96]);
			Preset pr;
			pr.name = r.name;
			pr.cat = r.cat;
			pr.v.assign(p.v, p.v + NPV);
			list.push_back(pr);
		}
	}
	return list;
}

// the 8 panel memories start as one patch from each of these groups
static const int DEFAULT_MEMS[8] = {0, 8, 16, 24, 32, 40, 64, 80};

} // namespace facet
