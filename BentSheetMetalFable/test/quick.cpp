#include "bridge.h"
#include <cstdio>
#include <chrono>
int main(int argc, char** argv) {
	const char* args[4] = {"x", argv[1], argc > 2 ? argv[2] : "", argc > 3 ? argv[3] : ""};
	auto t0 = std::chrono::steady_clock::now();
	refscene::scenes_main(argc, (char**)args);
	double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	std::string fn = std::string("cpp-") + (argc > 3 ? argv[3] : argv[1]) + ".f32";
	FILE* f = fopen(fn.c_str(), "wb"); fwrite(refscene::lastOut.data(), 4, refscene::lastOut.size(), f); fclose(f);
	fprintf(stderr, "%s: %.1fs audio in %.1fs (%.0f%% of a core), guards %ld limits %ld clamps %ld\n", argv[1], refscene::lastSeconds, s, 100 * s / refscene::lastSeconds, refscene::lastGuards, refscene::lastLimits, refscene::lastClamps);
}
