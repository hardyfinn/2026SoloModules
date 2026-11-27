// The six 909 Circuits modules: one module template (Voice909.hpp) around each engine
// (Engines909.hpp) and its knob table (res/voices.py -> VoiceSpecs.hpp).
#include "Voice909.hpp"

using namespace c909;

Model* modelKick909 = createModel<VoiceModule<KickEngine>, VoiceWidget<KickEngine>>("Kick909");
Model* modelSnare909 = createModel<VoiceModule<SnareEngine>, VoiceWidget<SnareEngine>>("Snare909");
Model* modelTom909 = createModel<VoiceModule<TomEngine>, VoiceWidget<TomEngine>>("Tom909");
Model* modelRim909 = createModel<VoiceModule<RimEngine>, VoiceWidget<RimEngine>>("Rim909");
Model* modelClap909 = createModel<VoiceModule<ClapEngine>, VoiceWidget<ClapEngine>>("Clap909");
Model* modelCymbals909 = createModel<VoiceModule<CymbalsEngine>, VoiceWidget<CymbalsEngine>>("Cymbals909");
