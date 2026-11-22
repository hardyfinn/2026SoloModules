// Writes presets/Birdie/NN_Name.vcvm from the reference presets (tools/proto/presets.h).
//   make presets
#include <cstdio>
#include <string>
#include "../src/dsp/presets.h"
#include "../src/dsp/BirdieCore.hpp"
using namespace birdie;
int main(int argc, char** argv) {
	const char* dir = argc > 1 ? argv[1] : "presets/Birdie";
	static BdPreset pre[32];
	int n = bd_presets(pre);
	for (int k = 0; k < n; k++) {
		float kv[NUM_P];
		paramsToKnobs(pre[k].p, kv);
		std::string path = std::string(dir) + "/" + (k < 9 ? "0" : "") + std::to_string(k + 1) + "_" + pre[k].name + ".vcvm";
		FILE* f = fopen(path.c_str(), "w");
		if (!f) { perror(path.c_str()); return 1; }
		fprintf(f, "{\n \"plugin\": \"BirdieFable\",\n \"model\": \"Birdie\",\n \"version\": \"2.0.0\",\n \"params\": [\n");
		for (int i = 0; i < NUM_P; i++)
			fprintf(f, "  {\n   \"value\": %.6g,\n   \"id\": %d\n  }%s\n", kv[i], i, i + 1 < NUM_P ? "," : "");
		fprintf(f, " ],\n \"data\": {\n  \"seed\": %u,\n  \"newEachPhrase\": false\n }\n}\n", (unsigned)pre[k].seed);
		fclose(f);
		printf("%s\n", path.c_str());
	}
	return 0;
}
