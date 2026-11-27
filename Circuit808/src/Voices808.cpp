// The seven 808 Circuits voices beside the kick. Each is the same module template
// (Voice808.hpp) around its own engine (VoiceEngines.hpp) and knob table.
#include "Voice808.hpp"

using namespace c808;

Model* modelSnare808 = createModel<VoiceModule<SnareEngine>, VoiceWidget<SnareEngine>>("Snare808");
Model* modelClap808 = createModel<VoiceModule<ClapEngine>, VoiceWidget<ClapEngine>>("Clap808");
Model* modelTomConga808 = createModel<VoiceModule<TomEngine>, VoiceWidget<TomEngine>>("TomConga808");
Model* modelRim808 = createModel<VoiceModule<RimEngine>, VoiceWidget<RimEngine>>("Rim808");
Model* modelClave808 = createModel<VoiceModule<ClaveEngine>, VoiceWidget<ClaveEngine>>("Clave808");
Model* modelMaracas808 = createModel<VoiceModule<MaracasEngine>, VoiceWidget<MaracasEngine>>("Maracas808");
Model* modelCowbell808 = createModel<VoiceModule<CowbellEngine>, VoiceWidget<CowbellEngine>>("Cowbell808");
