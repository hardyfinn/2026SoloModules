// 808 Circuit Bench: all thirteen 808 voices in one module. Sixteen knobs take every
// parameter of the selected voice, named and valued live under each knob and grouped by
// circuit stage. Each voice has its own trigger and output; voices whose output is not
// patched go to the mix. The voices are the same engines as the single modules
// (BenchVoices.hpp).
//
// Kits: four kit memories. The kit as it was loaded (a factory kit, a kit file, or the
// memory KITS stepped to) is kept as a snapshot: double-click a knob to go back to the
// snapshot value, double-click again for the stock 808. RAND rolls the selected voice near
// the snapshot; SHIFT + RAND rolls it anywhere. LEVEL and PAN are never rolled.
#include "plugin.hpp"
#include "BenchVoices.hpp"
#include <osdialog.h>
#include <atomic>

using namespace bench;

// ---- LAYOUT (same numbers in res/make_bench_panel.py) ----
static const float BW = 182.88f;                       // 36 HP
static const float GRID_X0 = 8.f, GRID_X1 = BW - 8.f;
static const int COLS = 8;
static const float GRID_PIT = (GRID_X1 - GRID_X0) / COLS;
static const float ROW_Y[2] = {29.f, 58.5f};
static const float LAB_DY = 9.4f, VAL_DY = 13.4f, HEAD_DY = -10.f;
static const float BTN_Y = 79.f, BTN_W = 12.f, BTN_H = 5.2f;
static const float STOCK_X = 14.f, COPY_X = 28.f, SHIFT_X = 42.f, KITS_X = 58.f;
static const float LED_X0 = 67.f, LED_DX = 6.f;
static const float RAND_X = 100.f, RAND_IN_X = 113.5f;
static const float VOICE_X0 = 13.f, VOICE_P = 10.5f;
static const float VOICE_Y = 92.4f, TRIG_Y = 102.4f, OUT_Y = 115.4f;
static const float MIX_X[2] = {BW - 23.5f, BW - 10.5f};
static const int KITS_N = 4;
static inline float slotX(int s) { return GRID_X0 + GRID_PIT * ((s % COLS) + 0.5f); }
static inline float slotY(int s) { return ROW_Y[s / COLS]; }

struct Bench808 : Module {
	enum ParamIds {
		ENUMS(KNOB_PARAM, NSLOT), STOCK_PARAM, COPY_PARAM, KITS_PARAM, SHIFT_PARAM, RAND_PARAM,
		ENUMS(VOICE_PARAM, NV),
		NUM_PARAMS
	};
	enum InputIds { ENUMS(TRIG_INPUT, NV), ACC_INPUT, POLY_INPUT, RAND_INPUT, NUM_INPUTS };
	enum OutputIds { ENUMS(OUT_OUTPUT, NV), L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };

	Kit kit;
	// four kit memories; the live kit is always memory `kitNo`
	float mem[KITS_N][NV][MAXK + 1];
	// the live kit as it was loaded (double-click and RAND go back to / roll around it)
	float snap[NV][MAXK + 1];
	int kitNo = 0;
	// what each memory was loaded from: a factory kit number, -1 = your own, -2 = a kit file
	int kitSrc[KITS_N] = {0, 0, 0, 0};
	uint32_t serial = 808;
	Rng rng;

	int selVoice = BD;
	int copyFrom = -1;
	// UI requests, applied in process()
	int reqSlot = -1;           // double-click on a knob
	int reqKit = -1;            // factory kit index
	float pend[NV][MAXK + 1];   // a kit file, read on the UI thread, applied in process()
	std::atomic<bool> reqPend{false};
	float knobShadow[NSLOT];
	float flash[NV] = {};

	dsp::SchmittTrigger trig[NV], polyTrig[NV], randTrig;
	dsp::BooleanTrigger voiceBtn[NV], stockBtn, copyBtn, kitsBtn, randBtn;
	int pending[NV];
	float pendingAcc[NV] = {};
	float sr = 48000.f;

	struct KnobQuantity : ParamQuantity {
		int s = 0;
		Bench808* m() { return dynamic_cast<Bench808*>(module); }
		std::string getLabel() override {
			Bench808* b = m();
			if (!b) return string::f("Knob %d", s + 1);
			int p = b->slotParam(s);
			if (p < 0) return "(not used on this voice)";
			return std::string(code(b->selVoice)) + " " + b->paramName(b->selVoice, p);
		}
		std::string getDisplayValueString() override {
			Bench808* b = m();
			if (!b || b->slotParam(s) < 0) return "";
			char buf[32];
			b->formatParam(b->selVoice, b->slotParam(s), getValue(), buf, sizeof(buf));
			return buf;
		}
		std::string getUnit() override { return ""; }
		std::string getDescription() override { return "Double-click: back to the loaded kit value, then to the stock 808"; }
	};

	Bench808() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		for (int s = 0; s < NSLOT; s++) {
			KnobQuantity* q = configParam<KnobQuantity>(KNOB_PARAM + s, 0.f, 1.f, 0.5f, "Knob");
			q->s = s;
			q->randomizeEnabled = false;   // Ctrl+R rolls the kit itself (onRandomize)
			q->resetEnabled = false;
			knobShadow[s] = -1.f;
		}
		configButton(STOCK_PARAM, "Stock (this voice back to the 808; with SHIFT: back to the loaded kit)");
		configButton(COPY_PARAM, "Copy this voice (then tap another voice of the same kind to paste)");
		configButton(KITS_PARAM, "Kits (next of four kit memories; with SHIFT: this kit back to the stock 808)");
		configSwitch(SHIFT_PARAM, 0.f, 1.f, 0.f, "Shift", {"off", "on"})->randomizeEnabled = false;
		configButton(RAND_PARAM, "Rand (roll this voice near the loaded kit; with SHIFT: anywhere. LEVEL and PAN stay)");
		for (int i = 0; i < NV; i++) {
			configButton(VOICE_PARAM + i, string::f("%s %s (tap = select; SHIFT + tap = hit)", code(i), longName(i)));
			configInput(TRIG_INPUT + i, string::f("%s %s trigger", code(i), longName(i)));
			configOutput(OUT_OUTPUT + i, string::f("%s %s (patched = out of the mix)", code(i), longName(i)));
			pending[i] = -1;
		}
		configInput(ACC_INPUT, "Accent (mono: all voices; poly: one channel per voice, BD first; gate = accented, 0-5 V = how much)");
		configInput(POLY_INPUT, "Poly trigger (channel 1 = BD ... 13 = CH)");
		configInput(RAND_INPUT, "Rand trigger (rolls the selected voice; wild while SHIFT is lit)");
		configOutput(L_OUTPUT, "Mix left");
		configOutput(R_OUTPUT, "Mix right");
		serial = random::u32();
		rng.s = random::u32() | 1u;
		kit.setSerial(serial);
		kit.setSampleRate(48000.f);
		for (int n = 0; n < KITS_N; n++) store(n);
		takeSnapshot();
		select(BD);
	}

	// ---- kit memories and the snapshot
	void copyOut(float dst[NV][MAXK + 1]) {
		for (int v = 0; v < NV; v++) {
			for (int i = 0; i < MAXK; i++) dst[v][i] = kit.v[v]->k[i];
			dst[v][MAXK] = kit.v[v]->pan;
		}
	}
	void copyIn(const float src[NV][MAXK + 1], int only = -1) {
		for (int v = 0; v < NV; v++) {
			if (only >= 0 && v != only) continue;
			for (int i = 0; i < kit.v[v]->nK(); i++) kit.v[v]->k[i] = src[v][i];
			kit.v[v]->pan = src[v][MAXK];
			kit.v[v]->fix();
		}
	}
	void store(int n) { copyOut(mem[n]); }
	void load(int n) { copyIn(mem[n]); }
	void takeSnapshot() { copyOut(snap); }
	float snapOf(int v, int p) const { return p == PAN ? snap[v][MAXK] : snap[v][p]; }

	// ---- selection helpers (also used by the widgets)
	int slotParam(int s) const { const Slots& S = slots(kindOf(selVoice)); return s < S.n ? S.p[s] : -1; }
	std::string paramName(int v, int p) const { return p == PAN ? "PAN" : kit.v[v]->def(p).label; }
	void formatParam(int v, int p, float k, char* buf, int n) const {
		if (p == PAN) {
			if (std::fabs(k - 0.5f) < 0.01f) snprintf(buf, n, "C");
			else snprintf(buf, n, "%s%.0f", k < 0.5f ? "L" : "R", std::fabs(k - 0.5f) * 200.f);
			return;
		}
		format(buf, n, kindOf(v), kit.v[v]->def(p), k);
	}
	void syncKnobs() {
		for (int s = 0; s < NSLOT; s++) {
			int p = slotParam(s);
			float v = p < 0 ? 0.5f : kit.v[selVoice]->get(p);
			params[KNOB_PARAM + s].setValue(v);
			knobShadow[s] = v;
		}
	}
	void select(int v) { selVoice = clamp(v, 0, NV - 1); syncKnobs(); }

	void rollVoice(int v, bool wild) {
		float home[MAXK + 1];
		for (int i = 0; i <= MAXK; i++) home[i] = snap[v][i];
		roll(kit.v[v], home, wild, rng);
	}
	// a kit file into `dst`: stock first, then what the file holds
	void kitFromJsonStock(json_t* kj, float dst[NV][MAXK + 1]) {
		for (int v = 0; v < NV; v++) {
			for (int i = 0; i < MAXK; i++) dst[v][i] = i < kit.v[v]->nK() ? kit.v[v]->def(i).stock : 0.f;
			dst[v][MAXK] = 0.5f;
		}
		kitFromJson(kj, dst);
		for (int v = 0; v < NV; v++) for (int i = 0; i <= MAXK; i++) dst[v][i] = clamp(dst[v][i], 0.f, 1.f);
	}
	json_t* kitToJson(const float k[NV][MAXK + 1]) {
		json_t* kj = json_object();
		for (int v = 0; v < NV; v++) {
			json_t* vj = json_object();
			for (int i = 0; i < kit.v[v]->nK(); i++) json_object_set_new(vj, kit.v[v]->def(i).id, json_real(k[v][i]));
			json_object_set_new(vj, "PAN", json_real(k[v][MAXK]));
			json_object_set_new(kj, code(v), vj);
		}
		return kj;
	}
	void kitFromJson(json_t* kj, float k[NV][MAXK + 1]) {
		for (int v = 0; v < NV; v++) {
			json_t* vj = json_object_get(kj, code(v));
			if (!vj) continue;
			for (int i = 0; i < kit.v[v]->nK(); i++) {
				json_t* x = json_object_get(vj, kit.v[v]->def(i).id);
				if (x) k[v][i] = (float)json_number_value(x);
			}
			json_t* pj = json_object_get(vj, "PAN");
			if (pj) k[v][MAXK] = (float)json_number_value(pj);
		}
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { sr = e.sampleRate; kit.setSampleRate(sr); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (auto* v : kit.v) { v->toStock(); v->reset(); }
		for (int n = 0; n < KITS_N; n++) { store(n); kitSrc[n] = 0; }
		takeSnapshot();
		kitNo = 0; copyFrom = -1;
		select(BD);
	}
	// Ctrl+R: every voice of the live kit rolled anywhere (LEVEL and PAN stay)
	void onRandomize(const RandomizeEvent& e) override {
		for (int v = 0; v < NV; v++) rollVoice(v, true);
		syncKnobs();
	}

	json_t* dataToJson() override {
		store(kitNo);
		json_t* root = json_object();
		json_object_set_new(root, "serial", json_integer(serial));
		json_object_set_new(root, "kit", json_integer(kitNo));
		json_object_set_new(root, "voice", json_integer(selVoice));
		json_t* kits = json_array();
		for (int n = 0; n < KITS_N; n++) json_array_append_new(kits, kitToJson(mem[n]));
		json_object_set_new(root, "kits", kits);
		json_object_set_new(root, "loaded", kitToJson(snap));
		json_t* src = json_array();
		for (int n = 0; n < KITS_N; n++) json_array_append_new(src, json_integer(kitSrc[n]));
		json_object_set_new(root, "from", src);
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "serial");
		if (s) { serial = (uint32_t)json_integer_value(s); kit.setSerial(serial); }
		json_t* kits = json_object_get(root, "kits");
		for (int n = 0; n < KITS_N && kits; n++) {
			json_t* kj = json_array_get(kits, n);
			if (kj) kitFromJson(kj, mem[n]);
		}
		json_t* src = json_object_get(root, "from");
		for (int n = 0; n < KITS_N; n++) {
			json_t* x = src ? json_array_get(src, n) : NULL;
			kitSrc[n] = x ? (int)json_integer_value(x) : -1;
		}
		json_t* k = json_object_get(root, "kit");
		kitNo = k ? clamp((int)json_integer_value(k), 0, KITS_N - 1) : 0;
		load(kitNo);
		takeSnapshot();
		json_t* lj = json_object_get(root, "loaded");
		if (lj) kitFromJson(lj, snap);
		json_t* vv = json_object_get(root, "voice");
		select(vv ? (int)json_integer_value(vv) : BD);
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != sr) { sr = args.sampleRate; kit.setSampleRate(sr); }
		bool shift = params[SHIFT_PARAM].getValue() > 0.5f;
		bool resync = false;

		// ---- UI: voice buttons, panel buttons, requests from the widgets and menu
		for (int i = 0; i < NV; i++) {
			if (!voiceBtn[i].process(params[VOICE_PARAM + i].getValue() > 0.5f)) continue;
			if (shift) { kit.hit(i, 0.f); flash[i] = 1.f; params[SHIFT_PARAM].setValue(0.f); }
			else if (copyFrom >= 0) {
				if (i != copyFrom && kindOf(i) == kindOf(copyFrom)) {
					for (int p = 0; p < kit.v[i]->nK(); p++) kit.v[i]->k[p] = kit.v[copyFrom]->k[p];
					kit.v[i]->pan = kit.v[copyFrom]->pan;
					kit.v[i]->fix();
				}
				copyFrom = -1;
				select(i);
			}
			else select(i);
		}
		if (stockBtn.process(params[STOCK_PARAM].getValue() > 0.5f)) {
			if (shift) { copyIn(snap, selVoice); params[SHIFT_PARAM].setValue(0.f); }
			else kit.v[selVoice]->toStock();
			resync = true;
		}
		if (copyBtn.process(params[COPY_PARAM].getValue() > 0.5f)) copyFrom = copyFrom == selVoice ? -1 : selVoice;
		if (kitsBtn.process(params[KITS_PARAM].getValue() > 0.5f)) {
			if (shift) { for (auto* v : kit.v) v->toStock(); kitSrc[kitNo] = 0; params[SHIFT_PARAM].setValue(0.f); }
			else { store(kitNo); kitNo = (kitNo + 1) % KITS_N; load(kitNo); }
			takeSnapshot();
			resync = true;
		}
		bool randHit = randBtn.process(params[RAND_PARAM].getValue() > 0.5f);
		bool randIn = randTrig.process(inputs[RAND_INPUT].getVoltage(), 0.1f, 1.f);
		if (randHit || randIn) {
			rollVoice(selVoice, shift);
			if (randHit && shift) params[SHIFT_PARAM].setValue(0.f);   // the RAND input leaves SHIFT lit
			resync = true;
		}
		if (reqSlot >= 0) {
			int p = slotParam(reqSlot);
			reqSlot = -1;
			if (p >= 0) {
				Voice* v = kit.v[selVoice];
				float home = snapOf(selVoice, p);
				v->set(p, std::fabs(v->get(p) - home) > 0.002f ? home : v->stockOf(p));
				resync = true;
			}
		}
		if (reqKit >= 0) {
			int n = reqKit; reqKit = -1;
			const auto& fk = factoryKits();
			if (n < (int)fk.size()) { applyKit(kit, fk[n].text); store(kitNo); takeSnapshot(); kitSrc[kitNo] = n; }
			resync = true;
		}
		if (reqPend) {
			copyIn(pend);
			store(kitNo);
			kitSrc[kitNo] = -2;
			takeSnapshot();
			reqPend = false;
			resync = true;
		}
		if (resync) syncKnobs();

		// ---- the sixteen knobs follow the selected voice
		Voice* sv = kit.v[selVoice];
		for (int s = 0; s < NSLOT; s++) {
			int p = slotParam(s);
			if (p < 0) continue;
			float pv = params[KNOB_PARAM + s].getValue();
			if (knobShadow[s] >= 0.f && pv != knobShadow[s]) { sv->set(p, pv); knobShadow[s] = sv->get(p); }
			else if (sv->get(p) != knobShadow[s]) { knobShadow[s] = sv->get(p); params[KNOB_PARAM + s].setValue(knobShadow[s]); }
		}

		// ---- triggers and accent (read over 0.5 ms, as on the single modules)
		int accCh = inputs[ACC_INPUT].getChannels();
		int polyCh = inputs[POLY_INPUT].getChannels();
		for (int i = 0; i < NV; i++) {
			bool hit = trig[i].process(inputs[TRIG_INPUT + i].getVoltage(), 0.1f, 1.f);
			if (i < polyCh) hit |= polyTrig[i].process(inputs[POLY_INPUT].getVoltage(i), 0.1f, 1.f);
			if (hit && pending[i] < 0) { pending[i] = (int)(0.0005f * sr); pendingAcc[i] = 0.f; }
			if (pending[i] >= 0) {
				float a = 0.f;
				if (accCh == 1) a = inputs[ACC_INPUT].getVoltage();
				else if (i < accCh) a = inputs[ACC_INPUT].getVoltage(i);
				pendingAcc[i] = std::max(pendingAcc[i], clamp(a / 5.f, 0.f, 1.f));
				if (pending[i]-- == 0) { kit.hit(i, pendingAcc[i]); flash[i] = 1.f; }
			}
		}

		// ---- voices, outputs and the mix (pan: centre = the same level on both sides)
		float l = 0.f, r = 0.f;
		const float fl = std::exp(-args.sampleTime / 0.08f);
		for (int i = 0; i < NV; i++) {
			Voice* v = kit.v[i];
			v->process();
			float o = v->out;
			outputs[OUT_OUTPUT + i].setVoltage(o);
			if (!outputs[OUT_OUTPUT + i].isConnected()) {
				l += o * std::min(1.f, 2.f * (1.f - v->pan));
				r += o * std::min(1.f, 2.f * v->pan);
			}
			flash[i] *= fl;
		}
		// the mix runs 6 dB below the single outputs, so a full kit stays near +-10 V
		outputs[L_OUTPUT].setVoltage(0.5f * l);
		outputs[R_OUTPUT].setVoltage(0.5f * r);
	}
};

// ---------------------------------------------------------------- widgets
static NVGcolor hexCol(uint32_t c, float a = 1.f) { return nvgRGBAf(((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, a); }
static const NVGcolor INK = nvgRGB(0xEC, 0xE6, 0xD8), DIM = nvgRGB(0x9A, 0x96, 0x8C), AMB = nvgRGB(0xF3, 0xB0, 0x4A),
	REDC = nvgRGB(0xE0, 0x53, 0x3D), RULE = nvgRGB(0x5A, 0x5B, 0x60), PANEL_BG = nvgRGB(0x1F, 0x20, 0x23);
static const float PX_PER_MM = 75.f / 25.4f;

static std::shared_ptr<window::Font> monoFont() { return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf")); }
static std::shared_ptr<window::Font> boldFont() { return APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DejaVuSans-Bold.ttf")); }

// a kit for the module browser picture
static Kit& demoKit() { static Kit* k = new Kit(); return *k; }

// names, values, stage headings and the stock / loaded dots of the sixteen knobs
struct KnobGrid : TransparentWidget {
	Bench808* module = nullptr;
	float knobR = 5.1f;   // mm, set from the knob widget
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<window::Font> bold = boldFont(), mono = monoFont();
		if (!bold || bold->handle < 0 || !mono || mono->handle < 0) return;
		NVGcontext* vg = args.vg;
		Kit& kit = module ? module->kit : demoKit();
		int v = module ? module->selVoice : SD;
		Voice* vo = kit.v[v];
		const Slots& S = slots(kindOf(v));
		const Layout& L = layout(kindOf(v));
		nvgSave(vg);
		nvgScale(vg, PX_PER_MM, PX_PER_MM);
		// stage headings: one bracket over each run of knobs from the same stage, per row
		for (int row = 0; row < 2; row++) {
			int i = row * COLS, end = std::min(S.n, (row + 1) * COLS);
			while (i < end) {
				int j = i;
				while (j + 1 < end && S.blk[j + 1] == S.blk[i]) j++;
				float xa = GRID_X0 + GRID_PIT * (i % COLS) + 1.2f, xb = GRID_X0 + GRID_PIT * ((j % COLS) + 1) - 1.2f;
				float hy = ROW_Y[row] + HEAD_DY;
				const char* name = L.blocks[S.blk[i]].name;
				nvgFontFaceId(vg, bold->handle);
				nvgFontSize(vg, 2.9f);
				nvgTextLetterSpacing(vg, 0.15f);
				float bounds[4];
				nvgTextBounds(vg, 0, 0, name, NULL, bounds);
				// a name wider than its knobs shrinks to fit (one-knob stages)
				float fit = std::min(1.f, (xb - xa - 2.4f) / (bounds[2] - bounds[0]));
				if (fit < 1.f) nvgFontSize(vg, 2.9f * fit);
				float tw = (bounds[2] - bounds[0]) * fit + 2.4f, cx = (xa + xb) / 2.f;
				nvgBeginPath(vg);
				nvgMoveTo(vg, xa, hy + 1.6f); nvgLineTo(vg, xa, hy); nvgLineTo(vg, std::max(xa, cx - tw / 2), hy);
				nvgMoveTo(vg, std::min(xb, cx + tw / 2), hy); nvgLineTo(vg, xb, hy); nvgLineTo(vg, xb, hy + 1.6f);
				nvgStrokeColor(vg, RULE); nvgStrokeWidth(vg, 0.35f); nvgStroke(vg);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, DIM);
				nvgText(vg, cx, hy + 0.1f, name, NULL);
				i = j + 1;
			}
		}
		nvgTextLetterSpacing(vg, 0.f);
		for (int s = 0; s < S.n; s++) {
			int p = S.p[s];
			float x = slotX(s), y = slotY(s);
			std::string nm = p == PAN ? "PAN" : vo->def(p).label;
			char buf[32];
			float k = vo->get(p);
			if (module) module->formatParam(v, p, k, buf, sizeof(buf));
			else if (p == PAN) snprintf(buf, sizeof(buf), "C");
			else format(buf, sizeof(buf), kindOf(v), vo->def(p), k);
			nvgFontFaceId(vg, bold->handle);
			nvgFontSize(vg, 3.2f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
			nvgFillColor(vg, INK);
			nvgText(vg, x, y + LAB_DY, nm.c_str(), NULL);
			nvgFontFaceId(vg, mono->handle);
			nvgFontSize(vg, 2.9f);
			nvgFillColor(vg, AMB);
			nvgText(vg, x, y + VAL_DY, buf, NULL);
			// dots round the knob: red = stock 808, amber = the loaded kit (when it differs)
			auto dot = [&](float kk, NVGcolor c) {
				float a = (-0.83f + 1.66f * kk) * M_PI - M_PI / 2.f, rr = knobR + 0.9f;
				nvgBeginPath(vg);
				nvgCircle(vg, x + rr * std::cos(a), y + rr * std::sin(a), 0.55f);
				nvgFillColor(vg, c);
				nvgFill(vg);
			};
			float st = vo->stockOf(p);
			float ld = module ? module->snapOf(v, p) : st;
			dot(st, REDC);
			if (std::fabs(ld - st) > 0.004f) dot(ld, AMB);
		}
		nvgRestore(vg);
	}
};

// voice name, kit and copy state, top right
struct BenchTitle : TransparentWidget {
	Bench808* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<window::Font> bold = boldFont();
		if (!bold || bold->handle < 0) return;
		NVGcontext* vg = args.vg;
		int v = module ? module->selVoice : SD;
		nvgSave(vg);
		nvgScale(vg, PX_PER_MM, PX_PER_MM);
		nvgFontFaceId(vg, bold->handle);
		nvgFontSize(vg, 4.4f);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, hexCol(colour(v)));
		std::string t = std::string(code(v)) + "  " + longName(v);
		nvgText(vg, box.size.x / PX_PER_MM, 4.6f, t.c_str(), NULL);
		nvgFontSize(vg, 2.5f);
		nvgFillColor(vg, DIM);
		std::string st = string::f("KIT %c", 'A' + (module ? module->kitNo : 0));
		int from = module ? module->kitSrc[module->kitNo] : 0;
		if (from >= 0 && from < (int)factoryKits().size()) st += std::string(": ") + factoryKits()[from].name;
		else if (from == -2) st += ": kit file";
		if (module && module->copyFrom >= 0) st = string::f("COPY %s: tap a voice of the same kind   |   ", code(module->copyFrom)) + st;
		nvgText(vg, box.size.x / PX_PER_MM, 8.6f, st.c_str(), NULL);
		nvgRestore(vg);
	}
};

// the four kit memory lights
struct KitLights : TransparentWidget {
	Bench808* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		NVGcontext* vg = args.vg;
		int k = module ? module->kitNo : 0;
		for (int n = 0; n < KITS_N; n++) {
			nvgBeginPath(vg);
			nvgCircle(vg, (1.4f + n * LED_DX) * PX_PER_MM, 1.4f * PX_PER_MM, 1.3f * PX_PER_MM);
			nvgFillColor(vg, n == k ? AMB : nvgRGB(0x3A, 0x30, 0x20));
			nvgFill(vg);
		}
	}
};

// a knob that hides on the slots this voice does not use, and whose double-click goes to
// the loaded kit value, then to stock
struct BenchKnob : RoundBlackKnob {
	int s = 0;
	Bench808* bench = nullptr;
	void step() override {
		const Slots& S = slots(kindOf(bench ? bench->selVoice : SD));
		visible = s < S.n;
		RoundBlackKnob::step();
	}
	void onDoubleClick(const DoubleClickEvent& e) override {
		if (bench) bench->reqSlot = s;
		e.consume(this);
	}
};

// a flat panel button drawn in the 808's colours
struct FlatButton : app::Switch {
	Bench808* bench = nullptr;
	std::string label;
	NVGcolor fill = nvgRGB(0x3A, 0x3B, 0x40), ink = INK;
	int voice = -1;        // voice buttons: lit when selected, flash on hits
	int role = 0;          // 1 = COPY (lit while copying), 2 = SHIFT (lit when on)
	FlatButton() { momentary = true; }
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		NVGcontext* vg = args.vg;
		bool lit = false;
		float glow = 0.f;
		if (bench) {
			if (voice >= 0) { lit = bench->selVoice == voice; glow = bench->flash[voice]; }
			if (role == 1) lit = bench->copyFrom >= 0;
			if (role == 2) lit = bench->params[Bench808::SHIFT_PARAM].getValue() > 0.5f;
		}
		else if (voice == SD) lit = true;
		NVGcolor f = fill;
		if (voice < 0 && lit) f = AMB;
		if (voice >= 0) f = nvgLerpRGBA(nvgLerpRGBA(f, nvgRGB(0, 0, 0), 0.22f), nvgRGB(255, 255, 255), 0.22f * (lit ? 1.f : 0.f) + 0.45f * glow);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(vg, f);
		nvgFill(vg);
		if (voice >= 0 && lit) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, -1.6f, -1.6f, box.size.x + 3.2f, box.size.y + 3.2f, 4.f);
			nvgStrokeColor(vg, INK); nvgStrokeWidth(vg, 1.2f); nvgStroke(vg);
		}
		std::shared_ptr<window::Font> font = boldFont();
		if (font && font->handle >= 0) {
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, voice >= 0 ? 3.2f * PX_PER_MM : 3.0f * PX_PER_MM);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, voice >= 0 || lit ? nvgRGB(0x1B, 0x1B, 0x1B) : ink);
			nvgText(vg, box.size.x / 2, box.size.y / 2 + 0.5f, label.c_str(), NULL);
		}
	}
};

struct Bench808Widget : ModuleWidget {
	Bench808Widget(Bench808* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bench808.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		BenchTitle* title = createWidget<BenchTitle>(mm2px(Vec(BW - 100.f, 6.7f)));
		title->box.size = mm2px(Vec(92.f, 10.f));
		title->module = module;
		addChild(title);

		float knobR = 5.1f;
		for (int s = 0; s < NSLOT; s++) {
			BenchKnob* k = createParamCentered<BenchKnob>(mm2px(Vec(slotX(s), slotY(s))), module, Bench808::KNOB_PARAM + s);
			k->s = s; k->bench = module;
			knobR = k->box.size.x / 2.f / PX_PER_MM;
			addParam(k);
		}
		KnobGrid* grid = createWidget<KnobGrid>(Vec(0, 0));
		grid->box.size = mm2px(Vec(BW, 74.f));
		grid->module = module; grid->knobR = knobR;
		addChild(grid);

		auto button = [&](float x, float y, float w, float h, int param, const std::string& label, int voice, int role) {
			FlatButton* b = createParam<FlatButton>(mm2px(Vec(x - w / 2, y - h / 2)), module, param);
			b->box.size = mm2px(Vec(w, h));
			b->bench = module; b->label = label; b->voice = voice; b->role = role;
			if (voice >= 0) b->fill = hexCol(colour(voice));
			if (role == 2) b->momentary = false;
			if (role == 3) { b->fill = nvgRGB(0x5A, 0x3A, 0x1A); b->ink = AMB; }
			addParam(b);
		};
		button(STOCK_X, BTN_Y, BTN_W, BTN_H, Bench808::STOCK_PARAM, "STOCK", -1, 0);
		button(COPY_X, BTN_Y, BTN_W, BTN_H, Bench808::COPY_PARAM, "COPY", -1, 1);
		button(SHIFT_X, BTN_Y, BTN_W, BTN_H, Bench808::SHIFT_PARAM, "SHIFT", -1, 2);
		button(KITS_X, BTN_Y, BTN_W, BTN_H, Bench808::KITS_PARAM, "KITS", -1, 0);
		button(RAND_X, BTN_Y, BTN_W, BTN_H, Bench808::RAND_PARAM, "RAND", -1, 3);
		KitLights* kl = createWidget<KitLights>(mm2px(Vec(LED_X0 - 1.4f, BTN_Y - 0.2f - 1.4f)));
		kl->box.size = mm2px(Vec(LED_DX * 3 + 2.8f, 2.8f));
		kl->module = module;
		addChild(kl);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(RAND_IN_X, BTN_Y)), module, Bench808::RAND_INPUT));

		for (int i = 0; i < NV; i++) {
			float x = VOICE_X0 + VOICE_P * (i + 0.5f);
			button(x, VOICE_Y, VOICE_P - 1.6f, 4.8f, Bench808::VOICE_PARAM + i, code(i), i, 0);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, TRIG_Y)), module, Bench808::TRIG_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, OUT_Y)), module, Bench808::OUT_OUTPUT + i));
		}
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(MIX_X[0], TRIG_Y)), module, Bench808::ACC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(MIX_X[1], TRIG_Y)), module, Bench808::POLY_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(MIX_X[0], OUT_Y)), module, Bench808::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(MIX_X[1], OUT_Y)), module, Bench808::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Bench808* m = dynamic_cast<Bench808*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem(string::f("Factory kit (into kit %c)", 'A' + m->kitNo), "", [=](Menu* sub) {
			const auto& fk = factoryKits();
			for (int n = 0; n < (int)fk.size(); n++)
				sub->addChild(createMenuItem(fk[n].name, "", [=]() { m->reqKit = n; }));
		}));
		menu->addChild(createMenuItem("Save this kit to a file...", "", [=]() {
			char* path = osdialog_file(OSDIALOG_SAVE, NULL, "kit.808kit", NULL);
			if (!path) return;
			m->store(m->kitNo);
			json_t* root = json_object();
			json_object_set_new(root, "kit", m->kitToJson(m->mem[m->kitNo]));
			json_dump_file(root, path, JSON_INDENT(1));
			json_decref(root);
			std::free(path);
		}));
		menu->addChild(createMenuItem("Load a kit file into this kit...", "", [=]() {
			char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
			if (!path) return;
			json_error_t err;
			json_t* root = json_load_file(path, 0, &err);
			std::free(path);
			if (!root) return;
			json_t* inner = json_object_get(root, "kit");
			if (!m->reqPend) {
				m->kitFromJsonStock(inner ? inner : root, m->pend);
				m->reqPend = true;
			}
			json_decref(root);
		}));
		menu->addChild(createMenuItem("This kit back to the stock 808", "", [=]() { m->reqKit = 0; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Serial number %u (sets this unit's AGE character)", (unsigned)m->serial)));
		menu->addChild(createMenuItem("New serial number (a different old unit)", "", [=]() {
			m->serial = random::u32();
			m->kit.setSerial(m->serial);
		}));
	}
};

Model* modelBench808 = createModel<Bench808, Bench808Widget>("Bench808");
