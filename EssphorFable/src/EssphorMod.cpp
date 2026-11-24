// Essphor Mod: WAVE, RANDOM, FOLLOW and ADSR. Part C of docs/BUILD-INSTRUCTIONS.md.
#include "plugin.hpp"
#include "dsp/Mod.hpp"
#include "layout.hpp"
#include <atomic>

using namespace essphor;

struct EssphorMod : Module {
	enum ParamIds {
		W_RATE_PARAM, W_AMOUNT_PARAM, W_PHASE_PARAM, W_OFFSET_PARAM, W_SKEW_PARAM, W_FOLD_PARAM, W_CURVE_PARAM, W_SPREAD_PARAM, W_RATE_MODE_PARAM, W_SHAPE_PARAM,
		R_RATE_PARAM, R_AMOUNT_PARAM, R_PHASE_PARAM, R_OFFSET_PARAM, R_LENGTH_PARAM, R_VARIATION_PARAM, R_SMOOTH_PARAM, R_SPREAD_PARAM, R_RATE_MODE_PARAM,
		F_DELAY_PARAM, F_AMOUNT_PARAM, F_RISE_PARAM, F_FALL_PARAM, F_INPUT_PARAM, F_FILTER_PARAM, F_HOLD_PARAM, F_SPREAD_PARAM,
		A_ATTACK_PARAM, A_DECAY_PARAM, A_SUSTAIN_PARAM, A_RELEASE_PARAM, A_ATT_CURVE_PARAM, A_DEC_CURVE_PARAM, A_REL_CURVE_PARAM, A_SPREAD_PARAM, A_TRIG_PARAM,
		NUM_PARAMS
	};
	enum InputIds { CLOCK_INPUT, RESET_INPUT, F_IN_L_INPUT, F_IN_R_INPUT, A_GATE_INPUT, NUM_INPUTS };
	enum OutputIds { W_OUT_A_OUTPUT, W_OUT_B_OUTPUT, R_OUT_A_OUTPUT, R_OUT_B_OUTPUT, F_OUT_A_OUTPUT, F_OUT_B_OUTPUT, A_OUT_A_OUTPUT, A_OUT_B_OUTPUT, NUM_OUTPUTS };
	enum LightIds { A_TRIG_LIGHT, NUM_LIGHTS };

	Clock clk;
	Wave wave;
	RandomSeq rnd;
	Follow fol;
	Adsr adsr;
	dsp::SchmittTrigger clockTrig, resetTrig, gateTrig;
	dsp::ClockDivider lightDiv;
	int ppqn = 1;
	std::atomic<bool> reqNewSeq{false};
	uint64_t seqSeedA = 1, seqSeedB = 2;

	static std::string rateText(int mode, float v) {
		if (mode == RM_FREE) return fmtHz(modFreeHz(v));
		const char* suffix = mode == RM_TRIPLET ? " triplet" : mode == RM_DOTTED ? " dotted" : "";
		return string::f("%s%s", MOD_DIV_NAMES[modDivIndex(v)], suffix);
	}

	EssphorMod() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const std::vector<std::string> rateModes = {"FREE", "STRAIGHT", "TRIPLET", "DOTTED"};
		// WAVE
		configParam<FnQuantity>(W_RATE_PARAM, 0.f, 1.f, 0.5f, "Wave rate")->fn = [this](float v) { return rateText((int)params[W_RATE_MODE_PARAM].getValue(), v); };
		configParam(W_AMOUNT_PARAM, -1.f, 1.f, 1.f, "Wave amount", "%", 0.f, 100.f);
		configParam(W_PHASE_PARAM, 0.f, 360.f, 0.f, "Wave phase", "°");
		configParam(W_OFFSET_PARAM, -1.f, 1.f, 0.f, "Wave offset", " V", 0.f, 5.f);
		configParam(W_SKEW_PARAM, -1.f, 1.f, 0.f, "Wave skew", "%", 0.f, 100.f);
		configParam(W_FOLD_PARAM, 0.f, 1.f, 0.f, "Wave fold", "%", 0.f, 100.f);
		configParam(W_CURVE_PARAM, -1.f, 1.f, 0.f, "Wave curve", "%", 0.f, 100.f);
		configParam(W_SPREAD_PARAM, 0.f, 1.f, 0.f, "Wave spread (B ahead of A)", "°", 0.f, 180.f);
		configSwitch(W_RATE_MODE_PARAM, 0.f, 3.f, 0.f, "Wave rate mode", rateModes);
		configSwitch(W_SHAPE_PARAM, 0.f, 2.f, 0.f, "Wave shape", {"SIN", "TRI", "SQUARE"});
		// RANDOM
		configParam<FnQuantity>(R_RATE_PARAM, 0.f, 1.f, 6.f / 9.f, "Random step rate")->fn = [this](float v) { return rateText((int)params[R_RATE_MODE_PARAM].getValue(), v); };
		configParam(R_AMOUNT_PARAM, -1.f, 1.f, 1.f, "Random amount", "%", 0.f, 100.f);
		configParam(R_PHASE_PARAM, 0.f, 360.f, 0.f, "Random phase", "°");
		configParam(R_OFFSET_PARAM, -1.f, 1.f, 0.f, "Random offset", " V", 0.f, 5.f);
		auto* len = configParam(R_LENGTH_PARAM, 1.f, 32.f, 8.f, "Random length", " steps");
		len->snapEnabled = true;
		configParam(R_VARIATION_PARAM, 0.f, 1.f, 0.f, "Random variation", "%", 0.f, 100.f)->description = "Chance that a step is re-rolled each time it comes round.";
		configParam(R_SMOOTH_PARAM, 0.f, 1.f, 0.f, "Random smooth", "%", 0.f, 100.f);
		configParam(R_SPREAD_PARAM, 0.f, 1.f, 0.f, "Random spread (B towards its own sequence)", "%", 0.f, 100.f);
		configSwitch(R_RATE_MODE_PARAM, 0.f, 3.f, 1.f, "Random rate mode", rateModes);
		// FOLLOW
		configParam(F_DELAY_PARAM, 0.f, 0.5f, 0.f, "Follow delay", " ms", 0.f, 1000.f);
		configParam(F_AMOUNT_PARAM, -1.f, 1.f, 1.f, "Follow amount", "%", 0.f, 100.f);
		configParam<FnQuantity>(F_RISE_PARAM, 0.f, 1.f, std::sqrt(0.005f), "Follow rise")->fn = [](float v) { return fmtSeconds((double)v * v); };
		configParam<FnQuantity>(F_FALL_PARAM, 0.f, 1.f, std::sqrt(0.15f / 4.f), "Follow fall")->fn = [](float v) { return fmtSeconds(4.0 * v * v); };
		configParam(F_INPUT_PARAM, 0.f, 4.f, 1.f, "Follow input gain", "x");
		configParam<FnQuantity>(F_FILTER_PARAM, -1.f, 1.f, 0.f, "Follow filter")->fn = [](float v) {
			if (v < -0.001f) return "low-pass " + fmtHz(20000.0 * std::pow(80.0 / 20000.0, -(double)v));
			if (v > 0.001f) return "high-pass " + fmtHz(20.0 * std::pow(250.0, (double)v));
			return std::string("off");
		};
		configParam<FnQuantity>(F_HOLD_PARAM, 0.f, 1.f, 0.f, "Follow sample and hold")->fn = [this](float v) {
			if (v <= 0.02f) return std::string("off");
			if (inputs[CLOCK_INPUT].isConnected()) {
				static const char* N[6] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"};
				return std::string(N[clamp((int)std::lround((v - 0.02f) / 0.98f * 5), 0, 5)]);
			}
			return fmtHz(0.5 * std::pow(100.0, (v - 0.02) / 0.98));
		};
		configParam(F_SPREAD_PARAM, 0.f, 1.f, 0.f, "Follow spread (A to left, B to right)", "%", 0.f, 100.f);
		// ADSR
		configParam(A_ATTACK_PARAM, 0.f, 1.f, (float)(std::log(5.0) / std::log(10000.0)), "Attack", " ms", 10000.f, 1.f);
		configParam(A_DECAY_PARAM, 0.f, 1.f, (float)(std::log(200.0) / std::log(10000.0)), "Decay", " ms", 10000.f, 1.f);
		configParam(A_SUSTAIN_PARAM, 0.f, 1.f, 0.6f, "Sustain", "%", 0.f, 100.f);
		configParam(A_RELEASE_PARAM, 0.f, 1.f, (float)(std::log(300.0) / std::log(20000.0)), "Release", " ms", 20000.f, 1.f);
		configParam(A_ATT_CURVE_PARAM, -1.f, 1.f, 0.f, "Attack curve (- fast start, + slow start)");
		configParam(A_DEC_CURVE_PARAM, -1.f, 1.f, 0.f, "Decay curve");
		configParam(A_REL_CURVE_PARAM, -1.f, 1.f, 0.f, "Release curve");
		configParam(A_SPREAD_PARAM, 0.f, 1.f, 0.f, "ADSR spread (B delayed)", " ms", 0.f, 250.f);
		configButton(A_TRIG_PARAM, "Trigger (acts as a gate while held)");
		for (int m : {W_RATE_MODE_PARAM, W_SHAPE_PARAM, R_RATE_MODE_PARAM, A_TRIG_PARAM}) getParamQuantity(m)->randomizeEnabled = false;

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(F_IN_L_INPUT, "Follow left");
		configInput(F_IN_R_INPUT, "Follow right (follows left when unpatched)");
		configInput(A_GATE_INPUT, "ADSR gate");
		const char* sec[4] = {"Wave", "Random", "Follow", "ADSR"};
		for (int i = 0; i < 4; i++) {
			configOutput(W_OUT_A_OUTPUT + 2 * i, string::f("%s A", sec[i]));
			configOutput(W_OUT_B_OUTPUT + 2 * i, string::f("%s B (shifted by spread)", sec[i]));
		}
		lightDiv.setDivision(64);
		newSequences();
		applySeeds();
		clk.reset();
	}

	void newSequences() { seqSeedA = random::u64() | 1; seqSeedB = random::u64() | 1; }
	void applySeeds() { rnd.seed(seqSeedA, seqSeedB); }

	void onSampleRateChange(const SampleRateChangeEvent& e) override { fol.init(e.sampleRate); adsr.init(e.sampleRate); }
	void onAdd(const AddEvent& e) override { fol.init(APP->engine->getSampleRate()); adsr.init(APP->engine->getSampleRate()); }

	double v(int id) { return params[id].getValue(); }

	void process(const ProcessArgs& args) override {
		if (fol.fs != args.sampleRate || (int)fol.dA.size() == 0) fol.init(args.sampleRate);
		if (adsr.fs != args.sampleRate || adsr.dl.empty()) adsr.init(args.sampleRate);
		if (reqNewSeq.exchange(false)) applySeeds();
		const double dt = args.sampleTime;

		clk.ppqn = ppqn;
		const bool clockOn = inputs[CLOCK_INPUT].isConnected();
		const bool edge = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		const bool rst = resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		clk.tick(dt, clockOn, edge, rst, 120.0);

		WaveParams& w = wave.p;
		w.rateMode = (int)v(W_RATE_MODE_PARAM); w.rate = v(W_RATE_PARAM); w.amount = v(W_AMOUNT_PARAM); w.phase = v(W_PHASE_PARAM);
		w.offset = v(W_OFFSET_PARAM); w.skew = v(W_SKEW_PARAM); w.fold = v(W_FOLD_PARAM); w.curve = v(W_CURVE_PARAM); w.spread = v(W_SPREAD_PARAM);
		w.shape = (int)v(W_SHAPE_PARAM);
		wave.process(clk, dt);

		RandomParams& r = rnd.p;
		r.rateMode = (int)v(R_RATE_MODE_PARAM); r.rate = v(R_RATE_PARAM); r.amount = v(R_AMOUNT_PARAM); r.phase = v(R_PHASE_PARAM);
		r.offset = v(R_OFFSET_PARAM); r.length = (int)std::lround(v(R_LENGTH_PARAM)); r.variation = v(R_VARIATION_PARAM);
		r.smooth = v(R_SMOOTH_PARAM); r.spread = v(R_SPREAD_PARAM);
		rnd.process(clk, dt);

		FollowParams& f = fol.p;
		f.delay = v(F_DELAY_PARAM); f.amount = v(F_AMOUNT_PARAM);
		f.rise = v(F_RISE_PARAM) * v(F_RISE_PARAM); f.fall = 4.0 * v(F_FALL_PARAM) * v(F_FALL_PARAM);
		f.input = v(F_INPUT_PARAM); f.filter = v(F_FILTER_PARAM); f.hold = v(F_HOLD_PARAM); f.spread = v(F_SPREAD_PARAM);
		if (outputs[F_OUT_A_OUTPUT].isConnected() || outputs[F_OUT_B_OUTPUT].isConnected()) {
			const bool lOn = inputs[F_IN_L_INPUT].isConnected(), rOn = inputs[F_IN_R_INPUT].isConnected();
			const float l = lOn ? inputs[F_IN_L_INPUT].getVoltage() : (rOn ? inputs[F_IN_R_INPUT].getVoltage() : 0.f);
			const float rr = rOn ? inputs[F_IN_R_INPUT].getVoltage() : l;
			fol.process(l, rr, clk, clockOn, dt);
		}

		AdsrParams& a = adsr.p;
		a.attack = 0.001 * std::pow(10000.0, v(A_ATTACK_PARAM));
		a.decay = 0.001 * std::pow(10000.0, v(A_DECAY_PARAM));
		a.release = 0.001 * std::pow(20000.0, v(A_RELEASE_PARAM));
		a.sustain = v(A_SUSTAIN_PARAM);
		a.attC = v(A_ATT_CURVE_PARAM); a.decC = v(A_DEC_CURVE_PARAM); a.relC = v(A_REL_CURVE_PARAM);
		a.spread = v(A_SPREAD_PARAM);
		gateTrig.process(inputs[A_GATE_INPUT].getVoltage(), 0.1f, 1.f);
		const bool gate = (inputs[A_GATE_INPUT].isConnected() && gateTrig.isHigh()) || params[A_TRIG_PARAM].getValue() > 0.5f;
		adsr.process(gate, dt);

		outputs[W_OUT_A_OUTPUT].setVoltage((float)wave.outA); outputs[W_OUT_B_OUTPUT].setVoltage((float)wave.outB);
		outputs[R_OUT_A_OUTPUT].setVoltage((float)rnd.outA); outputs[R_OUT_B_OUTPUT].setVoltage((float)rnd.outB);
		outputs[F_OUT_A_OUTPUT].setVoltage((float)fol.outA); outputs[F_OUT_B_OUTPUT].setVoltage((float)fol.outB);
		outputs[A_OUT_A_OUTPUT].setVoltage((float)adsr.outA); outputs[A_OUT_B_OUTPUT].setVoltage((float)adsr.outB);
		if (lightDiv.process()) lights[A_TRIG_LIGHT].setBrightness((float)(adsr.outA / 10.0));
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "ppqn", json_integer(ppqn));
		json_object_set_new(root, "seedA", json_string(string::f("%016llx", (unsigned long long)seqSeedA).c_str()));
		json_object_set_new(root, "seedB", json_string(string::f("%016llx", (unsigned long long)seqSeedB).c_str()));
		json_t* va = json_array(); json_t* vb = json_array();
		for (int i = 0; i < 32; i++) { json_array_append_new(va, json_real(rnd.valA[i])); json_array_append_new(vb, json_real(rnd.valB[i])); }
		json_object_set_new(root, "stepsA", va);
		json_object_set_new(root, "stepsB", vb);
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "ppqn")) { int p = (int)json_integer_value(j); ppqn = (p == 2 || p == 4 || p == 24) ? p : 1; }
		json_t* a = json_object_get(root, "seedA");
		json_t* b = json_object_get(root, "seedB");
		if (a && json_is_string(a)) seqSeedA = std::strtoull(json_string_value(a), nullptr, 16) | 1;
		if (b && json_is_string(b)) seqSeedB = std::strtoull(json_string_value(b), nullptr, 16) | 1;
		applySeeds();
		json_t* va = json_object_get(root, "stepsA");
		json_t* vb = json_object_get(root, "stepsB");
		if (va && vb && json_array_size(va) == 32 && json_array_size(vb) == 32) {
			for (int i = 0; i < 32; i++) {
				rnd.valA[i] = json_number_value(json_array_get(va, i));
				rnd.valB[i] = json_number_value(json_array_get(vb, i));
			}
		}
	}
};

struct EssphorModWidget : ModuleWidget {
	EssphorModWidget(EssphorMod* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/EssphorMod.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		namespace L = layout::EssphorMod;
		using M = EssphorMod;
		auto small = [&](float x, float y, int id) { addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(x, y)), module, id)); };
		auto in = [&](float x, float y, int id) { addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };
		auto out = [&](float x, float y, int id) { addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };

		const float x[36] = {
			L::W_RATE_X, L::W_AMOUNT_X, L::W_PHASE_X, L::W_OFFSET_X, L::W_SKEW_X, L::W_FOLD_X, L::W_CURVE_X, L::W_SPREAD_X, 0, 0,
			L::R_RATE_X, L::R_AMOUNT_X, L::R_PHASE_X, L::R_OFFSET_X, L::R_LENGTH_X, L::R_VARIATION_X, L::R_SMOOTH_X, L::R_SPREAD_X, 0,
			L::F_DELAY_X, L::F_AMOUNT_X, L::F_RISE_X, L::F_FALL_X, L::F_INPUT_X, L::F_FILTER_X, L::F_HOLD_X, L::F_SPREAD_X,
			L::A_ATTACK_X, L::A_DECAY_X, L::A_SUSTAIN_X, L::A_RELEASE_X, L::A_ATT_CURVE_X, L::A_DEC_CURVE_X, L::A_REL_CURVE_X, L::A_SPREAD_X, 0};
		const float y[36] = {
			L::W_RATE_Y, L::W_AMOUNT_Y, L::W_PHASE_Y, L::W_OFFSET_Y, L::W_SKEW_Y, L::W_FOLD_Y, L::W_CURVE_Y, L::W_SPREAD_Y, 0, 0,
			L::R_RATE_Y, L::R_AMOUNT_Y, L::R_PHASE_Y, L::R_OFFSET_Y, L::R_LENGTH_Y, L::R_VARIATION_Y, L::R_SMOOTH_Y, L::R_SPREAD_Y, 0,
			L::F_DELAY_Y, L::F_AMOUNT_Y, L::F_RISE_Y, L::F_FALL_Y, L::F_INPUT_Y, L::F_FILTER_Y, L::F_HOLD_Y, L::F_SPREAD_Y,
			L::A_ATTACK_Y, L::A_DECAY_Y, L::A_SUSTAIN_Y, L::A_RELEASE_Y, L::A_ATT_CURVE_Y, L::A_DEC_CURVE_Y, L::A_REL_CURVE_Y, L::A_SPREAD_Y, 0};
		for (int i = 0; i < M::NUM_PARAMS; i++) {
			if (i == M::W_RATE_MODE_PARAM || i == M::W_SHAPE_PARAM || i == M::R_RATE_MODE_PARAM || i == M::A_TRIG_PARAM) continue;
			small(x[i], y[i], i);
		}
		auto cyc = [&](float cx, float cy, int id) { addParam(createParamCentered<CycleButton>(mm2px(Vec(cx, cy)), module, id)); };
		cyc(L::W_RATE_MODE_X, L::W_RATE_MODE_Y, M::W_RATE_MODE_PARAM);
		cyc(L::W_SHAPE_X, L::W_SHAPE_Y, M::W_SHAPE_PARAM);
		cyc(L::R_RATE_MODE_X, L::R_RATE_MODE_Y, M::R_RATE_MODE_PARAM);
		static const std::vector<std::string> RM = {"FREE", "STRAIGHT", "TRIPLET", "DOTTED"};
		static const std::vector<std::string> SH = {"SIN", "TRI", "SQUARE"};
		auto modeText = [module](int id, const std::vector<std::string>* names, int dflt) {
			return [=]() { int i = module ? (int)module->params[id].getValue() : dflt; return (*names)[clamp(i, 0, (int)names->size() - 1)]; };
		};
		addChild(makeDisplay(L::W_RATE_MODE_DISP_X, L::W_RATE_MODE_DISP_Y, L::W_RATE_MODE_DISP_W, L::W_RATE_MODE_DISP_H, modeText(M::W_RATE_MODE_PARAM, &RM, 0), 9.f));
		addChild(makeDisplay(L::W_SHAPE_DISP_X, L::W_SHAPE_DISP_Y, L::W_SHAPE_DISP_W, L::W_SHAPE_DISP_H, modeText(M::W_SHAPE_PARAM, &SH, 0), 9.f));
		addChild(makeDisplay(L::R_RATE_MODE_DISP_X, L::R_RATE_MODE_DISP_Y, L::R_RATE_MODE_DISP_W, L::R_RATE_MODE_DISP_H, modeText(M::R_RATE_MODE_PARAM, &RM, 1), 9.f));
		addParam(createLightParamCentered<HoldButton>(mm2px(Vec(L::A_TRIG_X, L::A_TRIG_Y)), module, M::A_TRIG_PARAM, M::A_TRIG_LIGHT));

		in(L::CLOCK_X, L::CLOCK_Y, M::CLOCK_INPUT); in(L::RESET_X, L::RESET_Y, M::RESET_INPUT);
		in(L::F_IN_L_X, L::F_IN_L_Y, M::F_IN_L_INPUT); in(L::F_IN_R_X, L::F_IN_R_Y, M::F_IN_R_INPUT);
		in(L::A_GATE_X, L::A_GATE_Y, M::A_GATE_INPUT);
		out(L::W_OUT_A_X, L::W_OUT_A_Y, M::W_OUT_A_OUTPUT); out(L::W_OUT_B_X, L::W_OUT_B_Y, M::W_OUT_B_OUTPUT);
		out(L::R_OUT_A_X, L::R_OUT_A_Y, M::R_OUT_A_OUTPUT); out(L::R_OUT_B_X, L::R_OUT_B_Y, M::R_OUT_B_OUTPUT);
		out(L::F_OUT_A_X, L::F_OUT_A_Y, M::F_OUT_A_OUTPUT); out(L::F_OUT_B_X, L::F_OUT_B_Y, M::F_OUT_B_OUTPUT);
		out(L::A_OUT_A_X, L::A_OUT_A_Y, M::A_OUT_A_OUTPUT); out(L::A_OUT_B_X, L::A_OUT_B_Y, M::A_OUT_B_OUTPUT);
	}

	void appendContextMenu(Menu* menu) override {
		EssphorMod* m = dynamic_cast<EssphorMod*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(ppqnMenu(&m->ppqn));
		menu->addChild(createMenuItem("New random sequences", "", [=]() { m->newSequences(); m->reqNewSeq = true; }));
	}
};

Model* modelEssphorMod = createModel<EssphorMod, EssphorModWidget>("EssphorMod");
