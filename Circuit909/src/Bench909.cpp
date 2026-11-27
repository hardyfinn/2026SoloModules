// 909 Circuit Bench: all eleven 909 voices in one module. The screen draws the selected
// voice's circuit; tap a block and the four knobs under the screen take its parts. Each
// voice has its own trigger and output; voices whose output is not patched go to the mix.
// The voices are the same engines as the single modules (BenchVoices909.hpp). The hats,
// crash and ride can each play a WAV of your own instead of the built-in metal source.
#include "plugin.hpp"
#include "BenchVoices909.hpp"
#include "Wav909.hpp"
#include <osdialog.h>
#include <atomic>
#include <mutex>

using namespace bench909;

// ---- LAYOUT (same numbers in res/make_bench_panel.py) ----
static const float BW = 243.84f;
static const float SCR_X = 8.f, SCR_Y = 20.f, SCR_W = 227.84f, SCR_H = 47.f;
static const float EDIT_X[4] = {28.f, 50.f, 72.f, 94.f};
static const float SEL_X = 128.f, KNOB_Y = 84.5f;
static const float BTN_X[2] = {148.f, 164.f}, BTN_Y[2] = {81.f, 87.5f};
static const float VOICE_Y = 99.f, TRIG_Y = 106.4f, OUT_Y = 118.4f;
static const float PITCH = (BW - 44.f) / NV;
static const float MIX_X[2] = {BW - 26.f, BW - 14.f}, MIX_IN_Y = 104.6f;
static const int KITS_N = 4;

// screen geometry (mm, relative to the screen's top left)
static const float COL_PITCH = (SCR_W - 8.f) / 5.f, BLK_W = 37.f, ROW_H = 3.3f;
static inline float blockX(const Block& b) { return 4.f + b.col * COL_PITCH; }
static inline float blockY(const Block& b) { return b.row == 0.f ? 9.f : b.row == 1.f ? 28.f : 18.f; }
static inline float blockH(const Block& b) { return 4.8f + ROW_H * b.n; }

struct Bench909 : Module {
	enum ParamIds {
		ENUMS(EDIT_PARAM, 4), SELECT_PARAM, STOCK_PARAM, COPY_PARAM, KITS_PARAM, SHIFT_PARAM,
		ENUMS(VOICE_PARAM, NV),
		NUM_PARAMS
	};
	enum InputIds { ENUMS(TRIG_INPUT, NV), ACC_INPUT, POLY_INPUT, NUM_INPUTS };
	enum OutputIds { ENUMS(OUT_OUTPUT, NV), L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };

	Kit kit;
	// four kit memories; the live kit is always memory `kitNo`
	float mem[KITS_N][NV][MAXK + 1];
	int kitNo = 0;
	uint32_t serial = 909;
	std::atomic<uint32_t> newSerial{0};

	// own WAVs: slot 0 = hats (CH and OH), 1 = crash, 2 = ride. Converted off the audio
	// thread, handed over by swapping (as on the 909 Cymbals module)
	static const int NS = 3;
	std::string samplePath[NS], sampleStatus[NS];
	std::mutex sampleMutex;
	std::vector<float> newSample[NS][2];
	std::atomic<bool> sampleReady[NS];

	int selVoice = BD, selBlock = 2;
	int copyFrom = -1;
	// UI requests, applied in process()
	int reqVoice = -1, reqBlock = -1;
	float editShadow[4] = {-1, -1, -1, -1};
	float selShadow = -1.f;
	float flash[NV] = {};

	dsp::SchmittTrigger trig[NV], polyTrig[NV];
	dsp::BooleanTrigger voiceBtn[NV], stockBtn, copyBtn, kitsBtn;
	int pending[NV];
	float pendingAcc[NV] = {};
	float sr = 48000.f;

	struct EditQuantity : ParamQuantity {
		int j = 0;
		Bench909* m() { return dynamic_cast<Bench909*>(module); }
		std::string getLabel() override {
			Bench909* b = m();
			if (!b) return string::f("Edit %d", j + 1);
			int p = b->blockParam(j);
			if (p < 0) return "(not used on this block)";
			return std::string(code(b->selVoice)) + " " + b->paramName(b->selVoice, p);
		}
		std::string getDisplayValueString() override {
			Bench909* b = m();
			if (!b || b->blockParam(j) < 0) return "";
			char buf[32];
			b->formatParam(b->selVoice, b->blockParam(j), getValue(), buf, sizeof(buf));
			return buf;
		}
		std::string getUnit() override { return ""; }
	};

	Bench909() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		for (int j = 0; j < 4; j++) {
			EditQuantity* q = configParam<EditQuantity>(EDIT_PARAM + j, 0.f, 1.f, 0.5f, "Edit");
			q->j = j;
			q->randomizeEnabled = false;   // randomize would only scramble the block on screen
			q->resetEnabled = false;
		}
		configParam(SELECT_PARAM, 0.f, 1.f, 0.5f, "Select a block of the circuit")->randomizeEnabled = false;
		configButton(STOCK_PARAM, "Stock (the block on screen back to the 909; with SHIFT: the whole voice)");
		configButton(COPY_PARAM, "Copy this voice (then tap another voice of the same kind to paste)");
		configButton(KITS_PARAM, "Kits (next of four kit memories; with SHIFT: this kit back to the stock 909)");
		configSwitch(SHIFT_PARAM, 0.f, 1.f, 0.f, "Shift", {"off", "on"})->randomizeEnabled = false;
		for (int i = 0; i < NV; i++) {
			configButton(VOICE_PARAM + i, string::f("%s %s (tap = select; SHIFT + tap = hit)", code(i), longName(i)));
			configInput(TRIG_INPUT + i, string::f("%s %s trigger", code(i), longName(i)));
			configOutput(OUT_OUTPUT + i, string::f("%s %s (patched = out of the mix)", code(i), longName(i)));
			pending[i] = -1;
		}
		configInput(ACC_INPUT, "Accent (mono: all voices; poly: one channel per voice, BD first; gate = accented, 0-5 V = how much)");
		configInput(POLY_INPUT, "Poly trigger (channel 1 = BD ... 11 = RD)");
		for (int n = 0; n < NS; n++) sampleReady[n] = false;
		configOutput(L_OUTPUT, "Mix left");
		configOutput(R_OUTPUT, "Mix right");
		serial = random::u32();
		kit.setSerial(serial);
		kit.setSampleRate(48000.f);
		for (int n = 0; n < KITS_N; n++) store(n);
		select(BD, -1);
	}

	// ---- kit memories
	void store(int n) {
		for (int v = 0; v < NV; v++) {
			for (int i = 0; i < MAXK; i++) mem[n][v][i] = kit.v[v]->k[i];
			mem[n][v][MAXK] = kit.v[v]->pan;
		}
	}
	void load(int n) {
		for (int v = 0; v < NV; v++) {
			for (int i = 0; i < kit.v[v]->nK(); i++) kit.v[v]->k[i] = mem[n][v][i];
			kit.v[v]->pan = mem[n][v][MAXK];
			kit.v[v]->fix();
		}
	}

	// ---- selection helpers (also used by the widgets)
	const Layout& lay(int v) const { return layout(kindOf(v)); }
	int blockParam(int j) const {
		const Layout& L = lay(selVoice);
		int b = clamp(selBlock, 0, L.nBlocks - 1);
		return j < L.blocks[b].n ? L.blocks[b].p[j] : -1;
	}
	std::string paramName(int v, int p) const { return p == PAN ? "PAN" : kit.v[v]->def(p).label; }
	void formatParam(int v, int p, float k, char* buf, int n) const {
		if (p == PAN) {
			if (std::fabs(k - 0.5f) < 0.01f) snprintf(buf, n, "C");
			else snprintf(buf, n, "%s%.0f", k < 0.5f ? "L" : "R", std::fabs(k - 0.5f) * 200.f);
			return;
		}
		format(buf, n, v, kit.v[v]->def(p), k);
	}
	void syncKnobs() {
		for (int j = 0; j < 4; j++) {
			int p = blockParam(j);
			float v = p < 0 ? 0.5f : kit.v[selVoice]->get(p);
			params[EDIT_PARAM + j].setValue(v);
			editShadow[j] = v;
		}
		int nb = lay(selVoice).nBlocks;
		selShadow = (selBlock + 0.5f) / nb;
		params[SELECT_PARAM].setValue(selShadow);
	}
	void select(int v, int b) {
		selVoice = clamp(v, 0, NV - 1);
		const Layout& L = lay(selVoice);
		if (b < 0) {   // a new voice opens on its main block (the one with TUNE, or the second)
			b = L.nBlocks > 1 ? 1 : 0;
			for (int i = 0; i < L.nBlocks; i++)
				for (int j = 0; j < L.blocks[i].n; j++) {
					int p = L.blocks[i].p[j];
					if (p != PAN && !std::strcmp(kit.v[selVoice]->def(p).id, "TUNE")) b = i;
				}
		}
		selBlock = clamp(b, 0, L.nBlocks - 1);
		syncKnobs();
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { sr = e.sampleRate; kit.setSampleRate(sr); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (auto* v : kit.v) { v->toStock(); v->reset(); }
		for (int n = 0; n < KITS_N; n++) store(n);
		kitNo = 0; copyFrom = -1;
		select(BD, -1);
	}

	json_t* dataToJson() override {
		store(kitNo);
		json_t* root = json_object();
		json_object_set_new(root, "serial", json_integer(serial));
		json_object_set_new(root, "kit", json_integer(kitNo));
		json_object_set_new(root, "voice", json_integer(selVoice));
		json_object_set_new(root, "block", json_integer(selBlock));
		json_t* kits = json_array();
		for (int n = 0; n < KITS_N; n++) {
			json_t* kj = json_object();
			for (int v = 0; v < NV; v++) {
				json_t* vj = json_object();
				for (int i = 0; i < kit.v[v]->nK(); i++) json_object_set_new(vj, kit.v[v]->def(i).id, json_real(mem[n][v][i]));
				json_object_set_new(vj, "PAN", json_real(mem[n][v][MAXK]));
				json_object_set_new(kj, code(v), vj);
			}
			json_array_append_new(kits, kj);
		}
		json_object_set_new(root, "kits", kits);
		static const char* sk[NS] = {"sampleHats", "sampleCrash", "sampleRide"};
		for (int n = 0; n < NS; n++)
			if (!samplePath[n].empty()) json_object_set_new(root, sk[n], json_string(samplePath[n].c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "serial");
		if (s) { serial = (uint32_t)json_integer_value(s); kit.setSerial(serial); }
		json_t* kits = json_object_get(root, "kits");
		for (int n = 0; n < KITS_N && kits; n++) {
			json_t* kj = json_array_get(kits, n);
			if (!kj) continue;
			for (int v = 0; v < NV; v++) {
				json_t* vj = json_object_get(kj, code(v));
				if (!vj) continue;
				for (int i = 0; i < kit.v[v]->nK(); i++) {
					json_t* x = json_object_get(vj, kit.v[v]->def(i).id);
					if (x) mem[n][v][i] = (float)json_number_value(x);
				}
				json_t* pj = json_object_get(vj, "PAN");
				if (pj) mem[n][v][MAXK] = (float)json_number_value(pj);
			}
		}
		json_t* k = json_object_get(root, "kit");
		kitNo = k ? clamp((int)json_integer_value(k), 0, KITS_N - 1) : 0;
		load(kitNo);
		static const char* sk[NS] = {"sampleHats", "sampleCrash", "sampleRide"};
		for (int n = 0; n < NS; n++) {
			json_t* p = json_object_get(root, sk[n]);
			if (p && json_string_value(p)) {
				std::string path = json_string_value(p);
				if (!loadSample(n, path)) samplePath[n] = path;   // keep the path, so a missing file is not forgotten
			}
		}
		json_t* vv = json_object_get(root, "voice");
		json_t* bb = json_object_get(root, "block");
		select(vv ? (int)json_integer_value(vv) : BD, bb ? (int)json_integer_value(bb) : -1);
	}

	// ---- own WAVs
	bool loadSample(int n, const std::string& path) {
		std::vector<float> data;
		float wsr = 44100.f;
		if (!readWavFile(path, data, wsr)) {
			sampleStatus[n] = system::exists(path) ? "could not read " : "missing: ";
			sampleStatus[n] += system::getFilename(path);
			return false;
		}
		if (data.size() > (size_t)(wsr * 20.f)) data.resize((size_t)(wsr * 20.f));
		std::vector<float> conv = CymbalsEngine::prepareSample(data, wsr);
		if (conv.empty()) { sampleStatus[n] = "could not read " + system::getFilename(path); return false; }
		handOver(n, conv);
		samplePath[n] = path;
		sampleStatus[n].clear();
		return true;
	}
	void clearSample(int n) { std::vector<float> none; handOver(n, none); samplePath[n].clear(); sampleStatus[n].clear(); }
	void handOver(int n, std::vector<float>& d) {
		std::vector<float> copy;
		if (n == 0) copy = d;   // the hats: CH and OH each hold one
		std::lock_guard<std::mutex> lock(sampleMutex);
		newSample[n][0].swap(d);   // old pending buffers are freed here, on this thread
		newSample[n][1].swap(copy);
		sampleReady[n] = true;
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != sr) { sr = args.sampleRate; kit.setSampleRate(sr); }
		uint32_t ns = newSerial.exchange(0);
		if (ns) { serial = ns; kit.setSerial(ns); }
		if (sampleReady[0] || sampleReady[1] || sampleReady[2]) {
			std::unique_lock<std::mutex> lock(sampleMutex, std::try_to_lock);
			if (lock.owns_lock()) {
				if (sampleReady[0]) { kit.cymbal(CH).swapSample(newSample[0][0]); kit.cymbal(OH).swapSample(newSample[0][1]); sampleReady[0] = false; }
				if (sampleReady[1]) { kit.cymbal(CR).swapSample(newSample[1][0]); sampleReady[1] = false; }
				if (sampleReady[2]) { kit.cymbal(RD).swapSample(newSample[2][0]); sampleReady[2] = false; }
			}
		}
		bool shift = params[SHIFT_PARAM].getValue() > 0.5f;

		// ---- UI: voice buttons, screen taps, panel buttons
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
				select(i, -1);
			}
			else select(i, i == selVoice ? selBlock : -1);
		}
		if (reqVoice >= 0) { int v = reqVoice; reqVoice = -1; select(v, reqBlock); reqBlock = -1; }
		else if (reqBlock >= 0) { int b = reqBlock; reqBlock = -1; select(selVoice, b); }
		{
			float s = params[SELECT_PARAM].getValue();
			if (s != selShadow) {
				int nb = lay(selVoice).nBlocks;
				int b = clamp((int)(s * nb), 0, nb - 1);
				selShadow = s;
				if (b != selBlock) { selBlock = b; for (int j = 0; j < 4; j++) editShadow[j] = -1.f; }
			}
		}
		if (stockBtn.process(params[STOCK_PARAM].getValue() > 0.5f)) {
			Voice* v = kit.v[selVoice];
			if (shift) { v->toStock(); params[SHIFT_PARAM].setValue(0.f); }
			else {
				const Block& b = lay(selVoice).blocks[selBlock];
				for (int j = 0; j < b.n; j++) v->set(b.p[j], v->stockOf(b.p[j]));
			}
		}
		if (copyBtn.process(params[COPY_PARAM].getValue() > 0.5f)) copyFrom = copyFrom == selVoice ? -1 : selVoice;
		if (kitsBtn.process(params[KITS_PARAM].getValue() > 0.5f)) {
			if (shift) { for (auto* v : kit.v) v->toStock(); params[SHIFT_PARAM].setValue(0.f); }
			else { store(kitNo); kitNo = (kitNo + 1) % KITS_N; load(kitNo); }
			for (int j = 0; j < 4; j++) editShadow[j] = -1.f;
		}

		// ---- the four knobs follow the selected block (and show edits made on the screen)
		Voice* sv = kit.v[selVoice];
		for (int j = 0; j < 4; j++) {
			int p = blockParam(j);
			if (p < 0) continue;
			float pv = params[EDIT_PARAM + j].getValue();
			if (editShadow[j] >= 0.f && pv != editShadow[j]) { sv->set(p, pv); editShadow[j] = sv->get(p); }
			else if (sv->get(p) != editShadow[j]) { editShadow[j] = sv->get(p); params[EDIT_PARAM + j].setValue(editShadow[j]); }
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
static const NVGcolor INK = nvgRGB(0xEC, 0xE6, 0xD8), DIM = nvgRGB(0x8C, 0x8A, 0x84), AMB = nvgRGB(0xF3, 0xB0, 0x4A),
	REDC = nvgRGB(0xE5, 0x63, 0x1F), SCRL = nvgRGB(0x4A, 0x5A, 0x52), PANEL_INK = nvgRGB(0x26, 0x26, 0x26);

static std::shared_ptr<window::Font> monoFont() { return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf")); }

// a kit for the module browser picture
static Kit& demoKit() { static Kit* k = new Kit(); return *k; }

struct BenchScreen : OpaqueWidget {
	Bench909* module = nullptr;
	int dragVoice = -1, dragParam = -1;
	float scale() const { return box.size.x / SCR_W; }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<window::Font> font = monoFont();
		if (!font || font->handle < 0) return;
		NVGcontext* vg = args.vg;
		Kit& kit = module ? module->kit : demoKit();
		int v = module ? module->selVoice : BD;
		int sb = module ? module->selBlock : 1;
		const Layout& L = layout(kindOf(v));
		nvgSave(vg);
		nvgScale(vg, scale(), scale());
		nvgFontFaceId(vg, font->handle);
		// header
		nvgFontSize(vg, 4.2f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, hexCol(colour(v)));
		std::string title = std::string(code(v)) + "  " + longName(v);
		nvgText(vg, 3.f, 5.8f, title.c_str(), NULL);
		nvgFontSize(vg, 2.6f);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		nvgFillColor(vg, DIM);
		std::string right = string::f("KIT %c", 'A' + (module ? module->kitNo : 0));
		if (module && module->copyFrom >= 0) right = string::f("COPY %s: tap a voice   |   ", code(module->copyFrom)) + right;
		else right = "tap a block, drag a value   |   " + right;
		nvgText(vg, SCR_W - 3.f, 5.6f, right.c_str(), NULL);
		// arrows
		nvgStrokeColor(vg, DIM);
		nvgFillColor(vg, DIM);
		nvgStrokeWidth(vg, 0.3f);
		for (int a = 0; a < L.nArrows; a++) {
			const Block& A = L.blocks[L.arrows[a].from];
			const Block& B = L.blocks[L.arrows[a].to];
			float ax = blockX(A), ay = blockY(A), bx = blockX(B), by = blockY(B);
			nvgBeginPath(vg);
			if (A.col == B.col) {   // straight down
				float x = ax + BLK_W / 2;
				nvgMoveTo(vg, x, ay + blockH(A)); nvgLineTo(vg, x, by - 0.4f);
				nvgStroke(vg);
				nvgBeginPath(vg); nvgMoveTo(vg, x, by); nvgLineTo(vg, x - 0.8f, by - 1.4f); nvgLineTo(vg, x + 0.8f, by - 1.4f); nvgClosePath(vg); nvgFill(vg);
				continue;
			}
			float y0 = ay + 2.6f, y1 = by + 2.6f, x0 = ax + BLK_W, mx = (x0 + bx) / 2.f;
			nvgMoveTo(vg, x0, y0); nvgLineTo(vg, mx, y0); nvgLineTo(vg, mx, y1); nvgLineTo(vg, bx - 0.4f, y1);
			nvgStroke(vg);
			nvgBeginPath(vg); nvgMoveTo(vg, bx, y1); nvgLineTo(vg, bx - 1.4f, y1 - 0.8f); nvgLineTo(vg, bx - 1.4f, y1 + 0.8f); nvgClosePath(vg); nvgFill(vg);
		}
		// blocks
		Voice* vo = kit.v[v];
		for (int b = 0; b < L.nBlocks; b++) {
			const Block& B = L.blocks[b];
			bool sel = b == sb;
			float x = blockX(B), y = blockY(B), h = blockH(B);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x, y, BLK_W, h, 0.8f);
			nvgFillColor(vg, sel ? nvgRGB(0x2A, 0x3A, 0x31) : nvgRGB(0x15, 0x1D, 0x19));
			nvgFill(vg);
			nvgStrokeColor(vg, sel ? AMB : SCRL);
			nvgStrokeWidth(vg, sel ? 0.5f : 0.3f);
			nvgStroke(vg);
			nvgFontSize(vg, 2.6f);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgFillColor(vg, sel ? AMB : INK);
			nvgText(vg, x + 1.8f, y + 3.5f, B.name, NULL);
			for (int j = 0; j < B.n; j++) {
				int p = B.p[j];
				float yy = y + 7.1f + j * ROW_H;
				char buf[32];
				std::string nm = p == PAN ? "PAN" : vo->def(p).label;
				if (sel) nm = string::f("%d ", j + 1) + nm;
				if (module) module->formatParam(v, p, vo->get(p), buf, sizeof(buf));
				else if (p == PAN) snprintf(buf, sizeof(buf), "C");
				else format(buf, sizeof(buf), v, vo->def(p), vo->get(p));
				nvgFontSize(vg, 2.25f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
				nvgFillColor(vg, sel ? INK : DIM);
				nvgText(vg, x + 1.8f, yy, nm.c_str(), NULL);
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
				nvgFillColor(vg, AMB);
				nvgText(vg, x + BLK_W - 4.4f, yy, buf, NULL);
				// stock dot: filled at the 909 setting, a ring when moved
				bool atStock = std::fabs(vo->get(p) - vo->stockOf(p)) < 0.004f;
				nvgBeginPath(vg);
				nvgCircle(vg, x + BLK_W - 2.3f, yy - 0.75f, 0.6f);
				if (atStock) { nvgFillColor(vg, REDC); nvgFill(vg); }
				else { nvgStrokeColor(vg, REDC); nvgStrokeWidth(vg, 0.25f); nvgStroke(vg); }
			}
		}
		nvgRestore(vg);
	}

	// which block (and parameter row) is at a point in mm
	bool hitTest(Vec mm, int& block, int& param) {
		if (!module) return false;
		const Layout& L = layout(kindOf(module->selVoice));
		for (int b = 0; b < L.nBlocks; b++) {
			const Block& B = L.blocks[b];
			float x = blockX(B), y = blockY(B);
			if (mm.x < x || mm.x > x + BLK_W || mm.y < y || mm.y > y + blockH(B)) continue;
			block = b; param = -1;
			int row = (int)std::floor((mm.y - y - 4.6f) / ROW_H);
			if (row >= 0 && row < B.n) param = B.p[row];
			return true;
		}
		return false;
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			int b, p;
			if (hitTest(e.pos.div(scale()), b, p)) {
				module->reqBlock = b;
				dragVoice = module->selVoice; dragParam = p;
				e.consume(this);
				return;
			}
			dragParam = -1;
		}
		OpaqueWidget::onButton(e);
	}
	void onDragMove(const DragMoveEvent& e) override {
		if (!module || dragParam < 0 || dragVoice != module->selVoice) return;
		Voice* v = module->kit.v[dragVoice];
		float sens = (APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_SHIFT ? 0.0008f : 0.004f;
		v->set(dragParam, v->get(dragParam) - e.mouseDelta.y / getAbsoluteZoom() * sens);
	}
	void onDoubleClick(const DoubleClickEvent& e) override {
		if (module && dragParam >= 0 && dragVoice == module->selVoice) {
			Voice* v = module->kit.v[dragVoice];
			v->set(dragParam, v->stockOf(dragParam));
		}
	}
};

// a flat panel button drawn in the 909's colours
struct FlatButton : app::Switch {
	Bench909* bench = nullptr;
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
			if (role == 2) lit = bench->params[Bench909::SHIFT_PARAM].getValue() > 0.5f;
		}
		else if (voice == BD) lit = true;
		NVGcolor f = fill;
		if (voice < 0 && lit) f = AMB;
		if (voice >= 0) f = nvgLerpRGBA(nvgLerpRGBA(f, nvgRGB(0, 0, 0), lit ? 0.f : 0.08f), nvgRGB(255, 255, 255), 0.5f * glow);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(vg, f);
		nvgFill(vg);
		if (voice >= 0) {
			nvgStrokeColor(vg, nvgRGB(0x9C, 0x9A, 0x93)); nvgStrokeWidth(vg, 0.75f); nvgStroke(vg);
		}
		if (voice >= 0 && lit) {   // the selected voice: a dark ring, as on the light panel
			nvgBeginPath(vg);
			nvgRoundedRect(vg, -1.6f, -1.6f, box.size.x + 3.2f, box.size.y + 3.2f, 4.f);
			nvgStrokeColor(vg, PANEL_INK); nvgStrokeWidth(vg, 1.2f); nvgStroke(vg);
		}
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		if (font && font->handle >= 0) {
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, voice >= 0 ? 9.5f : 7.5f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, voice >= 0 || lit ? nvgRGB(0x1B, 0x1B, 0x1B) : ink);
			nvgText(vg, box.size.x / 2, box.size.y / 2 + 0.5f, label.c_str(), NULL);
		}
	}
};

// the name of what each edit knob turns, above it
struct EditLabel : TransparentWidget {
	Bench909* module = nullptr;
	int j = 0;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::string s;
		if (module) { int p = module->blockParam(j); s = p < 0 ? "-" : module->paramName(module->selVoice, p); }
		else { static const char* d[4] = {"TUNE", "SWEEP", "BEND", "-"}; s = d[j]; }
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		if (!font || font->handle < 0) return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 10.f);
		nvgTextLetterSpacing(args.vg, 0.4f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
		nvgFillColor(args.vg, PANEL_INK);
		nvgText(args.vg, box.size.x / 2, box.size.y, (std::to_string(j + 1) + " " + s).c_str(), NULL);
	}
};

struct Bench909Widget : ModuleWidget {
	Bench909Widget(Bench909* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bench909.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		BenchScreen* scr = createWidget<BenchScreen>(mm2px(Vec(SCR_X, SCR_Y)));
		scr->box.size = mm2px(Vec(SCR_W, SCR_H));
		scr->module = module;
		addChild(scr);

		for (int j = 0; j < 4; j++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(EDIT_X[j], KNOB_Y)), module, Bench909::EDIT_PARAM + j));
			EditLabel* l = createWidget<EditLabel>(mm2px(Vec(EDIT_X[j] - 10.f, 73.f)));
			l->box.size = mm2px(Vec(20.f, 4.3f));
			l->module = module; l->j = j;
			addChild(l);
		}
		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(SEL_X, KNOB_Y)), module, Bench909::SELECT_PARAM));

		auto button = [&](float x, float y, float w, float h, int param, const std::string& label, int voice, int role) {
			FlatButton* b = createParam<FlatButton>(mm2px(Vec(x - w / 2, y - h / 2)), module, param);
			b->box.size = mm2px(Vec(w, h));
			b->bench = module; b->label = label; b->voice = voice; b->role = role;
			if (voice >= 0) b->fill = hexCol(colour(voice));
			if (role == 2) b->momentary = false;
			addParam(b);
		};
		button(BTN_X[0], BTN_Y[0], 11.f, 4.6f, Bench909::STOCK_PARAM, "STOCK", -1, 0);
		button(BTN_X[0], BTN_Y[1], 11.f, 4.6f, Bench909::COPY_PARAM, "COPY", -1, 1);
		button(BTN_X[1], BTN_Y[0], 11.f, 4.6f, Bench909::KITS_PARAM, "KITS", -1, 0);
		button(BTN_X[1], BTN_Y[1], 11.f, 4.6f, Bench909::SHIFT_PARAM, "SHIFT", -1, 2);

		for (int i = 0; i < NV; i++) {
			float x = 8.f + PITCH * (i + 0.5f);
			button(x, VOICE_Y, PITCH - 2.4f, 4.6f, Bench909::VOICE_PARAM + i, code(i), i, 0);
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, TRIG_Y)), module, Bench909::TRIG_INPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, OUT_Y)), module, Bench909::OUT_OUTPUT + i));
		}
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(MIX_X[0], MIX_IN_Y)), module, Bench909::ACC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(MIX_X[1], MIX_IN_Y)), module, Bench909::POLY_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(MIX_X[0], OUT_Y)), module, Bench909::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(MIX_X[1], OUT_Y)), module, Bench909::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Bench909* m = dynamic_cast<Bench909*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Serial number %u (sets this unit's AGE character)", (unsigned)m->serial)));
		menu->addChild(createMenuItem("New serial number (a different old unit)", "", [=]() {
			m->newSerial = random::u32();   // applied on the audio thread
		}));
		menu->addChild(createMenuItem("This kit back to the stock 909", "", [=]() { for (auto* v : m->kit.v) v->toStock(); }));
		menu->addChild(new MenuSeparator);
		static const char* names[Bench909::NS] = {"Hats (CH + OH)", "Crash (CR)", "Ride (RD)"};
		for (int n = 0; n < Bench909::NS; n++) {
			std::string src = !m->sampleStatus[n].empty() ? "built-in metal (" + m->sampleStatus[n] + ")"
				: m->samplePath[n].empty() ? "built-in metal" : system::getFilename(m->samplePath[n]);
			menu->addChild(createSubmenuItem(std::string(names[n]) + " source", src, [=](Menu* sub) {
				sub->addChild(createMenuItem("Load sample WAV (replaces the built-in source)", "", [=]() {
					char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, osdialog_filters_parse("WAV:wav,WAV"));
					if (path) { m->loadSample(n, path); std::free(path); }
				}));
				if (!m->samplePath[n].empty()) sub->addChild(createMenuItem("Back to the built-in source", "", [=]() { m->clearSample(n); }));
			}));
		}
	}
};

Model* modelBench909 = createModel<Bench909, Bench909Widget>("Bench909");
