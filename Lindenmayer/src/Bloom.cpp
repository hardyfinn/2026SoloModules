// Bloom: chord progressions grown by L-system rules on scale degrees.
// Generation 0 is the home chord; each generation rewrites every chord by the
// rule set (e.g. HOMECOMING: 1 -> 1 4, 4 -> 2 5, 5 -> 5 1). The progression is
// played one chord per LENGTH clocks; with GROW on, after each full pass it
// moves to the next, longer generation, and starts again from GEN when it
// gets too long. Chords are diatonic to MODE / KEY and voice-led.
#include "plugin.hpp"
#include "LSys.hpp"
#include "Layout.hpp"

using namespace lsys;
using namespace bloom;

struct BRuleQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		int r = clamp((int) std::round(getValue()), 0, BLOOM_RULES - 1);
		return std::string(BLOOM_RULESETS[r].name) + "  " + BLOOM_RULESETS[r].text;
	}
};
struct ModeQuantity : ParamQuantity {
	std::string getDisplayValueString() override { return BLOOM_MODE_NAMES[clamp((int) std::round(getValue()), 0, BLOOM_MODES - 1)]; }
};
struct KeyQuantity : ParamQuantity {
	std::string getDisplayValueString() override { return NOTE_NAMES[clamp((int) std::round(getValue()), 0, 11)]; }
};

// Chord name like "Gm", "Bb", "Ddim", "Fmaj7".
static std::string chordName(const Bloom& b, int semis) {
	const int* sc = BLOOM_SCALES[b.mode];
	int d = b.degree;
	auto at = [&](int k) { int x = d + k; return sc[x % 7] + 12 * (x / 7); };
	int root = (sc[d] + b.key + semis + 120) % 12;
	int third = at(2) - at(0), fifth = at(4) - at(0), sev = at(6) - at(0);
	std::string q = third == 4 ? "" : (fifth == 6 ? "dim" : "m");
	if (b.sevenths) {
		if (third == 4 && sev == 11) q = "maj7";
		else if (third == 4) q = "7";
		else if (fifth == 6) q = "m7b5";
		else q = "m7";
	}
	return std::string(NOTE_NAMES[root]) + q;
}

struct BloomModule : Module {
	enum ParamIds { RULE_PARAM, MODE_PARAM, KEY_PARAM, LENGTH_PARAM, GEN_PARAM, GROW_PARAM, SEVENTH_PARAM, NUM_PARAMS };
	enum InputIds { CLOCK_INPUT, RESET_INPUT, ROOT_INPUT, NUM_INPUTS };
	enum OutputIds { POLY_OUTPUT, BASS_OUTPUT, GATE_OUTPUT, CHANGE_OUTPUT, NUM_OUTPUTS };
	enum LightIds { NUM_LIGHTS };

	Bloom bl;
	ClockTracker clk;
	dsp::SchmittTrigger clockTrig, resetTrig;
	dsp::PulseGenerator changePulse;
	int lastGenKnob = -1, lastRule = -1;
	int semis = 0;

	BloomModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<BRuleQuantity>(RULE_PARAM, 0.f, BLOOM_RULES - 1, 0.f, "Rule");
		configParam<ModeQuantity>(MODE_PARAM, 0.f, BLOOM_MODES - 1, 1.f, "Mode");
		configParam<KeyQuantity>(KEY_PARAM, 0.f, 11.f, 5.f, "Key");
		configParam(LENGTH_PARAM, 1.f, 32.f, 16.f, "Chord length", " clocks");
		configParam(GEN_PARAM, 0.f, 8.f, 1.f, "Start generation");
		for (int p : {RULE_PARAM, MODE_PARAM, KEY_PARAM, LENGTH_PARAM, GEN_PARAM}) paramQuantities[p]->snapEnabled = true;
		configSwitch(GROW_PARAM, 0.f, 1.f, 1.f, "Grow", {"Off: repeat this generation", "On: next generation after each pass"});
		configSwitch(SEVENTH_PARAM, 0.f, 1.f, 0.f, "Sevenths", {"Triads", "Seventh chords"});
		configInput(CLOCK_INPUT, "Clock (16ths)");
		configInput(RESET_INPUT, "Reset");
		configInput(ROOT_INPUT, "Transpose (V/oct, in semitones)");
		configOutput(POLY_OUTPUT, "Chord (polyphonic V/oct)");
		configOutput(BASS_OUTPUT, "Bass root (V/oct)");
		configOutput(GATE_OUTPUT, "Gate (re-opens on each chord)");
		configOutput(CHANGE_OUTPUT, "Chord change trigger");
	}

	void process(const ProcessArgs& args) override {
		bl.rule = clamp((int) std::round(params[RULE_PARAM].getValue()), 0, BLOOM_RULES - 1);
		bl.mode = clamp((int) std::round(params[MODE_PARAM].getValue()), 0, BLOOM_MODES - 1);
		bl.key = clamp((int) std::round(params[KEY_PARAM].getValue()), 0, 11);
		bl.length = clamp((int) std::round(params[LENGTH_PARAM].getValue()), 1, 32);
		bl.growOn = params[GROW_PARAM].getValue() > 0.5f;
		bl.sevenths = params[SEVENTH_PARAM].getValue() > 0.5f;
		int knob = (int) std::round(params[GEN_PARAM].getValue());
		bl.startGen = std::min(knob, bl.maxGen());
		if (knob != lastGenKnob || bl.rule != lastRule) {
			bl.gen = bl.startGen;
			if (bl.idx >= (int) bl.current().size()) bl.idx = 0;
			lastGenKnob = knob;
			lastRule = bl.rule;
		}
		clk.tick(args.sampleTime);
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) bl.reset();
		bool edge = clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f);
		if (edge) clk.edge();
		bl.process(edge, clk);
		if (bl.changed) changePulse.trigger(1e-3f);
		semis = (int) std::round(inputs[ROOT_INPUT].getVoltage() * 12.f);
		float t = semis / 12.f;
		outputs[POLY_OUTPUT].setChannels(bl.nVoices);
		for (int i = 0; i < bl.nVoices; i++) outputs[POLY_OUTPUT].setVoltage(bl.voices[i] + t, i);
		outputs[BASS_OUTPUT].setVoltage(bl.bass + t);
		outputs[GATE_OUTPUT].setVoltage(bl.gate || !clk.have ? 10.f : 0.f);
		outputs[CHANGE_OUTPUT].setVoltage(changePulse.process(args.sampleTime) ? 10.f : 0.f);
	}
};

// Screen: rule, key and mode; the chord now; the whole progression with
// the current chord lit.
struct BloomDisplay : Widget {
	BloomModule* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		Bloom demo;
		demo.gen = 3;
		const Bloom& b = module ? module->bl : demo;
		int semis = module ? module->semis : 0;
		auto font = screenFont();
		float w = box.size.x, h = box.size.y, pad = mm2px(1.4f);
		screenText(args.vg, font, pad, mm2px(3.4f), 2.6f, LEAF, BLOOM_RULESETS[b.rule].name);
		std::string km = std::string(NOTE_NAMES[(b.key + semis + 120) % 12]) + " " + BLOOM_MODE_NAMES[b.mode];
		screenText(args.vg, font, w - pad, mm2px(3.4f), 2.6f, CREAM, km, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		std::string big = roman(b.mode, b.degree) + "  " + chordName(b, semis);
		screenText(args.vg, font, pad, mm2px(10.2f), 5.0f, CREAM, big);
		screenText(args.vg, font, w - pad, mm2px(10.2f), 2.6f, DIM, string::f("GEN %d", b.gen), NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		// progression, wrapped over up to two lines
		const std::vector<uint8_t>& prog = b.current();
		float x = pad, y = mm2px(15.4f), lineH = mm2px(3.6f);
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(2.6f));
		}
		for (size_t i = 0; i < prog.size(); i++) {
			std::string r = roman(b.mode, prog[i]);
			float bounds[4];
			nvgTextBounds(args.vg, 0, 0, r.c_str(), NULL, bounds);
			float tw = bounds[2] - bounds[0] + mm2px(1.4f);
			if (x + tw > w - pad) {
				x = pad;
				y += lineH;
				if (y > h - mm2px(0.5f)) break;
			}
			bool now = (int) i == b.idx;
			screenText(args.vg, font, x, y, 2.6f, now ? AMBER : DIM, r);
			x += tw;
		}
		Widget::drawLayer(args, layer);
	}
};

struct BloomWidget : ModuleWidget {
	BloomWidget(BloomModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bloom.svg")));
		screws(this);
		BloomDisplay* d = createWidget<BloomDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_RULE_X, K_RULE_Y)), module, BloomModule::RULE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_MODE_X, K_MODE_Y)), module, BloomModule::MODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(K_KEY_X, K_KEY_Y)), module, BloomModule::KEY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_LENGTH_X, K_LENGTH_Y)), module, BloomModule::LENGTH_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(K_GEN_X, K_GEN_Y)), module, BloomModule::GEN_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(S_GROW_X, S_GROW_Y)), module, BloomModule::GROW_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(S_SEVENTH_X, S_SEVENTH_Y)), module, BloomModule::SEVENTH_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_CLOCK_X, I_CLOCK_Y)), module, BloomModule::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_RESET_X, I_RESET_Y)), module, BloomModule::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(I_ROOT_CV_X, I_ROOT_CV_Y)), module, BloomModule::ROOT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_POLY_X, O_POLY_Y)), module, BloomModule::POLY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_BASS_X, O_BASS_Y)), module, BloomModule::BASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_GATE_X, O_GATE_Y)), module, BloomModule::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(O_CHANGE_X, O_CHANGE_Y)), module, BloomModule::CHANGE_OUTPUT));
	}
};

Model* modelBloom = createModel<BloomModule, BloomWidget>("Bloom");
