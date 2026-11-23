// Clapfall: multi-particle percussion. A burst of tiny modelled impacts
// (hand claps, snaps, sticks, coins, drum-machine noise pulses) that covers
// everything from a tight 808 clap to a hall full of applause.
// The sound engine is in dsp/Engine.hpp, the particle models in dsp/Particles.hpp.
#include "plugin.hpp"
#include "dsp/Engine.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <osdialog.h>

#define DR_WAV_IMPLEMENTATION
#include "dsp/dr_wav.h"

using namespace clapfall;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float DISP_X = 5.0f, DISP_Y = 14.5f, DISP_W = 46.0f, DISP_H = 9.0f;
static const float PLIGHT_X = 55.0f, PLIGHT_Y = 19.0f;
static const float HIT_X = 66.0f, FREE_X = 81.0f, BTN_Y = 19.0f;
static const float COLX[3] = {14.0f, 40.0f, 66.0f};
static const float ROW1_Y = 40.0f, ROW2_Y = 58.0f, ROW3_Y = 77.0f;
static const float SW_X[2] = {58.5f, 75.5f};
static const float JX0 = 9.0f, JDX = 14.68f;
static const float JA_Y = 93.0f, JB_Y = 105.0f, JC_Y = 117.0f;

// ---- built-in particle tables: built once per process on a worker thread
static std::vector<Surface> gSurfaces;
static std::atomic<bool> gReady{false};
static std::once_flag gOnce;
static void startBuild() {
	std::call_once(gOnce, []() {
		std::thread([]() {
			gSurfaces = buildSurfaces();
			gReady.store(true, std::memory_order_release);
		}).detach();
	});
}

// Load up to 64 short WAVs from a folder into a USER surface.
static Surface* loadFolder(const std::string& dir) {
	std::vector<std::string> files;
	for (const std::string& p : system::getEntries(dir)) {
		std::string ext = string::lowercase(system::getExtension(p));
		if (ext == ".wav" && system::isFile(p)) files.push_back(p);
	}
	std::sort(files.begin(), files.end());
	if (files.size() > 64) files.resize(64);
	Surface* s = new Surface;
	s->name = "USER";
	for (const std::string& f : files) {
		unsigned int ch = 0, rate = 0;
		drwav_uint64 frames = 0;
		float* d = drwav_open_file_and_read_pcm_frames_f32(f.c_str(), &ch, &rate, &frames, NULL);
		if (!d || ch == 0 || frames < 16) { if (d) drwav_free(d, NULL); continue; }
		drwav_uint64 maxF = (drwav_uint64)rate * 2;
		if (frames > maxF) frames = maxF;
		std::vector<float> v(frames);
		float pk = 1e-9f;
		for (drwav_uint64 i = 0; i < frames; i++) {
			float m = 0.f;
			for (unsigned c = 0; c < ch; c++) m += d[i * ch + c];
			v[i] = m / ch;
			pk = std::max(pk, std::fabs(v[i]));
		}
		drwav_free(d, NULL);
		int fade = std::min((int)frames, (int)(0.003f * rate));
		for (int i = 0; i < fade; i++) v[frames - fade + i] *= 1.f - (float)i / std::max(1, fade - 1);
		for (float& x : v) x /= pk;
		s->vars.push_back(std::move(v));
		s->rates.push_back((float)rate);
	}
	return s;
}

struct Clapfall : Module {
	enum ParamIds {
		SURFACE_PARAM, DENSITY_PARAM, DECAY_PARAM, LOOSE_PARAM, TONE_PARAM, SPREAD_PARAM,
		REVERB_PARAM, SIZE_PARAM, ENV_PARAM, FILTER_PARAM, HIT_PARAM, FREE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		SURFACE_INPUT, DENSITY_INPUT, DECAY_INPUT, LOOSE_INPUT, TONE_INPUT, SPREAD_INPUT,
		REVERB_INPUT, SIZE_INPUT, VOCT_INPUT, FREE_INPUT, ACCENT_INPUT, ONE_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds { OUT_L_OUTPUT, OUT_R_OUTPUT, NUM_OUTPUTS };
	enum LightIds { HIT_LIGHT, FREE_LIGHT, PARTICLE_LIGHT, NUM_LIGHTS };

	clapfall::Engine engine;
	dsp::SchmittTrigger trigIn, oneIn, hitBtn;
	dsp::ClockDivider lightDiv;
	float hitLight = 0.f, partLight = 0.f;
	bool sameEveryHit = false;

	// USER folder
	std::string userPath;
	std::atomic<Surface*> userSurf{nullptr};
	std::vector<std::unique_ptr<Surface>> keep;   // every loaded folder stays alive until the module goes
	std::mutex keepMutex;
	std::thread loader;
	std::atomic<bool> loading{false};

	// for the display
	int dispSurface = 0;
	bool dispFree = false;

	Clapfall() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		struct SurfaceQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				int i = clamp((int)std::round(getValue()), 0, NUM_SURFACES - 1);
				return SURFACE_NAMES[i];
			}
		};
		configParam<SurfaceQuantity>(SURFACE_PARAM, 0.f, (float)(NUM_SURFACES - 1), 0.f, "Surface",
			"")->description = "What each particle is: hands, snaps, sticks, coins, drum-machine noise pulses... USER plays your own folder of samples.";
		getParamQuantity(SURFACE_PARAM)->snapEnabled = true;
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.6f, "Density", " particles/s", 400.f, 1.5f)->description =
			"How many particles per second: one person clapping at the bottom, a crowd at the top.";
		configParam(DECAY_PARAM, 0.f, 1.f, 0.35f, "Decay", " s", 500.f, 0.012f)->description =
			"How long a burst lasts after a trigger. In FREE mode it sets the level or chance of the cloud directly.";
		configParam(LOOSE_PARAM, 0.f, 1.f, 0.5f, "Loose", "%", 0.f, 100.f)->description =
			"From machine-exact (every hit identical, strict timing) to human (random timing and loudness, like real people).";
		configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone", " Hz", 10000.f / 150.f, 150.f)->description =
			"Centre of the band-pass filter. In PITCH and PUNCH it also moves the pitch of the particles, two octaves either way.";
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.3f, "Spread", "%", 0.f, 100.f)->description =
			"How far particles scatter in pitch and across the stereo field.";
		configParam(REVERB_PARAM, 0.f, 1.f, 0.f, "Reverb", "%", 0.f, 100.f)->description =
			"Send into the built-in room/hall. Open it by CV after a hit for a late tail.";
		configParam(SIZE_PARAM, 0.f, 1.f, 0.3f, "Size", " s", 12.8f, 0.25f)->description =
			"Small room to big hall (reverb time at 1 kHz).";
		configSwitch(ENV_PARAM, 0.f, 2.f, 1.f, "Envelope", {"CHANCE: the burst thins out", "BOTH: quieter and rarer", "AMP: each particle quieter"});
		configSwitch(FILTER_PARAM, 0.f, 2.f, 0.f, "Filter", {"PUNCH: dry plus band-pass, TONE moves pitch", "PITCH: band-pass, TONE moves pitch", "BAND: band-pass only"});
		configButton(HIT_PARAM, "Hit (same as a trigger)");
		configSwitch(FREE_PARAM, 0.f, 1.f, 0.f, "Free-running cloud", {"Off", "On"});
		configInput(SURFACE_INPUT, "Surface CV (5 V sweeps every surface)");
		configInput(DENSITY_INPUT, "Density CV");
		configInput(DECAY_INPUT, "Decay CV");
		configInput(LOOSE_INPUT, "Loose CV");
		configInput(TONE_INPUT, "Tone CV");
		configInput(SPREAD_INPUT, "Spread CV");
		configInput(REVERB_INPUT, "Reverb send CV");
		configInput(SIZE_INPUT, "Size CV");
		configInput(VOCT_INPUT, "Pitch (1 V/oct, replaces TONE's pitch)");
		configInput(FREE_INPUT, "Free-running gate");
		configInput(ACCENT_INPUT, "Accent (high at the trigger: louder, longer burst)");
		configInput(ONE_INPUT, "One particle per trigger");
		configInput(TRIG_INPUT, "Trigger a burst");
		configOutput(OUT_L_OUTPUT, "Left (mono sum when R is not patched)");
		configOutput(OUT_R_OUTPUT, "Right");
		lightDiv.setDivision(64);
		startBuild();
		engine.setSampleRate(48000.f);
	}

	~Clapfall() {
		if (loader.joinable()) loader.join();
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		engine.reset();
		sameEveryHit = false;
	}

	void loadUser(const std::string& path) {
		if (loading.exchange(true)) return;
		if (loader.joinable()) loader.join();
		userPath = path;
		loader = std::thread([this, path]() {
			Surface* s = loadFolder(path);
			{
				std::lock_guard<std::mutex> lk(keepMutex);
				keep.emplace_back(s);
			}
			userSurf.store(s, std::memory_order_release);
			loading.store(false);
		});
	}
	void clearUser() {
		userSurf.store(nullptr, std::memory_order_release);
		userPath.clear();
	}

	float knob(int p, int in, float lo = 0.f, float hi = 1.f) {
		float v = params[p].getValue();
		if (inputs[in].isConnected()) v += inputs[in].getVoltage() * (hi - lo) / 5.f;
		return clamp(v, lo, hi);
	}

	void process(const ProcessArgs& args) override {
		Controls c;
		float sIdx = knob(SURFACE_PARAM, SURFACE_INPUT, 0.f, (float)(NUM_SURFACES - 1));
		int si = clamp((int)std::round(sIdx), 0, NUM_SURFACES - 1);
		dispSurface = si;
		if (gReady.load(std::memory_order_acquire)) {
			if (si == USER_INDEX) c.surf = userSurf.load(std::memory_order_acquire);
			else c.surf = &gSurfaces[si];
		}
		c.density = knob(DENSITY_PARAM, DENSITY_INPUT);
		c.decay = knob(DECAY_PARAM, DECAY_INPUT);
		c.loose = knob(LOOSE_PARAM, LOOSE_INPUT);
		c.tone = knob(TONE_PARAM, TONE_INPUT);
		c.spread = knob(SPREAD_PARAM, SPREAD_INPUT);
		c.reverb = knob(REVERB_PARAM, REVERB_INPUT);
		c.size = knob(SIZE_PARAM, SIZE_INPUT);
		// switches: CKSSThree top = 2
		c.env = 2 - (int)std::round(params[ENV_PARAM].getValue());
		c.filter = 2 - (int)std::round(params[FILTER_PARAM].getValue());
		c.vOctOn = inputs[VOCT_INPUT].isConnected();
		c.vOct = inputs[VOCT_INPUT].getVoltage();
		bool freeGate = inputs[FREE_INPUT].getVoltage() > 2.f;   // gate, not edge
		c.free = params[FREE_PARAM].getValue() > 0.5f || freeGate;
		c.sameEveryHit = sameEveryHit;
		dispFree = c.free;

		bool trig = trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f);
		bool hit = hitBtn.process(params[HIT_PARAM].getValue());
		bool one = oneIn.process(inputs[ONE_INPUT].getVoltage(), 0.1f, 2.f);
		bool accent = inputs[ACCENT_INPUT].getVoltage() > 2.f;
		if (trig || hit) hitLight = 1.f;

		int before = engine.particleCount;
		float l, r;
		engine.process(c, trig || hit, one, accent, l, r);
		if (engine.particleCount != before) {
			partLight = std::max(partLight, std::min(1.f, engine.lastParticleAmp));
			engine.lastParticleAmp = 0.f;
		}

		if (outputs[OUT_R_OUTPUT].isConnected()) {
			outputs[OUT_L_OUTPUT].setVoltage(l);
			outputs[OUT_R_OUTPUT].setVoltage(r);
		}
		else {
			outputs[OUT_L_OUTPUT].setVoltage((l + r) * 0.7071f);
			outputs[OUT_R_OUTPUT].setVoltage(0.f);
		}

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			lights[HIT_LIGHT].setBrightness(hitLight);
			lights[FREE_LIGHT].setBrightness(c.free ? 1.f : 0.f);
			lights[PARTICLE_LIGHT].setBrightness(partLight);
			hitLight = std::max(0.f, hitLight - dt / 0.08f);
			partLight = std::max(0.f, partLight - dt / 0.02f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "sameEveryHit", json_boolean(sameEveryHit));
		if (!userPath.empty()) json_object_set_new(root, "userFolder", json_string(userPath.c_str()));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "sameEveryHit");
		if (s) sameEveryHit = json_boolean_value(s);
		json_t* u = json_object_get(root, "userFolder");
		if (u && json_string_value(u)) {
			std::string p = json_string_value(u);
			if (system::isDirectory(p)) loadUser(p);
			else userPath = p;   // keep it so the patch remembers, even if the folder is missing here
		}
	}
};

// Surface name screen
struct ClapfallDisplay : Widget {
	Clapfall* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) { Widget::drawLayer(args, layer); return; }
		int si = module ? module->dispSurface : 0;
		bool free = module ? module->dispFree : false;
		std::string name = SURFACE_NAMES[si];
		std::string sub = free ? "FREE" : "";
		const NVGcolor coral = nvgRGB(255, 122, 92), ink = nvgRGB(244, 232, 220);
		if (module && si == USER_INDEX) {
			Surface* u = module->userSurf.load();
			if (module->loading) sub = "loading";
			else if (!u) sub = "right-click: load";
			else sub = string::f("%d files", (int)u->vars.size());
		}
		else if (module && !gReady.load()) sub = "building";
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, mm2px(5.6f));
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, coral);
			nvgText(args.vg, mm2px(1.8f), box.size.y / 2, name.c_str(), NULL);
			if (!sub.empty()) {
				nvgFontSize(args.vg, mm2px(2.6f));
				nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
				nvgFillColor(args.vg, ink);
				nvgText(args.vg, box.size.x - mm2px(1.6f), box.size.y / 2, sub.c_str(), NULL);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct ClapfallWidget : ModuleWidget {
	ClapfallWidget(Clapfall* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Clapfall.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		ClapfallDisplay* d = createWidget<ClapfallDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		d->box.size = mm2px(Vec(DISP_W, DISP_H));
		d->module = module;
		addChild(d);
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(PLIGHT_X, PLIGHT_Y)), module, Clapfall::PARTICLE_LIGHT));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(HIT_X, BTN_Y)), module, Clapfall::HIT_PARAM, Clapfall::HIT_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(FREE_X, BTN_Y)), module, Clapfall::FREE_PARAM, Clapfall::FREE_LIGHT));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(COLX[0], ROW1_Y)), module, Clapfall::SURFACE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[1], ROW1_Y)), module, Clapfall::DENSITY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[2], ROW1_Y)), module, Clapfall::DECAY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[0], ROW2_Y)), module, Clapfall::LOOSE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[1], ROW2_Y)), module, Clapfall::TONE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[2], ROW2_Y)), module, Clapfall::SPREAD_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[0], ROW3_Y)), module, Clapfall::REVERB_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COLX[1], ROW3_Y)), module, Clapfall::SIZE_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(SW_X[0], ROW3_Y)), module, Clapfall::ENV_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(SW_X[1], ROW3_Y)), module, Clapfall::FILTER_PARAM));

		const int rowA[6] = {Clapfall::SURFACE_INPUT, Clapfall::DENSITY_INPUT, Clapfall::DECAY_INPUT,
		                     Clapfall::LOOSE_INPUT, Clapfall::TONE_INPUT, Clapfall::SPREAD_INPUT};
		const int rowB[6] = {Clapfall::REVERB_INPUT, Clapfall::SIZE_INPUT, Clapfall::VOCT_INPUT,
		                     Clapfall::FREE_INPUT, Clapfall::ACCENT_INPUT, Clapfall::ONE_INPUT};
		for (int i = 0; i < 6; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JA_Y)), module, rowA[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * i, JB_Y)), module, rowB[i]));
		}
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(JX0, JC_Y)), module, Clapfall::TRIG_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * 4, JC_Y)), module, Clapfall::OUT_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(JX0 + JDX * 5, JC_Y)), module, Clapfall::OUT_R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Clapfall* module = dynamic_cast<Clapfall*>(this->module);
		if (!module) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Same every hit", "", &module->sameEveryHit));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("USER surface (last SURFACE position)"));
		menu->addChild(createMenuItem("Load sample folder...", "", [=]() {
			char* path = osdialog_file(OSDIALOG_OPEN_DIR, module->userPath.empty() ? NULL : module->userPath.c_str(), NULL, NULL);
			if (path) {
				module->loadUser(path);
				std::free(path);
			}
		}));
		if (!module->userPath.empty()) {
			menu->addChild(createMenuLabel(module->userPath));
			menu->addChild(createMenuItem("Clear sample folder", "", [=]() { module->clearUser(); }));
		}
	}
};

Model* modelClapfall = createModel<Clapfall, ClapfallWidget>("Clapfall");
