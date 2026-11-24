// The band's link on the Rack side: four expander buffers, reading both neighbours, passing the message along.
// Comper, Walker and Rider all use it, so the three can sit in a row in any order.
#pragma once
#include "plugin.hpp"
#include "dsp/Link.hpp"

struct LinkPort {
	CompLink buf[4];
	uint32_t lastStamp[2] = {0, 0};
	int stale[2] = {1 << 20, 1 << 20};
	bool ok[2] = {false, false};
	CompLink got[2];
	uint32_t stamp = 0;

	static bool isBand(Model* m) { return m == modelComper || m == modelWalker || m == modelRider; }

	void attach(Module* m) {
		m->leftExpander.producerMessage = &buf[0]; m->leftExpander.consumerMessage = &buf[1];
		m->rightExpander.producerMessage = &buf[2]; m->rightExpander.consumerMessage = &buf[3];
	}
	// read both neighbours; a message whose stamp has not moved for 3 samples counts as none
	void read(Module* m) {
		for (int side = 0; side < 2; side++) {
			Module::Expander& e = side ? m->rightExpander : m->leftExpander;
			ok[side] = false;
			if (!e.module || !isBand(e.module->model)) { stale[side] = 1 << 20; continue; }
			const CompLink* L = (const CompLink*)e.consumerMessage;
			if (!L || L->magic != 0x434F4D50) continue;
			if (L->stamp != lastStamp[side]) { lastStamp[side] = L->stamp; stale[side] = 0; }
			else if (stale[side] < (1 << 20)) stale[side]++;
			if (stale[side] < 3) { ok[side] = true; got[side] = *L; }
		}
	}
	CompLink merged() const { return linkMerge(ok[0] ? &got[0] : nullptr, ok[1] ? &got[1] : nullptr); }
	// pass on: to each side, our own sections plus whatever came from the other side
	void send(Module* m, const CompLink& own) {
		stamp++;
		for (int side = 0; side < 2; side++) {
			Module::Expander& e = side ? m->rightExpander : m->leftExpander;
			if (!e.module || !isBand(e.module->model)) continue;
			Module::Expander& theirs = side ? e.module->leftExpander : e.module->rightExpander;
			CompLink* out = (CompLink*)theirs.producerMessage;
			if (!out) continue;
			int other = 1 - side;
			*out = linkForward(own, ok[other] ? &got[other] : nullptr);
			out->magic = 0x434F4D50; out->stamp = stamp;
			theirs.requestMessageFlip();
		}
	}
};
