// Rimbaud presets (right-click menu). No Rack code, so test/render.cpp can play them too.
// SYMP and LEVEL are left alone when a preset loads.
#pragma once

namespace rimbaud {

struct Preset {
	const char* name;
	float size, depth, tuneHz, reso, damp, shell;
	float position, stick, buzz, rim, hoop, xstick;
	float snares, tension, wires, mic;
};

static const int NUM_PRESETS = 20;
static const Preset PRESETS[NUM_PRESETS] = {
	//  name                                      size depth tune  reso  damp shell  pos  stick buzz rim  hoop xstk  snr  tens wires mic
	{"Maple 14x6.5, all-round",                   14, 6.5f, 240, 1.5f, .15f, .2f,  .3f, .15f, 0,  .5f, .3f, .5f,  1, .5f,  .5f,  .3f},
	{"Brass 14x5, bright and crisp",              14, 5.f,  300, 1.6f, .1f,  .85f, .3f, .12f, 0,  .5f, .2f, .5f,  1, .7f,  .55f, .3f},
	{"Piccolo 13x3.5, high and tight",            13, 3.5f, 380, 1.7f, .15f, .6f,  .3f, .1f,  0,  .5f, .6f, .4f,  1, .85f, .4f,  .3f},
	{"Fat 14x8, low and loose",                   14, 8.f,  150, 1.3f, .45f, .1f,  .2f, .2f,  0,  .5f, .8f, .6f,  1, .2f,  .7f,  .4f},
	{"Dry and tight (funk)",                      14, 5.5f, 330, 1.5f, .65f, .3f,  .3f, .12f, 0,  .5f, .9f, .5f,  1, .8f,  .35f, .3f},
	{"Open and ringy, no muffling",               14, 6.5f, 220, 1.4f, 0,    .35f, .4f, .15f, 0,  .5f, .1f, .5f,  1, .35f, .5f,  .3f},
	{"Detroit backbeat (tight brass)",            14, 5.f,  320, 1.6f, .25f, .75f, .25f,.1f,  0,  .55f,.4f, .5f,  1, .75f, .5f,  .25f},
	{"80s big snare (ringy, roomy)",              14, 6.5f, 190, 1.4f, .05f, .3f,  .35f,.15f, 0,  .6f, .2f, .5f,  1, .45f, .6f,  .55f},
	{"Steel 14x6.5, cutting rimshots",            14, 6.5f, 280, 1.6f, .1f,  1.f,  .3f, .08f, 0,  .7f, .15f,.5f,  1, .6f,  .5f,  .3f},
	{"Marching (very tight, deep)",               14, 8.f,  450, 2.f,  .2f,  .7f,  .3f, .05f, 0,  .5f, .9f, .5f,  1, 1.f,  .9f,  .2f},
	{"Jazz, soft and sensitive",                  14, 5.5f, 260, 1.5f, .1f,  .2f,  .35f,.55f, 0,  .4f, .3f, .6f,  1, .3f,  .45f, .35f},
	{"Press roll (buzz strokes)",                 14, 6.5f, 260, 1.5f, .15f, .3f,  .3f, .15f, .7f,.5f, .3f, .5f,  1, .45f, .55f, .35f},
	{"Tom-tom (snares off, low)",                 14, 8.f,  140, 1.1f, .2f,  .2f,  .25f,.2f,  0,  .5f, .4f, .5f,  0, .5f,  .5f,  .2f},
	{"Timbale (snares off, metal, high)",         13, 6.5f, 420, 1.f,  .05f, .95f, .5f, .08f, 0,  .6f, .1f, .5f,  0, .5f,  .5f,  .2f},
	{"Field drum (deep, felt mallet)",            15, 8.f,  170, 1.4f, .3f,  .15f, .2f, .75f, 0,  .4f, .6f, .5f,  1, .4f,  .7f,  .4f},
	{"Cross-stick ballad (woody knock)",          13, 5.5f, 260, 1.5f, .3f,  .15f, .3f, .15f, 0,  .5f, .5f, .9f,  1, .5f,  .45f, .3f},
	{"Lo-fi trashy (loose, slack wires)",         12, 4.f,  210, .9f,  .55f, .2f,  .6f, .25f, 0,  .5f, .7f, .3f,  1, .05f, .95f, .8f},
	{"Sizzle (bottom mic, all the wires)",        14, 6.5f, 250, 1.9f, .2f,  .3f,  .3f, .12f, 0,  .5f, .3f, .5f,  1, .3f,  1.f,  1.f},
	{"Popcorn 10\" (tiny and very high)",         10, 4.f,  500, 1.8f, .2f,  .5f,  .3f, .1f,  0,  .5f, .4f, .3f,  1, .9f,  .3f,  .3f},
	{"Dusty breakbeat (dead, mid)",               14, 6.f,  230, 1.3f, .8f,  .25f, .35f,.2f,  0,  .55f,.8f, .5f,  1, .55f, .6f,  .45f},
};

static inline void presetToControls(const Preset& p, Controls& c) {
	c.size = p.size; c.depth = p.depth; c.tuneHz = p.tuneHz; c.reso = p.reso; c.damp = p.damp; c.shell = p.shell;
	c.position = p.position; c.stick = p.stick; c.buzz = p.buzz; c.rim = p.rim; c.hoop = p.hoop; c.xstick = p.xstick;
	c.snares = p.snares > 0.5f; c.tension = p.tension; c.wires = p.wires; c.mic = p.mic;
}

}  // namespace rimbaud
