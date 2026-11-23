// Joanna presets (right-click menu). No Rack code, so test/render.cpp can play them too.
// Each preset is one particular piano (its serial number) in one particular place.
// TOUCH, FINE, LEVEL, SUSTAIN and SOFT pedals are left alone; FELT is set (it is a lever).
#pragma once
#include "JoannaEngine.hpp"

namespace joanna {

struct Preset {
	const char* name;
	float size, hammer, sustain, symp, noise;
	float tuning, unison, wear, age;
	int lid;
	float wall, room, furnish, distance;
	uint32_t serial;
	int felt;   // practice felt on (it is a lever on many uprights, so presets set it)
};

static const int NUM_PRESETS = 20;
static const Preset PRESETS[NUM_PRESETS] = {
	//  name                                    size hammer sust symp noise  tune  unis wear  age  lid wall room  furn  dist  serial
	{"Front room (the family piano)",           .5f, .4f,  .5f, .5f, .4f,   .3f,  0,   .35f, .35f, 1, .3f, .35f, .35f, .4f,  4127, 0},
	{"Tuner came yesterday",                    .55f,.45f, .55f,.5f, .3f,   0,    0,   .2f,  .2f,  1, .4f, .35f, .4f,  .4f,  1902, 0},
	{"Gran's piano (out of tune, loved)",       .6f, .35f, .45f,.55f,.5f,   .7f,  0,   .5f,  .7f,  0, .2f, .3f,  .2f,  .42f, 7731, 0},
	{"Sunday service (church hall)",            .7f, .55f, .6f, .6f, .35f,  .35f, .05f,.55f, .45f, 1, .5f, .9f,  .75f, .5f,  3318, 0},
	{"Late set (jazz club corner)",             .65f,.5f,  .5f, .45f,.35f,  .25f, 0,   .6f,  .4f,  2, .6f, .55f, .45f, .3f,  5560, 0},
	{"Felt practice (late at night)",           .5f, .3f,  .45f,.5f, .55f,  .3f,  0,   .3f,  .3f,  1, .3f, .3f,  .3f,  .25f, 4127, 1},
	{"Pub joanna (honky-tonk)",                 .45f,.65f, .45f,.4f, .5f,   .8f,  .55f,.85f, .6f,  1, .2f, .6f,  .55f, .45f, 6681, 0},
	{"School hall (assembly)",                  .6f, .6f,  .5f, .45f,.4f,   .55f, .1f, .8f,  .55f, 1, .4f, .85f, .85f, .55f, 2219, 0},
	{"From the kitchen (next room)",            .5f, .4f,  .5f, .5f, .3f,   .3f,  0,   .35f, .35f, 1, .3f, .35f, .35f, .92f, 4127, 0},
	{"Rehearsal room (bright and close)",       .55f,.7f,  .5f, .4f, .35f,  .2f,  0,   .65f, .3f,  2, .6f, .25f, .7f,  .3f,  9047, 0},
	{"Front off (mics in the action)",          .6f, .5f,  .5f, .5f, .5f,   .25f, 0,   .45f, .4f,  2, .5f, .35f, .4f,  .05f, 5560, 0},
	{"Under the lid (closed, intimate)",        .5f, .35f, .45f,.5f, .45f,  .3f,  0,   .3f,  .35f, 0, .15f,.3f,  .25f, .2f,  4127, 0},
	{"Tack piano (drawing pins)",               .5f, .95f, .4f, .4f, .4f,   .45f, .25f,.6f,  .5f,  2, .4f, .4f,  .55f, .35f, 8812, 0},
	{"Spinet (small, boxy)",                    0,   .45f, .4f, .4f, .4f,   .4f,  0,   .4f,  .45f, 0, .2f, .2f,  .3f,  .38f, 1144, 0},
	{"Tall old upright (big bass)",             1,   .4f,  .65f,.6f, .4f,   .35f, 0,   .45f, .65f, 1, .55f,.45f, .4f,  .42f, 1888, 0},
	{"Damp garage (neglected)",                 .5f, .55f, .35f,.35f,.6f,   1,    .2f, .75f, 1,    1, .1f, .45f, 1,    .45f, 6066, 0},
	{"Bare boards (lively room)",               .55f,.5f,  .55f,.55f,.4f,   .3f,  0,   .4f,  .4f,  1, .35f, .5f, 1,    .55f, 3318, 0},
	{"Carpet and curtains (dry)",               .55f,.4f,  .5f, .5f, .35f,  .3f,  0,   .4f,  .4f,  1, .35f, .4f, 0,    .4f,  3318, 0},
	{"Stride (bright, punchy, slightly sour)",  .6f, .7f,  .45f,.45f,.4f,   .45f, .15f,.7f,  .45f, 2, .45f,.45f, .5f,  .38f, 2462, 0},
	{"Last hymn (big hall, long pedal)",        .75f,.4f,  .75f,.75f,.3f,   .4f,  .05f,.5f,  .55f, 1, .6f, 1,    .9f,  .6f,  3318, 0},
};

static inline void presetToControls(const Preset& p, Controls& c) {
	c.size = p.size; c.hammer = p.hammer; c.sustain = p.sustain; c.symp = p.symp; c.noise = p.noise;
	c.tuning = p.tuning; c.unison = p.unison; c.wear = p.wear; c.age = p.age;
	c.lid = p.lid; c.wall = p.wall; c.room = p.room; c.furnish = p.furnish; c.distance = p.distance;
	c.serial = p.serial;
	c.felt = p.felt != 0;
}

}  // namespace joanna
