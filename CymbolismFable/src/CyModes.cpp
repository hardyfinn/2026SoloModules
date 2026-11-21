// Cymbolism Fable: geometry solver. See CyModes.hpp for the model.
#include "CyModes.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <tuple>

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

std::shared_ptr<Model> buildModel(const GeomParams& gp) { return buildModel(gp, SolverOpts()); }

std::shared_ptr<Model> buildModel(const GeomParams& gp, const SolverOpts& opt) {
	auto t0 = std::chrono::steady_clock::now();
	auto mdl = std::make_shared<Model>();
	Model& M = *mdl;
	M.gp = gp;
	M.fs = gp.sampleRate;
	M.sh.make(gp);
	const Shape& sh = M.sh;
	const Material& mt = sh.mat;

	static const int CORE_N[3] = {48, 96, 160};
	static const int STRESS_N[3] = {32, 56, 96};
	static const double FMAX[3] = {15000, 19500, 20000};
	int q = std::max(0, std::min(2, gp.quality));
	double fMax = opt.fMax > 0 ? opt.fMax : std::min(FMAX[q], 0.42 * M.fs);
	int nCoreWanted = opt.coreModes >= 0 ? opt.coreModes : CORE_N[q];
	int nStressWanted = opt.stressModes >= 0 ? opt.stressModes : STRESS_N[q];
	const double fMin = 25.0;

	// Mount: line springs around the hole (per metre of circumference)
	double mnt = std::min(1.0, std::max(0.0, gp.mount));
	double kLine = opt.mountSprings ? 4e6 * std::pow(0.004, mnt) : 0.0;     // N/m^2, tight 4e6 .. loose 1.6e4
	double kRot = opt.mountSprings ? 40.0 * std::pow(0.004, mnt) : 0.0;     // N m / rad / m

	// Base mesh (all m): resolves the bending wavelength at fMax.
	std::vector<Mesh> meshes;
	meshes.push_back(makeMesh(sh, sh.r0, fMax, opt.elements));
	bool curved = opt.membrane && (sh.H > 0 || sh.Hb > 0 || sh.tanF != 0.0);

	std::vector<RawMode> raw;
	double kEdge;
	{
		double hh = sh.he;
		double D = mt.E * hh * hh * hh / (12.0 * (1.0 - mt.nu * mt.nu));
		double w = 2.0 * CY_PI * fMax;
		kEdge = std::pow(mt.rho * hh * w * w / D, 0.25);
	}

	for (int m = 0; m < 400; m++) {
		// For high m the field is evanescent inside the turning point r = m / k:
		// use a truncated mesh, clamped at its inner end.
		int meshIdx = 0;
		double rTurn = 0.75 * m / kEdge;
		bool truncated = false;
		if (rTurn > sh.r0 + 0.25 * (sh.a - sh.r0) && opt.elements <= 0) {
			meshes.push_back(makeMesh(sh, rTurn, fMax, 0));
			meshIdx = (int)meshes.size() - 1;
			truncated = true;
		}
		const Mesh& ms = meshes[meshIdx];
		int nd = ms.ndof();
		std::vector<double> K(nd * nd, 0.0), Mm(nd * nd, 0.0);
		double Th = thetaFactor(m);
		double m2 = (double)m * m;
		double nu = mt.nu;
		for (const QPt& p : ms.q) {
			double r = p.r;
			double k1[4], k2[4], tw[4];
			for (int a = 0; a < 4; a++) {
				k1[a] = p.ddN[a];
				k2[a] = p.dN[a] / r - m2 * p.N[a] / (r * r);
				tw[a] = m * (p.dN[a] / r - p.N[a] / (r * r));
			}
			for (int a = 0; a < 4; a++) {
				int ga = 2 * p.e + a;
				for (int b = 0; b < 4; b++) {
					int gb = 2 * p.e + b;
					double kb = k1[a] * k1[b] + k2[a] * k2[b] + nu * (k1[a] * k2[b] + k2[a] * k1[b]);
					if (m > 0) kb += 2.0 * (1.0 - nu) * tw[a] * tw[b];
					K[ga * nd + gb] += Th * p.D * kb * p.wr;
					Mm[ga * nd + gb] += Th * mt.rho * p.h * p.N[a] * p.N[b] * p.wr;
				}
			}
		}
		if (curved) {
			// condensed linear membrane stiffness K_mem = C^T G^-1 C
			std::vector<double> G, MF;
			int nF;
			airyMatrices(ms, m, mt, sh.he, G, MF, nF);
			std::vector<double> C(nF * nd, 0.0);
			for (const QPt& p : ms.q) {
				double r = p.r;
				double nrr[4], tt[4];
				for (int a = 0; a < 4; a++) {
					nrr[a] = p.dN[a] / r - m2 * p.N[a] / (r * r);
					tt[a] = p.dN[a] / r - p.N[a] / (r * r);
				}
				for (int a = 0; a < 4; a++) {
					int ga = 2 * p.e + a - 2;
					if (ga < 0 || ga >= nF) continue;
					for (int b = 0; b < 4; b++) {
						int gb = 2 * p.e + b;
						double v = nrr[a] * p.zp * p.dN[b] * Th;
						if (m > 0) v += (-tt[a]) * p.zp * (p.N[b] / r) * m2 * CY_PI;
						C[ga * nd + gb] += v * p.wr;
					}
				}
			}
			std::vector<double> L = G;
			if (cholesky(nF, L)) {
				std::vector<double> Y = C;   // nF x nd
				cholFwd(nF, L, Y.data(), nd);
				// K_mem = Y^T Y
				for (int i = 0; i < nd; i++)
					for (int k = 0; k < nF; k++) {
						double yki = Y[k * nd + i];
						if (yki == 0.0) continue;
						for (int j = 0; j < nd; j++) K[i * nd + j] += yki * Y[k * nd + j];
					}
			}
		}
		if (!truncated) {
			K[0] += Th * sh.r0 * kLine;
			K[1 * nd + 1] += Th * sh.r0 * kRot;
		}
		// clamp the inner end of a truncated mesh: drop dofs 0,1
		int off = truncated ? 2 : 0;
		int n = nd - off;
		std::vector<double> Kr(n * n), Mr(n * n);
		for (int i = 0; i < n; i++)
			for (int j = 0; j < n; j++) {
				Kr[i * n + j] = K[(i + off) * nd + (j + off)];
				Mr[i * n + j] = Mm[(i + off) * nd + (j + off)];
			}
		// tiny regularisation for the free-free rigid modes
		for (int i = 0; i < n; i++) Kr[i * n + i] += 1e-12 * Kr[i * n + i];
		std::vector<double> d, X;
		if (!genEig(n, Kr, Mr, d, X)) break;
		int kept = 0;
		double lowest = 1e30;
		for (int j = 0; j < n; j++) {
			double w2 = d[j];
			double f = w2 > 0 ? std::sqrt(w2) / (2.0 * CY_PI) : 0.0;
			lowest = std::min(lowest, f);
			if (f < fMin || f > fMax) continue;
			RawMode rm;
			rm.m = m;
			rm.f = f;
			rm.mesh = meshIdx;
			rm.u.assign(nd, 0.0);
			for (int i = 0; i < n; i++) rm.u[i + off] = X[i * n + j];
			raw.push_back(std::move(rm));
			kept++;
		}
		if (lowest > fMax) break;
	}

	// Sort by frequency
	std::sort(raw.begin(), raw.end(), [](const RawMode& x, const RawMode& y) { return x.f < y.f; });

	// Radial lookup grid
	const int NG = 192;
	M.gridR.resize(NG);
	for (int i = 0; i < NG; i++) M.gridR[i] = (float)(sh.r0 + (sh.a - sh.r0) * i / (NG - 1));

	// Shapes: evaluate on the grid; core shapes also kept at quadrature points
	// of the base mesh (for the coupling integrals).
	const Mesh& base = meshes[0];
	int NQ = (int)base.q.size();
	auto evalNodal = [&](const RawMode& rm, double r, double& v, double& d1) {
		const Mesh& ms = meshes[rm.mesh];
		if (r <= ms.nodes[0]) { v = 0; d1 = 0; if (rm.mesh == 0) { v = rm.u[0]; d1 = rm.u[1]; } return; }
		int e = (int)(std::upper_bound(ms.nodes.begin(), ms.nodes.end(), r) - ms.nodes.begin()) - 1;
		e = std::max(0, std::min(ms.E - 1, e));
		double L = ms.nodes[e + 1] - ms.nodes[e];
		double xi = (r - ms.nodes[e]) / L;
		double N[4], dN[4], ddN[4];
		hermite(xi, L, N, dN, ddN);
		const double* x = rm.u.data() + 2 * e;
		v = N[0] * x[0] + N[1] * x[1] + N[2] * x[2] + N[3] * x[3];
		d1 = dN[0] * x[0] + dN[1] * x[1] + dN[2] * x[2] + dN[3] * x[3];
	};

	// Core selection: lowest modes, keeping cos/sin twins together
	int nPairsCore = 0, countCore = 0;
	for (size_t k = 0; k < raw.size(); k++) {
		int c = raw[k].m == 0 ? 1 : 2;
		if (countCore + c > nCoreWanted) break;
		countCore += c;
		nPairsCore = (int)k + 1;
	}

	std::vector<std::vector<double>> qW(raw.size()), qWr(raw.size());
	for (size_t k = 0; k < raw.size(); k++) {
		RadialShape rs;
		rs.W.resize(NG);
		double v, d1;
		for (int i = 0; i < NG; i++) {
			evalNodal(raw[k], M.gridR[i], v, d1);
			rs.W[i] = (float)v;
		}
		evalNodal(raw[k], sh.r0, v, d1);
		rs.centreW = v;
		rs.centreWr = d1;
		// mean square over the outer 15% (r-weighted)
		double num = 0, den = 0;
		for (int i = 0; i < NG; i++) {
			double r = M.gridR[i];
			if (r < 0.85 * sh.a) continue;
			num += rs.W[i] * rs.W[i] * r;
			den += r;
		}
		rs.edgeE = den > 0 ? num / den : 0;
		M.shapes.push_back(std::move(rs));
		// quadrature samples for core modes and for the upper diagonal terms
		qW[k].resize(NQ);
		qWr[k].resize(NQ);
		for (int i = 0; i < NQ; i++) {
			evalNodal(raw[k], base.q[i].r, v, d1);
			qW[k][i] = v;
			qWr[k][i] = d1;
		}
	}

	// Mode list (twins as separate modes); core first
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
	M.fTopCore = M.nCore > 0 ? M.modes[M.nCore - 1].f : 0;

	// Core classes
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

	// Energy scale: lowest core mode with amplitude of one edge thickness
	if (M.nCore > 0) {
		int sidx = M.modes[0].shape;
		double wmax = 0;
		for (float w : M.shapes[sidx].W) wmax = std::max(wmax, (double)std::fabs(w));
		double qh = sh.he / std::max(1e-30, wmax);
		double om = 2 * CY_PI * M.modes[0].f;
		M.Eref = 0.5 * om * om * qh * qh;
	}

	// ---- stress modes and coupling tensors (core only) ----
	int mMaxCore = 0;
	for (int i = 0; i < M.nCore; i++) mMaxCore = std::max(mMaxCore, M.modes[i].m);
	int sMax = 2 * mMaxCore;
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

		// angular quadrature
		int NT = 4 * (sMax + 2 * mMaxCore) + 16;
		std::vector<double> th(NT);
		for (int j = 0; j < NT; j++) th[j] = 2.0 * CY_PI * j / NT;
		double dth = 2.0 * CY_PI / NT;
		auto T = [&](int m, int par, double t) { return par ? std::sin(m * t) : std::cos(m * t); };
		auto Tp = [&](int m, int par, double t) { return par ? m * std::cos(m * t) : -m * std::sin(m * t); };

		// core shapes at quadrature points
		int NC = M.nCore;
		for (const Cand* cp : chosen) {
			for (int par = 0; par < (cp->s == 0 ? 1 : 2); par++) {
				StressMode sm;
				sm.s = cp->s;
				sm.par = par;
				sm.lambda = cp->lam;
				double isl = 1.0 / std::sqrt(cp->lam);
				// radial fields of this stress mode
				std::vector<double> u(base.ndof(), 0.0);
				for (int i = 0; i < (int)cp->v.size(); i++) u[i + 2] = cp->v[i];
				std::vector<double> nrr(NQ), ntt(NQ), tt(NQ);
				double s2 = (double)cp->s * cp->s;
				for (int i = 0; i < NQ; i++) {
					double v, d1, d2;
					evalAt(base.q[i], u.data(), v, d1, d2);
					double r = base.q[i].r;
					nrr[i] = (d1 / r - s2 * v / (r * r)) * base.q[i].wr;
					ntt[i] = d2 * base.q[i].wr;
					tt[i] = -(d1 / r - v / (r * r)) * base.q[i].wr;   // N_rt = tt * S'
				}
				auto Sf = [&](double t) { return T(cp->s, par, t); };
				auto Spf = [&](double t) { return Tp(cp->s, par, t); };
				// A (linear, curvature)
				if (curved) {
					for (int i = 0; i < NC; i++) {
						const Mode& mi = M.modes[i];
						if (mi.m != cp->s) continue;
						double I1 = 0, I2 = 0;
						for (int j = 0; j < NT; j++) {
							I1 += Sf(th[j]) * T(mi.m, mi.par, th[j]);
							I2 += Spf(th[j]) * Tp(mi.m, mi.par, th[j]);
						}
						I1 *= dth;
						I2 *= dth;
						if (std::fabs(I1) < 1e-9 && std::fabs(I2) < 1e-9) continue;
						const std::vector<double>& Wq = qW[mi.shape];
						const std::vector<double>& Wrq = qWr[mi.shape];
						double R1 = 0, R2 = 0;
						for (int k = 0; k < NQ; k++) {
							R1 += nrr[k] * base.q[k].zp * Wrq[k];
							R2 += tt[k] * base.q[k].zp * Wq[k] / base.q[k].r;
						}
						double a = (R1 * I1 + R2 * I2) * isl;
						if (a != 0.0) {
							sm.aIdx.push_back(i);
							sm.aVal.push_back(a);
						}
					}
				}
				// B blocks
				for (int c1 = 0; c1 < (int)M.classes.size(); c1++) {
					for (int c2 = 0; c2 < (int)M.classes.size(); c2++) {
						const auto& C1 = M.classes[c1];
						const auto& C2 = M.classes[c2];
						int m1 = C1.m, m2 = C2.m;
						if (!(cp->s == m1 + m2 || cp->s == std::abs(m1 - m2))) continue;
						double I1 = 0, I2 = 0, I3 = 0, I4 = 0;
						for (int j = 0; j < NT; j++) {
							double t = th[j];
							double S = Sf(t), Sp = Spf(t);
							double a1 = T(m1, C1.par, t), a2 = T(m2, C2.par, t);
							double b1 = Tp(m1, C1.par, t), b2 = Tp(m2, C2.par, t);
							I1 += S * a1 * a2;
							I2 += S * b1 * b2;
							I3 += Sp * a1 * b2;
							I4 += Sp * a2 * b1;
						}
						I1 *= dth; I2 *= dth; I3 *= dth; I4 *= dth;
						if (std::fabs(I1) + std::fabs(I2) + std::fabs(I3) + std::fabs(I4) < 1e-9) continue;
						BBlock bb;
						bb.c1 = c1;
						bb.c2 = c2;
						int n1 = (int)C1.idx.size(), n2 = (int)C2.idx.size();
						bb.v.assign(n1 * n2, 0.0);
						double amax = 0;
						for (int a = 0; a < n1; a++) {
							int pa = M.modes[C1.idx[a]].shape;
							const double* Wp = qW[pa].data();
							const double* Wrp = qWr[pa].data();
							for (int b = 0; b < n2; b++) {
								int pb = M.modes[C2.idx[b]].shape;
								const double* Wq = qW[pb].data();
								const double* Wrq = qWr[pb].data();
								double R1 = 0, R2 = 0, R3 = 0, R4 = 0;
								for (int k = 0; k < NQ; k++) {
									double ir = 1.0 / base.q[k].r;
									R1 += nrr[k] * Wrp[k] * Wrq[k];
									R2 += ntt[k] * Wp[k] * Wq[k] * ir * ir;
									R3 += tt[k] * Wrp[k] * Wq[k] * ir;
									R4 += tt[k] * Wrq[k] * Wp[k] * ir;
								}
								double v = 0.5 * (R1 * I1 + R2 * I2 + R3 * I3 + R4 * I4) * isl;
								bb.v[a * n2 + b] = v;
								amax = std::max(amax, std::fabs(v));
							}
						}
						if (amax > 0) sm.blocks.push_back(std::move(bb));
					}
				}
				// upper diagonal terms (s = 0): stiffness modulation of the upper modes
				if (cp->s == 0) {
					int NU = (int)M.modes.size() - M.nCore;
					sm.bdiagUpper.assign(NU, 0.0);
					for (int i = 0; i < NU; i++) {
						const Mode& mi = M.modes[M.nCore + i];
						int pa = mi.shape;
						double R1 = 0, R2 = 0;
						for (int k = 0; k < NQ; k++) {
							double ir = 1.0 / base.q[k].r;
							R1 += nrr[k] * qWr[pa][k] * qWr[pa][k];
							R2 += ntt[k] * qW[pa][k] * qW[pa][k] * ir * ir;
						}
						double I1 = mi.m == 0 ? 2 * CY_PI : CY_PI;     // S = 1
						double I2 = mi.m == 0 ? 0.0 : (double)mi.m * mi.m * CY_PI;
						sm.bdiagUpper[i] = 0.5 * (R1 * I1 + R2 * I2) * isl;
					}
				}
				M.stress.push_back(std::move(sm));
			}
		}
	}

	auto t1 = std::chrono::steady_clock::now();
	M.buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
	return mdl;
}

} // namespace cy
