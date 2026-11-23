// Writes presets/Chirp/NN_Name.vcvm from src/ChirpPresets.hpp.
//   g++ -std=c++17 tools/export_presets.cpp -o tools/export_presets && ./tools/export_presets
#include "../src/ChirpPresets.hpp"
#include <cstdio>
using namespace chirp;
int main() {
	for (int i = 0; i < NUM_PRESETS; i++) {
		const Preset& p = PRESETS[i];
		char path[256];
		snprintf(path, sizeof path, "presets/Chirp/%02d_%s.vcvm", i + 1, p.name);
		FILE* f = fopen(path, "w");
		if (!f) { perror(path); return 1; }
		float v[10] = {knobPitch(p.pitch), knobSweep(p.sweep), knobTime(p.time), knobCurve(p.curve),
		               knobDecay(p.decay), p.tone, knobRepeat(p.repeat), knobRate(p.rate), 0.f, p.loop ? 1.f : 0.f};
		fprintf(f, "{\n \"plugin\": \"HaerChirp\",\n \"model\": \"Chirp\",\n \"version\": \"2.0.0\",\n \"params\": [\n");
		for (int k = 0; k < 10; k++)
			fprintf(f, "  {\"value\": %.6f, \"id\": %d}%s\n", v[k], k, k < 9 ? "," : "");
		fprintf(f, " ]\n}\n");
		fclose(f);
		printf("%s\n", path);
	}
	return 0;
}
