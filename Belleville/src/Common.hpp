// Rack-side pieces the three modules share: the link along the row, note outputs with swing and glide,
// and the green screen.
#pragma once
#include "plugin.hpp"
#include "Layout.hpp"
#include "dsp/Link.hpp"

// ---------------- the link: four expander buffers, reading both neighbours, passing the message along
struct LinkPort {
	bv::TrioLink buf[4];
	uint32_t lastStamp[2] = {0, 0};
	int stale[2] = {1 << 20, 1 << 20};
	bool ok[2] = {false, false};
	bv::TrioLink got[2];
	uint32_t stamp = 0;

	static bool isTrio(Model* m) { return m == modelStrata || m == modelUndertow || m == modelSignal; }
	void attach(Module* m) {
		m->leftExpander.producerMessage = &buf[0]; m->leftExpander.consumerMessage = &buf[1];
		m->rightExpander.producerMessage = &buf[2]; m->rightExpander.consumerMessage = &buf[3];
	}
	void read(Module* m) {
		for (int side = 0; side < 2; side++) {
			Module::Expander& e = side ? m->rightExpander : m->leftExpander;
			ok[side] = false;
			if (!e.module || !isTrio(e.module->model)) { stale[side] = 1 << 20; continue; }
			const bv::TrioLink* L = (const bv::TrioLink*)e.consumerMessage;
			if (!L || L->magic != 0x42454C4C) continue;
			if (L->stamp != lastStamp[side]) { lastStamp[side] = L->stamp; stale[side] = 0; }
			else if (stale[side] < (1 << 20)) stale[side]++;
			if (stale[side] < 3) { ok[side] = true; got[side] = *L; }
		}
	}
	bv::TrioLink merged() const { return bv::linkMerge(ok[0] ? &got[0] : nullptr, ok[1] ? &got[1] : nullptr); }
	void send(Module* m, const bv::TrioLink& own) {
		stamp++;
		for (int side = 0; side < 2; side++) {
			Module::Expander& e = side ? m->rightExpander : m->leftExpander;
			if (!e.module || !isTrio(e.module->model)) continue;
			Module::Expander& theirs = side ? e.module->leftExpander : e.module->rightExpander;
			bv::TrioLink* out = (bv::TrioLink*)theirs.producerMessage;
			if (!out) continue;
			int other = 1 - side;
			*out = bv::linkForward(own, ok[other] ? &got[other] : nullptr);
			out->magic = 0x42454C4C; out->stamp = stamp;
			theirs.requestMessageFlip();
		}
	}
};

// ---------------- a note output: holds one pending (swung) event, plays it, times the gate
struct NotePlayer {
	bv::NoteEvent cur, pend;
	bool hasPend = false;
	float pendIn = 0.f;            // samples until the pending event plays
	float gateLeft = 0.f;          // samples of gate left
	float retrig = 0.f;            // samples of forced low gate (so back-to-back notes retrigger)
	float changePulse = 0.f;
	bool prevSlide = false, gliding = false;
	float voct[6] = {}, target[6] = {};
	int channels = 1;
	float stepSamples = 5000.f;

	void reset() { hasPend = false; gateLeft = 0.f; retrig = 0.f; prevSlide = false; }
	void schedule(const bv::NoteEvent& e, float delaySamples, float stepLen) {
		stepSamples = stepLen;
		if (hasPend) fire();             // never drop a note
		pend = e; hasPend = true; pendIn = delaySamples;
		if (delaySamples <= 0.f) fire();
	}
	void fire(float sampleRate = 48000.f) {
		hasPend = false;
		bool wasHigh = gateLeft > 0.f;
		bool tie = prevSlide && wasHigh;
		cur = pend;
		channels = std::max(1, cur.n);
		for (int k = 0; k < channels; k++) {
			target[k] = bv::midiToV((float)cur.note[k]);
			if (!tie) voct[k] = target[k];
		}
		gliding = tie;
		gateLeft = std::max(1.f, cur.gateSteps * stepSamples);
		if (wasHigh && !tie) retrig = 0.0015f * sampleRate;
		prevSlide = cur.slide;
		if (cur.change) changePulse = 0.001f * sampleRate;
	}
	// one sample; glideCoef 0..1 per sample (1 = jump)
	void process(float sampleRate, float glideCoef = 1.f) {
		if (hasPend) { pendIn -= 1.f; if (pendIn <= 0.f) fire(sampleRate); }
		if (gateLeft > 0.f) gateLeft -= 1.f;
		if (retrig > 0.f) retrig -= 1.f;
		if (changePulse > 0.f) changePulse -= 1.f;
		for (int k = 0; k < channels; k++) voct[k] += (target[k] - voct[k]) * (gliding ? glideCoef : 1.f);
	}
	bool gate() const { return gateLeft > 0.f && retrig <= 0.f; }
};

// ---------------- the screen: up to four lines and an optional sixteen-step strip
struct ScreenInfo {
	std::string line[4];
	int lines = 0;
	bool grid = false;
	uint16_t mask = 0;
	int pos = -1;
};
static inline NVGcolor SCR_INK() { return nvgRGB(0x7E, 0xF0, 0xC0); }
static inline NVGcolor SCR_DIM() { return nvgRGB(0x3C, 0x7A, 0x62); }

template <class TModule>
struct TrioScreen : TransparentWidget {
	TModule* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		ScreenInfo s;
		if (module) module->screen(s);
		else { s.lines = 2; s.line[0] = "Belleville trio"; s.line[1] = "chords - bass - melody"; }
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		float w = box.size.x, h = box.size.y;
		float gridH = s.grid ? 9.f : 0.f;
		float lh = (h - gridH - 6.f) / std::max(1, s.lines);
		nvgFontSize(args.vg, std::min(15.f, lh * 0.95f));
		nvgScissor(args.vg, 0, 0, w, h);
		for (int i = 0; i < s.lines; i++) {
			nvgFillColor(args.vg, i == 0 ? SCR_INK() : (i == s.lines - 1 && s.lines > 2 ? SCR_DIM() : SCR_INK()));
			nvgText(args.vg, 7.f, 4.f + lh * (i + 0.8f), s.line[i].c_str(), NULL);
		}
		if (s.grid) {
			float gx = 7.f, gw = (w - 14.f) / 16.f, gy = h - gridH - 2.f;
			for (int k = 0; k < 16; k++) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, gx + k * gw + 1.f, gy, gw - 2.f, gridH - 3.f);
				bool on = s.mask & (1 << k);
				nvgFillColor(args.vg, on ? SCR_INK() : nvgRGBA(0x7E, 0xF0, 0xC0, k % 4 == 0 ? 0x38 : 0x1C));
				nvgFill(args.vg);
				if (k == s.pos) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, gx + k * gw + 0.5f, gy - 0.5f, gw - 1.f, gridH - 2.f);
					nvgStrokeColor(args.vg, nvgRGB(0xFF, 0xFF, 0xFF));
					nvgStrokeWidth(args.vg, 1.f);
					nvgStroke(args.vg);
				}
			}
		}
		nvgResetScissor(args.vg);
	}
};

struct LinkLight : GrayModuleLightWidget { LinkLight() { addBaseColor(nvgRGB(0x3C, 0xD0, 0x8C)); } };

// a knob that shows a name instead of a number
struct NamedQuantity : ParamQuantity {
	std::function<std::string(int)> namer;
	std::string getDisplayValueString() override { return namer ? namer((int)std::round(getValue())) : ParamQuantity::getDisplayValueString(); }
};
template <class TModule>
static inline void configNamed(TModule* m, int id, float lo, float hi, float def, const std::string& name, std::function<std::string(int)> namer) {
	NamedQuantity* q = m->template configParam<NamedQuantity>(id, lo, hi, def, name);
	q->snapEnabled = true;
	q->namer = namer;
}
