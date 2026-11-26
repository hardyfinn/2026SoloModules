// Deidre's Latest Workout: eight clocked modulation outputs with every setting on the panel.
// The 23 knobs on the right always edit the selected output; the big screen shows all eight.
#include "plugin.hpp"
#include "DeidreEngine.hpp"
#include "Layout.hpp"

using namespace deidre;

static const int NUM_PAGES = 7;
static const char* PAGE_NAMES[NUM_PAGES] = {"HOME", "WAVE", "EUCLID", "LOOP", "LOGIC", "CV", "BANK"};
enum { PG_HOME, PG_WAVE, PG_EUCLID, PG_LOOP, PG_LOGIC, PG_CV, PG_BANK };

static NVGcolor laneColor(int i) {
	static const unsigned char c[8][3] = {{0xE4, 0x57, 0x2E}, {0xF3, 0xA7, 0x12}, {0xC9, 0xB2, 0x00}, {0x4C, 0xAF, 0x50},
		{0x17, 0xBE, 0xBB}, {0x2E, 0x86, 0xDE}, {0x8E, 0x5C, 0xD9}, {0xD9, 0x48, 0x9B}};
	return nvgRGB(c[i & 7][0], c[i & 7][1], c[i & 7][2]);
}
static NVGcolor cvColor(int src) {  // 1..6
	static const unsigned char c[6][3] = {{0xE4, 0x57, 0x2E}, {0x2E, 0x86, 0xDE}, {0x4C, 0xAF, 0x50}, {0x8E, 0x5C, 0xD9},
		{0xC9, 0xB2, 0x00}, {0x17, 0xBE, 0xBB}};
	int k = src - 1;
	if (k < 0) k = 0;
	if (k > 5) k = 5;
	return nvgRGB(c[k][0], c[k][1], c[k][2]);
}
static const char* cvSrcName(int s) {
	static const char* n[7] = {"--", "CV 1", "CV 2", "CV 3", "CV 4", "CLK", "RUN"};
	return n[s < 0 ? 0 : s > 6 ? 6 : s];
}

// where each per-output parameter sits in the knob bank
static void knobPos(int p, int& row, int& col) {
	if (p < P_LOOP) { row = p / 5; col = p % 5; }
	else if (p < P_CROSS) { row = 3; col = p - P_LOOP; }
	else { row = 4; col = p - P_CROSS; }
}
static int pageOfParam(int p) {
	int r, c;
	knobPos(p, r, c);
	static const int pg[5] = {PG_WAVE, PG_WAVE, PG_EUCLID, PG_LOOP, PG_LOGIC};
	return pg[r];
}

struct Deidre : Module {
	enum ParamId {
		ENUMS(EDIT_PARAM, NUM_P),
		BPM_PARAM, RUN_PARAM, RESET_PARAM, TAP_PARAM, RESEED_PARAM, COPY_PARAM,
		BANK_PREV_PARAM, BANK_NEXT_PARAM, SAVE_PARAM,
		ENUMS(PAGE_PARAM, NUM_PAGES),
		ENUMS(SELECT_PARAM, NUM_OUT),
		ENUMS(MUTE_PARAM, NUM_OUT),
		PARAMS_LEN
	};
	enum InputId { CLK_INPUT, RUN_INPUT, CV1_INPUT, CV2_INPUT, CV3_INPUT, CV4_INPUT, INPUTS_LEN };
	enum OutputId { ENUMS(OUT_OUTPUT, NUM_OUT), OUTPUTS_LEN };
	enum LightId {
		RUN_LIGHT, RESET_LIGHT, TAP_LIGHT, RESEED_LIGHT, COPY_LIGHT, BANK_PREV_LIGHT, BANK_NEXT_LIGHT, SAVE_LIGHT,
		INVERT_LIGHT,
		ENUMS(PAGE_LIGHT, NUM_PAGES),
		ENUMS(SELECT_LIGHT, NUM_OUT),
		ENUMS(MUTE_LIGHT, NUM_OUT),
		ENUMS(OUT_LIGHT, NUM_OUT),
		LIGHTS_LEN
	};

	deidre::Engine eng;
	int selected = 0;
	int page = PG_HOME;
	int focusP = P_RATE;          // last touched knob (DATA fine-tunes it, CV page cursor)
	bool followKnobs = true;      // screen jumps to the page of a touched knob
	bool copyArmed = false;
	int bank = 0;
	bool bankUsed[NUM_BANKS];
	OutConfig bankCfg[NUM_BANKS][NUM_OUT];
	float bankBpm[NUM_BANKS];
	float lastKnob[NUM_P];
	bool needSync = true;
	int userEdit = 0;             // which user scale the LOGIC page keyboard edits

	dsp::SchmittTrigger clkTrig, runTrig;
	dsp::BooleanTrigger runBtn, resetBtn, tapBtn, reseedBtn, copyBtn, prevBtn, nextBtn, saveBtn;
	dsp::BooleanTrigger pageBtn[NUM_PAGES], selBtn[NUM_OUT];
	dsp::ClockDivider lightDiv;
	float saveFlash = 0.f, reseedFlash = 0.f, resetFlash = 0.f, bankFlash = 0.f;
	double sinceTap = 1e9;
	double tapIntervals[3] = {0, 0, 0};
	int tapCount = 0;

	Deidre() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int p = 0; p < NUM_P; p++) {
			const PSpec& s = pspec(p);
			ParamQuantity* q;
			if (s.unit == 3) {
				std::vector<std::string> labels;
				for (int k = (int) s.min; k <= (int) s.max; k++) labels.push_back(valueText(p, (float) k));
				q = configSwitch(EDIT_PARAM + p, s.min, s.max, s.def, s.longName, labels);
			}
			else if (s.unit == 1) q = configParam(EDIT_PARAM + p, s.min, s.max, s.def, s.longName, "%", 0.f, 100.f);
			else if (s.unit == 2) q = configParam(EDIT_PARAM + p, s.min, s.max, s.def, s.longName, "\xC2\xB0", 0.f, 360.f);
			else if (s.unit == 4) q = configParam(EDIT_PARAM + p, s.min, s.max, s.def, s.longName, " beats");
			else q = configParam(EDIT_PARAM + p, s.min, s.max, s.def, s.longName);
			q->snapEnabled = s.stepped;
			if (p == P_LEVEL) q->randomizeEnabled = false;  // never make an output suddenly loud or silent
			lastKnob[p] = s.def;
		}
		configSwitch(EDIT_PARAM + P_INVERT, 0.f, 1.f, 0.f, "Invert", {"Off", "On"});
		configParam(BPM_PARAM, 10.f, 330.f, 120.f, "Tempo (when CLK is not patched)", " BPM");
		getParamQuantity(BPM_PARAM)->randomizeEnabled = false;
		configButton(RUN_PARAM, "Run / stop (stop also resets)");
		configButton(RESET_PARAM, "Reset to the first step");
		configButton(TAP_PARAM, "Tap tempo");
		configButton(RESEED_PARAM, "Reseed the selected output's random choices");
		configButton(COPY_PARAM, "Copy the selected output (then press an output's SELECT to paste)");
		configButton(BANK_PREV_PARAM, "Previous bank");
		configButton(BANK_NEXT_PARAM, "Next bank");
		configButton(SAVE_PARAM, "Save all outputs and the tempo to this bank");
		for (int i = 0; i < NUM_PAGES; i++) configButton(PAGE_PARAM + i, std::string("Screen page: ") + PAGE_NAMES[i]);
		for (int i = 0; i < NUM_OUT; i++) {
			configButton(SELECT_PARAM + i, string::f("Select output %d", i + 1));
			configSwitch(MUTE_PARAM + i, 0.f, 1.f, 0.f, string::f("Mute output %d", i + 1), {"Playing", "Muted"});
			getParamQuantity(MUTE_PARAM + i)->randomizeEnabled = false;
			configOutput(OUT_OUTPUT + i, string::f("Output %d (0-5 V)", i + 1));
		}
		configInput(CLK_INPUT, "Clock (24 PPQN by default), or CV 5, or next bank (BANK page)");
		configInput(RUN_INPUT, "Run gate, or reset, or CV 6, or previous bank (BANK page)");
		configInput(CV1_INPUT, "CV 1 (0-5 V)");
		configInput(CV2_INPUT, "CV 2 (0-5 V)");
		configInput(CV3_INPUT, "CV 3 (0-5 V)");
		configInput(CV4_INPUT, "CV 4 (0-5 V)");
		for (int b = 0; b < NUM_BANKS; b++) { bankUsed[b] = false; bankBpm[b] = 120.f; }
		lightDiv.setDivision(64);
		defaultPatch();
	}

	// a starting point that shows what the module does
	void defaultPatch() {
		for (int o = 0; o < NUM_OUT; o++) eng.cfg[o].reset(), eng.cfg[o].seed = 1 + o * 7919;
		static const int rate[8] = {22, 16, 20, 18, 14, 21, 10, 8};
		static const int shp[8] = {SH_PULSE, SH_SMOOTH_EXP, SH_SMOOTH_PULSE, SH_TRIANGLE, SH_SINE, SH_RATCHET2, SH_SMOOTH_RANDOM, SH_RANDOM};
		for (int o = 0; o < NUM_OUT; o++) { eng.cfg[o].v[P_RATE] = rate[o]; eng.cfg[o].v[P_SHAPE] = shp[o]; }
		eng.cfg[2].v[P_STEPS] = 8; eng.cfg[2].v[P_TRIGS] = 3; eng.cfg[2].v[P_WIDTH] = 0.4f;
		eng.cfg[1].v[P_WIDTH] = 0.3f;
		needSync = true;
	}

	void onReset() override {
		defaultPatch();
		for (int b = 0; b < NUM_BANKS; b++) bankUsed[b] = false;
		bank = 0; selected = 0; page = PG_HOME; copyArmed = false;
		eng.clkMode = CLK_CLOCK; eng.runMode = RUN_GATE; eng.ppqnIdx = PPQN_24; eng.runLatch = true;
		eng.resetClock();
	}

	void selectOutput(int o) {
		selected = clamp(o, 0, NUM_OUT - 1);
		needSync = true;
	}
	void syncKnobs() {
		for (int p = 0; p < NUM_P; p++) {
			float v = eng.cfg[selected].v[p];
			params[EDIT_PARAM + p].setValue(v);
			lastKnob[p] = v;
		}
		needSync = false;
	}
	// change a parameter of the selected output from the screen / DATA
	void setParam(int p, float v) {
		const PSpec& s = pspec(p);
		v = clamp(v, s.min, s.max);
		if (s.stepped) v = std::round(v);
		eng.cfg[selected].v[p] = v;
		params[EDIT_PARAM + p].setValue(v);
		lastKnob[p] = v;
	}
	void dataTurn(int steps) {
		if (page == PG_HOME) { selectOutput(((selected + steps) % NUM_OUT + NUM_OUT) % NUM_OUT); return; }
		if (page == PG_BANK) { gotoBank(bank + steps); return; }
		if (page == PG_CV) {
			OutConfig& c = eng.cfg[selected];
			c.cvAmt[focusP] = clamp(c.cvAmt[focusP] + steps * 0.01f, -1.f, 1.f);
			return;
		}
		const PSpec& s = pspec(focusP);
		float step = s.stepped ? 1.f : (s.max - s.min) * 0.01f;
		setParam(focusP, eng.cfg[selected].v[focusP] + steps * step);
	}

	void saveBank(int b) {
		for (int o = 0; o < NUM_OUT; o++) bankCfg[b][o] = eng.cfg[o];
		bankBpm[b] = params[BPM_PARAM].getValue();
		bankUsed[b] = true;
		saveFlash = 0.3f;
	}
	void loadBank(int b) {
		if (!bankUsed[b]) return;
		for (int o = 0; o < NUM_OUT; o++) eng.cfg[o] = bankCfg[b][o];
		params[BPM_PARAM].setValue(bankBpm[b]);
		needSync = true;
		bankFlash = 0.2f;
	}
	void gotoBank(int b) {
		bank = ((b % NUM_BANKS) + NUM_BANKS) % NUM_BANKS;
		loadBank(bank);
	}
	void copyTo(int dst) {
		if (dst == selected) return;
		bool keepMute = eng.cfg[dst].mute;
		eng.cfg[dst] = eng.cfg[selected];
		eng.cfg[dst].mute = keepMute;
	}
	void copyToAll() { for (int o = 0; o < NUM_OUT; o++) copyTo(o); }

	void process(const ProcessArgs& args) override {
		if (needSync) syncKnobs();
		// knobs -> selected output (only when a knob actually moved, so selecting never overwrites)
		for (int p = 0; p < NUM_P; p++) {
			float v = params[EDIT_PARAM + p].getValue();
			if (v != lastKnob[p]) {
				lastKnob[p] = v;
				eng.cfg[selected].v[p] = v;
				focusP = p;
				if (followKnobs && page != PG_CV) page = pageOfParam(p);
			}
		}
		eng.bpm = params[BPM_PARAM].getValue();

		deidre::Inputs in;
		in.cv[0] = inputs[CV1_INPUT].getVoltage();
		in.cv[1] = inputs[CV2_INPUT].getVoltage();
		in.cv[2] = inputs[CV3_INPUT].getVoltage();
		in.cv[3] = inputs[CV4_INPUT].getVoltage();
		float clkV = inputs[CLK_INPUT].getVoltage(), runV = inputs[RUN_INPUT].getVoltage();
		in.cv[4] = eng.clkMode == CLK_CV ? clkV : 0.f;
		in.cv[5] = eng.runMode == RUN_CV ? runV : 0.f;
		bool clkEdge = clkTrig.process(clkV, 0.1f, 0.7f);
		bool runEdge = runTrig.process(runV, 0.1f, 0.7f);
		in.clockConnected = inputs[CLK_INPUT].isConnected();
		in.clockEdge = clkEdge && (eng.clkMode == CLK_CLOCK || eng.clkMode == CLK_NEXT_BANK);
		in.runConnected = inputs[RUN_INPUT].isConnected() && eng.runMode == RUN_GATE;
		in.runGate = runTrig.isHigh();
		in.runToggle = runBtn.process(params[RUN_PARAM].getValue() > 0.f);
		bool rst = resetBtn.process(params[RESET_PARAM].getValue() > 0.f);
		if (eng.runMode == RUN_RESET && runEdge) rst = true;
		in.resetEdge = rst;
		if (rst) resetFlash = 0.15f;
		if (eng.runMode == RUN_PREV_BANK && runEdge) gotoBank(bank - 1);

		// buttons
		sinceTap += args.sampleTime;
		if (tapBtn.process(params[TAP_PARAM].getValue() > 0.f)) {
			if (sinceTap < 2.0) {
				tapIntervals[tapCount % 3] = sinceTap;
				tapCount++;
				int n = std::min(tapCount, 3);
				double sum = 0;
				for (int i = 0; i < n; i++) sum += tapIntervals[i];
				params[BPM_PARAM].setValue(clamp((float) (60.0 * n / sum), 10.f, 330.f));
			}
			else tapCount = 0;
			sinceTap = 0.0;
		}
		if (reseedBtn.process(params[RESEED_PARAM].getValue() > 0.f)) { eng.reseed(selected); reseedFlash = 0.2f; }
		if (copyBtn.process(params[COPY_PARAM].getValue() > 0.f)) copyArmed = !copyArmed;
		if (prevBtn.process(params[BANK_PREV_PARAM].getValue() > 0.f)) gotoBank(bank - 1);
		if (nextBtn.process(params[BANK_NEXT_PARAM].getValue() > 0.f)) gotoBank(bank + 1);
		if (saveBtn.process(params[SAVE_PARAM].getValue() > 0.f)) saveBank(bank);
		for (int i = 0; i < NUM_PAGES; i++)
			if (pageBtn[i].process(params[PAGE_PARAM + i].getValue() > 0.f)) page = i;
		for (int i = 0; i < NUM_OUT; i++) {
			if (selBtn[i].process(params[SELECT_PARAM + i].getValue() > 0.f)) {
				if (copyArmed) { copyTo(i); copyArmed = false; }
				else selectOutput(i);
			}
			eng.cfg[i].mute = params[MUTE_PARAM + i].getValue() > 0.5f;
		}

		int bs = eng.process(args.sampleTime, in);
		if (bs) gotoBank(bank + bs);

		for (int i = 0; i < NUM_OUT; i++) outputs[OUT_OUTPUT + i].setVoltage(eng.st[i].volts);

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			double beatFrac = eng.beat - std::floor(eng.beat);
			lights[RUN_LIGHT].setBrightness(eng.running ? 1.f : 0.f);
			lights[TAP_LIGHT].setBrightness(eng.running && beatFrac < 0.1 ? 1.f : 0.f);
			lights[RESET_LIGHT].setBrightness(resetFlash > 0.f ? 1.f : 0.f);
			lights[RESEED_LIGHT].setBrightness(reseedFlash > 0.f ? 1.f : 0.f);
			lights[COPY_LIGHT].setBrightness(copyArmed ? 1.f : 0.f);
			lights[SAVE_LIGHT].setBrightness(saveFlash > 0.f ? 1.f : (bankUsed[bank] ? 0.f : 0.f));
			lights[BANK_PREV_LIGHT].setBrightness(bankFlash > 0.f ? 0.8f : 0.f);
			lights[BANK_NEXT_LIGHT].setBrightness(bankFlash > 0.f ? 0.8f : 0.f);
			lights[INVERT_LIGHT].setBrightness(params[EDIT_PARAM + P_INVERT].getValue() > 0.5f ? 1.f : 0.f);
			resetFlash -= dt; reseedFlash -= dt; saveFlash -= dt; bankFlash -= dt;
			for (int i = 0; i < NUM_PAGES; i++) lights[PAGE_LIGHT + i].setBrightness(page == i ? 1.f : 0.f);
			bool blink = std::fmod(eng.beat * 2.0, 1.0) < 0.5;
			for (int i = 0; i < NUM_OUT; i++) {
				float b = i == selected ? 1.f : (eng.st[i].active ? 0.35f : 0.08f);
				if (copyArmed && i != selected) b = blink ? 0.6f : 0.1f;
				lights[SELECT_LIGHT + i].setBrightness(b);
				lights[MUTE_LIGHT + i].setBrightness(eng.cfg[i].mute ? 1.f : 0.f);
				lights[OUT_LIGHT + i].setBrightnessSmooth(eng.st[i].volts / 5.f, dt * 2.f);
			}
		}
	}

	// ---- saving
	static json_t* cfgToJson(const OutConfig& c) {
		json_t* j = json_object();
		json_t* v = json_array();
		json_t* cs = json_array();
		json_t* ca = json_array();
		for (int p = 0; p < NUM_P; p++) {
			json_array_append_new(v, json_real(c.v[p]));
			json_array_append_new(cs, json_integer(c.cvSrc[p]));
			json_array_append_new(ca, json_real(c.cvAmt[p]));
		}
		json_object_set_new(j, "v", v);
		json_object_set_new(j, "cvSrc", cs);
		json_object_set_new(j, "cvAmt", ca);
		json_object_set_new(j, "seed", json_integer(c.seed));
		return j;
	}
	static void cfgFromJson(OutConfig& c, json_t* j) {
		c.reset();
		if (!j) return;
		json_t* v = json_object_get(j, "v");
		json_t* cs = json_object_get(j, "cvSrc");
		json_t* ca = json_object_get(j, "cvAmt");
		for (int p = 0; p < NUM_P; p++) {
			if (v && json_array_get(v, p)) c.v[p] = json_number_value(json_array_get(v, p));
			if (cs && json_array_get(cs, p)) c.cvSrc[p] = json_integer_value(json_array_get(cs, p));
			if (ca && json_array_get(ca, p)) c.cvAmt[p] = json_number_value(json_array_get(ca, p));
		}
		json_t* s = json_object_get(j, "seed");
		if (s) c.seed = (uint32_t) json_integer_value(s);
	}
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "selected", json_integer(selected));
		json_object_set_new(root, "page", json_integer(page));
		json_object_set_new(root, "bank", json_integer(bank));
		json_object_set_new(root, "followKnobs", json_boolean(followKnobs));
		json_object_set_new(root, "clkMode", json_integer(eng.clkMode));
		json_object_set_new(root, "runMode", json_integer(eng.runMode));
		json_object_set_new(root, "ppqn", json_integer(eng.ppqnIdx));
		json_object_set_new(root, "running", json_boolean(eng.runLatch));
		json_t* us = json_array();
		for (int i = 0; i < NUM_USER; i++) json_array_append_new(us, json_integer(eng.user[i]));
		json_object_set_new(root, "userScales", us);
		json_t* outs = json_array();
		for (int o = 0; o < NUM_OUT; o++) json_array_append_new(outs, cfgToJson(eng.cfg[o]));
		json_object_set_new(root, "outputs", outs);
		json_t* banks = json_object();
		for (int b = 0; b < NUM_BANKS; b++) {
			if (!bankUsed[b]) continue;
			json_t* bj = json_object();
			json_object_set_new(bj, "bpm", json_real(bankBpm[b]));
			json_t* bo = json_array();
			for (int o = 0; o < NUM_OUT; o++) json_array_append_new(bo, cfgToJson(bankCfg[b][o]));
			json_object_set_new(bj, "outputs", bo);
			json_object_set_new(banks, std::to_string(b).c_str(), bj);
		}
		json_object_set_new(root, "banks", banks);
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "selected"))) selected = clamp((int) json_integer_value(j), 0, NUM_OUT - 1);
		if ((j = json_object_get(root, "page"))) page = clamp((int) json_integer_value(j), 0, NUM_PAGES - 1);
		if ((j = json_object_get(root, "bank"))) bank = clamp((int) json_integer_value(j), 0, NUM_BANKS - 1);
		if ((j = json_object_get(root, "followKnobs"))) followKnobs = json_boolean_value(j);
		if ((j = json_object_get(root, "clkMode"))) eng.clkMode = clamp((int) json_integer_value(j), 0, NUM_CLKMODES - 1);
		if ((j = json_object_get(root, "runMode"))) eng.runMode = clamp((int) json_integer_value(j), 0, NUM_RUNMODES - 1);
		if ((j = json_object_get(root, "ppqn"))) eng.ppqnIdx = clamp((int) json_integer_value(j), 0, NUM_PPQN - 1);
		if ((j = json_object_get(root, "running"))) eng.runLatch = json_boolean_value(j);
		if ((j = json_object_get(root, "userScales")))
			for (int i = 0; i < NUM_USER; i++)
				if (json_array_get(j, i)) eng.user[i] = (uint16_t) json_integer_value(json_array_get(j, i));
		if ((j = json_object_get(root, "outputs")))
			for (int o = 0; o < NUM_OUT; o++) cfgFromJson(eng.cfg[o], json_array_get(j, o));
		for (int b = 0; b < NUM_BANKS; b++) bankUsed[b] = false;
		if ((j = json_object_get(root, "banks"))) {
			const char* key;
			json_t* bj;
			json_object_foreach(j, key, bj) {
				int b = atoi(key);
				if (b < 0 || b >= NUM_BANKS) continue;
				bankUsed[b] = true;
				json_t* bp = json_object_get(bj, "bpm");
				bankBpm[b] = bp ? json_number_value(bp) : 120.f;
				json_t* bo = json_object_get(bj, "outputs");
				for (int o = 0; o < NUM_OUT; o++) cfgFromJson(bankCfg[b][o], bo ? json_array_get(bo, o) : nullptr);
			}
		}
		needSync = true;
	}
};

// ============================================================ screen
struct DeidreScreen : OpaqueWidget {
	Deidre* module = nullptr;
	std::string fontPath;
	// click targets of the current frame (mm, screen-local)
	struct Hit { float x, y, w, h; int kind, a, b; };
	std::vector<Hit> hits;
	enum { HK_TAB, HK_LANE, HK_PARAM, HK_CVSRC, HK_CVAMT, HK_USERNOTE, HK_USERSEL, HK_BANK, HK_SETTING };

	DeidreScreen() { fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf"); }

	void addHit(float x, float y, float w, float h, int kind, int a, int b = 0) {
		Hit k; k.x = x; k.y = y; k.w = w; k.h = h; k.kind = kind; k.a = a; k.b = b;
		hits.push_back(k);
	}

	// colours
	NVGcolor BG() { return nvgRGB(0x0E, 0x11, 0x16); }
	NVGcolor BAR() { return nvgRGB(0x1B, 0x20, 0x28); }
	NVGcolor TXT() { return nvgRGB(0xF4, 0xEF, 0xE6); }
	NVGcolor DIM() { return nvgRGB(0x8D, 0x96, 0xA3); }
	NVGcolor INFOC() { return nvgRGB(0x9F, 0xB3, 0xC8); }
	NVGcolor FAINT() { return nvgRGB(0x3A, 0x42, 0x4E); }

	void rect(NVGcontext* vg, float x, float y, float w, float h, NVGcolor c, float r = 0.f) {
		nvgBeginPath(vg);
		if (r > 0) nvgRoundedRect(vg, x, y, w, h, r); else nvgRect(vg, x, y, w, h);
		nvgFillColor(vg, c);
		nvgFill(vg);
	}
	void frame(NVGcontext* vg, float x, float y, float w, float h, NVGcolor c, float sw = 0.3f, float r = 0.6f) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, w, h, r);
		nvgStrokeColor(vg, c);
		nvgStrokeWidth(vg, sw);
		nvgStroke(vg);
	}
	void txt(NVGcontext* vg, float x, float y, const std::string& s, float size, NVGcolor c, int align = NVG_ALIGN_LEFT) {
		nvgFontSize(vg, size);
		nvgFillColor(vg, c);
		nvgTextAlign(vg, align | NVG_ALIGN_BASELINE);
		nvgText(vg, x, y, s.c_str(), nullptr);
	}

	// one output's 4-beat scope as a line
	void scope(NVGcontext* vg, const OutState* s, float x, float y, float w, float h, NVGcolor c, float sw = 0.35f) {
		nvgBeginPath(vg);
		for (int i = 0; i < SCOPE_N; i++) {
			float v = s ? s->scope[i] : 0.5f + 0.5f * std::sin(i * 0.2f);
			float px = x + w * i / (SCOPE_N - 1), py = y + h - clamp(v, 0.f, 1.f) * h;
			if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
		}
		nvgStrokeColor(vg, c);
		nvgStrokeWidth(vg, sw);
		nvgLineJoin(vg, NVG_ROUND);
		nvgStroke(vg);
	}

	void euclidRing(NVGcontext* vg, float cx, float cy, float r, const OutConfig& c, int cur, NVGcolor col, float dot) {
		int S = (int) c.v[P_STEPS], K = (int) c.v[P_TRIGS], L = S + (int) c.v[P_PAD];
		if (S <= 0) {
			txt(vg, cx, cy + 1.f, "OFF", 2.6f, DIM(), NVG_ALIGN_CENTER);
			return;
		}
		for (int k = 0; k < L; k++) {
			float t = (-90.f + 360.f * k / L) * (float) M_PI / 180.f;
			float px = cx + r * std::cos(t), py = cy + r * std::sin(t);
			bool hit = k < S && euclidHit(k, K, S);
			nvgBeginPath(vg);
			nvgCircle(vg, px, py, dot);
			if (hit) { nvgFillColor(vg, col); nvgFill(vg); }
			nvgStrokeColor(vg, k < S ? col : FAINT());
			nvgStrokeWidth(vg, 0.3f);
			nvgStroke(vg);
			if (k == cur) {
				nvgBeginPath(vg);
				nvgCircle(vg, px, py, dot + 0.8f);
				nvgStrokeColor(vg, TXT());
				nvgStroke(vg);
			}
		}
		txt(vg, cx, cy + 1.f, string::f("%d/%d", K, S), 2.8f, TXT(), NVG_ALIGN_CENTER);
	}

	// list of parameter values (clickable), returns next y
	float paramList(NVGcontext* vg, const OutConfig& c, const float* eff, int focus, const int* ps, int n, float x, float y, float w, float rowH) {
		for (int i = 0; i < n; i++) {
			int p = ps[i];
			float yy = y + i * rowH;
			bool f = p == focus;
			if (f) rect(vg, x - 0.8f, yy - rowH + 0.9f, w + 1.6f, rowH, nvgRGB(0x26, 0x30, 0x3D), 0.5f);
			txt(vg, x, yy, pspec(p).name, 2.3f, f ? TXT() : DIM());
			std::string val = valueText(p, c.v[p]);
			if (c.cvSrc[p]) {
				float e = eff ? eff[p] : c.v[p];
				val = valueText(p, e);
				nvgBeginPath(vg);
				nvgCircle(vg, x + w + 0.2f, yy - 0.8f, 0.7f);
				nvgFillColor(vg, cvColor(c.cvSrc[p]));
				nvgFill(vg);
			}
			txt(vg, x + w - 1.2f, yy, val, 2.4f, TXT(), NVG_ALIGN_RIGHT);
			addHit(x - 0.8f, yy - rowH + 0.9f, w + 1.6f, rowH, HK_PARAM, p);
		}
		return y + n * rowH;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { OpaqueWidget::drawLayer(args, layer); return; }
		NVGcontext* vg = args.vg;
		std::shared_ptr<window::Font> font = APP->window->loadFont(fontPath);
		if (!font) return;
		nvgFontFaceId(vg, font->handle);
		float s = mm2px(1.f);
		nvgSave(vg);
		nvgScale(vg, s, s);
		hits.clear();
		draw(vg);
		nvgRestore(vg);
		OpaqueWidget::drawLayer(args, layer);
	}

	void draw(NVGcontext* vg) {
		const float W = lay::SW, H = lay::SH;
		rect(vg, 0, 0, W, H, BG(), 1.f);
		Deidre* m = module;
		deidre::Engine dummy;
		const deidre::Engine& e = m ? m->eng : dummy;
		int sel = m ? m->selected : 2;
		int page = m ? m->page : PG_HOME;
		int focus = m ? m->focusP : P_RATE;
		if (!m) {  // library preview
			dummy.cfg[2].v[P_STEPS] = 8; dummy.cfg[2].v[P_TRIGS] = 3;
			static const int rate[8] = {22, 16, 20, 18, 14, 21, 10, 8};
			for (int o = 0; o < 8; o++) dummy.cfg[o].v[P_RATE] = rate[o];
		}

		// header
		rect(vg, 0, 0, W, 6, BAR(), 1.f);
		int bank = m ? m->bank : 0;
		txt(vg, 2, 4.3f, string::f("BANK %02d%s", bank + 1, (m && m->bankUsed[bank]) ? "" : " (empty)"), 2.4f, INFOC());
		bool ext = m && e.extActive;
		txt(vg, 58, 4.4f, string::f("%.1f BPM", e.tempo()), 2.9f, TXT(), NVG_ALIGN_CENTER);
		txt(vg, 74, 4.3f, e.running ? (ext ? "> EXT" : "> RUN") : "STOP", 2.4f, e.running ? nvgRGB(0x7F, 0xD0, 0x8A) : nvgRGB(0xE4, 0x57, 0x2E));
		long long bt = (long long) std::floor(e.beat);
		txt(vg, W - 2, 4.3f, string::f("BAR %lld.%lld", bt / 4 + 1, bt % 4 + 1), 2.4f, INFOC(), NVG_ALIGN_RIGHT);
		if (m && m->copyArmed) {
			rect(vg, 30, 0.6f, 40, 4.8f, nvgRGB(0xF3, 0xA7, 0x12), 0.6f);
			txt(vg, 50, 4.2f, string::f("COPY OUT %d > press SELECT", sel + 1), 2.2f, BG(), NVG_ALIGN_CENTER);
		}

		// body
		switch (page) {
			case PG_HOME: drawHome(vg, e, sel); break;
			case PG_WAVE: drawWave(vg, e, sel, focus); break;
			case PG_EUCLID: drawEuclid(vg, e, sel, focus); break;
			case PG_LOOP: drawLoop(vg, e, sel, focus); break;
			case PG_LOGIC: drawLogic(vg, e, sel, focus); break;
			case PG_CV: drawCV(vg, e, sel, focus); break;
			case PG_BANK: drawBank(vg, e); break;
		}

		// tabs
		float ty = H - 6;
		rect(vg, 0, ty, W, 6, BAR(), 1.f);
		for (int k = 0; k < NUM_PAGES; k++) {
			float x = lay::PAGE_X0 - lay::SX + k * lay::PAGE_DX;
			if (k == page) rect(vg, x - 6.6f, ty + 0.8f, 13.2f, 4.4f, TXT(), 0.8f);
			txt(vg, x, ty + 4.f, PAGE_NAMES[k], 2.3f, k == page ? BG() : INFOC(), NVG_ALIGN_CENTER);
			addHit(x - 7.2f, ty, 14.4f, 6, HK_TAB, k);
		}
	}

	void detailBox(NVGcontext* vg, const deidre::Engine& e, int sel, float x, float y, float w, float h) {
		const OutConfig& c = e.cfg[sel];
		NVGcolor col = laneColor(sel);
		rect(vg, x, y, w, h, nvgRGB(0x15, 0x1A, 0x21), 1.f);
		frame(vg, x, y, w, h, col, 0.35f, 1.f);
		txt(vg, x + 2, y + 4.5f, string::f("OUT %d", sel + 1), 3.f, col);
		txt(vg, x + w - 2, y + 4.5f, std::string(rateName((int) c.v[P_RATE])) + " " + shapeName((int) c.v[P_SHAPE]), 2.1f, TXT(), NVG_ALIGN_RIGHT);
		euclidRing(vg, x + 10.5f, y + 17.f, 7.f, c, e.st[sel].curStep, col, 1.3f);
		txt(vg, x + 10.5f, y + 28.f, "EUCLID", 1.9f, DIM(), NVG_ALIGN_CENTER);
		static const int ps[9] = {P_WIDTH, P_LEVEL, P_OFFSET, P_PHASE, P_PROB, P_FLEX, P_CROSS, P_SCALE, P_LOOP};
		paramList(vg, c, e.st[sel].eff, -1, ps, 9, x + 21.5f, y + 9.f, w - 23.5f, 4.6f);
		// big-ish scope
		rect(vg, x + 2, y + 31.5f, 17, 18, BG());
		scope(vg, &e.st[sel], x + 2.5f, y + 33.f, 16, 15, col, 0.3f);
		txt(vg, x + 10.5f, y + h - 1.2f, "4 BEATS", 1.8f, DIM(), NVG_ALIGN_CENTER);
	}

	void drawHome(NVGcontext* vg, const deidre::Engine& e, int sel) {
		float LX = 2, LY = 8, LH = 6.6f;
		for (int i = 0; i < NUM_OUT; i++) {
			float y = LY + i * LH;
			const OutConfig& c = e.cfg[i];
			NVGcolor col = laneColor(i);
			if (i == sel) {
				rect(vg, 0.6f, y - 0.4f, 72.4f, LH - 0.4f, nvgRGB(0x26, 0x30, 0x3D));
				frame(vg, 0.6f, y - 0.4f, 72.4f, LH - 0.4f, col, 0.35f, 0.3f);
			}
			rect(vg, LX, y + 0.6f, 3.6f, 4.4f, col, 0.5f);
			txt(vg, LX + 1.8f, y + 3.9f, string::f("%d", i + 1), 2.6f, BG(), NVG_ALIGN_CENTER);
			txt(vg, LX + 5, y + 3.9f, rateName((int) e.st[i].eff[P_RATE]), 2.6f, TXT());
			scope(vg, &e.st[i], LX + 13, y + 0.8f, 40, 4.2f, c.mute ? FAINT() : col, 0.4f);
			txt(vg, LX + 55, y + 3.6f, c.mute ? "MUTE" : shapeName((int) c.v[P_SHAPE]), 2.0f, DIM());
			addHit(0, y - 0.4f, 73, LH, HK_LANE, i);
		}
		detailBox(vg, e, sel, 76, 8, W_() - 78, 56.5f);
	}
	float W_() { return lay::SW; }

	void drawWave(NVGcontext* vg, const deidre::Engine& e, int sel, int focus) {
		const OutConfig& c = e.cfg[sel];
		NVGcolor col = laneColor(sel);
		txt(vg, 2, 11, string::f("OUT %d  TIME + WAVE", sel + 1), 2.8f, col);
		rect(vg, 2, 13, 66, 50, nvgRGB(0x12, 0x16, 0x1C));
		for (int b = 1; b < 4; b++) rect(vg, 2 + 66.f * b / 4, 13, 0.2f, 50, FAINT());
		scope(vg, &e.st[sel], 3, 15, 64, 46, col, 0.5f);
		txt(vg, 35, 66.f - 0.6f, "last 4 beats", 1.9f, DIM(), NVG_ALIGN_CENTER);
		static const int t[5] = {P_RATE, P_PHASE, P_PROB, P_FLEX, P_AMOUNT};
		static const int w[5] = {P_SHAPE, P_WIDTH, P_SLEW, P_LEVEL, P_OFFSET};
		txt(vg, 72, 11, "TIME", 2.2f, nvgRGB(0xF6, 0xB9, 0x99));
		paramList(vg, c, e.st[sel].eff, focus, t, 5, 72, 15.5f, 45, 4.5f);
		txt(vg, 72, 41, "WAVE", 2.2f, nvgRGB(0xF8, 0xD7, 0x7D));
		paramList(vg, c, e.st[sel].eff, focus, w, 5, 72, 45.5f, 45, 4.5f);
	}

	void drawEuclid(NVGcontext* vg, const deidre::Engine& e, int sel, int focus) {
		const OutConfig& c = e.cfg[sel];
		NVGcolor col = laneColor(sel);
		txt(vg, 2, 11, string::f("OUT %d  EUCLID", sel + 1), 2.8f, col);
		euclidRing(vg, 20, 35, 16, c, e.st[sel].curStep, col, 1.8f);
		static const int ps[5] = {P_STEPS, P_TRIGS, P_PAD, P_RAND, P_ESHIFT};
		// values under the ring in one line
		for (int i = 0; i < 5; i++) {
			float x = 2 + i * 7.6f;
			bool f = ps[i] == focus;
			txt(vg, x + 3.6f, 59.f, pspec(ps[i]).name, 1.8f, f ? TXT() : DIM(), NVG_ALIGN_CENTER);
			txt(vg, x + 3.6f, 63.f, valueText(ps[i], c.v[ps[i]]), 2.3f, TXT(), NVG_ALIGN_CENTER);
			addHit(x, 56, 7.6f, 8, HK_PARAM, ps[i]);
		}
		// all eight patterns as rows
		txt(vg, 42, 11, "ALL OUTPUTS", 2.0f, DIM());
		for (int o = 0; o < NUM_OUT; o++) {
			const OutConfig& oc = e.cfg[o];
			float y = 16 + o * 6;
			NVGcolor lc = laneColor(o);
			if (o == sel) rect(vg, 40.5f, y - 3.f, 78, 5.6f, nvgRGB(0x26, 0x30, 0x3D));
			rect(vg, 42, y - 2.2f, 3.2f, 3.8f, lc, 0.5f);
			txt(vg, 43.6f, y + 0.8f, string::f("%d", o + 1), 2.3f, BG(), NVG_ALIGN_CENTER);
			int S = (int) oc.v[P_STEPS], K = (int) oc.v[P_TRIGS], L = S + (int) oc.v[P_PAD];
			if (S <= 0) { txt(vg, 48, y + 0.8f, "every step", 2.f, DIM()); }
			else {
				int show = std::min(L, 32);
				float dx = std::min(2.2f, 70.f / show);
				for (int k = 0; k < show; k++) {
					bool hit = k < S && euclidHit(k, K, S);
					nvgBeginPath(vg);
					nvgRect(vg, 48 + k * dx, y - 1.6f, dx * 0.75f, 2.6f);
					if (hit) { nvgFillColor(vg, lc); nvgFill(vg); }
					else { nvgStrokeColor(vg, k < S ? nvgTransRGBA(lc, 140) : FAINT()); nvgStrokeWidth(vg, 0.2f); nvgStroke(vg); }
					if (k == e.st[o].curStep) rect(vg, 48 + k * dx, y + 1.4f, dx * 0.75f, 0.5f, TXT());
				}
			}
			addHit(40.5f, y - 3.f, 78, 5.6f, HK_LANE, o);
		}
	}

	void drawLoop(NVGcontext* vg, const deidre::Engine& e, int sel, int focus) {
		const OutConfig& c = e.cfg[sel];
		NVGcolor col = laneColor(sel);
		txt(vg, 2, 11, string::f("OUT %d  LOOP", sel + 1), 2.8f, col);
		int loop = (int) e.st[sel].eff[P_LOOP];
		static const int ps[4] = {P_LOOP, P_NAP, P_WAKE, P_LSHIFT};
		paramList(vg, c, e.st[sel].eff, focus, ps, 4, 2, 17.f, 36, 4.6f);
		if (loop <= 0) {
			txt(vg, 45, 20, "LOOP is off: the output runs freely.", 2.2f, DIM());
			txt(vg, 45, 24, "Set LOOP (beats) to repeat its random,", 2.2f, DIM());
			txt(vg, 45, 28, "flex and euclid choices, then NAP and", 2.2f, DIM());
			txt(vg, 45, 32, "WAKE to rest it for whole loops.", 2.2f, DIM());
		}
		else {
			// beats of the loop with playhead
			double inLoop = std::fmod(e.beat, (double) loop);
			txt(vg, 45, 15, string::f("%d BEATS", loop), 2.1f, DIM());
			float bw = 72.f / loop;
			for (int b = 0; b < loop; b++) {
				bool now = (int) inLoop == b;
				rect(vg, 45 + b * bw, 17, std::max(bw - 0.4f, 0.3f), 4, now ? col : nvgRGB(0x26, 0x30, 0x3D));
			}
			rect(vg, 45 + (float) (inLoop / loop) * 72.f, 16, 0.4f, 6, TXT());
			// nap / wake cycle
			int nap = (int) e.st[sel].eff[P_NAP], wake = (int) e.st[sel].eff[P_WAKE], sh = (int) e.st[sel].eff[P_LSHIFT];
			txt(vg, 45, 28, nap > 0 ? string::f("WAKE %d  NAP %d  (cycle %d loops)", wake, nap, wake + nap) : "NAP off: plays every loop", 2.1f, DIM());
			long long li = (long long) std::floor(e.beat / loop);
			int cyc = nap > 0 ? wake + nap : 1;
			int show = std::min(cyc * 2, 24);
			float cw = std::min(6.f, 72.f / std::max(show, 1));
			for (int k = 0; k < show; k++) {
				long long L = li + k;
				bool awake = nap <= 0 || (((L + sh) % cyc + cyc) % cyc) < wake;
				float x = 45 + k * cw;
				nvgBeginPath(vg);
				nvgRect(vg, x, 30, cw - 0.6f, 5);
				if (awake) { nvgFillColor(vg, k == 0 ? col : nvgTransRGBA(col, 120)); nvgFill(vg); }
				else { nvgStrokeColor(vg, FAINT()); nvgStrokeWidth(vg, 0.3f); nvgStroke(vg); }
			}
			txt(vg, 45, 39, "now >", 1.9f, DIM());
		}
		// everyone's loops
		txt(vg, 2, 40, "ALL OUTPUTS", 2.0f, DIM());
		for (int o = 0; o < NUM_OUT; o++) {
			const OutConfig& oc = e.cfg[o];
			float x = 2 + (o % 4) * 29, y = 45 + (o / 4) * 9;
			rect(vg, x, y - 2.6f, 3.2f, 3.8f, laneColor(o), 0.5f);
			txt(vg, x + 1.6f, y + 0.4f, string::f("%d", o + 1), 2.2f, BG(), NVG_ALIGN_CENTER);
			int l = (int) oc.v[P_LOOP];
			std::string t = l ? string::f("%db", l) : "free";
			if (l && oc.v[P_NAP] > 0) t += string::f(" W%d N%d", (int) oc.v[P_WAKE], (int) oc.v[P_NAP]);
			txt(vg, x + 4.6f, y + 0.4f, t, 2.2f, e.st[o].sleeping ? DIM() : TXT());
			if (e.st[o].sleeping) txt(vg, x + 4.6f, y + 3.6f, "napping", 1.8f, DIM());
			addHit(x, y - 3, 28, 8, HK_LANE, o);
		}
	}

	void drawLogic(NVGcontext* vg, const deidre::Engine& e, int sel, int focus) {
		const OutConfig& c = e.cfg[sel];
		NVGcolor col = laneColor(sel);
		txt(vg, 2, 11, string::f("OUT %d  LOGIC + PITCH", sel + 1), 2.8f, col);
		static const int ps[4] = {P_CROSS, P_SOURCE, P_SCALE, P_INVERT};
		paramList(vg, c, e.st[sel].eff, focus, ps, 4, 2, 17.f, 40, 4.6f);
		// routing of all outputs
		txt(vg, 48, 11, "CROSS ROUTING", 2.0f, DIM());
		for (int o = 0; o < NUM_OUT; o++) {
			const OutConfig& oc = e.cfg[o];
			float y = 16 + o * 4.4f;
			if (o == sel) rect(vg, 47, y - 3.2f, 71, 4.3f, nvgRGB(0x26, 0x30, 0x3D));
			rect(vg, 48, y - 2.6f, 3.2f, 3.4f, laneColor(o), 0.5f);
			txt(vg, 49.6f, y, string::f("%d", o + 1), 2.1f, BG(), NVG_ALIGN_CENTER);
			int op = (int) oc.v[P_CROSS];
			if (op == CR_NONE) txt(vg, 53, y, "-", 2.1f, FAINT());
			else {
				int src = (int) oc.v[P_SOURCE];
				txt(vg, 53, y, std::string(crossName(op)) + "  < " + sourceName(src), 2.1f, TXT());
				if (src < 8) rect(vg, 85, y - 2.2f, 2.4f, 2.4f, laneColor(src), 0.4f);
			}
			txt(vg, 116, y, (int) oc.v[P_SCALE] ? scaleName((int) oc.v[P_SCALE]) : "", 1.8f, DIM(), NVG_ALIGN_RIGHT);
			addHit(47, y - 3.2f, 71, 4.3f, HK_LANE, o);
		}
		// keyboard: the scale of the selected output, editable for USER 1-3
		int sc = (int) c.v[P_SCALE];
		int ue = m_userEdit();
		bool isUser = sc >= SC_USER1;
		if (isUser) ue = sc - SC_USER1;
		uint16_t mask = isUser ? e.user[ue] : (sc > 0 ? scaleMask(sc) : 0);
		txt(vg, 2, 40.5f, isUser ? string::f("USER %d: click keys to edit", ue + 1) : (sc > 0 ? std::string("SCALE ") + scaleName(sc) : "QUANTISER OFF"), 2.1f, DIM());
		static const bool black[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
		static const char* nn[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		for (int k = 0; k < 12; k++) {
			float x = 2 + k * 3.6f;
			bool on = mask & (1 << k);
			rect(vg, x, black[k] ? 43 : 45, 3.2f, black[k] ? 8 : 10, on ? col : (black[k] ? nvgRGB(0x22, 0x26, 0x2E) : nvgRGB(0x3A, 0x42, 0x4E)), 0.4f);
			txt(vg, x + 1.6f, 59, nn[k], 1.6f, on ? TXT() : FAINT(), NVG_ALIGN_CENTER);
			if (isUser) addHit(x, 43, 3.2f, 12, HK_USERNOTE, ue, k);
		}
	}
	int m_userEdit() { return module ? module->userEdit : 0; }

	void drawCV(NVGcontext* vg, const deidre::Engine& e, int sel, int focus) {
		const OutConfig& c = e.cfg[sel];
		NVGcolor col = laneColor(sel);
		txt(vg, 2, 11, string::f("OUT %d  CV ASSIGN", sel + 1), 2.8f, col);
		txt(vg, 118, 11, "click source to change, DATA = amount", 1.9f, DIM(), NVG_ALIGN_RIGHT);
		// meters
		Deidre* m = module;
		for (int k = 0; k < 6; k++) {
			float v = 0.f;
			if (m) {
				if (k < 4) v = m->inputs[Deidre::CV1_INPUT + k].getVoltage();
				else if (k == 4 && e.clkMode == CLK_CV) v = m->inputs[Deidre::CLK_INPUT].getVoltage();
				else if (k == 5 && e.runMode == RUN_CV) v = m->inputs[Deidre::RUN_INPUT].getVoltage();
			}
			float x = 2 + k * 19.5f;
			txt(vg, x, 16, cvSrcName(k + 1), 2.f, cvColor(k + 1));
			rect(vg, x + 7, 13.6f, 11, 2.2f, nvgRGB(0x22, 0x26, 0x2E));
			rect(vg, x + 7, 13.6f, 11 * clamp(v / 5.f, 0.f, 1.f), 2.2f, cvColor(k + 1));
		}
		// two columns of parameters
		for (int p = 0; p < NUM_P; p++) {
			int colI = p < 12 ? 0 : 1, row = p < 12 ? p : p - 12;
			float x = 2 + colI * 59, y = 22 + row * 3.75f;
			bool f = p == focus;
			if (f) rect(vg, x - 0.8f, y - 2.9f, 57.5f, 3.75f, nvgRGB(0x26, 0x30, 0x3D), 0.4f);
			txt(vg, x, y, pspec(p).name, 2.1f, f ? TXT() : DIM());
			int src = c.cvSrc[p];
			if (src) rect(vg, x + 14.5f, y - 2.5f, 8.5f, 3.f, cvColor(src), 0.5f);
			else frame(vg, x + 14.5f, y - 2.5f, 8.5f, 3.f, FAINT(), 0.25f, 0.5f);
			txt(vg, x + 18.75f, y - 0.2f, cvSrcName(src), 1.9f, src ? BG() : DIM(), NVG_ALIGN_CENTER);
			addHit(x + 14.5f, y - 2.9f, 8.5f, 3.75f, HK_CVSRC, p);
			// amount bar (-100..+100)
			float bx = x + 25, bw = 22, a = c.cvAmt[p];
			rect(vg, bx, y - 1.9f, bw, 1.6f, nvgRGB(0x22, 0x26, 0x2E));
			rect(vg, bx + bw / 2, y - 2.3f, 0.2f, 2.4f, FAINT());
			float ax = bx + bw / 2, aw = a * bw / 2;
			rect(vg, aw < 0 ? ax + aw : ax, y - 1.9f, std::fabs(aw), 1.6f, src ? cvColor(src) : FAINT());
			txt(vg, x + 56, y, string::f("%+d", (int) std::lround(a * 100)), 1.9f, src ? TXT() : FAINT(), NVG_ALIGN_RIGHT);
			addHit(bx, y - 2.9f, 31, 3.75f, HK_CVAMT, p);
			addHit(x - 0.8f, y - 2.9f, 15, 3.75f, HK_PARAM, p);
		}
	}

	void drawBank(NVGcontext* vg, const deidre::Engine& e) {
		Deidre* m = module;
		txt(vg, 2, 11, "BANKS  click to load, SAVE stores here", 2.4f, TXT());
		for (int b = 0; b < NUM_BANKS; b++) {
			float x = 2 + (b % 8) * 8.6f, y = 14 + (b / 8) * 6.f;
			bool used = m && m->bankUsed[b], cur = m ? m->bank == b : b == 0;
			rect(vg, x, y, 7.8f, 5.2f, used ? nvgRGB(0x2E, 0x86, 0xDE) : nvgRGB(0x1B, 0x20, 0x28), 0.5f);
			if (cur) frame(vg, x - 0.3f, y - 0.3f, 8.4f, 5.8f, TXT(), 0.4f, 0.6f);
			txt(vg, x + 3.9f, y + 3.6f, string::f("%02d", b + 1), 2.1f, used ? TXT() : DIM(), NVG_ALIGN_CENTER);
			addHit(x, y, 7.8f, 5.2f, HK_BANK, b);
		}
		// settings
		txt(vg, 74, 16, "SETUP (click to change)", 2.0f, DIM());
		const char* names[4] = {"CLK IN", "PPQN", "RUN IN", "TEMPO"};
		std::string vals[4] = {clkModeName(e.clkMode), string::f("%d", ppqnValue(e.ppqnIdx)), runModeName(e.runMode),
			e.extActive ? string::f("EXT %.1f", e.extBpm) : string::f("INT %.1f", e.bpm)};
		for (int k = 0; k < 4; k++) {
			float y = 22 + k * 6.f;
			rect(vg, 73, y - 4.f, 45, 5.2f, nvgRGB(0x15, 0x1A, 0x21), 0.5f);
			txt(vg, 75, y, names[k], 2.1f, DIM());
			txt(vg, 116, y, vals[k], 2.3f, TXT(), NVG_ALIGN_RIGHT);
			if (k < 3) addHit(73, y - 4.f, 45, 5.2f, HK_SETTING, k);
		}
		txt(vg, 74, 49, "CLK as NEXT BANK and RUN as PREV BANK", 1.8f, DIM());
		txt(vg, 74, 52, "step through banks with triggers.", 1.8f, DIM());
		txt(vg, 74, 56, "Right-click the panel: copy to all,", 1.8f, DIM());
		txt(vg, 74, 59, "init output, clear banks.", 1.8f, DIM());
	}

	// ---- mouse
	void onButton(const ButtonEvent& ev) override {
		if (ev.action != GLFW_PRESS || ev.button != GLFW_MOUSE_BUTTON_LEFT || !module) { OpaqueWidget::onButton(ev); return; }
		float s = mm2px(1.f);
		float x = ev.pos.x / s, y = ev.pos.y / s;
		Deidre* m = module;
		for (int i = (int) hits.size() - 1; i >= 0; i--) {
			const Hit& h = hits[i];
			if (x < h.x || x > h.x + h.w || y < h.y || y > h.y + h.h) continue;
			switch (h.kind) {
				case HK_TAB: m->page = h.a; break;
				case HK_LANE: m->selectOutput(h.a); break;
				case HK_PARAM: m->focusP = h.a; break;
				case HK_CVSRC: {
					m->focusP = h.a;
					int& src = m->eng.cfg[m->selected].cvSrc[h.a];
					src = (src + 1) % 7;
					break;
				}
				case HK_CVAMT: {
					m->focusP = h.a;
					float bx = h.x, bw = 22;
					float a = clamp(((x - bx) - bw / 2) / (bw / 2), -1.f, 1.f);
					m->eng.cfg[m->selected].cvAmt[h.a] = std::round(a * 100.f) / 100.f;
					break;
				}
				case HK_USERNOTE: m->eng.user[h.a] ^= (uint16_t) (1 << h.b); break;
				case HK_BANK: m->bank = h.a; m->loadBank(h.a); break;
				case HK_SETTING:
					if (h.a == 0) m->eng.clkMode = (m->eng.clkMode + 1) % NUM_CLKMODES;
					if (h.a == 1) m->eng.ppqnIdx = (m->eng.ppqnIdx + 1) % NUM_PPQN;
					if (h.a == 2) m->eng.runMode = (m->eng.runMode + 1) % NUM_RUNMODES;
					break;
			}
			ev.consume(this);
			return;
		}
		OpaqueWidget::onButton(ev);
	}
	void onHoverScroll(const HoverScrollEvent& ev) override {
		if (!module) return;
		int steps = ev.scrollDelta.y > 0 ? 1 : ev.scrollDelta.y < 0 ? -1 : 0;
		if (steps) { module->dataTurn(steps); ev.consume(this); }
	}
};

// ============================================================ DATA encoder (endless)
struct DataEncoder : OpaqueWidget {
	Deidre* module = nullptr;
	float angle = 0.f, accum = 0.f;
	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float r = box.size.x / 2;
		nvgBeginPath(vg); nvgCircle(vg, r, r, r); nvgFillColor(vg, nvgRGB(0x9C, 0x95, 0x8A)); nvgFill(vg);
		nvgBeginPath(vg); nvgCircle(vg, r, r, r * 0.9f); nvgFillColor(vg, nvgRGB(0x2A, 0x28, 0x26)); nvgFill(vg);
		for (int k = 0; k < 18; k++) {  // grip
			float t = angle + k * 2.f * (float) M_PI / 18;
			nvgBeginPath(vg);
			nvgCircle(vg, r + r * 0.8f * std::cos(t), r + r * 0.8f * std::sin(t), r * 0.06f);
			nvgFillColor(vg, nvgRGB(0x4A, 0x46, 0x42)); nvgFill(vg);
		}
		nvgBeginPath(vg); nvgCircle(vg, r, r, r * 0.62f); nvgFillColor(vg, nvgRGB(0x3A, 0x37, 0x34)); nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, r + r * 0.15f * std::cos(angle), r + r * 0.15f * std::sin(angle));
		nvgLineTo(vg, r + r * 0.58f * std::cos(angle), r + r * 0.58f * std::sin(angle));
		nvgStrokeColor(vg, nvgRGB(0xF4, 0xEF, 0xE6)); nvgStrokeWidth(vg, r * 0.12f); nvgLineCap(vg, NVG_ROUND); nvgStroke(vg);
	}
	void turn(float d) {
		accum += d;
		while (accum >= 1.f) { accum -= 1.f; if (module) module->dataTurn(1); angle += 0.35f; }
		while (accum <= -1.f) { accum += 1.f; if (module) module->dataTurn(-1); angle -= 0.35f; }
	}
	void onButton(const ButtonEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) e.consume(this);
		OpaqueWidget::onButton(e);
	}
	void onDragStart(const DragStartEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) APP->window->cursorLock();
	}
	void onDragEnd(const DragEndEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) APP->window->cursorUnlock();
	}
	void onDragMove(const DragMoveEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		turn((e.mouseDelta.x - e.mouseDelta.y) / 12.f);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		turn(e.scrollDelta.y > 0 ? 1.f : e.scrollDelta.y < 0 ? -1.f : 0.f);
		e.consume(this);
	}
};

// ============================================================ knobs with a CV menu
template <typename TBase>
struct CvKnob : TBase {
	int p = 0;
	void appendContextMenu(Menu* menu) override {
		Deidre* m = dynamic_cast<Deidre*>(this->module);
		if (!m) return;
		int pp = p;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("CV source (this output)",
			{"None", "CV 1", "CV 2", "CV 3", "CV 4", "CLK (set CLK IN to CV)", "RUN (set RUN IN to CV)"},
			[=]() { return m->eng.cfg[m->selected].cvSrc[pp]; },
			[=](size_t i) { m->eng.cfg[m->selected].cvSrc[pp] = (int) i; }));
		menu->addChild(createSubmenuItem("CV amount", string::f("%+d%%", (int) std::lround(m->eng.cfg[m->selected].cvAmt[pp] * 100)), [=](Menu* sub) {
			static const int amts[9] = {-100, -50, -25, -10, 10, 25, 50, 75, 100};
			for (int k = 0; k < 9; k++) {
				int a = amts[k];
				sub->addChild(createCheckMenuItem(string::f("%+d%%", a), "",
					[=]() { return (int) std::lround(m->eng.cfg[m->selected].cvAmt[pp] * 100) == a; },
					[=]() { m->eng.cfg[m->selected].cvAmt[pp] = a / 100.f; }));
			}
		}));
	}
};

// coloured lights for the eight SELECT buttons
template <int I>
struct LaneLight : GrayModuleLightWidget {
	LaneLight() { addBaseColor(laneColor(I)); }
};

// a coloured dot on each knob that has CV assigned on the selected output
struct CvBadges : TransparentWidget {
	Deidre* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			for (int p = 0; p < NUM_P; p++) {
				int src = module->eng.cfg[module->selected].cvSrc[p];
				if (!src) continue;
				int r, c;
				knobPos(p, r, c);
				Vec pos = mm2px(Vec(lay::knobX(c) + 5.0f, lay::knobY(r) - 4.6f)).minus(box.pos);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, pos.x, pos.y, mm2px(1.05f));
				nvgFillColor(args.vg, cvColor(src));
				nvgFill(args.vg);
			}
		}
		TransparentWidget::drawLayer(args, layer);
	}
};

struct DeidreWidget : ModuleWidget {
	template <int I>
	void addSelect(Deidre* module) {
		Vec pos = mm2px(Vec(lay::outC(I) + 5.f, lay::OY + 9.2f));
		addParam(createLightParamCentered<VCVLightBezel<LaneLight<I>>>(pos, module, Deidre::SELECT_PARAM + I, Deidre::SELECT_LIGHT + I));
	}

	DeidreWidget(Deidre* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Deidre.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		DeidreScreen* scr = createWidget<DeidreScreen>(mm2px(Vec(lay::SX, lay::SY)));
		scr->box.size = mm2px(Vec(lay::SW, lay::SH));
		scr->module = module;
		addChild(scr);

		// left column
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(lay::BPM_X, lay::BPM_Y)), module, Deidre::BPM_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(lay::RUN_X, lay::RUN_Y)), module, Deidre::RUN_PARAM, Deidre::RUN_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(lay::RUN_X, lay::RESET_Y)), module, Deidre::RESET_PARAM, Deidre::RESET_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(lay::COL_A, lay::ROW_BTN)), module, Deidre::TAP_PARAM, Deidre::TAP_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(lay::COL_B, lay::ROW_BTN)), module, Deidre::RESEED_PARAM, Deidre::RESEED_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(lay::COL_C, lay::ROW_BTN)), module, Deidre::COPY_PARAM, Deidre::COPY_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<BlueLight>>(mm2px(Vec(lay::COL_A, lay::ROW_BANK)), module, Deidre::BANK_PREV_PARAM, Deidre::BANK_PREV_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<BlueLight>>(mm2px(Vec(lay::COL_B, lay::ROW_BANK)), module, Deidre::BANK_NEXT_PARAM, Deidre::BANK_NEXT_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(lay::COL_C, lay::ROW_BANK)), module, Deidre::SAVE_PARAM, Deidre::SAVE_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lay::COL_A, lay::ROW_IN1)), module, Deidre::CLK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lay::COL_B, lay::ROW_IN1)), module, Deidre::RUN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lay::COL_C, lay::ROW_IN1)), module, Deidre::CV1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lay::COL_A, lay::ROW_IN2)), module, Deidre::CV2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lay::COL_B, lay::ROW_IN2)), module, Deidre::CV3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lay::COL_C, lay::ROW_IN2)), module, Deidre::CV4_INPUT));

		// page buttons + DATA
		for (int k = 0; k < NUM_PAGES; k++)
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(lay::PAGE_X0 + k * lay::PAGE_DX, lay::PAGE_Y)), module, Deidre::PAGE_PARAM + k, Deidre::PAGE_LIGHT + k));
		DataEncoder* enc = createWidget<DataEncoder>(mm2px(Vec(lay::DATA_X - 5.2f, lay::DATA_Y - 5.2f)));
		enc->box.size = mm2px(Vec(10.4f, 10.4f));
		enc->module = module;
		addChild(enc);

		// knob bank
		for (int p = 0; p < NUM_P; p++) {
			int r, c;
			knobPos(p, r, c);
			Vec pos = mm2px(Vec(lay::knobX(c), lay::knobY(r)));
			if (p == P_INVERT) {
				addParam(createLightParamCentered<VCVLightBezelLatch<WhiteLight>>(pos, module, Deidre::EDIT_PARAM + p, Deidre::INVERT_LIGHT));
				continue;
			}
			CvKnob<RoundBlackKnob>* k = createParamCentered<CvKnob<RoundBlackKnob>>(pos, module, Deidre::EDIT_PARAM + p);
			k->p = p;
			addParam(k);
		}
		CvBadges* badges = createWidget<CvBadges>(mm2px(Vec(lay::BX0, lay::BY0)));
		badges->box.size = mm2px(Vec(lay::W - lay::BX0, 5 * lay::BH));
		badges->module = module;
		addChild(badges);

		// outputs
		addSelect<0>(module); addSelect<1>(module); addSelect<2>(module); addSelect<3>(module);
		addSelect<4>(module); addSelect<5>(module); addSelect<6>(module); addSelect<7>(module);
		for (int i = 0; i < NUM_OUT; i++) {
			float c = lay::outC(i);
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(c + 5.f, lay::OY + 16.8f)), module, Deidre::MUTE_PARAM + i, Deidre::MUTE_LIGHT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(c + 16.5f, lay::OY + 10.5f)), module, Deidre::OUT_OUTPUT + i));
			addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(c + 21.6f, lay::OY + 5.4f)), module, Deidre::OUT_LIGHT + i));
		}
	}

	void appendContextMenu(Menu* menu) override {
		Deidre* m = dynamic_cast<Deidre*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Screen follows the knob you touch", "", &m->followKnobs));
		menu->addChild(createMenuItem(string::f("Copy output %d to all outputs", m->selected + 1), "", [=]() { m->copyToAll(); }));
		menu->addChild(createMenuItem(string::f("Initialise output %d", m->selected + 1), "", [=]() {
			uint32_t seed = m->eng.cfg[m->selected].seed;
			m->eng.cfg[m->selected].reset();
			m->eng.cfg[m->selected].seed = seed;
			m->needSync = true;
		}));
		menu->addChild(createMenuItem(string::f("Clear CV assignments of output %d", m->selected + 1), "", [=]() {
			for (int p = 0; p < NUM_P; p++) m->eng.cfg[m->selected].cvSrc[p] = 0;
		}));
		menu->addChild(createMenuItem("Clear all 64 banks", "", [=]() { for (int b = 0; b < NUM_BANKS; b++) m->bankUsed[b] = false; }));
		menu->addChild(createIndexPtrSubmenuItem("Edit user scale (LOGIC page)", {"USER 1", "USER 2", "USER 3"}, &m->userEdit));
	}
};

Model* modelDeidre = createModel<Deidre, DeidreWidget>("Deidre");
