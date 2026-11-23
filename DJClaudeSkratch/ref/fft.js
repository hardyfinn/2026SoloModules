// band powers by a plain DFT over log-spaced bands (for the checks only)
function bands(x, sr) {
	const N = 1 << Math.floor(Math.log2(x.length)), re = new Float64Array(N), im = new Float64Array(N);
	for (let i = 0; i < N; i++) re[i] = x[i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / N));
	// iterative radix-2 FFT
	for (let i = 1, j = 0; i < N; i++) { let bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; } }
	for (let len = 2; len <= N; len <<= 1) {
		const ang = -2 * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang);
		for (let i = 0; i < N; i += len) { let cr = 1, ci = 0; for (let j = 0; j < len / 2; j++) {
			const ar = re[i + j], ai = im[i + j], br = re[i + j + len / 2] * cr - im[i + j + len / 2] * ci, bi = re[i + j + len / 2] * ci + im[i + j + len / 2] * cr;
			re[i + j] = ar + br; im[i + j] = ai + bi; re[i + j + len / 2] = ar - br; im[i + j + len / 2] = ai - bi;
			const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t; } }
	}
	const edges = [30, 100, 300, 1000, 3000, 6000, 10000, 13000, 16000], out = [];
	for (let b = 0; b < edges.length - 1; b++) {
		let p = 0; const a = Math.ceil(edges[b] * N / sr), z = Math.floor(edges[b + 1] * N / sr);
		for (let k = a; k < z; k++) p += re[k] * re[k] + im[k] * im[k];
		out.push({lo: edges[b], hi: edges[b + 1], p});
	}
	return out;
}
module.exports = {bands};
