#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelVinylizerFable);
	p->addModel(modelVinylMaster);
	p->addModel(modelVinylPressing);
}
