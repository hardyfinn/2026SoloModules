#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelTakt);
	p->addModel(modelBlech);
	p->addModel(modelZap);
	p->addModel(modelVokoder);
	p->addModel(modelSprecher);
	p->addModel(modelOrchester);
	p->addModel(modelDynamo);
}
