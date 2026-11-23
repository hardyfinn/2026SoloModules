// Cymbolism Fable: geometry solver. See CyModes.hpp for the model.
#include "CyModes.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <tuple>
#include <array>

namespace cy {

// ---------------------------------------------------------------------------
// Dense symmetric eigen-solver: Householder tridiagonalisation + implicit QL
// (after the public-domain JAMA routines tred2 / tql2).
// ---------------------------------------------------------------------------
static void tred2(int n, std::vector<double>& V, std::vector<double>& d, std::vector<double>& e) {
	for (int j = 0; j < n; j++) d[j] = V[(n - 1) * n + j];
	for (int i = n - 1; i > 0; i--) {
		double scale = 0.0, h = 0.0;
		for (int k = 0; k < i; k++) scale += std::fabs(d[k]);
		if (scale == 0.0) {
			e[i] = d[i - 1];
			for (int j = 0; j < i; j++) {
				d[j] = V[(i - 1) * n + j];
				V[i * n + j] = 0.0;
				V[j * n + i] = 0.0;
			}
		}
		else {
			for (int k = 0; k < i; k++) {
				d[k] /= scale;
				h += d[k] * d[k];
			}
			double f = d[i - 1];
			double g = std::sqrt(h);
			if (f > 0) g = -g;
			e[i] = scale * g;
			h = h - f * g;
			d[i - 1] = f - g;
			for (int j = 0; j < i; j++) e[j] = 0.0;
			for (int j = 0; j < i; j++) {
				f = d[j];
				V[j * n + i] = f;
				g = e[j] + V[j * n + j] * f;
				for (int k = j + 1; k <= i - 1; k++) {
					g += V[k * n + j] * d[k];
					e[k] += V[k * n + j] * f;
				}
				e[j] = g;
			}
			f = 0.0;
			for (int j = 0; j < i; j++) {
				e[j] /= h;
				f += e[j] * d[j];
			}
			double hh = f / (h + h);
			for (int j = 0; j < i; j++) e[j] -= hh * d[j];
			for (int j = 0; j < i; j++) {
				f = d[j];
				g = e[j];
				for (int k = j; k <= i - 1; k++) V[k * n + j] -= (f * e[k] + g * d[k]);
				d[j] = V[(i - 1) * n + j];
				V[i * n + j] = 0.0;
			}
		}
		d[i] = h;
	}
	for (int i = 0; i < n - 1; i++) {
		V[(n - 1) * n + i] = V[i * n + i];
		V[i * n + i] = 1.0;
		double h = d[i + 1];
		if (h != 0.0) {
			for (int k = 0; k <= i; k++) d[k] = V[k * n + (i + 1)] / h;
			for (int j = 0; j <= i; j++) {
				double g = 0.0;
				for (int k = 0; k <= i; k++) g += V[k * n + (i + 1)] * V[k * n + j];
				for (int k = 0; k <= i; k++) V[k * n + j] -= g * d[k];
			}
		}
		for (int k = 0; k <= i; k++) V[k * n + (i + 1)] = 0.0;
	}
	for (int j = 0; j < n; j++) {
		d[j] = V[(n - 1) * n + j];
		V[(n - 1) * n + j] = 0.0;
	}
	V[(n - 1) * n + (n - 1)] = 1.0;
	e[0] = 0.0;
}

static void tql2(int n, std::vector<double>& V, std::vector<double>& d, std::vector<double>& e) {
	for (int i = 1; i < n; i++) e[i - 1] = e[i];
	e[n - 1] = 0.0;
	double f = 0.0, tst1 = 0.0;
	const double eps = std::pow(2.0, -52.0);
	for (int l = 0; l < n; l++) {
		tst1 = std::max(tst1, std::fabs(d[l]) + std::fabs(e[l]));
		int m = l;
		while (m < n) {
			if (std::fabs(e[m]) <= eps * tst1) break;
			m++;
		}
		if (m == n) m = n - 1;
		if (m > l) {
			int iter = 0;
			do {
				iter++;
				double g = d[l];
				double p = (d[l + 1] - g) / (2.0 * e[l]);
				double r = std::hypot(p, 1.0);
				if (p < 0) r = -r;
				d[l] = e[l] / (p + r);
				d[l + 1] = e[l] * (p + r);
				double dl1 = d[l + 1];
				double h = g - d[l];
				for (int i = l + 2; i < n; i++) d[i] -= h;
				f += h;
				p = d[m];
				double c = 1.0, c2 = c, c3 = c;
				double el1 = e[l + 1];
				double s = 0.0, s2 = 0.0;
				for (int i = m - 1; i >= l; i--) {
					c3 = c2;
					c2 = c;
					s2 = s;
					g = c * e[i];
					h = c * p;
					r = std::hypot(p, e[i]);
					e[i + 1] = s * r;
					s = e[i] / r;
					c = p / r;
					p = c * d[i] - s * g;
					d[i + 1] = h + s * (c * g + s * d[i]);
					for (int k = 0; k < n; k++) {
						h = V[k * n + (i + 1)];
						V[k * n + (i + 1)] = s * V[k * n + i] + c * h;
						V[k * n + i] = c * V[k * n + i] - s * h;
					}
				}
				p = -s * s2 * c3 * el1 * e[l] / dl1;
				e[l] = s * p;
				d[l] = c * p;
			} while (std::fabs(e[l]) > eps * tst1 && iter < 80);
		}
		d[l] = d[l] + f;
		e[l] = 0.0;
	}
	// sort ascending
	for (int i = 0; i < n - 1; i++) {
		int k = i;
		double p = d[i];
		for (int j = i + 1; j < n; j++)
			if (d[j] < p) {
				k = j;
				p = d[j];
			}
		if (k != i) {
			d[k] = d[i];
			d[i] = p;
			for (int j = 0; j < n; j++) std::swap(V[j * n + i], V[j * n + k]);
		}
	}
}

void symEig(int n, std::vector<double>& A, std::vector<double>& d, std::vector<double>& V) {
	V = A;
	d.assign(n, 0.0);
	std::vector<double> e(n, 0.0);
	tred2(n, V, d, e);
	tql2(n, V, d, e);
}

// Cholesky in place (lower), returns false if not positive definite.
static bool cholesky(int n, std::vector<double>& L) {
	for (int j = 0; j < n; j++) {
		double s = L[j * n + j];
		for (int k = 0; k < j; k++) s -= L[j * n + k] * L[j * n + k];
		if (s <= 0) return false;
		double ljj = std::sqrt(s);
		L[j * n + j] = ljj;
		for (int i = j + 1; i < n; i++) {
			double t = L[i * n + j];
			for (int k = 0; k < j; k++) t -= L[i * n + k] * L[j * n + k];
			L[i * n + j] = t / ljj;
		}
		for (int k = j + 1; k < n; k++) L[j * n + k] = 0.0;
	}
	return true;
}

// solve L y = b (in place), b has nrhs columns, stored row-major n x nrhs
static void cholFwd(int n, const std::vector<double>& L, double* b, int nrhs) {
	for (int i = 0; i < n; i++) {
		for (int k = 0; k < i; k++) {
			double l = L[i * n + k];
			if (l == 0.0) continue;
			for (int c = 0; c < nrhs; c++) b[i * nrhs + c] -= l * b[k * nrhs + c];
		}
		double inv = 1.0 / L[i * n + i];
		for (int c = 0; c < nrhs; c++) b[i * nrhs + c] *= inv;
	}
}
// solve L^T x = y (in place)
static void cholBwd(int n, const std::vector<double>& L, double* b, int nrhs) {
	for (int i = n - 1; i >= 0; i--) {
		for (int k = i + 1; k < n; k++) {
			double l = L[k * n + i];
			if (l == 0.0) continue;
			for (int c = 0; c < nrhs; c++) b[i * nrhs + c] -= l * b[k * nrhs + c];
		}
		double inv = 1.0 / L[i * n + i];
		for (int c = 0; c < nrhs; c++) b[i * nrhs + c] *= inv;
	}
}

bool genEig(int n, const std::vector<double>& K, const std::vector<double>& M, std::vector<double>& d, std::vector<double>& X) {
	std::vector<double> L = M;
	if (!cholesky(n, L)) return false;
	// A = L^-1 K L^-T : first Y = L^-1 K, then A = (L^-1 Y^T)^T
	std::vector<double> Y = K;
	cholFwd(n, L, Y.data(), n);
	std::vector<double> Yt(n * n);
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++) Yt[i * n + j] = Y[j * n + i];
	cholFwd(n, L, Yt.data(), n);
	std::vector<double> A(n * n);
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++) A[i * n + j] = 0.5 * (Yt[i * n + j] + Yt[j * n + i]);
	std::vector<double> V;
	symEig(n, A, d, V);
	// x = L^-T y
	X = V;
	cholBwd(n, L, X.data(), n);
	return true;
}

// ---------------------------------------------------------------------------
// Radial finite elements
// ---------------------------------------------------------------------------
namespace {

struct QPt {
	double r, wr;      // radius, quadrature weight * r * dr
	int e;             // element
	double N[4], dN[4], ddN[4];
	double zp, h, D, Eh;
};

struct Mesh {
	std::vector<double> nodes;
	std::vector<QPt> q;
	int E = 0;
	int ndof() const { return 2 * (E + 1); }
};

const double GX[4] = {-0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
const double GW[4] = {0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};

void hermite(double xi, double L, double* N, double* dN, double* ddN) {
	double x2 = xi * xi, x3 = x2 * xi;
	N[0] = 1 - 3 * x2 + 2 * x3;
	N[1] = L * (xi - 2 * x2 + x3);
	N[2] = 3 * x2 - 2 * x3;
	N[3] = L * (-x2 + x3);
	dN[0] = (-6 * xi + 6 * x2) / L;
	dN[1] = 1 - 4 * xi + 3 * x2;
	dN[2] = (6 * xi - 6 * x2) / L;
	dN[3] = -2 * xi + 3 * x2;
	ddN[0] = (-6 + 12 * xi) / (L * L);
	ddN[1] = (-4 + 6 * xi) / L;
	ddN[2] = (6 - 12 * xi) / (L * L);
	ddN[3] = (-2 + 6 * xi) / L;
}

// Nodes spaced so that each element covers the same bending phase at fMax.
Mesh makeMesh(const Shape& sh, double rStart, double fMax, int forceE) {
	Mesh M;
	const Material& mt = sh.mat;
	auto kOf = [&](double r) {
		double hh = sh.h(r);
		double D = mt.E * hh * hh * hh / (12.0 * (1.0 - mt.nu * mt.nu));
		double w = 2.0 * CY_PI * fMax;
		return std::pow(mt.rho * hh * w * w / D, 0.25);
	};
	const int NS = 400;
	std::vector<double> rr(NS + 1), ph(NS + 1);
	ph[0] = 0;
	for (int i = 0; i <= NS; i++) rr[i] = rStart + (sh.a - rStart) * i / NS;
	for (int i = 1; i <= NS; i++) {
		double k = kOf(0.5 * (rr[i] + rr[i - 1]));
		// also resolve the geometry: never coarser than 1/40 of the radius
		k = std::max(k, 2.0 * CY_PI * 6.0 / (40.0 * (sh.a - sh.r0)) * 1.0);
		ph[i] = ph[i - 1] + k * (rr[i] - rr[i - 1]);
	}
	int E = forceE > 0 ? forceE : (int)std::ceil(ph[NS] / (2.0 * CY_PI) * 6.5);
	if (forceE <= 0) E = std::max(16, std::min(E, 150));
	M.E = E;
	M.nodes.resize(E + 1);
	int j = 0;
	for (int i = 0; i <= E; i++) {
		double target = ph[NS] * i / E;
		while (j < NS - 1 && ph[j + 1] < target) j++;
		double t = (target - ph[j]) / std::max(1e-30, ph[j + 1] - ph[j]);
		t = std::min(1.0, std::max(0.0, t));
		M.nodes[i] = rr[j] + t * (rr[j + 1] - rr[j]);
	}
	M.nodes[0] = rStart;
	M.nodes[E] = sh.a;
	for (int e = 0; e < E; e++) {
		double r1 = M.nodes[e], r2 = M.nodes[e + 1], L = r2 - r1;
		for (int g = 0; g < 4; g++) {
			QPt p;
			double xi = 0.5 * (1 + GX[g]);
			p.r = r1 + xi * L;
			p.wr = 0.5 * GW[g] * L * p.r;
			p.e = e;
			hermite(xi, L, p.N, p.dN, p.ddN);
			double zz;
			sh.z(p.r, zz, p.zp);
			p.zp = p.zp / std::sqrt(1.0 + p.zp * p.zp);   // sin(phi): keeps steep (bell) regions closer to shell theory
			p.h = sh.h(p.r);
			p.D = mt.E * p.h * p.h * p.h / (12.0 * (1.0 - mt.nu * mt.nu));
			p.Eh = mt.E * p.h;
			M.q.push_back(p);
		}
	}
	return M;
}

// Fields of a nodal vector at a quadrature point
inline void evalAt(const QPt& p, const double* u, double& v, double& d1, double& d2) {
	const double* x = u + 2 * p.e;
	v = p.N[0] * x[0] + p.N[1] * x[1] + p.N[2] * x[2] + p.N[3] * x[3];
	d1 = p.dN[0] * x[0] + p.dN[1] * x[1] + p.dN[2] * x[2] + p.dN[3] * x[3];
	d2 = p.ddN[0] * x[0] + p.ddN[1] * x[1] + p.ddN[2] * x[2] + p.ddN[3] * x[3];
}

double thetaFactor(int m) { return m == 0 ? 2.0 * CY_PI : CY_PI; }

// Airy (complementary energy) matrix G for angular order s on interior dofs
// [2, ndof-2) (F = F' = 0 at both edges), and the metric MF.
void airyMatrices(const Mesh& M, int s, const Material& mt, double heRef, std::vector<double>& G, std::vector<double>& MF, int& nF) {
	int nd = M.ndof();
	nF = nd - 4;
	G.assign(nF * nF, 0.0);
	MF.assign(nF * nF, 0.0);
	double Th = thetaFactor(s);
	double s2 = (double)s * s;
	double nu = mt.nu;
	for (const QPt& p : M.q) {
		double r = p.r;
		double nrr[4], ntt[4], tt[4];
		for (int a = 0; a < 4; a++) {
			nrr[a] = p.dN[a] / r - s2 * p.N[a] / (r * r);
			ntt[a] = p.ddN[a];
			tt[a] = p.dN[a] / r - p.N[a] / (r * r);
		}
		double c = p.wr / p.Eh;
		for (int a = 0; a < 4; a++) {
			int ga = 2 * p.e + a - 2;
			if (ga < 0 || ga >= nF) continue;
			for (int b = 0; b < 4; b++) {
				int gb = 2 * p.e + b - 2;
				if (gb < 0 || gb >= nF) continue;
				double v = Th * (nrr[a] * nrr[b] + ntt[a] * ntt[b] - nu * (nrr[a] * ntt[b] + ntt[a] * nrr[b]));
				if (s > 0) v += 2.0 * (1.0 + nu) * tt[a] * tt[b] * s2 * CY_PI;
				G[ga * nF + gb] += c * v;
				MF[ga * nF + gb] += p.wr * Th * p.N[a] * p.N[b] / (mt.E * heRef);
			}
		}
	}
}

struct RawMode {
	int m;
	double f;
	std::vector<double> u;   // nodal vector (on this m's mesh)
	int mesh;                // which mesh
};

} // namespace

// ---------------------------------------------------------------------------
double Model::W(int i, double r) const {
	const RadialShape& s = shapes[modes[i].shape];
	int n = (int)gridR.size();
	double r0 = gridR[0], r1 = gridR[n - 1];
	double t = (r - r0) / (r1 - r0) * (n - 1);
	if (t <= 0) return s.W[0];
	if (t >= n - 1) return s.W[n - 1];
	int k = (int)t;
	double fr = t - k;
	return s.W[k] + fr * (s.W[k + 1] - s.W[k]);
}

double Model::phi(int i, double r, double th, double thOff) const {
	const Mode& md = modes[i];
	double w = W(i, r);
	double a = md.m * (th - thOff);
	return w * (md.par ? std::sin(a) : std::cos(a));
}

void buildPickup(Model& M);
std::shared_ptr<Model> buildModel(const GeomParams& gp) { return buildModel(gp, SolverOpts()); }


// ---------------------------------------------------------------------------
// General shell of revolution: conical frustum elements along the meridian.
//   meridian node j at (r_j, z_j); element e from node e to e+1, generator
//   angle phi_e (from the r axis), length L_e. Local displacements: u along the
//   generator (quadratic, with an internal node), v circumferential (quadratic),
//   w normal (cubic Hermite; nodal rotation Theta = dw/ds is frame invariant).
//   Love's first-approximation strains, Fourier order m:
//     u = U cos, v = V sin, w = W cos.
//   Global nodal dofs [t, V, w, Theta] in a nodal frame (average generator
//   angle); t, V and the internal nodes are condensed statically (no in-plane
//   inertia), leaving (w, Theta) per node.
// ---------------------------------------------------------------------------
namespace {

struct SMesh {
	std::vector<double> r, z;     // nodes
	std::vector<double> phi, L;   // per element
	std::vector<double> psi;      // nodal frame angle
	int E = 0;
	bool clampedInner = false;
};

SMesh makeShellMesh(const Shape& sh, double rStart, double fMax, int forceE, bool clamped) {
	SMesh M;
	const Material& mt = sh.mat;
	const int NS = 600;
	std::vector<double> rr(NS + 1), ph(NS + 1);
	for (int i = 0; i <= NS; i++) rr[i] = rStart + (sh.a - rStart) * i / NS;
	ph[0] = 0;
	double w = 2.0 * CY_PI * fMax;
	for (int i = 1; i <= NS; i++) {
		double rm = 0.5 * (rr[i] + rr[i - 1]);
		double hh = sh.h(rm);
		double D = mt.E * hh * hh * hh / (12.0 * (1.0 - mt.nu * mt.nu));
		double k = std::pow(mt.rho * hh * w * w / D, 0.25);
		double zz, zp;
		sh.z(rm, zz, zp);
		double ds = std::sqrt(1.0 + zp * zp) * (rr[i] - rr[i - 1]);
		// also resolve the geometry (curvature): at least 48 elements over the radius
		k = std::max(k, 2.0 * CY_PI * 6.5 / (48.0 * (sh.a - sh.r0)) / 1.0);
		ph[i] = ph[i - 1] + k * ds;
	}
	int E = forceE > 0 ? forceE : (int)std::ceil(ph[NS] / (2.0 * CY_PI) * 6.5);
	if (forceE <= 0) E = std::max(20, std::min(E, 170));
	M.E = E;
	M.r.resize(E + 1);
	M.z.resize(E + 1);
	int j = 0;
	for (int i = 0; i <= E; i++) {
		double target = ph[NS] * i / E;
		while (j < NS - 1 && ph[j + 1] < target) j++;
		double t = (target - ph[j]) / std::max(1e-30, ph[j + 1] - ph[j]);
		t = std::min(1.0, std::max(0.0, t));
		M.r[i] = rr[j] + t * (rr[j + 1] - rr[j]);
	}
	M.r[0] = rStart;
	M.r[E] = sh.a;
	for (int i = 0; i <= E; i++) {
		double zz, zp;
		sh.z(M.r[i], zz, zp);
		M.z[i] = zz;
	}
	M.phi.resize(E);
	M.L.resize(E);
	for (int e = 0; e < E; e++) {
		double dr = M.r[e + 1] - M.r[e], dz = M.z[e + 1] - M.z[e];
		M.L[e] = std::sqrt(dr * dr + dz * dz);
		M.phi[e] = std::atan2(dz, dr);
	}
	M.psi.resize(E + 1);
	for (int i = 0; i <= E; i++) {
		if (i == 0) M.psi[i] = M.phi[0];
		else if (i == E) M.psi[i] = M.phi[E - 1];
		else M.psi[i] = 0.5 * (M.phi[i - 1] + M.phi[i]);
	}
	M.clampedInner = clamped;
	return M;
}

// Residual (hammering) stress resultants for eigenstrain eps in [r1, r2] of an
// annulus [ri, ro] with free edges (plane stress, uniform thickness formulas
// scaled by the local thickness).
struct Prestress {
	double eps = 0, r1 = 0, r2 = 0, ri = 0, ro = 1, E = 1;
	double I(double r) const {  // int_ri^r eps(rho) rho drho
		double a = std::max(ri, r1), b = std::min(r, r2);
		if (b <= a) return 0.0;
		return eps * 0.5 * (b * b - a * a);
	}
	void N(double r, double h, double& Nrr, double& Ntt) const {
		if (eps == 0.0) { Nrr = Ntt = 0; return; }
		double Itot = I(ro), Ir = I(r);
		double den = ro * ro - ri * ri;
		double er = (r >= r1 && r <= r2) ? eps : 0.0;
		double sr = E / (r * r) * ((r * r - ri * ri) / den * Itot - Ir);
		double st = E / (r * r) * ((r * r + ri * ri) / den * Itot + Ir) - E * er;
		Nrr = sr * h;
		Ntt = st * h;
	}
};

// Solve one angular order. Returns frequencies (Hz, ascending, only within
// [fMin, fMax]) and, per mode, element-local end values [w_a, th_a, w_b, th_b]
// for every element (E*4 numbers).
struct HarmonicResult {
	std::vector<double> f;
	std::vector<std::vector<double>> ev;   // per mode: E*4
	double lowestW2 = 1e300;
};

HarmonicResult solveHarmonic(const Shape& sh, const SMesh& ms, int m, double fMin, double fMax, const Prestress& ps) {
	HarmonicResult res;
	const Material& mt = sh.mat;
	const double nu = mt.nu;
	const int E = ms.E, NN = E + 1;
	const int G = 4 * NN + 2 * E;
	const double Thc = m == 0 ? 2.0 * CY_PI : CY_PI;   // int cos^2
	const double Ths = m == 0 ? 0.0 : CY_PI;           // int sin^2
	const double md = m;
	std::vector<double> K(G * G, 0.0), Mw(G * G, 0.0);
	auto nodeDof = [&](int j, int k) { return 4 * j + k; };      // k: 0 t, 1 V, 2 w, 3 Theta
	auto bubDof = [&](int e, int k) { return 4 * NN + 2 * e + k; };

	for (int e = 0; e < E; e++) {
		double c = std::cos(ms.phi[e]), s = std::sin(ms.phi[e]);
		double L = ms.L[e];
		double Ke[10][10] = {}, Me[10][10] = {};
		for (int g = 0; g < 4; g++) {
			double xi = 0.5 * (1 + GX[g]);
			double wq = 0.5 * GW[g] * L;
			double r = ms.r[e] + xi * (ms.r[e + 1] - ms.r[e]);
			double hh = sh.h(r);
			double C = mt.E * hh / (1.0 - nu * nu);
			double D = mt.E * hh * hh * hh / (12.0 * (1.0 - nu * nu));
			// quadratic Lagrange (ends a, b and middle) and derivatives d/ds
			double L1 = (1 - xi) * (1 - 2 * xi), L3 = xi * (2 * xi - 1), L2 = 4 * xi * (1 - xi);
			double dL1 = (4 * xi - 3) / L, dL3 = (4 * xi - 1) / L, dL2 = (4 - 8 * xi) / L;
			double H[4], dH[4], ddH[4];
			hermite(xi, L, H, dH, ddH);
			// dof order: 0 u_a, 1 v_a, 2 w_a, 3 th_a, 4 u_b, 5 v_b, 6 w_b, 7 th_b, 8 u_m, 9 v_m
			double U[10] = {}, dU[10] = {}, V[10] = {}, dV[10] = {}, W[10] = {}, dW[10] = {}, ddW[10] = {};
			U[0] = L1; U[4] = L3; U[8] = L2;
			dU[0] = dL1; dU[4] = dL3; dU[8] = dL2;
			V[1] = L1; V[5] = L3; V[9] = L2;
			dV[1] = dL1; dV[5] = dL3; dV[9] = dL2;
			int wi[4] = {2, 3, 6, 7};
			for (int k = 0; k < 4; k++) {
				W[wi[k]] = H[k];
				dW[wi[k]] = dH[k];
				ddW[wi[k]] = ddH[k];
			}
			double es[10], et[10], gm[10], ks[10], kt[10], tw[10];
			for (int i = 0; i < 10; i++) {
				es[i] = dU[i];
				// radial displacement of the mid-surface: u_r = c u - s w (normal n = (-s, c)), so 1/R2 = -s/r
				et[i] = (md * V[i] + c * U[i] - s * W[i]) / r;
				gm[i] = -md * U[i] / r + dV[i] - c * V[i] / r;
				ks[i] = -ddW[i];
				kt[i] = md * (-s * V[i] + md * W[i]) / (r * r) - c * dW[i] / r;
				tw[i] = (-s * dV[i] + 2 * md * dW[i]) / r - 2 * c * (-s * V[i] + md * W[i]) / (r * r);
			}
			double Nrr, Ntt;
			ps.N(r, hh, Nrr, Ntt);
			double f = wq * r;
			for (int i = 0; i < 10; i++)
				for (int k = 0; k < 10; k++) {
					double v = C * Thc * (es[i] * es[k] + et[i] * et[k] + nu * (es[i] * et[k] + et[i] * es[k]));
					v += C * 0.5 * (1 - nu) * Ths * gm[i] * gm[k];
					v += D * Thc * (ks[i] * ks[k] + kt[i] * kt[k] + nu * (ks[i] * kt[k] + kt[i] * ks[k]));
					v += D * 0.5 * (1 - nu) * Ths * tw[i] * tw[k];
					// geometric stiffness of the residual stress
					v += Thc * (Nrr * dW[i] * dW[k] + Ntt * md * md * W[i] * W[k] / (r * r));
					Ke[i][k] += f * v;
					Me[i][k] += f * mt.rho * hh * Thc * W[i] * W[k];
				}
		}
		// transform end dofs from nodal frames: u = cos(d) t + sin(d) w, w_loc = -sin(d) t + cos(d) w
		double T[10][10] = {};
		for (int end = 0; end < 2; end++) {
			int j = e + end;
			double d = ms.phi[e] - ms.psi[j];
			double cd = std::cos(d), sd = std::sin(d);
			int o = 4 * end;
			T[o + 0][o + 0] = cd; T[o + 0][o + 2] = sd;      // u from (t, w)
			T[o + 1][o + 1] = 1;                              // v
			T[o + 2][o + 0] = -sd; T[o + 2][o + 2] = cd;     // w_loc
			T[o + 3][o + 3] = 1;                              // theta
		}
		T[8][8] = 1;
		T[9][9] = 1;
		int gdof[10] = {nodeDof(e, 0), nodeDof(e, 1), nodeDof(e, 2), nodeDof(e, 3), nodeDof(e + 1, 0), nodeDof(e + 1, 1),
		                nodeDof(e + 1, 2), nodeDof(e + 1, 3), bubDof(e, 0), bubDof(e, 1)};
		// Kg = T^T Ke T
		double KT[10][10] = {};
		for (int i = 0; i < 10; i++)
			for (int k = 0; k < 10; k++) {
				double a = 0;
				for (int l = 0; l < 10; l++) a += Ke[i][l] * T[l][k];
				KT[i][k] = a;
			}
		for (int i = 0; i < 10; i++)
			for (int k = 0; k < 10; k++) {
				double a = 0;
				for (int l = 0; l < 10; l++) a += T[l][i] * KT[l][k];
				K[gdof[i] * G + gdof[k]] += a;
				// normal mass only (in-plane inertia neglected): use nodal w ~ local w
				Mw[gdof[i] * G + gdof[k]] += Me[i][k];
			}
	}
	// in-plane support of the hole (the sleeve)
	if (!ms.clampedInner) {
		// the sleeve holds the hole's horizontal position: spring on u_r = cos(psi) t - sin(psi) w
		// (vertical translation and rocking stay free: they are the stand, not the cymbal)
		double kip = 2e7;
		double cp = std::cos(ms.psi[0]), sp = std::sin(ms.psi[0]);
		int it = nodeDof(0, 0), iw = nodeDof(0, 2);
		double kk = Thc * sh.r0 * kip;
		K[it * G + it] += kk * cp * cp;
		K[it * G + iw] -= kk * cp * sp;
		K[iw * G + it] -= kk * cp * sp;
		K[iw * G + iw] += kk * sp * sp;
		if (m > 0) K[nodeDof(0, 1) * G + nodeDof(0, 1)] += Ths * sh.r0 * kip;
	}
	// partition
	std::vector<int> kept, cond;
	for (int j = 0; j < NN; j++) {
		bool fixed = ms.clampedInner && j == 0;
		if (fixed) continue;
		kept.push_back(nodeDof(j, 2));
		kept.push_back(nodeDof(j, 3));
		cond.push_back(nodeDof(j, 0));
		if (m > 0) cond.push_back(nodeDof(j, 1));
	}
	for (int e = 0; e < E; e++) {
		cond.push_back(bubDof(e, 0));
		if (m > 0) cond.push_back(bubDof(e, 1));
	}
	int nk = (int)kept.size(), nc = (int)cond.size();
	std::vector<double> Kcc(nc * nc), Kck(nc * nk), Kkk(nk * nk), Mkk(nk * nk);
	for (int i = 0; i < nc; i++)
		for (int k = 0; k < nc; k++) Kcc[i * nc + k] = K[cond[i] * G + cond[k]];
	for (int i = 0; i < nc; i++)
		for (int k = 0; k < nk; k++) Kck[i * nk + k] = K[cond[i] * G + kept[k]];
	for (int i = 0; i < nk; i++)
		for (int k = 0; k < nk; k++) {
			Kkk[i * nk + k] = K[kept[i] * G + kept[k]];
			Mkk[i * nk + k] = Mw[kept[i] * G + kept[k]];
		}
	std::vector<double> Lc = Kcc;
	if (!cholesky(nc, Lc)) return res;
	std::vector<double> Y = Kck;     // nc x nk  -> Kcc^-1 Kck
	cholFwd(nc, Lc, Y.data(), nk);
	// Kred = Kkk - (L^-1 Kck)^T (L^-1 Kck)
	for (int i = 0; i < nk; i++)
		for (int l = 0; l < nc; l++) {
			double yli = Y[l * nk + i];
			if (yli == 0.0) continue;
			for (int k = 0; k < nk; k++) Kkk[i * nk + k] -= yli * Y[l * nk + k];
		}
	cholBwd(nc, Lc, Y.data(), nk);   // now Y = Kcc^-1 Kck
	for (int i = 0; i < nk; i++)
		for (int k = i + 1; k < nk; k++) {
			double a = 0.5 * (Kkk[i * nk + k] + Kkk[k * nk + i]);
			Kkk[i * nk + k] = Kkk[k * nk + i] = a;
		}
	std::vector<double> d, X;
	if (!genEig(nk, Kkk, Mkk, d, X)) return res;
	for (int j = 0; j < nk; j++) {
		res.lowestW2 = std::min(res.lowestW2, d[j]);
		double w2 = d[j];
		double f = w2 > 0 ? std::sqrt(w2) / (2.0 * CY_PI) : 0.0;
		if (f < fMin || f > fMax) continue;
		// full nodal vector
		std::vector<double> full(G, 0.0);
		for (int i = 0; i < nk; i++) full[kept[i]] = X[i * nk + j];
		for (int i = 0; i < nc; i++) {
			double a = 0;
			for (int k = 0; k < nk; k++) a += Y[i * nk + k] * X[k * nk + j];
			full[cond[i]] = -a;
		}
		std::vector<double> ev(E * 4);
		for (int e = 0; e < E; e++) {
			for (int end = 0; end < 2; end++) {
				int jn = e + end;
				double dd = ms.phi[e] - ms.psi[jn];
				double t = full[nodeDof(jn, 0)], wn = full[nodeDof(jn, 2)];
				ev[e * 4 + 2 * end] = -std::sin(dd) * t + std::cos(dd) * wn;
				ev[e * 4 + 2 * end + 1] = full[nodeDof(jn, 3)];
			}
		}
		res.f.push_back(f);
		res.ev.push_back(std::move(ev));
	}
	return res;
}

struct ShellRaw {
	int m;
	double f;
	int mesh;
	std::vector<double> ev;
};

} // namespace

std::shared_ptr<Model> buildModel(const GeomParams& gp, const SolverOpts& opt) {
	auto t0 = std::chrono::steady_clock::now();
	auto mdl = std::make_shared<Model>();
	Model& M = *mdl;
	M.gp = gp;
	M.fs = gp.sampleRate;
	M.sh.make(gp);
	const Shape& sh = M.sh;
	const Material& mt = sh.mat;

	// quality codes: 0 ECO, 1 HIGH, 2 ULTRA, 3 MID (added later; the codes stay as saved in patches)
	static const int CORE_N[4] = {48, 64, 96, 56};
	static const int STRESS_N[4] = {24, 40, 56, 32};
	static const int LOW_N[4] = {0, 4, 8, 4};
	static const int PAIRS_K[4] = {0, 1, 2, 1};
	static const double PAIR_FMAX[4] = {0, 3.5, 6.0, 3.5};   // x the core's top frequency
	static const double FOUT[4] = {16000, 20000, 20000, 20000};
	int q = std::max(0, std::min(3, gp.quality));
	// build up to 1.5x the output band so SIZE can grow the cymbal without a rebuild
	double fMax = opt.fMax > 0 ? opt.fMax : FOUT[q] * (gp.band > 0 ? gp.band : CY_SIZE_MAX);
	int nCoreWanted = opt.coreModes >= 0 ? opt.coreModes : CORE_N[q];
	int nStressWanted = opt.stressModes >= 0 ? opt.stressModes : STRESS_N[q];
	int nLowWanted = opt.lowStress >= 0 ? opt.lowStress : LOW_N[q];
	int pairsK = opt.pairsPerMode >= 0 ? opt.pairsPerMode : PAIRS_K[q];
	const double fMin = 20.0;

	// residual stress
	Prestress ps;
	ps.E = mt.E / (1.0 - mt.nu);   // plane-stress isotropic eigenstrain
	ps.ri = sh.r0;
	ps.ro = sh.a;
	ps.r1 = std::max(sh.rb, sh.r0 + 0.2 * (sh.a - sh.r0));
	ps.r2 = 0.8 * sh.a;
	double tension = std::max(-1.0, std::min(1.0, gp.tension));

	double kEdge;
	{
		double hh = sh.he;
		double D = mt.E * hh * hh * hh / (12.0 * (1.0 - mt.nu * mt.nu));
		double w = 2.0 * CY_PI * fMax;
		kEdge = std::pow(mt.rho * hh * w * w / D, 0.25);
	}

	std::vector<SMesh> meshes;
	std::vector<ShellRaw> raw;
	for (int attempt = 0; attempt < 6; attempt++) {
		ps.eps = 3e-4 * tension;
		meshes.clear();
		raw.clear();
		meshes.push_back(makeShellMesh(sh, sh.r0, fMax, opt.elements, false));
		bool buckled = false;
		for (int m = 0; m < 600; m++) {
			int meshIdx = 0;
			double rTurn = 0.75 * m / kEdge;
			if (rTurn > sh.r0 + 0.25 * (sh.a - sh.r0) && opt.elements <= 0) {
				meshes.push_back(makeShellMesh(sh, rTurn, fMax, 0, true));
				meshIdx = (int)meshes.size() - 1;
			}
			HarmonicResult hr = solveHarmonic(sh, meshes[meshIdx], m, fMin, fMax, ps);
			// rigid modes sit near 0; a clearly negative eigenvalue means buckling
			if (m >= 2 && hr.lowestW2 < -std::pow(2 * CY_PI * 5.0, 2)) { buckled = true; break; }
			for (size_t k = 0; k < hr.f.size(); k++) raw.push_back({m, hr.f[k], meshIdx, std::move(hr.ev[k])});
			double lowestF = hr.lowestW2 > 0 ? std::sqrt(hr.lowestW2) / (2 * CY_PI) : 0;
			if (m >= 2 && lowestF > fMax) break;
		}
		if (!buckled) break;
		tension *= 0.6;
	}
	M.tensionUsed = tension;
	std::sort(raw.begin(), raw.end(), [](const ShellRaw& x, const ShellRaw& y) { return x.f < y.f; });

	// evaluate a mode's normal displacement W and dW/dr at radius r
	auto evalW = [&](const ShellRaw& rm, double r, double& v, double& dvdr) {
		const SMesh& ms = meshes[rm.mesh];
		if (r < ms.r[0]) { v = 0; dvdr = 0; if (!ms.clampedInner) { v = rm.ev[0]; dvdr = rm.ev[1] / std::cos(ms.phi[0]); } return; }
		int e = (int)(std::upper_bound(ms.r.begin(), ms.r.end(), r) - ms.r.begin()) - 1;
		e = std::max(0, std::min(ms.E - 1, e));
		double L = ms.L[e];
		double xi = (r - ms.r[e]) / (ms.r[e + 1] - ms.r[e]);
		xi = std::min(1.0, std::max(0.0, xi));
		double H[4], dH[4], ddH[4];
		hermite(xi, L, H, dH, ddH);
		const double* x = rm.ev.data() + 4 * e;
		v = H[0] * x[0] + H[1] * x[1] + H[2] * x[2] + H[3] * x[3];
		double ds = dH[0] * x[0] + dH[1] * x[1] + dH[2] * x[2] + dH[3] * x[3];
		dvdr = ds / std::max(0.05, std::cos(ms.phi[e]));
	};

	// Radial lookup grid
	const int NG = 256;
	M.gridR.resize(NG);
	for (int i = 0; i < NG; i++) M.gridR[i] = (float)(sh.r0 + (sh.a - sh.r0) * i / (NG - 1));

	// Airy base mesh in r (shallow theory, for the nonlinear coupling)
	Mesh base = makeMesh(sh, sh.r0, std::min(fMax, 12000.0), 0);
	int NQ = (int)base.q.size();

	// core selection
	int nPairsCore = 0, countCore = 0;
	for (size_t k = 0; k < raw.size(); k++) {
		int c = raw[k].m == 0 ? 1 : 2;
		if (countCore + c > nCoreWanted) break;
		countCore += c;
		nPairsCore = (int)k + 1;
	}

	std::vector<std::vector<float>> qW(raw.size()), qWr(raw.size());
	// live TENSION: geometric stiffness of the unit residual-stress pattern, per shape
	Prestress psU = ps;
	psU.eps = 3e-4;
	std::vector<double> shapeTens(raw.size(), 0.0);
	for (size_t k = 0; k < raw.size(); k++) {
		RadialShape rs;
		rs.W.resize(NG);
		double v, d1;
		double Gk = 0;
		const double Thk = raw[k].m == 0 ? 2.0 * CY_PI : CY_PI, mk2 = (double)raw[k].m * raw[k].m;
		const double dr = (sh.a - sh.r0) / (NG - 1);
		for (int i = 0; i < NG; i++) {
			evalW(raw[k], M.gridR[i], v, d1);
			rs.W[i] = (float)v;
			double r = M.gridR[i];
			double Nrr, Ntt;
			psU.N(r, sh.h(r), Nrr, Ntt);
			double wt = (i == 0 || i == NG - 1) ? 0.5 : 1.0;
			Gk += wt * dr * r * Thk * (Nrr * d1 * d1 + Ntt * mk2 * v * v / (r * r));
		}
		double om = 2.0 * CY_PI * raw[k].f;
		shapeTens[k] = Gk / std::max(1e-9, om * om);
		evalW(raw[k], sh.r0, v, d1);
		rs.centreW = v;
		rs.centreWr = d1;
		double num = 0, den = 0;
		for (int i = 0; i < NG; i++) {
			double r = M.gridR[i];
			if (r < 0.85 * sh.a) continue;
			num += rs.W[i] * rs.W[i] * r;
			den += r;
		}
		rs.edgeE = den > 0 ? num / den : 0;
		M.shapes.push_back(std::move(rs));
		qW[k].resize(NQ);
		qWr[k].resize(NQ);
		for (int i = 0; i < NQ; i++) {
			evalW(raw[k], base.q[i].r, v, d1);
			qW[k][i] = (float)v;
			qWr[k][i] = (float)d1;
		}
		raw[k].ev.clear();
		raw[k].ev.shrink_to_fit();
	}

	for (size_t k = 0; k < raw.size(); k++) {
		Mode md;
		md.m = raw[k].m;
		md.f = raw[k].f;
		md.shape = (int)k;
		md.par = 0;
		M.modes.push_back(md);
		if (md.m > 0) {
			md.par = 1;
			M.modes.push_back(md);
		}
		if ((int)k + 1 == nPairsCore) M.nCore = (int)M.modes.size();
	}
	if (nPairsCore == 0) M.nCore = 0;
	M.tensS.assign(M.modes.size(), 0.0);
	M.fTopCore = 0;
	for (int i = 0; i < M.nCore; i++) M.fTopCore = std::max(M.fTopCore, M.modes[i].f);

	// core modes grouped by class (m, parity) so the coupling blocks address contiguous ranges
	std::stable_sort(M.modes.begin(), M.modes.begin() + M.nCore, [](const Mode& x, const Mode& y) {
		if (x.m != y.m) return x.m < y.m;
		return x.par < y.par;
	});
	M.classOf.assign(M.nCore, -1);
	{
		std::map<std::pair<int, int>, int> cmap;
		for (int i = 0; i < M.nCore; i++) {
			auto key = std::make_pair(M.modes[i].m, M.modes[i].par);
			auto it = cmap.find(key);
			int c;
			if (it == cmap.end()) {
				c = (int)M.classes.size();
				cmap[key] = c;
				M.classes.push_back({key.first, key.second, {}});
			}
			else c = it->second;
			M.classes[c].idx.push_back(i);
			M.classOf[i] = c;
		}
	}

	for (size_t i = 0; i < M.modes.size(); i++) M.tensS[i] = shapeTens[M.modes[i].shape];

	if (M.nCore > 0) {
		int lowest = 0;
		for (int i = 0; i < M.nCore; i++) if (M.modes[i].f < M.modes[lowest].f) lowest = i;
		int sidx = M.modes[lowest].shape;
		double wmax = 0;
		for (float w : M.shapes[sidx].W) wmax = std::max(wmax, (double)std::fabs(w));
		double qh = sh.he / std::max(1e-30, wmax);
		double om = 2 * CY_PI * M.modes[lowest].f;
		M.Eref = 0.5 * om * om * qh * qh;
	}

	// ---- stress modes and coupling ----
	int mMaxCore = 0;
	for (int i = 0; i < M.nCore; i++) mMaxCore = std::max(mMaxCore, M.modes[i].m);
	int sMax = 2 * mMaxCore;
	bool curved = sh.H > 0 || sh.Hb > 0 || sh.tanF != 0.0 || sh.sphR > 0;
	if (M.nCore > 0 && nStressWanted > 0) {
		struct Cand { int s; double lam; std::vector<double> v; };
		std::vector<Cand> cands;
		for (int s = 0; s <= sMax; s++) {
			std::vector<double> G, MF;
			int nF;
			airyMatrices(base, s, mt, sh.he, G, MF, nF);
			std::vector<double> d, X;
			if (!genEig(nF, G, MF, d, X)) continue;
			int keep = std::min(nF, nStressWanted);
			for (int j = 0; j < keep; j++) {
				Cand c;
				c.s = s;
				c.lam = d[j];
				c.v.resize(nF);
				for (int i = 0; i < nF; i++) c.v[i] = X[i * nF + j];
				cands.push_back(std::move(c));
			}
		}
		std::sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) { return x.lam < y.lam; });
		int count = 0;
		std::vector<const Cand*> chosen;
		for (const Cand& c : cands) {
			int cc = c.s == 0 ? 1 : 2;
			if (count + cc > nStressWanted) continue;
			chosen.push_back(&c);
			count += cc;
			if (count >= nStressWanted) break;
		}

		auto T = [](int m, int par, double t) { return par ? std::sin(m * t) : std::cos(m * t); };
		auto Tp = [](int m, int par, double t) { return par ? m * std::cos(m * t) : -m * std::sin(m * t); };
		// angular integrals over a full turn (exact for the trig degrees involved)
		auto angular = [&](int s, int ps_, int m1, int p1, int m2, int p2, double* I) {
			int NT = 4 * (s + m1 + m2) + 16;
			double dth = 2.0 * CY_PI / NT;
			I[0] = I[1] = I[2] = I[3] = 0;
			for (int j = 0; j < NT; j++) {
				double t = dth * j;
				double S = T(s, ps_, t), Sp = Tp(s, ps_, t);
				double a1 = T(m1, p1, t), a2 = T(m2, p2, t), b1 = Tp(m1, p1, t), b2 = Tp(m2, p2, t);
				I[0] += S * a1 * a2;
				I[1] += S * b1 * b2;
				I[2] += Sp * a1 * b2;
				I[3] += Sp * a2 * b1;
			}
			for (int k = 0; k < 4; k++) I[k] *= dth;
		};
		std::map<std::tuple<int, int, int, int, int, int>, std::array<double, 4>> angCache;
		auto ang = [&](int s, int ps_, int m1, int p1, int m2, int p2) {
			auto key = std::make_tuple(s, ps_, m1, p1, m2, p2);
			auto it = angCache.find(key);
			if (it != angCache.end()) return it->second;
			std::array<double, 4> I;
			angular(s, ps_, m1, p1, m2, p2, I.data());
			angCache[key] = I;
			return I;
		};

		int NC = M.nCore;
		int nModes = (int)M.modes.size();
		// modes of each m (all), for extended pair search
		std::map<int, std::vector<int>> byM;
		for (int i = 0; i < nModes; i++) byM[M.modes[i].m].push_back(i);
		int lowCount = 0;

		for (const Cand* cp : chosen) {
			for (int par = 0; par < (cp->s == 0 ? 1 : 2); par++) {
				StressMode sm;
				sm.s = cp->s;
				sm.par = par;
				sm.lambda = cp->lam;
				double isl = 1.0 / std::sqrt(cp->lam);
				std::vector<double> u(base.ndof(), 0.0);
				for (int i = 0; i < (int)cp->v.size(); i++) u[i + 2] = cp->v[i];
				std::vector<double> nrr(NQ), ntt(NQ), tt(NQ), ir(NQ);
				double s2 = (double)cp->s * cp->s;
				for (int i = 0; i < NQ; i++) {
					double v, d1, d2;
					evalAt(base.q[i], u.data(), v, d1, d2);
					double r = base.q[i].r;
					nrr[i] = (d1 / r - s2 * v / (r * r)) * base.q[i].wr;
					ntt[i] = d2 * base.q[i].wr;
					tt[i] = -(d1 / r - v / (r * r)) * base.q[i].wr;
					ir[i] = 1.0 / r;
				}
				auto pairB = [&](int ia, int ib, const std::array<double, 4>& I) {
					const float* Wp = qW[M.modes[ia].shape].data();
					const float* Wrp = qWr[M.modes[ia].shape].data();
					const float* Wq = qW[M.modes[ib].shape].data();
					const float* Wrq = qWr[M.modes[ib].shape].data();
					double R1 = 0, R2 = 0, R3 = 0, R4 = 0;
					for (int k = 0; k < NQ; k++) {
						R1 += nrr[k] * Wrp[k] * Wrq[k];
						R2 += ntt[k] * Wp[k] * Wq[k] * ir[k] * ir[k];
						R3 += tt[k] * Wrp[k] * Wq[k] * ir[k];
						R4 += tt[k] * Wrq[k] * Wp[k] * ir[k];
					}
					return 0.5 * (R1 * I[0] + R2 * I[1] + R3 * I[2] + R4 * I[3]) * isl;
				};
				if (curved) {
					for (int i = 0; i < NC; i++) {
						const Mode& mi = M.modes[i];
						if (mi.m != cp->s) continue;
						if (mi.m == 1) continue;   // shallow theory mixes rigid tilt into m = 1: no linear coupling there
						double I1 = 0, I2 = 0;
						int NT = 4 * (mi.m + cp->s) + 16;
						double dth = 2 * CY_PI / NT;
						for (int j = 0; j < NT; j++) {
							double t = dth * j;
							I1 += T(cp->s, par, t) * T(mi.m, mi.par, t);
							I2 += Tp(cp->s, par, t) * Tp(mi.m, mi.par, t);
						}
						I1 *= dth;
						I2 *= dth;
						if (std::fabs(I1) < 1e-9 && std::fabs(I2) < 1e-9) continue;
						const float* Wq = qW[mi.shape].data();
						const float* Wrq = qWr[mi.shape].data();
						double R1 = 0, R2 = 0;
						for (int k = 0; k < NQ; k++) {
							R1 += nrr[k] * base.q[k].zp * Wrq[k];
							R2 += tt[k] * base.q[k].zp * Wq[k] * ir[k];
						}
						double a = (R1 * I1 + R2 * I2) * isl;
						if (a != 0.0) {
							sm.aIdx.push_back(i);
							sm.aVal.push_back(a);
						}
					}
				}
				for (int c1 = 0; c1 < (int)M.classes.size(); c1++) {
					for (int c2 = 0; c2 < (int)M.classes.size(); c2++) {
						const auto& C1 = M.classes[c1];
						const auto& C2 = M.classes[c2];
						if (!(cp->s == C1.m + C2.m || cp->s == std::abs(C1.m - C2.m))) continue;
						auto I = ang(cp->s, par, C1.m, C1.par, C2.m, C2.par);
						if (std::fabs(I[0]) + std::fabs(I[1]) + std::fabs(I[2]) + std::fabs(I[3]) < 1e-9) continue;
						BBlock bb;
						bb.c1 = c1;
						bb.c2 = c2;
						int n1 = (int)C1.idx.size(), n2 = (int)C2.idx.size();
						bb.v.assign(n1 * n2, 0.0);
						double amax = 0;
						for (int a = 0; a < n1; a++)
							for (int b = 0; b < n2; b++) {
								double v = pairB(C1.idx[a], C2.idx[b], I);
								bb.v[a * n2 + b] = v;
								amax = std::max(amax, std::fabs(v));
							}
						if (amax > 0) sm.blocks.push_back(std::move(bb));
					}
				}
				// extended pairs (upper modes) for the lowest stress modes
				if (lowCount < nLowWanted && pairsK > 0) {
					std::map<std::pair<int, int>, double> pairs;
					double win = 2.0 * M.fTopCore;
					double fPairMax = (opt.pairFMax > 0 ? opt.pairFMax : PAIR_FMAX[q]) * M.fTopCore;
					for (int i = NC; i < nModes; i++) {
						const Mode& mi = M.modes[i];
						if (mi.f > fPairMax) break;
						std::vector<std::pair<double, int>> cand;
						for (int sign = -1; sign <= 1; sign += 2) {
							int mj = std::abs(mi.m + sign * cp->s);
							if (sign == 1 && cp->s == 0) continue;
							auto it = byM.find(mj);
							if (it == byM.end()) continue;
							for (int j : it->second) {
								const Mode& mjm = M.modes[j];
								if (std::fabs(mjm.f - mi.f) > win) continue;
								if (j < NC && i < NC) continue;
								auto I = ang(cp->s, par, mi.m, mi.par, mjm.m, mjm.par);
								if (std::fabs(I[0]) + std::fabs(I[1]) + std::fabs(I[2]) + std::fabs(I[3]) < 1e-9) continue;
								double b = pairB(i, j, I);
								cand.push_back({std::fabs(b), j});
							}
						}
						std::sort(cand.begin(), cand.end(), [](const std::pair<double, int>& x, const std::pair<double, int>& y) { return x.first > y.first; });
						for (int k = 0; k < (int)cand.size() && k < pairsK; k++) {
							int j = cand[k].second;
							int a = std::min(i, j), b = std::max(i, j);
							if (pairs.count({a, b})) continue;
							auto I = ang(cp->s, par, M.modes[a].m, M.modes[a].par, M.modes[b].m, M.modes[b].par);
							pairs[{a, b}] = pairB(a, b, I);
						}
					}
					for (auto& kv : pairs) {
						sm.pi.push_back(kv.first.first);
						sm.pj.push_back(kv.first.second);
						sm.pc.push_back(kv.first.first == kv.first.second ? kv.second : 2.0 * kv.second);
					}
					M.nUpperPairs += (int)pairs.size();
				}
				M.stress.push_back(std::move(sm));
			}
			if (lowCount < nLowWanted) lowCount += (cp->s == 0 ? 1 : 2);
		}
		// stress modes that got pairs come first in "chosen" order (lowest lambda)
		M.nLow = 0;
		for (auto& smd : M.stress) {
			if (smd.pi.empty()) break;
			M.nLow++;
		}

		// keep diag(w^2) - A^T A positive semi-definite on the core: first per mode
		// (where shallow theory overestimates, e.g. a steep bell), then globally
		if (curved && NC > 0) {
			std::vector<double> AA(NC, 0.0);
			for (auto& smd : M.stress)
				for (size_t a = 0; a < smd.aIdx.size(); a++) AA[smd.aIdx[a]] += smd.aVal[a] * smd.aVal[a];
			std::vector<double> sc(NC, 1.0);
			for (int i = 0; i < NC; i++) {
				double w2 = std::pow(2 * CY_PI * M.modes[i].f, 2);
				if (AA[i] > 0.9 * w2) sc[i] = std::sqrt(0.9 * w2 / AA[i]);
			}
			for (auto& smd : M.stress)
				for (size_t a = 0; a < smd.aIdx.size(); a++) smd.aVal[a] *= sc[smd.aIdx[a]];
		}
		if (curved && NC > 0) {
			std::vector<double> Am(NC * NC, 0.0);
			for (auto& smd : M.stress)
				for (size_t a = 0; a < smd.aIdx.size(); a++)
					for (size_t b = 0; b < smd.aIdx.size(); b++) Am[smd.aIdx[a] * NC + smd.aIdx[b]] += smd.aVal[a] * smd.aVal[b];
			// D^-1/2 Am D^-1/2
			std::vector<double> Sm(NC * NC);
			for (int i = 0; i < NC; i++)
				for (int j = 0; j < NC; j++) {
					double wi = 2 * CY_PI * M.modes[i].f, wj = 2 * CY_PI * M.modes[j].f;
					Sm[i * NC + j] = Am[i * NC + j] / (wi * wj);
				}
			std::vector<double> d, V;
			symEig(NC, Sm, d, V);
			double mu = d[NC - 1];
			M.minEigCore = 1.0 - mu;
			if (mu > 1.0) {
				M.aScale = std::sqrt(1.0 / mu) * 0.999;
				for (auto& smd : M.stress)
					for (double& a : smd.aVal) a *= M.aScale;
			}
		}
	}

	buildPickup(M);
	auto t1 = std::chrono::steady_clock::now();
	M.buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
	return mdl;
}

// Rayleigh-integral pickup for an unbaffled thin shell (dipole kernel):
//   W = int phi cos(gamma) (1 + i k R) e^{-i k R} / R^2 dS
// Mic positions sit in the plane theta = 0, so the kernel is even in theta and only
// the cos twin needs integrating (over half the circle, doubled).
void buildPickup(Model& M) {
	auto t0 = std::chrono::steady_clock::now();
	const Shape& sh = M.sh;
	double a = sh.a;
	struct P { double x, z; };
	auto zAt = [&](double rf) { double zz, zp; sh.z(sh.r0 + rf * (sh.a - sh.r0), zz, zp); return zz; };
	P pos[Model::NMIC] = {
		{0.05 * a + 0.03, zAt(0.05) + 0.06},          // BELL: 6 cm above the bell, a little off axis
		{0.50 * a, zAt(0.50) + 0.08},                  // BOW: 8 cm above mid-bow
		{0.98 * a, zAt(0.98) + 0.06},                  // EDGE: 6 cm above the rim
		{0.40, 0.90},                                  // OVERHEAD: 0.9 m up, 0.4 m aside
		{3.0 * std::cos(0.6109), 3.0 * std::sin(0.6109)},   // ROOM: 3 m at 35 degrees
	};
	const int NR = 48, NT = 48;   // NT over half the circle
	const double c0 = 343.0;
	int NS = (int)M.shapes.size();
	// frequency and angular order of each shape (cos twin)
	std::vector<double> fS(NS, 0.0);
	std::vector<int> mS(NS, 0);
	for (const Mode& md : M.modes)
		if (fS[md.shape] == 0) { fS[md.shape] = md.f; mS[md.shape] = md.m; }
	std::vector<double> pr(NR), pz(NR), pnr(NR), pnz(NR), pA(NR), rabs(NR);
	for (int j = 0; j < NR; j++) {
		double rf = (j + 0.5) / NR;
		double r = sh.r0 + rf * (sh.a - sh.r0);
		double zz, zp;
		sh.z(r, zz, zp);
		double nrm = std::sqrt(1 + zp * zp);
		rabs[j] = r;
		pr[j] = r;
		pz[j] = zz;
		pnr[j] = -zp / nrm;
		pnz[j] = 1.0 / nrm;
		pA[j] = 2.0 * r * ((sh.a - sh.r0) / NR) * nrm * (CY_PI / NT);   // x2: other half circle
	}
	// shape values on the rings
	std::vector<float> Wr((size_t)NS * NR);
	{
		int n = (int)M.gridR.size();
		double g0 = M.gridR[0], g1 = M.gridR[n - 1];
		for (int j = 0; j < NR; j++) {
			double t = (rabs[j] - g0) / (g1 - g0) * (n - 1);
			int k = std::max(0, std::min(n - 2, (int)t));
			double fr = std::min(1.0, std::max(0.0, t - k));
			for (int s = 0; s < NS; s++) Wr[(size_t)s * NR + j] = (float)(M.shapes[s].W[k] + fr * (M.shapes[s].W[k + 1] - M.shapes[s].W[k]));
		}
	}
	std::vector<double> th(NT);
	for (int t = 0; t < NT; t++) th[t] = CY_PI * (t + 0.5) / NT;
	M.micRe.assign((size_t)Model::NMIC * NS, 0.f);
	M.micIm.assign((size_t)Model::NMIC * NS, 0.f);
	std::vector<double> R(NR * NT), K(NR * NT);
	std::vector<double> cmt(NT);
	for (int p = 0; p < Model::NMIC; p++) {
		for (int j = 0; j < NR; j++)
			for (int t = 0; t < NT; t++) {
				double ct = std::cos(th[t]), st = std::sin(th[t]);
				double dx = pos[p].x - pr[j] * ct, dy = -pr[j] * st, dz = pos[p].z - pz[j];
				double Rv = std::sqrt(dx * dx + dy * dy + dz * dz);
				double cg = (dx * pnr[j] * ct + dy * pnr[j] * st + dz * pnz[j]) / Rv;
				R[j * NT + t] = Rv;
				K[j * NT + t] = cg / (Rv * Rv) * pA[j];
			}
		double pw = 0;
		for (int s = 0; s < NS; s++) {
			double kk = 2 * CY_PI * fS[s] / c0;
			int m = mS[s];
			for (int t = 0; t < NT; t++) cmt[t] = std::cos(m * th[t]);
			double re = 0, im = 0;
			for (int j = 0; j < NR; j++) {
				double w = Wr[(size_t)s * NR + j];
				if (w == 0) continue;
				double rr = 0, ii = 0;
				for (int t = 0; t < NT; t++) {
					double kr = kk * R[j * NT + t];
					double c = std::cos(kr), sn = std::sin(kr);
					// (1 + i kr) (c - i sn) = (c + kr sn) + i (kr c - sn)
					double kw = K[j * NT + t] * cmt[t];
					rr += kw * (c + kr * sn);
					ii += kw * (kr * c - sn);
				}
				re += w * rr;
				im += w * ii;
			}
			M.micRe[(size_t)p * NS + s] = (float)re;
			M.micIm[(size_t)p * NS + s] = (float)im;
			// loudness reference: unit impulse near the edge, |W| w phi(0.8) per mode
			if (fS[s] < 12000) {
				double wv = 2 * CY_PI * fS[s];
				double ph = M.shapes[s].W[(int)(0.8 * (M.gridR.size() - 1))];
				pw += (re * re + im * im) * wv * wv * ph * ph;
			}
		}
		M.micNorm[p] = pw;
	}
	double ref = M.micNorm[1];
	for (int p = 0; p < Model::NMIC; p++) M.micNorm[p] = M.micNorm[p] > 0 ? std::sqrt(ref / M.micNorm[p]) : 1.0;
	auto t1 = std::chrono::steady_clock::now();
	M.pickupMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

} // namespace cy
