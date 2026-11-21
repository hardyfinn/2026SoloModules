// Cymbolism Fable: the geometry solver.
//
// The cymbal is a shallow shell of revolution (Marguerre / Donnell strains) with a
// radially varying thickness and profile, free outer edge and an elastic, lossy
// mount at the centre hole. For each angular order m a radial finite-element
// model (cubic Hermite elements) gives the linear shell modes, with the
// membrane (in-plane) part condensed through an Airy stress function.
// For the nonlinear core, the Airy problem is solved per angular order s to give
// "stress modes"; the membrane energy is then V = 1/2 sum_n eta_n^2 with
//     eta_n(q) = A_n . q + q^T B_n q
// (A: curvature coupling, B: von Karman quadratic strain).
#pragma once
#include "CyGeometry.hpp"
#include <vector>
#include <memory>

namespace cy {

// One linear mode of the shell.
struct Mode {
	int m = 0;          // angular order
	int par = 0;        // 0 = cos(m th), 1 = sin(m th)
	double f = 0;       // frequency (Hz), before hammering
	int shape = 0;      // index into Model::shapes (cos/sin twins share one)
};

// Radial shape of a mode, sampled on a uniform grid for point lookups, and at
// the quadrature points for integrals.
struct RadialShape {
	std::vector<float> W;     // on Model::gridR
	double edgeE = 0;         // mean square shape over the outer 15% (choke)
	double centreW = 0, centreWr = 0;  // W and W' at r0 (mount)
};

// A block of the coupling tensor B_n between two classes of core modes.
struct BBlock {
	int c1, c2;               // classes (rows from c1, columns from c2)
	std::vector<double> v;    // n_c1 x n_c2, row-major
};

struct StressMode {
	int s = 0, par = 0;
	double lambda = 0;
	std::vector<BBlock> blocks;
	std::vector<int> aIdx;      // core indices with nonzero A
	std::vector<double> aVal;
	// extended coupling (low stress modes only): zeta_n += sum_k pc[k] q[pi[k]] q[pj[k]]
	// (pc already includes the factor 2 for i != j)
	std::vector<int> pi, pj;
	std::vector<double> pc;
};

struct Model {
	GeomParams gp;
	Shape sh;
	double fs = 48000;

	std::vector<Mode> modes;        // [0, nCore) is the core, grouped by class; the rest sorted by frequency
	std::vector<RadialShape> shapes;
	int nCore = 0;

	// core mode classes: modes with the same (m, par)
	struct CoreClass { int m, par; std::vector<int> idx; int start() const { return idx.empty() ? 0 : idx[0]; } };
	std::vector<CoreClass> classes;
	std::vector<int> classOf;       // core index -> class

	std::vector<StressMode> stress;  // stress[0 .. nLow) carry extended (upper-mode) pairs
	int nLow = 0;
	double aScale = 1.0;            // factor applied to A to keep diag(w^2) - A^T A >= 0
	double minEigCore = 0.0;        // smallest eigenvalue of that matrix (before scaling), for tests
	double tensionUsed = 0.0;       // TENSION after clamping
	int nUpperPairs = 0;

	std::vector<float> gridR;       // radial lookup grid
	double fTopCore = 0;
	double Eref = 1e-6;             // energy where the nonlinearity gets strong
	double buildMs = 0;

	// value of mode i's radial shape at radius r (linear interpolation)
	double W(int i, double r) const;
	// full shape at (r, th), with an orientation offset for the mode pair
	double phi(int i, double r, double th, double thOff = 0.0) const;
};

// Build a model. Cheap to call on a background thread; never on the audio thread.
std::shared_ptr<Model> buildModel(const GeomParams& gp);

// ---- small dense linear algebra (exposed for tests) ----
// Symmetric eigen-decomposition: A (n x n row-major) -> eigenvalues d (ascending), eigenvectors as columns of V.
void symEig(int n, std::vector<double>& A, std::vector<double>& d, std::vector<double>& V);
// Generalised symmetric-definite problem K x = lambda M x. Eigenvectors M-normalised, columns of X.
bool genEig(int n, const std::vector<double>& K, const std::vector<double>& M, std::vector<double>& d, std::vector<double>& X);

// Mode-solver options for tests (flat-plate checks etc.).
struct SolverOpts {
	int elements = 0;          // 0 = automatic
	bool membrane = true;      // include curvature coupling in the linear modes
	double fMax = 0;           // 0 = automatic (min(20k, 0.42 fs))
	int coreModes = -1;        // -1 = from quality
	int stressModes = -1;
	int lowStress = -1;        // stress modes with extended coupling
	int pairsPerMode = -1;
	double pairFMax = 0;       // upper modes with pairs: up to this x the core's top frequency (0 = from quality)
	bool mountSprings = true;
};
std::shared_ptr<Model> buildModel(const GeomParams& gp, const SolverOpts& opt);

} // namespace cy
