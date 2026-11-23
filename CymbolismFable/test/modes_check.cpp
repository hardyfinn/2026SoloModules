// quick solver checks during development
#include "../src/CyModes.hpp"
#include <cstdio>
#include <cmath>
#include <random>
using namespace cy;
int main(int argc, char** argv) {
	// eigen solver vs a random symmetric matrix: check A v = l v
	{
		int n = 60;
		std::mt19937 g(1);
		std::uniform_real_distribution<double> u(-1, 1);
		std::vector<double> A(n * n);
		for (int i = 0; i < n; i++) for (int j = 0; j <= i; j++) A[i * n + j] = A[j * n + i] = u(g);
		std::vector<double> d, V, B = A;
		symEig(n, B, d, V);
		double err = 0;
		for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
			double s = 0;
			for (int k = 0; k < n; k++) s += A[i * n + k] * V[k * n + j];
			err = std::max(err, std::fabs(s - d[j] * V[i * n + j]));
		}
		printf("symEig residual %.2e\n", err);
	}
	// T1 flat free plate
	GeomParams gp;
	gp.type = 6;
	gp.rise = 0; gp.bell = 0; gp.taper = 1.0; gp.flangeDeg = 0;
	gp.overrideSizeIn = 24; gp.overrideEdgeMm = 1.0; gp.overrideR0 = 0.002;
	gp.quality = 0;
	gp.sampleRate = 96000;
	SolverOpts op; op.fMax = 3000; op.coreModes = 0; op.stressModes = 0;
	auto M = buildModel(gp, op);
	const Shape& sh = M->sh;
	double D = sh.mat.E * pow(sh.he, 3) / (12 * (1 - sh.mat.nu * sh.mat.nu));
	double sc = sh.a * sh.a * sqrt(sh.mat.rho * sh.he / D);
	printf("flat plate nu=%.2f a=%.3f: %zu modes, %.0f ms\n", sh.mat.nu, sh.a, M->modes.size(), M->buildMs);
	int shown = 0;
	for (size_t i = 0; i < M->modes.size() && shown < 12; i++) {
		if (M->modes[i].par) continue;
		double l2 = 2 * CY_PI * M->modes[i].f * sc;
		printf("  m=%d f=%.1f lambda^2=%.3f\n", M->modes[i].m, M->modes[i].f, l2);
		shown++;
	}
	// crash
	for (int t : {1, 4, 7, 9}) {
		GeomParams g2; g2.fromType(t); g2.quality = 1; g2.sampleRate = 96000;
		auto C = buildModel(g2);
		printf("%s: %zu modes, core %d (top %.0f Hz), stress %zu, low %d, pairs %d, aScale %.3f minEig %.3f, %.0f ms\n", CY_TYPES[t].name,
		       C->modes.size(), C->nCore, C->fTopCore, C->stress.size(), C->nLow, C->nUpperPairs, C->aScale, C->minEigCore, C->buildMs);
		printf("   first modes:");
		int k = 0;
		for (size_t i = 0; i < C->modes.size() && k < 14; i++) { if (C->modes[i].par) continue; printf(" %.0f(m%d)", C->modes[i].f, C->modes[i].m); k++; }
		printf("\n");
	}
}
