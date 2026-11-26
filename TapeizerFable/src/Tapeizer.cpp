#include "plugin.hpp"
#include "dsp/Engine.hpp"
#include "layout.hpp"

using namespace tapeizer;

static const float VOLT = 0.2828f;   // internal units per volt: 5 V peak = +3 dB over 0 VU

struct Tapeizer : Module {
	enum ParamIds {
		STRENGTH_PARAM, AGE_PARAM, MIX_PARAM, OUTPUT_PARAM,
		TRIM_PARAM, LOW_PARAM, LOW_FREQ_PARAM, HIGH_PARAM, HIGH_FREQ_PARAM, DESK_ON_PARAM,
		NR_TYPE_PARAM, NR_ENC_PARAM, NR_DEC_PARAM, CAL_PARAM,
		MULTI_MACHINE_PARAM, REC_PARAM, TAPE_PARAM, BIAS_PARAM, BOUNCE_PARAM, MULTI_ON_PARAM,
		WOW_PARAM, FLUTTER_PARAM, DROPOUT_PARAM, HISS_PARAM, AZIMUTH_PARAM, WEAR_PARAM,
		THRESH_PARAM, RATIO_PARAM, BUS_ON_PARAM,
		MASTER_MACHINE_PARAM, MASTER_REC_PARAM, MASTER_TAPE_PARAM, MASTER_NR_PARAM, MASTER_ON_PARAM,
		BYPASS_PARAM,   // added in 2.2.0, last so older patches and presets keep their ids
		NUM_PARAMS
	};
	enum InputIds {
		IN_L_INPUT, IN_R_INPUT, TRACKS_INPUT, MASTER_IN_L_INPUT, MASTER_IN_R_INPUT,
		STRENGTH_CV_INPUT, AGE_CV_INPUT, REC_CV_INPUT, MASTER_REC_CV_INPUT, WOW_CV_INPUT, AZIMUTH_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { TRACKS_OUTPUT, MIX_L_OUTPUT, MIX_R_OUTPUT, OUT_L_OUTPUT, OUT_R_OUTPUT, NUM_OUTPUTS };
	enum LightIds {
		DESK_ON_LIGHT, NR_ENC_LIGHT, NR_DEC_LIGHT, MULTI_ON_LIGHT, BUS_ON_LIGHT, MASTER_NR_LIGHT, MASTER_ON_LIGHT,
		ENUMS(MVU_LIGHT, 5), ENUMS(SVU_LIGHT, 5), GR_LIGHT, BYPASS_LIGHT,
		NUM_LIGHTS
	};

	tapeizer::Engine eng;
	uint64_t seed = 1;
	float humHz = 50.f;
	dsp::ClockDivider lightDiv;
	float mPeakHold = 0.f, sPeakHold = 0.f;
	float bypassMix = 0.f;   // 0 = through the tape, 1 = bypassed (ramped, no clicks)
	int presetIdx = -1;      // factory preset last picked with the panel arrows, -1 = none

	Tapeizer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(STRENGTH_PARAM, 0.f, 1.f, 0.4f, "Strength", "%", 0.f, 100.f)->description =
			"How much of the tape character you hear, in both decks at once. 0% is transparent, 100% is the real machine.";
		configParam(AGE_PARAM, 0.f, 1.f, 0.2f, "Age", "%", 0.f, 100.f)->description =
			"State of the whole rig: head wear and dirt, azimuth, worn pinch roller and belts, calibration drift, drop-outs, hiss and hum.";
		configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix (dry to wet)", "%", 0.f, 100.f);
		configParam(OUTPUT_PARAM, -12.f, 12.f, 0.f, "Output level", " dB")->randomizeEnabled = false;

		configParam(TRIM_PARAM, 0.f, 24.f, 0.f, "Desk trim (drive into the channel amp)", " dB")->description = "Clean until it clips hard. The level is compensated afterwards.";
		configParam(LOW_PARAM, -15.f, 15.f, 0.f, "Desk low shelf", " dB");
		configParam(LOW_FREQ_PARAM, 0.f, 1.f, 0.0824f, "Desk low frequency", " Hz", 15.f, 80.f);
		configParam(HIGH_PARAM, -15.f, 15.f, 0.f, "Desk high shelf", " dB");
		configParam(HIGH_FREQ_PARAM, 0.f, 1.f, 1.f, "Desk high frequency", " Hz", 10000.f / 700.f, 700.f);
		configSwitch(DESK_ON_PARAM, 0.f, 1.f, 1.f, "Desk", {"Bypassed", "On"});

		configSwitch(NR_TYPE_PARAM, 0.f, 3.f, 3.f, "Noise reduction type", {"Off", "B (one-band sliding)", "C (two-stage sliding)", "2:1 (wideband compander)"});
		configSwitch(NR_ENC_PARAM, 0.f, 1.f, 1.f, "Noise reduction encode (record)", {"Off", "On"});
		configSwitch(NR_DEC_PARAM, 0.f, 1.f, 1.f, "Noise reduction decode (playback)", {"Off", "On"});
		configParam(CAL_PARAM, -6.f, 6.f, 0.f, "Noise reduction calibration error", " dB")->description = "Level error between tape and decoder. Makes the noise reduction mistrack.";

		configSwitch(MULTI_MACHINE_PARAM, 0.f, 5.f, 0.f, "Multitrack machine",
			{"4TR: cassette four-track, 4.76 cm/s", "4TR HI: cassette four-track, 9.5 cm/s", "4TR C: cassette four-track, 9.5 cm/s, C-type NR era",
			 "8TR CASS: eight tracks on a cassette, 9.5 cm/s", "8TR 7.5: quarter-inch eight-track, 19 cm/s", "8TR 15: quarter-inch eight-track, 38 cm/s"});
		configParam(REC_PARAM, -12.f, 12.f, 0.f, "Multitrack record level", " dB")->description = "Low is clean and hissy, high is thick and squashed. Output is compensated.";
		configSwitch(TAPE_PARAM, 0.f, 2.f, 1.f, "Multitrack tape", {"I (normal)", "II (chrome)", "IV (metal)"});
		configParam(BIAS_PARAM, -1.f, 1.f, 0.f, "Bias")->description = "Under-biased is bright and gritty on quiet sounds; over-biased is smooth and dull.";
		configSwitch(BOUNCE_PARAM, 0.f, 3.f, 0.f, "Bounce generations", {"1", "2", "3", "4"});
		configSwitch(MULTI_ON_PARAM, 0.f, 1.f, 1.f, "Multitrack deck", {"Bypassed", "On"});

		configParam(WOW_PARAM, 0.f, 1.f, 0.5f, "Wow", "%", 0.f, 200.f)->description = "100% is the machine's own amount.";
		configParam(FLUTTER_PARAM, 0.f, 1.f, 0.5f, "Flutter", "%", 0.f, 200.f);
		configParam(DROPOUT_PARAM, 0.f, 1.f, 0.5f, "Drop-outs", "%", 0.f, 200.f);
		configParam(HISS_PARAM, 0.f, 1.f, 0.5f, "Hiss and hum", "%", 0.f, 200.f);
		configParam(AZIMUTH_PARAM, -1.f, 1.f, 0.f, "Azimuth error", " arc minutes", 0.f, 20.f);
		configParam(WEAR_PARAM, 0.f, 1.f, 0.f, "Tape wear", "%", 0.f, 100.f)->description = "Condition of the tape: shed oxide, less headroom, more drop-outs.";

		configParam(THRESH_PARAM, -24.f, 0.f, -8.f, "Bus compressor threshold", " dB re 0 VU");
		configParam(RATIO_PARAM, 1.5f, 20.f, 4.f, "Bus compressor ratio", ":1");
		configSwitch(BUS_ON_PARAM, 0.f, 1.f, 0.f, "Bus compressor", {"Bypassed", "On"});

		configSwitch(MASTER_MACHINE_PARAM, 0.f, 3.f, 0.f, "Master machine",
			{"DECK: hi-fi cassette deck", "BOOMBOX: portable cassette", "1/4 INCH: quarter-inch two-track, 19 cm/s", "DAT: 16-bit digital"});
		configParam(MASTER_REC_PARAM, -12.f, 12.f, 0.f, "Master record level", " dB");
		configSwitch(MASTER_TAPE_PARAM, 0.f, 2.f, 1.f, "Master tape", {"I (normal)", "II (chrome)", "IV (metal)"});
		configSwitch(MASTER_NR_PARAM, 0.f, 1.f, 1.f, "Master B-type noise reduction", {"Off", "On"});
		configSwitch(MASTER_ON_PARAM, 0.f, 1.f, 1.f, "Master deck", {"Bypassed", "On"});
		configSwitch(BYPASS_PARAM, 0.f, 1.f, 0.f, "Bypass (whole module)", {"Off: through the tape", "Bypassed: clean signal"});
		paramQuantities[BYPASS_PARAM]->randomizeEnabled = false;
		paramQuantities[BYPASS_PARAM]->description = "Ctrl+R randomize never touches this, and the panel presets leave it as it is.";

		configInput(IN_L_INPUT, "Left (records to its own track)");
		configInput(IN_R_INPUT, "Right (follows left when unpatched)");
		configInput(TRACKS_INPUT, "Tracks: each poly channel is a track on the same tape, up to 8");
		configInput(MASTER_IN_L_INPUT, "Master in left (replaces the mix)");
		configInput(MASTER_IN_R_INPUT, "Master in right (follows left when unpatched)");
		configInput(STRENGTH_CV_INPUT, "Strength CV (10 V sweeps the knob)");
		configInput(AGE_CV_INPUT, "Age CV");
		configInput(REC_CV_INPUT, "Multitrack record level CV");
		configInput(MASTER_REC_CV_INPUT, "Master record level CV");
		configInput(WOW_CV_INPUT, "Wow CV");
		configInput(AZIMUTH_CV_INPUT, "Azimuth CV");
		configOutput(TRACKS_OUTPUT, "Tracks after the multitrack deck (poly)");
		configOutput(MIX_L_OUTPUT, "Mix left (after the multitrack deck)");
		configOutput(MIX_R_OUTPUT, "Mix right");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);

		lightDiv.setDivision(64);
		seed = random::u64() | 1;
		eng.seed = seed;
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { eng.init(e.sampleRate); }
	void onAdd(const AddEvent& e) override { eng.seed = seed; eng.init(APP->engine->getSampleRate()); }
	void newUnit() { seed = random::u64() | 1; eng.seed = seed; eng.needInit = true; }

	float knobCv(int param, int input, float lo, float hi) {
		float v = params[param].getValue();
		if (inputs[input].isConnected()) v += inputs[input].getVoltage() * 0.1f * (hi - lo);
		return clamp(v, lo, hi);
	}

	void process(const ProcessArgs& args) override {
		if (eng.fs != args.sampleRate) eng.init(args.sampleRate);
		tapeizer::Params& p = eng.p;
		p.strength = knobCv(STRENGTH_PARAM, STRENGTH_CV_INPUT, 0.f, 1.f);
		p.age = knobCv(AGE_PARAM, AGE_CV_INPUT, 0.f, 1.f);
		p.mix = params[MIX_PARAM].getValue();
		p.outputDb = params[OUTPUT_PARAM].getValue();
		p.deskOn = params[DESK_ON_PARAM].getValue() > 0.5f;
		p.trimDb = params[TRIM_PARAM].getValue();
		p.loDb = params[LOW_PARAM].getValue();
		p.loF = 80.0 * std::pow(15.0, (double)params[LOW_FREQ_PARAM].getValue());
		p.hiDb = params[HIGH_PARAM].getValue();
		p.hiF = 700.0 * std::pow(10000.0 / 700.0, (double)params[HIGH_FREQ_PARAM].getValue());
		p.nrType = (int)std::round(params[NR_TYPE_PARAM].getValue());
		p.nrEnc = params[NR_ENC_PARAM].getValue() > 0.5f;
		p.nrDec = params[NR_DEC_PARAM].getValue() > 0.5f;
		p.calDb = params[CAL_PARAM].getValue();
		p.multiOn = params[MULTI_ON_PARAM].getValue() > 0.5f;
		p.multiMachine = (int)std::round(params[MULTI_MACHINE_PARAM].getValue());
		p.multiTape = (int)std::round(params[TAPE_PARAM].getValue());
		p.bounce = 1 + (int)std::round(params[BOUNCE_PARAM].getValue());
		p.recDb = knobCv(REC_PARAM, REC_CV_INPUT, -12.f, 12.f);
		p.bias = params[BIAS_PARAM].getValue();
		p.wow = knobCv(WOW_PARAM, WOW_CV_INPUT, 0.f, 1.f);
		p.flutter = params[FLUTTER_PARAM].getValue();
		p.dropout = params[DROPOUT_PARAM].getValue();
		p.hiss = params[HISS_PARAM].getValue();
		p.azimuth = knobCv(AZIMUTH_PARAM, AZIMUTH_CV_INPUT, -1.f, 1.f);
		p.wear = params[WEAR_PARAM].getValue();
		p.busOn = params[BUS_ON_PARAM].getValue() > 0.5f;
		p.thrDb = params[THRESH_PARAM].getValue();
		p.ratio = params[RATIO_PARAM].getValue();
		p.masterOn = params[MASTER_ON_PARAM].getValue() > 0.5f;
		p.masterMachine = (int)std::round(params[MASTER_MACHINE_PARAM].getValue());
		p.masterTape = (int)std::round(params[MASTER_TAPE_PARAM].getValue());
		p.masterNr = params[MASTER_NR_PARAM].getValue() > 0.5f;
		p.masterRecDb = knobCv(MASTER_REC_PARAM, MASTER_REC_CV_INPUT, -12.f, 12.f);
		p.humHz = humHz;

		tapeizer::IO io;
		io.nTracks = std::min(tapeizer::MAX_TRACKS_IN, inputs[TRACKS_INPUT].getChannels());
		for (int c = 0; c < io.nTracks; c++) io.tracks[c] = inputs[TRACKS_INPUT].getVoltage(c) * VOLT;
		const bool lOn = inputs[IN_L_INPUT].isConnected(), rOn = inputs[IN_R_INPUT].isConnected();
		io.inOn = lOn || rOn;
		if (io.inOn) {
			float l = lOn ? inputs[IN_L_INPUT].getVoltage() : inputs[IN_R_INPUT].getVoltage();
			float r = rOn ? inputs[IN_R_INPUT].getVoltage() : l;
			io.inL = l * VOLT; io.inR = r * VOLT;
		}
		const bool mlOn = inputs[MASTER_IN_L_INPUT].isConnected(), mrOn = inputs[MASTER_IN_R_INPUT].isConnected();
		io.masterInOn = mlOn || mrOn;
		if (io.masterInOn) {
			float l = mlOn ? inputs[MASTER_IN_L_INPUT].getVoltage() : inputs[MASTER_IN_R_INPUT].getVoltage();
			float r = mrOn ? inputs[MASTER_IN_R_INPUT].getVoltage() : l;
			io.masterL = l * VOLT; io.masterR = r * VOLT;
		}
		io.tracksOutOn = outputs[TRACKS_OUTPUT].isConnected();

		eng.process(io);

		// BYPASS: 15 ms crossfade to the clean signal that would have gone in. The engine keeps
		// running underneath so switching back is smooth too.
		const bool byp = params[BYPASS_PARAM].getValue() > 0.5f;
		const float bStep = args.sampleTime / 0.015f;
		bypassMix = byp ? std::min(1.f, bypassMix + bStep) : std::max(0.f, bypassMix - bStep);
		const float b = bypassMix, w = 1.f - b;
		// clean mix: IN L/R plus the tracks (unless TRACKS OUT takes them out of the mix), in volts
		float cleanL = 0.f, cleanR = 0.f;
		if (b > 0.f) {
			if (io.inOn) { cleanL = (float)io.inL / VOLT; cleanR = (float)io.inR / VOLT; }
			if (!io.tracksOutOn)
				for (int c = 0; c < io.nTracks; c++) { cleanL += inputs[TRACKS_INPUT].getVoltage(c); cleanR += inputs[TRACKS_INPUT].getVoltage(c); }
		}
		const float cleanOutL = io.masterInOn ? (float)io.masterL / VOLT : cleanL;
		const float cleanOutR = io.masterInOn ? (float)io.masterR / VOLT : cleanR;

		outputs[TRACKS_OUTPUT].setChannels(std::max(1, io.nTracks));
		for (int c = 0; c < io.nTracks; c++)
			outputs[TRACKS_OUTPUT].setVoltage(w * (float)io.tracksOut[c] / VOLT + b * inputs[TRACKS_INPUT].getVoltage(c), c);
		if (io.nTracks == 0) outputs[TRACKS_OUTPUT].setVoltage(0.f, 0);
		outputs[MIX_L_OUTPUT].setVoltage(w * (float)io.mixL / VOLT + b * cleanL);
		outputs[MIX_R_OUTPUT].setVoltage(w * (float)io.mixR / VOLT + b * cleanR);
		outputs[OUT_L_OUTPUT].setVoltage(w * (float)io.outL / VOLT + b * cleanOutL);
		outputs[OUT_R_OUTPUT].setVoltage(w * (float)io.outR / VOLT + b * cleanOutR);

		if (lightDiv.process()) {
			const float dt = args.sampleTime * lightDiv.getDivision();
			lights[DESK_ON_LIGHT].setBrightness(p.deskOn);
			lights[NR_ENC_LIGHT].setBrightness(p.nrEnc && p.nrType != tapeizer::NR_OFF);
			lights[NR_DEC_LIGHT].setBrightness(p.nrDec && p.nrType != tapeizer::NR_OFF);
			lights[MULTI_ON_LIGHT].setBrightness(p.multiOn);
			lights[BUS_ON_LIGHT].setBrightness(p.busOn);
			lights[MASTER_NR_LIGHT].setBrightness(p.masterNr && p.masterMachine != tapeizer::MM_DAT);
			lights[MASTER_ON_LIGHT].setBrightness(p.masterOn);
			const float th[4] = {3.f, 0.f, -5.f, -10.f};
			float mdb = 20.f * std::log10(std::max(1e-6, eng.multiVu));
			float sdb = 20.f * std::log10(std::max(1e-6, eng.masterVu));
			for (int i = 0; i < 4; i++) {
				lights[MVU_LIGHT + 1 + i].setBrightness(mdb >= th[i] ? 1.f : 0.f);
				lights[SVU_LIGHT + 1 + i].setBrightness(sdb >= th[i] ? 1.f : 0.f);
			}
			mPeakHold = eng.takeMultiPeak() > 1.0 ? 1.f : std::max(0.f, mPeakHold - dt * 3.f);
			sPeakHold = eng.takeMasterPeak() > 1.0 ? 1.f : std::max(0.f, sPeakHold - dt * 3.f);
			lights[MVU_LIGHT].setBrightness(mPeakHold);
			lights[SVU_LIGHT].setBrightness(sPeakHold);
			lights[GR_LIGHT].setBrightness(clamp((float)eng.busGr / 12.f, 0.f, 1.f));
			lights[BYPASS_LIGHT].setBrightness(byp ? 1.f : 0.f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_string(string::f("%016llx", (unsigned long long)seed).c_str()));
		json_object_set_new(root, "humHz", json_real(humHz));
		json_object_set_new(root, "presetIdx", json_integer(presetIdx));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s && json_is_string(s)) { seed = std::strtoull(json_string_value(s), nullptr, 16) | 1; eng.seed = seed; eng.needInit = true; }
		json_t* h = json_object_get(root, "humHz");
		if (h) humHz = json_number_value(h) > 55.0 ? 60.f : 50.f;
		json_t* pi = json_object_get(root, "presetIdx");
		presetIdx = pi ? (int)json_integer_value(pi) : -1;
	}
	void onReset(const ResetEvent& e) override { Module::onReset(e); presetIdx = -1; }
};

// ---------------------------------------------------------------- widgets
struct AmberLightBase : GrayModuleLightWidget {
	AmberLightBase() { addBaseColor(nvgRGB(217, 164, 65)); }
};
struct OnButton : VCVLightBezel<AmberLightBase> {
	OnButton() { momentary = false; }
};

struct NameDisplay : Widget {   // multitrack machine readout
	Tapeizer* module = nullptr;
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		static const char* MM[6] = {"4TR", "4TR HI", "4TR C", "8TR CASS", "8TR 7.5", "8TR 15"};
		static const char* TP[3] = {"I", "II", "IV"};
		static const char* NR[4] = {"OFF", "B", "C", "2:1"};
		std::string s;
		{
			int m = module ? (int)module->params[Tapeizer::MULTI_MACHINE_PARAM].getValue() : 0;
			int t = module ? (int)module->params[Tapeizer::TAPE_PARAM].getValue() : 1;
			int n = module ? (int)module->params[Tapeizer::NR_TYPE_PARAM].getValue() : 3;
			int b = module ? 1 + (int)module->params[Tapeizer::BOUNCE_PARAM].getValue() : 1;
			s = string::f("%s  %s  %s", MM[clamp(m, 0, 5)], TP[clamp(t, 0, 2)], NR[clamp(n, 0, 3)]);
			if (b > 1) s += string::f("  x%d", b);
			if (module && module->params[Tapeizer::MULTI_ON_PARAM].getValue() < 0.5f) s = "BYPASSED";
		}
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 13.f);
		nvgTextLetterSpacing(args.vg, 0.5f);
		nvgFillColor(args.vg, nvgRGB(242, 192, 96));
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, box.size.x / 2, box.size.y / 2 + 0.5f, s.c_str(), NULL);
	}
};

struct BypassButton : VCVLightBezel<RedLight> {
	BypassButton() { momentary = false; }
};

// ---------------------------------------------------------------- factory presets on the panel
// The same .vcvm files as the right-click Preset menu (presets/Tapeizer/), read once.
struct FactoryPreset { std::string name, path; std::vector<std::pair<int, float>> values; };
static std::vector<FactoryPreset>& factoryPresets() {
	static std::vector<FactoryPreset> list;
	static bool loaded = false;
	if (loaded) return list;
	loaded = true;
	std::string dir = asset::plugin(pluginInstance, "presets/Tapeizer");
	std::vector<std::string> files = system::getEntries(dir);
	std::sort(files.begin(), files.end());
	for (const std::string& f : files) {
		if (system::getExtension(f) != ".vcvm") continue;
		FactoryPreset p;
		p.path = f;
		std::string stem = system::getStem(f);   // "01_Four-Track To Cassette"
		size_t us = stem.find('_');
		p.name = us == std::string::npos ? stem : stem.substr(us + 1);
		FILE* fp = std::fopen(f.c_str(), "r");
		if (!fp) continue;
		json_error_t err;
		json_t* root = json_loadf(fp, 0, &err);
		std::fclose(fp);
		if (!root) continue;
		json_t* ps = json_object_get(root, "params");
		size_t i; json_t* pj;
		json_array_foreach(ps, i, pj) {
			json_t* id = json_object_get(pj, "id"); json_t* v = json_object_get(pj, "value");
			if (id && v) p.values.push_back({(int)json_integer_value(id), (float)json_number_value(v)});
		}
		json_decref(root);
		list.push_back(p);
	}
	return list;
}
// true once a knob or switch the preset sets has been moved
static bool presetEdited(Tapeizer* m, const FactoryPreset& p) {
	for (const auto& v : p.values) {
		if (v.first < 0 || v.first >= Tapeizer::NUM_PARAMS || v.first == Tapeizer::BYPASS_PARAM) continue;
		if (std::fabs(m->params[v.first].getValue() - v.second) > 1e-4f) return true;
	}
	return false;
}

struct PresetScreen : Widget {
	Tapeizer* module = nullptr;
	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, mm2px(0.8f));
		nvgFillColor(args.vg, nvgRGB(0x1B, 0x19, 0x16));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0x4C, 0x46, 0x3F));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		std::string s;
		if (!module) s = "01  Four-Track To Cassette";
		else {
			auto& list = factoryPresets();
			int i = module->presetIdx;
			if (i < 0 || i >= (int)list.size()) s = "-- PRESETS --";
			else s = string::f("%02d  %s%s", i + 1, list[i].name.c_str(), presetEdited(module, list[i]) ? " *" : "");
		}
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 13.f);
		nvgTextLetterSpacing(args.vg, 0.5f);
		nvgFillColor(args.vg, nvgRGB(242, 192, 96));
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, box.size.x / 2, box.size.y / 2 + 0.5f, s.c_str(), NULL);
	}
	// click the name for the full list
	void onButton(const ButtonEvent& e) override;
};

static void loadFactoryPreset(ModuleWidget* mw, Tapeizer* m, int i) {
	auto& list = factoryPresets();
	if (i < 0 || i >= (int)list.size()) return;
	try {
		mw->loadAction(list[i].path);   // same as the Preset menu: one undo step, BYPASS untouched
		m->presetIdx = i;
	}
	catch (Exception& e) {
		WARN("Tapeizer: could not load preset %s: %s", list[i].path.c_str(), e.what());
	}
}

void PresetScreen::onButton(const ButtonEvent& e) {
	if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
	ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
	if (!mw) return;
	Tapeizer* m = module;
	Menu* menu = createMenu();
	menu->addChild(createMenuLabel("Factory presets"));
	auto& list = factoryPresets();
	for (int i = 0; i < (int)list.size(); i++)
		menu->addChild(createCheckMenuItem(string::f("%02d  %s", i + 1, list[i].name.c_str()), "",
			[=]() { return m->presetIdx == i; }, [=]() { loadFactoryPreset(mw, m, i); }));
	e.consume(this);
}

// left / right arrows step through the factory presets (wrapping round)
struct PresetArrow : OpaqueWidget {
	Tapeizer* module = nullptr;
	ModuleWidget* mw = nullptr;
	int dir = 1;
	bool hover = false;
	void draw(const DrawArgs& args) override {
		float w = box.size.x, h = box.size.y;
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.f, h - 1.f, mm2px(0.9f));
		nvgFillColor(args.vg, hover ? nvgRGB(0x44, 0x3f, 0x3c) : nvgRGB(0x2b, 0x28, 0x26));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);
		float cx = w / 2, cy = h / 2, a = mm2px(1.2f), b = mm2px(1.6f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, cx + a * dir, cy);
		nvgLineTo(args.vg, cx - a * dir, cy - b);
		nvgLineTo(args.vg, cx - a * dir, cy + b);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(0xF0, 0xE7, 0xD8));
		nvgFill(args.vg);
	}
	void onEnter(const EnterEvent& e) override { hover = true; }
	void onLeave(const LeaveEvent& e) override { hover = false; }
	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (!module || !mw || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
		int n = (int)factoryPresets().size();
		if (n == 0) return;
		int i = module->presetIdx;
		i = i < 0 ? (dir > 0 ? 0 : n - 1) : (i + dir + n) % n;
		loadFactoryPreset(mw, module, i);
		e.consume(this);
	}
};

struct TapeizerWidget : ModuleWidget {
	TapeizerWidget(Tapeizer* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Tapeizer.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		using namespace layout;
		auto knob = [&](float x, float y, int id) { addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x, y)), module, id)); };
		auto big = [&](float x, float y, int id) { addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(x, y)), module, id)); };
		auto snap = [&](float x, float y, int id) { addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(x, y)), module, id)); };
		auto on = [&](float x, float y, int id, int light) { addParam(createLightParamCentered<OnButton>(mm2px(Vec(x, y)), module, id, light)); };
		auto in = [&](float x, float y, int id) { addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };
		auto out = [&](float x, float y, int id) { addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, y)), module, id)); };
		using T = Tapeizer;

		knob(TRIM_X, TRIM_Y, T::TRIM_PARAM);
		knob(LOW_X, LOW_Y, T::LOW_PARAM); knob(LOW_FREQ_X, LOW_FREQ_Y, T::LOW_FREQ_PARAM);
		knob(HIGH_X, HIGH_Y, T::HIGH_PARAM); knob(HIGH_FREQ_X, HIGH_FREQ_Y, T::HIGH_FREQ_PARAM);
		on(DESK_ON_X, DESK_ON_Y, T::DESK_ON_PARAM, T::DESK_ON_LIGHT);

		snap(MULTI_MACHINE_X, MULTI_MACHINE_Y, T::MULTI_MACHINE_PARAM); snap(TAPE_X, TAPE_Y, T::TAPE_PARAM); snap(BOUNCE_X, BOUNCE_Y, T::BOUNCE_PARAM);
		big(REC_X, REC_Y, T::REC_PARAM); knob(BIAS_X, BIAS_Y, T::BIAS_PARAM); knob(CAL_X, CAL_Y, T::CAL_PARAM);
		snap(NR_TYPE_X, NR_TYPE_Y, T::NR_TYPE_PARAM);
		on(NR_ENC_X, NR_ENC_Y, T::NR_ENC_PARAM, T::NR_ENC_LIGHT); on(NR_DEC_X, NR_DEC_Y, T::NR_DEC_PARAM, T::NR_DEC_LIGHT);
		on(MULTI_ON_X, MULTI_ON_Y, T::MULTI_ON_PARAM, T::MULTI_ON_LIGHT);

		knob(WOW_X, WOW_Y, T::WOW_PARAM); knob(FLUTTER_X, FLUTTER_Y, T::FLUTTER_PARAM);
		knob(DROPOUT_X, DROPOUT_Y, T::DROPOUT_PARAM); knob(HISS_X, HISS_Y, T::HISS_PARAM);
		knob(AZIMUTH_X, AZIMUTH_Y, T::AZIMUTH_PARAM); knob(WEAR_X, WEAR_Y, T::WEAR_PARAM);

		knob(THRESH_X, THRESH_Y, T::THRESH_PARAM); knob(RATIO_X, RATIO_Y, T::RATIO_PARAM);
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(GR_X, GR_Y)), module, T::GR_LIGHT));
		on(BUS_ON_X, BUS_ON_Y, T::BUS_ON_PARAM, T::BUS_ON_LIGHT);

		snap(MASTER_MACHINE_X, MASTER_MACHINE_Y, T::MASTER_MACHINE_PARAM); snap(MASTER_TAPE_X, MASTER_TAPE_Y, T::MASTER_TAPE_PARAM);
		knob(MASTER_REC_X, MASTER_REC_Y, T::MASTER_REC_PARAM);
		on(MASTER_NR_X, MASTER_NR_Y, T::MASTER_NR_PARAM, T::MASTER_NR_LIGHT);
		on(MASTER_ON_X, MASTER_ON_Y, T::MASTER_ON_PARAM, T::MASTER_ON_LIGHT);

		big(STRENGTH_X, STRENGTH_Y, T::STRENGTH_PARAM); big(AGE_X, AGE_Y, T::AGE_PARAM);
		knob(MIX_X, MIX_Y, T::MIX_PARAM); knob(OUTPUT_X, OUTPUT_Y, T::OUTPUT_PARAM);

		const float mvx[5] = {MVU0_X, MVU1_X, MVU2_X, MVU3_X, MVU4_X}, mvy[5] = {MVU0_Y, MVU1_Y, MVU2_Y, MVU3_Y, MVU4_Y};
		const float svx[5] = {SVU0_X, SVU1_X, SVU2_X, SVU3_X, SVU4_X}, svy[5] = {SVU0_Y, SVU1_Y, SVU2_Y, SVU3_Y, SVU4_Y};
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(mvx[0], mvy[0])), module, T::MVU_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(svx[0], svy[0])), module, T::SVU_LIGHT));
		for (int i = 1; i < 5; i++) {
			if (i == 1) {
				addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(mvx[i], mvy[i])), module, T::MVU_LIGHT + i));
				addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(svx[i], svy[i])), module, T::SVU_LIGHT + i));
			} else {
				addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(mvx[i], mvy[i])), module, T::MVU_LIGHT + i));
				addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(svx[i], svy[i])), module, T::SVU_LIGHT + i));
			}
		}

		NameDisplay* d1 = new NameDisplay; d1->module = module;
		d1->box.pos = mm2px(Vec(MULTI_DISP_X - MULTI_DISP_W / 2, MULTI_DISP_Y - MULTI_DISP_H / 2)); d1->box.size = mm2px(Vec(MULTI_DISP_W, MULTI_DISP_H));
		addChild(d1);

		addParam(createLightParamCentered<BypassButton>(mm2px(Vec(BYPASS_X, BYPASS_Y)), module, T::BYPASS_PARAM, T::BYPASS_LIGHT));

		// preset selector: < name >
		PresetScreen* ps = new PresetScreen; ps->module = module;
		ps->box.pos = mm2px(Vec(PRESET_DISP_X - PRESET_DISP_W / 2, PRESET_DISP_Y - PRESET_DISP_H / 2)); ps->box.size = mm2px(Vec(PRESET_DISP_W, PRESET_DISP_H));
		addChild(ps);
		for (int dir : {-1, 1}) {
			PresetArrow* a = new PresetArrow; a->module = module; a->mw = this; a->dir = dir;
			float ax = dir < 0 ? PRESET_PREV_X : PRESET_NEXT_X, ay = dir < 0 ? PRESET_PREV_Y : PRESET_NEXT_Y;
			a->box.size = mm2px(Vec(6.f, 6.f)); a->box.pos = mm2px(Vec(ax - 3.f, ay - 3.f));
			addChild(a);
		}

		in(IN_L_X, IN_L_Y, T::IN_L_INPUT); in(IN_R_X, IN_R_Y, T::IN_R_INPUT); in(TRACKS_X, TRACKS_Y, T::TRACKS_INPUT);
		in(REC_CV_X, REC_CV_Y, T::REC_CV_INPUT); in(WOW_CV_X, WOW_CV_Y, T::WOW_CV_INPUT); in(AZIMUTH_CV_X, AZIMUTH_CV_Y, T::AZIMUTH_CV_INPUT);
		in(MASTER_IN_L_X, MASTER_IN_L_Y, T::MASTER_IN_L_INPUT); in(MASTER_IN_R_X, MASTER_IN_R_Y, T::MASTER_IN_R_INPUT);
		in(MASTER_REC_CV_X, MASTER_REC_CV_Y, T::MASTER_REC_CV_INPUT);
		in(STRENGTH_CV_X, STRENGTH_CV_Y, T::STRENGTH_CV_INPUT); in(AGE_CV_X, AGE_CV_Y, T::AGE_CV_INPUT);
		out(TRACKS_OUT_X, TRACKS_OUT_Y, T::TRACKS_OUTPUT); out(MIX_L_X, MIX_L_Y, T::MIX_L_OUTPUT); out(MIX_R_X, MIX_R_Y, T::MIX_R_OUTPUT);
		out(OUT_L_X, OUT_L_Y, T::OUT_L_OUTPUT); out(OUT_R_X, OUT_R_Y, T::OUT_R_OUTPUT);
	}

	void appendContextMenu(Menu* menu) override {
		Tapeizer* m = dynamic_cast<Tapeizer*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Hum", {"50 Hz", "60 Hz"},
			[=]() { return m->humHz > 55.f ? 1 : 0; }, [=](size_t i) { m->humHz = i ? 60.f : 50.f; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Unit %08llx", (unsigned long long)(m->seed >> 32))));
		menu->addChild(createMenuItem("New unit", "", [=]() { m->newUnit(); }));
	}
};

Model* modelTapeizer = createModel<Tapeizer, TapeizerWidget>("Tapeizer");
