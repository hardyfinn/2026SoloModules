#pragma once
#include <rack.hpp>
#include "HorseDSP.hpp"

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelSwerve;
extern Model* modelGallop;
extern Model* modelTumble;

// Every Paddock module behaves like one physical unit: a saved seed fixes its small
// part tolerances (so it sounds the same next session), and CHARACTER scales them.
struct PaddockModule : Module {
	uint64_t seed = 0;
	int character = 1;   // 0 clean, 1 factory, 2 worn

	float age() const { return character == 0 ? 0.f : (character == 1 ? 1.f : 3.f); }
	virtual void applySeed() = 0;
	void newUnit() {
		seed = ((uint64_t) random::u32() << 32) | random::u32();
		applySeed();
	}
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_string(std::to_string(seed).c_str()));
		json_object_set_new(root, "character", json_integer(character));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* s = json_object_get(root, "seed");
		if (s && json_is_string(s)) { seed = std::strtoull(json_string_value(s), nullptr, 10); applySeed(); }
		json_t* c = json_object_get(root, "character");
		if (c) character = clamp((int) json_integer_value(c), 0, 2);
	}
};

inline void appendCharacterMenu(Menu* menu, PaddockModule* m) {
	menu->addChild(new MenuSeparator);
	menu->addChild(createIndexSubmenuItem("Character",
		{"Clean (ideal parts)", "Factory (normal tolerances)", "Worn (loose parts, more drift)"},
		[=]() { return m->character; },
		[=](int i) { m->character = i; }));
	menu->addChild(createMenuItem("New unit (re-roll part tolerances)", "", [=]() { m->newUnit(); }));
}

// 8x oversampled outputs -> one sample.
typedef dsp::Decimator<paddock::OS, 8> Dec;
