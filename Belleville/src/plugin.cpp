#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelStrata);
	p->addModel(modelUndertow);
	p->addModel(modelSignal);
}
