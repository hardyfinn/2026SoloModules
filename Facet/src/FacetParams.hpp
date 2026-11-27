// Facet patch parameters: one table that the module, the screen, the randomiser, the presets
// and the engine all share. Values are kept in JD-style units (mostly 0..100 or -50..+50).
#pragma once
#include "FacetWaves.hpp"
#include <cstring>

namespace facet {

// ---- groups (what RAND / MUTATE may touch; SEL picks one of the first six)
enum Scope { SC_ALL, SC_WAVES, SC_FILTER, SC_ENVS, SC_LFOS, SC_FX, NUM_SCOPES };
static const char* const SCOPE_NAMES[NUM_SCOPES] = {"ALL", "WAVES", "FILTER", "ENVS", "LFOS", "FX"};

enum Group {
	G_LAYER,  // tone on/off (only RAND ALL, keeps one tone on)
	G_WAVE,   // wave choice
	G_PCAP,   // pitch: randomised in tune (musical intervals, small detunes) unless "Allow wild pitch"
	G_FILTER,
	G_ENV,
	G_LFO,
	G_AMP,    // tone level, velocity, bias, pan (RAND ALL only)
	G_FX,
	G_PROT    // never randomised or mutated
};

struct Def {
	const char* shortName; // screen (max ~12 chars)
	const char* longName;  // Rack tooltip
	float min, max, def;
	bool snap;
	int group;
	float rlo, rhi;        // sane random range (param units)
	int choices;           // index into CHOICE lists, -1 = number
};

enum ChoiceList { CH_OFFON, CH_FMODE, CH_VCURVE, CH_BIASDIR, CH_LFOWAVE, CH_LFOOFS, CH_DIST, CH_ORDER, CH_RVTYPE, CH_HFDAMP, CH_LOFREQ, CH_HIFREQ, CH_MIDQ, CH_WAVE };
static const char* const CHOICE_OFFON[] = {"Off", "On"};
static const char* const CHOICE_FMODE[] = {"LPF", "BPF", "HPF"};
static const char* const CHOICE_VCURVE[] = {"1 linear", "2 soft", "3 hard", "4 S-curve"};
static const char* const CHOICE_BIASDIR[] = {"Upper", "Lower", "Upper+Lower"};
static const char* const CHOICE_LFOWAVE[] = {"Tri", "Saw", "Square", "S&H", "Random"};
static const char* const CHOICE_LFOOFS[] = {"-", "0", "+"};
static const char* const CHOICE_DIST[] = {"Mellow Drive", "Overdrive", "Cry Drive", "Mellow Dist", "Light Dist", "Fat Dist", "Fuzz Dist"};
static const char* const CHOICE_ORDER[] = {"DS-PH-SP-EH", "DS-SP-PH-EH", "EH-DS-PH-SP", "SP-EH-DS-PH"};
static const char* const CHOICE_RVTYPE[] = {"Room 1", "Room 2", "Hall 1", "Hall 2", "Hall 3", "Hall 4", "Gate", "Reverse", "Flying 1", "Flying 2"};
static const char* const CHOICE_HFDAMP[] = {"500", "630", "800", "1k", "1.25k", "1.6k", "2k", "2.5k", "3.15k", "4k", "5k", "6.3k", "8k", "10k", "12.5k", "16k", "Bypass"};
static const char* const CHOICE_LOFREQ[] = {"200 Hz", "400 Hz"};
static const char* const CHOICE_HIFREQ[] = {"4 kHz", "8 kHz"};
static const char* const CHOICE_MIDQ[] = {"0.5", "1.0", "2.0", "4.0", "9.0"};
static const float HFDAMP_HZ[17] = {500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};
static const float MIDQ_VAL[5] = {0.5f, 1.f, 2.f, 4.f, 9.f};

inline const char* const* choiceList(int c, int& n) {
	switch (c) {
		case CH_OFFON: n = 2; return CHOICE_OFFON;
		case CH_FMODE: n = 3; return CHOICE_FMODE;
		case CH_VCURVE: n = 4; return CHOICE_VCURVE;
		case CH_BIASDIR: n = 3; return CHOICE_BIASDIR;
		case CH_LFOWAVE: n = 5; return CHOICE_LFOWAVE;
		case CH_LFOOFS: n = 3; return CHOICE_LFOOFS;
		case CH_DIST: n = 7; return CHOICE_DIST;
		case CH_ORDER: n = 4; return CHOICE_ORDER;
		case CH_RVTYPE: n = 10; return CHOICE_RVTYPE;
		case CH_HFDAMP: n = 17; return CHOICE_HFDAMP;
		case CH_LOFREQ: n = 2; return CHOICE_LOFREQ;
		case CH_HIFREQ: n = 2; return CHOICE_HIFREQ;
		case CH_MIDQ: n = 5; return CHOICE_MIDQ;
		default: n = 0; return nullptr;
	}
}

// ================================================================ per-tone parameters
enum ToneParam {
	T_ON, T_WAVE, T_COARSE, T_FINE, T_PRAND, T_PKF, T_BENDSW, T_ATBEND,
	T_PE_DEPTH, T_PE_VELO, T_PE_TVELO, T_PE_TKF, T_PE_L0, T_PE_T1, T_PE_L1, T_PE_T2, T_PE_T3, T_PE_L2,
	T_F_MODE, T_F_CUT, T_F_RES, T_F_KF, T_F_ENV, T_F_AT,
	T_FE_VELO, T_FE_TVELO, T_FE_TKF, T_FE_T1, T_FE_L1, T_FE_T2, T_FE_L2, T_FE_T3, T_FE_SUS, T_FE_T4, T_FE_L4,
	T_LEVEL, T_VCURVE, T_VSENS, T_BIASDIR, T_BIASPT, T_BIASLV, T_A_AT, T_PAN,
	T_AE_TVELO, T_AE_TKF, T_AE_T1, T_AE_L1, T_AE_T2, T_AE_L2, T_AE_T3, T_AE_SUS, T_AE_T4,
	T_L1_RATE, T_L1_DELAY, T_L1_FADE, T_L1_WAVE, T_L1_OFS, T_L1_KEY, T_L1_PITCH, T_L1_TVF, T_L1_TVA,
	T_L2_RATE, T_L2_DELAY, T_L2_FADE, T_L2_WAVE, T_L2_OFS, T_L2_KEY, T_L2_PITCH, T_L2_TVF, T_L2_TVA,
	T_KEYLO, T_KEYHI, T_MODLFO, T_MODCUT,
	NT
};
static const int LFO_STRIDE = T_L2_RATE - T_L1_RATE;

inline const Def& toneDef(int i) {
	static const float NW = 200.f; // replaced at run time by the ROM size (see waveMax())
	(void) NW;
	static const Def D[NT] = {
		{"LAYER", "layer on", 0, 1, 0, true, G_LAYER, 0, 1, CH_OFFON},
		{"WAVE", "waveform", 0, 199, 0, true, G_WAVE, 0, 199, CH_WAVE},
		{"COARSE", "pitch coarse (semitones)", -48, 48, 0, true, G_PCAP, -48, 48, -1},
		{"FINE", "pitch fine (cents)", -50, 50, 0, true, G_PCAP, -50, 50, -1},
		{"P RANDOM", "pitch random per note", 0, 100, 0, true, G_PCAP, 0, 100, -1},
		{"P KEYFLW", "pitch key follow (%)", -100, 200, 100, true, G_PROT, 100, 100, -1},
		{"BENDER", "pitch bend on", 0, 1, 1, true, G_PROT, 1, 1, CH_OFFON},
		{"AT BEND", "aftertouch bend (semitones)", -12, 12, 0, true, G_PROT, 0, 0, -1},
		{"PE DEPTH", "pitch env depth (semitones at level 50)", 0, 48, 12, true, G_PCAP, 0, 48, -1},
		{"PE VELO", "pitch env velocity sens", -50, 50, 0, true, G_ENV, -20, 30, -1},
		{"PE T-VELO", "pitch env time velocity", -50, 50, 0, true, G_ENV, -20, 20, -1},
		{"PE T-KF", "pitch env time key follow", -10, 10, 0, true, G_ENV, -3, 3, -1},
		{"PE L0", "pitch env start level", -50, 50, 0, true, G_ENV, -50, 50, -1},
		{"PE T1", "pitch env time 1", 0, 100, 0, true, G_ENV, 0, 60, -1},
		{"PE L1", "pitch env level 1", -50, 50, 0, true, G_ENV, -50, 50, -1},
		{"PE T2", "pitch env time 2", 0, 100, 0, true, G_ENV, 0, 70, -1},
		{"PE T3", "pitch env release time", 0, 100, 0, true, G_ENV, 0, 60, -1},
		{"PE L2", "pitch env release level", -50, 50, 0, true, G_ENV, -10, 10, -1},
		{"TVF MODE", "filter mode", 0, 2, 0, true, G_FILTER, 0, 2, CH_FMODE},
		{"CUTOFF", "filter cutoff", 0, 100, 80, true, G_FILTER, 30, 100, -1},
		{"RESO", "filter resonance", 0, 100, 0, true, G_FILTER, 0, 65, -1},
		{"F KEYFLW", "filter key follow (%)", -100, 150, 50, true, G_FILTER, 0, 100, -1},
		{"TVF ENV", "filter env depth", -50, 50, 0, true, G_FILTER, -20, 45, -1},
		{"F AT", "aftertouch -> cutoff", -50, 50, 0, true, G_FILTER, 0, 30, -1},
		{"FE VELO", "filter env velocity sens", -50, 50, 0, true, G_ENV, 0, 40, -1},
		{"FE T-VELO", "filter env time velocity", -50, 50, 0, true, G_ENV, -20, 20, -1},
		{"FE T-KF", "filter env time key follow", -10, 10, 0, true, G_ENV, -3, 3, -1},
		{"FE T1", "filter env time 1", 0, 100, 0, true, G_ENV, 0, 70, -1},
		{"FE L1", "filter env level 1", -50, 50, 50, true, G_ENV, 0, 50, -1},
		{"FE T2", "filter env time 2", 0, 100, 40, true, G_ENV, 0, 80, -1},
		{"FE L2", "filter env level 2", -50, 50, 20, true, G_ENV, -20, 50, -1},
		{"FE T3", "filter env time 3", 0, 100, 50, true, G_ENV, 0, 80, -1},
		{"FE SUS", "filter env sustain level", -50, 50, 0, true, G_ENV, -20, 40, -1},
		{"FE T4", "filter env release time", 0, 100, 40, true, G_ENV, 10, 80, -1},
		{"FE L4", "filter env release level", -50, 50, 0, true, G_ENV, -20, 20, -1},
		{"TONE LEVEL", "tone level", 0, 100, 100, true, G_AMP, 55, 100, -1},
		{"V CURVE", "velocity curve", 0, 3, 0, true, G_AMP, 0, 3, CH_VCURVE},
		{"VELO SENS", "level velocity sens", -50, 50, 25, true, G_AMP, 0, 40, -1},
		{"BIAS DIR", "bias direction", 0, 2, 0, true, G_AMP, 0, 2, CH_BIASDIR},
		{"BIAS PT", "bias point (note)", 0, 127, 60, true, G_AMP, 36, 84, -1},
		{"BIAS LVL", "bias level", -10, 10, 0, true, G_AMP, -4, 2, -1},
		{"A AT", "aftertouch -> level", -50, 50, 0, true, G_AMP, 0, 20, -1},
		{"PAN", "pan", -50, 50, 0, true, G_AMP, -35, 35, -1},
		{"AE T-VELO", "amp env time velocity", -50, 50, 0, true, G_ENV, -20, 20, -1},
		{"AE T-KF", "amp env time key follow", -10, 10, 0, true, G_ENV, -3, 3, -1},
		{"AE T1", "amp env attack time", 0, 100, 0, true, G_ENV, 0, 65, -1},
		{"AE L1", "amp env level 1", 0, 100, 100, true, G_ENV, 70, 100, -1},
		{"AE T2", "amp env time 2", 0, 100, 0, true, G_ENV, 0, 75, -1},
		{"AE L2", "amp env level 2", 0, 100, 100, true, G_ENV, 40, 100, -1},
		{"AE T3", "amp env time 3", 0, 100, 0, true, G_ENV, 0, 80, -1},
		{"AE SUS", "amp env sustain level", 0, 100, 100, true, G_ENV, 30, 100, -1},
		{"AE T4", "amp env release time", 0, 100, 35, true, G_ENV, 15, 80, -1},
		{"L1 RATE", "LFO 1 rate", 0, 100, 50, true, G_LFO, 15, 70, -1},
		{"L1 DELAY", "LFO 1 delay", 0, 100, 0, true, G_LFO, 0, 60, -1},
		{"L1 FADE", "LFO 1 fade (+ in / - out)", -50, 50, 0, true, G_LFO, -20, 40, -1},
		{"L1 WAVE", "LFO 1 wave", 0, 4, 0, true, G_LFO, 0, 4, CH_LFOWAVE},
		{"L1 OFFSET", "LFO 1 offset", 0, 2, 1, true, G_LFO, 0, 2, CH_LFOOFS},
		{"L1 KEYTRG", "LFO 1 key trigger", 0, 1, 0, true, G_LFO, 0, 1, CH_OFFON},
		{"L1>PITCH", "LFO 1 -> pitch", -50, 50, 0, true, G_PCAP, -50, 50, -1},
		{"L1>TVF", "LFO 1 -> cutoff", -50, 50, 0, true, G_LFO, -25, 25, -1},
		{"L1>TVA", "LFO 1 -> level", -50, 50, 0, true, G_LFO, -20, 20, -1},
		{"L2 RATE", "LFO 2 rate", 0, 100, 30, true, G_LFO, 5, 60, -1},
		{"L2 DELAY", "LFO 2 delay", 0, 100, 0, true, G_LFO, 0, 60, -1},
		{"L2 FADE", "LFO 2 fade (+ in / - out)", -50, 50, 0, true, G_LFO, -20, 40, -1},
		{"L2 WAVE", "LFO 2 wave", 0, 4, 0, true, G_LFO, 0, 4, CH_LFOWAVE},
		{"L2 OFFSET", "LFO 2 offset", 0, 2, 1, true, G_LFO, 0, 2, CH_LFOOFS},
		{"L2 KEYTRG", "LFO 2 key trigger", 0, 1, 0, true, G_LFO, 0, 1, CH_OFFON},
		{"L2>PITCH", "LFO 2 -> pitch", -50, 50, 0, true, G_PCAP, -50, 50, -1},
		{"L2>TVF", "LFO 2 -> cutoff", -50, 50, 0, true, G_LFO, -25, 25, -1},
		{"L2>TVA", "LFO 2 -> level", -50, 50, 0, true, G_LFO, -20, 20, -1},
		{"KEY LOW", "key range low (note)", 0, 127, 0, true, G_PROT, 0, 0, -1},
		{"KEY HIGH", "key range high (note)", 0, 127, 127, true, G_PROT, 127, 127, -1},
		{"MOD>VIB", "mod wheel -> LFO 1 vibrato", 0, 100, 30, true, G_PROT, 30, 30, -1},
		{"MOD>TVF", "mod wheel -> cutoff", -50, 50, 0, true, G_FILTER, 0, 30, -1},
	};
	return D[i];
}

// ================================================================ patch-wide parameters
enum CommonParam {
	C_LEVEL, C_OCT, C_BENDR, C_SOLO, C_LEGATO, C_PORTA, C_PTIME, C_DAC90,
	C_ORDER,
	C_DS_ON, C_DS_TYPE, C_DS_DRIVE, C_DS_LEVEL,
	C_PH_ON, C_PH_MAN, C_PH_RATE, C_PH_DEPTH, C_PH_RESO, C_PH_MIX,
	C_SP_ON, C_SP_B1, C_SP_B2, C_SP_B3, C_SP_B4, C_SP_B5, C_SP_B6, C_SP_WIDTH,
	C_EH_ON, C_EH_SENS, C_EH_MIX,
	C_CH_ON, C_CH_RATE, C_CH_DEPTH, C_CH_DELAY, C_CH_FB, C_CH_LEVEL,
	C_DL_ON, C_DL_CTIME, C_DL_CLEV, C_DL_LTIME, C_DL_LLEV, C_DL_RTIME, C_DL_RLEV, C_DL_FB,
	C_RV_ON, C_RV_TYPE, C_RV_PRE, C_RV_EARLY, C_RV_HFD, C_RV_TIME, C_RV_LEVEL,
	C_EQ_LOF, C_EQ_LOG, C_EQ_MIDF, C_EQ_MIDQ, C_EQ_MIDG, C_EQ_HIF, C_EQ_HIG,
	NC
};

inline const Def& commonDef(int i) {
	static const Def D[NC] = {
		{"LEVEL", "patch output level", 0, 127, 80, true, G_PROT, 80, 80, -1},
		{"OCTAVE", "octave shift", -2, 2, 0, true, G_PROT, 0, 0, -1},
		{"BEND RANGE", "bend range (semitones)", 0, 12, 2, true, G_PROT, 2, 2, -1},
		{"SOLO", "solo (mono, uses channel 1)", 0, 1, 0, true, G_PROT, 0, 0, CH_OFFON},
		{"LEGATO", "solo legato", 0, 1, 0, true, G_PROT, 0, 0, CH_OFFON},
		{"PORTAMENTO", "portamento", 0, 1, 0, true, G_PROT, 0, 0, CH_OFFON},
		{"PORTA TIME", "portamento time", 0, 100, 20, true, G_PROT, 20, 20, -1},
		{"90s DAC", "90s converter colour", 0, 1, 0, true, G_PROT, 0, 0, CH_OFFON},
		{"A ORDER", "effects group A order", 0, 3, 0, true, G_FX, 0, 3, CH_ORDER},
		{"DIST", "distortion on", 0, 1, 0, true, G_FX, 0, 1, CH_OFFON},
		{"DIST TYPE", "distortion type", 0, 6, 1, true, G_FX, 0, 6, CH_DIST},
		{"DRIVE", "distortion drive", 0, 100, 30, true, G_FX, 5, 60, -1},
		{"DIST LEVEL", "distortion level", 0, 100, 60, true, G_FX, 40, 70, -1},
		{"PHASER", "phaser on", 0, 1, 0, true, G_FX, 0, 1, CH_OFFON},
		{"PH MANUAL", "phaser manual (centre)", 0, 100, 50, true, G_FX, 20, 80, -1},
		{"PH RATE", "phaser rate", 0, 100, 30, true, G_FX, 5, 60, -1},
		{"PH DEPTH", "phaser depth", 0, 100, 50, true, G_FX, 20, 90, -1},
		{"PH RESO", "phaser resonance", 0, 100, 30, true, G_FX, 0, 70, -1},
		{"PH MIX", "phaser mix", 0, 100, 50, true, G_FX, 30, 70, -1},
		{"SPECTRUM", "spectrum on", 0, 1, 0, true, G_FX, 0, 1, CH_OFFON},
		{"SP 250", "spectrum 250 Hz (dB)", -15, 15, 0, true, G_FX, -8, 8, -1},
		{"SP 500", "spectrum 500 Hz (dB)", -15, 15, 0, true, G_FX, -8, 8, -1},
		{"SP 1k", "spectrum 1 kHz (dB)", -15, 15, 0, true, G_FX, -8, 8, -1},
		{"SP 2k", "spectrum 2 kHz (dB)", -15, 15, 0, true, G_FX, -8, 8, -1},
		{"SP 4k", "spectrum 4 kHz (dB)", -15, 15, 0, true, G_FX, -8, 8, -1},
		{"SP 8k", "spectrum 8 kHz (dB)", -15, 15, 0, true, G_FX, -8, 8, -1},
		{"SP WIDTH", "spectrum band width", 1, 5, 3, true, G_FX, 1, 5, -1},
		{"ENHANCER", "enhancer on", 0, 1, 0, true, G_FX, 0, 1, CH_OFFON},
		{"EH SENS", "enhancer sens", 0, 100, 50, true, G_FX, 20, 80, -1},
		{"EH MIX", "enhancer mix", 0, 100, 40, true, G_FX, 20, 70, -1},
		{"CHORUS", "chorus on", 0, 1, 1, true, G_FX, 0, 1, CH_OFFON},
		{"CH RATE", "chorus rate", 0, 100, 25, true, G_FX, 5, 50, -1},
		{"CH DEPTH", "chorus depth", 0, 100, 40, true, G_FX, 15, 70, -1},
		{"CH DELAY", "chorus delay", 0, 100, 70, true, G_FX, 55, 85, -1},
		{"CH FDBK", "chorus feedback (%)", -98, 98, 0, true, G_FX, -30, 30, -1},
		{"CH LEVEL", "chorus level", 0, 100, 50, true, G_FX, 30, 70, -1},
		{"DELAY", "delay on", 0, 1, 0, true, G_FX, 0, 1, CH_OFFON},
		{"DL C TIME", "delay centre tap (ms)", 1, 1200, 375, true, G_FX, 100, 750, -1},
		{"DL C LVL", "delay centre level", 0, 100, 40, true, G_FX, 15, 50, -1},
		{"DL L TIME", "delay left tap (ms)", 1, 1200, 250, true, G_FX, 60, 600, -1},
		{"DL L LVL", "delay left level", 0, 100, 25, true, G_FX, 0, 40, -1},
		{"DL R TIME", "delay right tap (ms)", 1, 1200, 500, true, G_FX, 60, 600, -1},
		{"DL R LVL", "delay right level", 0, 100, 25, true, G_FX, 0, 40, -1},
		{"DL FDBK", "delay feedback (%)", -98, 98, 30, true, G_FX, 0, 60, -1},
		{"REVERB", "reverb on", 0, 1, 1, true, G_FX, 0, 1, CH_OFFON},
		{"RV TYPE", "reverb type", 0, 9, 3, true, G_FX, 0, 9, CH_RVTYPE},
		{"RV PRE", "reverb pre-delay (ms)", 0, 120, 10, true, G_FX, 0, 60, -1},
		{"RV EARLY", "reverb early reflections", 0, 100, 40, true, G_FX, 10, 70, -1},
		{"RV HF DAMP", "reverb HF damp", 0, 16, 12, true, G_FX, 6, 16, CH_HFDAMP},
		{"RV TIME", "reverb time", 0, 100, 50, true, G_FX, 25, 75, -1},
		{"RV LEVEL", "reverb level", 0, 100, 35, true, G_FX, 15, 55, -1},
		{"EQ LO FRQ", "EQ low frequency", 0, 1, 0, true, G_FX, 0, 1, CH_LOFREQ},
		{"EQ LO", "EQ low gain (dB)", -15, 15, 0, true, G_FX, -4, 4, -1},
		{"EQ MID FRQ", "EQ mid frequency", 0, 100, 50, true, G_FX, 20, 80, -1},
		{"EQ MID Q", "EQ mid Q", 0, 4, 2, true, G_FX, 0, 4, CH_MIDQ},
		{"EQ MID", "EQ mid gain (dB)", -15, 15, 0, true, G_FX, -4, 4, -1},
		{"EQ HI FRQ", "EQ high frequency", 0, 1, 0, true, G_FX, 0, 1, CH_HIFREQ},
		{"EQ HI", "EQ high gain (dB)", -15, 15, 0, true, G_FX, -4, 4, -1},
	};
	return D[i];
}

// ================================================================ the whole patch
static const int NPV = NT * 4 + NC;
inline int tp(int tone, int p) { return tone * NT + p; }
inline int cp(int p) { return NT * 4 + p; }
inline bool isTone(int i) { return i < NT * 4; }

inline Def defOf(int i) {
	Def d = isTone(i) ? toneDef(i % NT) : commonDef(i - NT * 4);
	if (isTone(i) && i % NT == T_WAVE) {
		d.max = d.rhi = (float) (Rom::get().size() - 1);
	}
	return d;
}

inline float paramDefault(int i) {
	if (isTone(i) && i % NT == T_ON) return i / NT == 0 ? 1.f : 0.f;
	return defOf(i).def;
}

inline void patchDefaults(float* v) {
	for (int i = 0; i < NPV; i++) v[i] = paramDefault(i);
}

inline float clampTo(int i, float x) {
	Def d = defOf(i);
	x = std::max(d.min, std::min(d.max, x));
	if (d.snap) x = std::round(x);
	return x;
}

inline bool inScope(int i, int scope) {
	int g = defOf(i).group;
	if (g == G_PROT) return false;
	switch (scope) {
		case SC_ALL: return true;
		case SC_WAVES: return g == G_WAVE || g == G_PCAP;
		case SC_FILTER: return g == G_FILTER;
		case SC_ENVS: return g == G_ENV;
		case SC_LFOS: return g == G_LFO;
		case SC_FX: return g == G_FX;
	}
	return false;
}

// ================================================================ RAND
// amt 0..1: near zero nudges the patch, 1 rolls a new one. Pitch stays musical unless wild.
inline float pickMusicalCoarse(Rng& r) {
	static const float S[8] = {0, 0, 0, 12, -12, 7, 19, 24};
	return S[(int) (r.uni() * 8) % 8];
}

inline float randomTarget(int i, Rng& r, bool wild, const float* v) {
	Def d = defOf(i);
	int p = isTone(i) ? i % NT : -1;
	if (p >= 0 && !wild) {
		switch (p) {
			case T_COARSE: return pickMusicalCoarse(r);
			case T_FINE: return std::round(r.bi() * 15.f);
			case T_PRAND: return std::round(r.uni() * 8.f);
			case T_PE_DEPTH: return std::round(r.uni() * 2.f);
			case T_L1_PITCH:
			case T_L2_PITCH: return std::round(r.bi() * 8.f);
		}
	}
	if (p == T_WAVE) {
		// waves: attack one-shots, moving and static loops, noise rarely; user slots only when loaded
		const Rom& rom = Rom::get();
		int tone = i / NT;
		float u = r.uni();
		int want = WK_LOOP;
		if (tone == 0) want = u < 0.55f ? WK_LOOP : WK_MOVING;
		else want = u < 0.3f ? WK_ATTACK : u < 0.6f ? WK_LOOP : u < 0.92f ? WK_MOVING : WK_NOISE;
		for (int tries = 0; tries < 200; tries++) {
			int w = (int) (r.uni() * rom.firstUser) % rom.firstUser;
			if (rom.waves[w].kind == want) return (float) w;
		}
		return 0.f;
	}
	(void) v;
	float lo = wild ? d.min : d.rlo, hi = wild ? d.max : d.rhi;
	if (d.group == G_PCAP && wild) { lo = d.min; hi = d.max; }
	float x = lo + (hi - lo) * r.uni();
	return d.snap ? std::round(x) : x;
}

inline void randomizePatch(float* v, float amt, int scope, bool wild, Rng& r) {
	amt = std::max(0.f, std::min(1.f, amt));
	for (int i = 0; i < NPV; i++) {
		if (!inScope(i, scope)) continue;
		Def d = defOf(i);
		if (d.group == G_LAYER) continue; // handled below
		if (d.group == G_AMP && scope != SC_ALL) continue;
		float tgt = randomTarget(i, r, wild, v);
		bool discrete = d.choices >= 0 || (isTone(i) && (i % NT == T_WAVE || i % NT == T_COARSE));
		if (discrete) {
			if (r.uni() < amt) v[i] = tgt;
		}
		else
			v[i] = clampTo(i, v[i] + amt * (tgt - v[i]));
	}
	// layers: only on a big roll of everything; at least one tone stays on and audible
	if (scope == SC_ALL && amt >= 0.5f) {
		for (int t = 1; t < 4; t++)
			if (r.uni() < amt * 0.5f) v[tp(t, T_ON)] = r.uni() < 0.55f ? 1.f : 0.f;
	}
	int on = 0;
	for (int t = 0; t < 4; t++) on += v[tp(t, T_ON)] > 0.5f;
	if (!on) v[tp(0, T_ON)] = 1.f;
	for (int t = 0; t < 4; t++)
		if (v[tp(t, T_ON)] > 0.5f) {
			v[tp(t, T_LEVEL)] = std::max(v[tp(t, T_LEVEL)], 50.f);
		}
	// never more than one loud drive stage
	if (v[cp(C_DS_ON)] > 0.5f) v[cp(C_DS_DRIVE)] = std::min(v[cp(C_DS_DRIVE)], 70.f);
}

// ================================================================ MUTATE (random walk offsets)
inline bool mutable_(int i, int scope) {
	if (!inScope(i, scope)) return false;
	Def d = defOf(i);
	if (d.group == G_LAYER || d.group == G_PCAP) return false;
	if (d.choices == CH_OFFON) return false; // effects and switches stay where you put them
	return true;
}

inline void mutateStep(const float* base, float* off, float amount, int scope, Rng& r) {
	for (int i = 0; i < NPV; i++) {
		if (!mutable_(i, scope)) continue;
		if (r.uni() >= amount) continue;
		Def d = defOf(i);
		float range = d.max - d.min;
		bool discrete = d.choices >= 0 || (isTone(i) && i % NT == T_WAVE);
		if (discrete) {
			if (r.uni() < 0.15f) {
				float step = r.uni() < 0.5f ? -1.f : 1.f;
				float nv = clampTo(i, base[i] + off[i] + step);
				if (isTone(i) && i % NT == T_WAVE) {
					const Rom& rom = Rom::get();
					int a = (int) clampTo(i, base[i]), b = (int) nv;
					if (b >= rom.firstUser || rom.waves[a].kind != rom.waves[b].kind) continue;
				}
				// waves wander at most 2 steps, other choices 1 step, from where you left them
				float lim = (isTone(i) && i % NT == T_WAVE) ? 2.f : 1.f;
				if (std::fabs(nv - base[i]) <= lim) off[i] = nv - base[i];
			}
		}
		else {
			float g = (r.uni() + r.uni() + r.uni() - 1.5f) * 2.f; // roughly gaussian
			off[i] += g * 0.04f * range;
			off[i] = std::max(-0.25f * range, std::min(0.25f * range, off[i]));
		}
	}
}

} // namespace facet
