// generated from skratch_core.gen.tmpl by tools/genexpr.py: do not edit
function makeCore(BUF, SRATE) {
  var pi = Math.PI, twopi = 2 * Math.PI;
  var floor = Math.floor, ceil = Math.ceil, abs = Math.abs, exp = Math.exp, cos = Math.cos, sin = Math.sin,
      tanh = Math.tanh, sqrt = Math.sqrt, pow = Math.pow, sign = Math.sign;
  function min(a, b) { return a < b ? a : b; }
  function max(a, b) { return a > b ? a : b; }
  function dim(b) { return b.frames; }
  function peek(b, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return 0; return b.ch[ch | 0][i]; }
  function poke(b, v, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return; b.ch[(ch | 0)][i] = v; }
  var P = {tempo: 120.0, playing: 0.0, songbeat: 0.0, sbseq: 0.0, qtail: 0.0, ftail: 0.0, fgen: 0.0, taustop: 0.009, tremor: 0.01, fadet: 0.0014, haverec: 0.0, recsr4: 176400.0, rlen: 0.0, level: 1.0, posset: 0.0, poscmd: 0.0, pb0a: 0.10079887, pb1a: -0.09901778, pa1a: -0.9982189, pb0b: 0.13435687, pb1b: -0.06149022, pa1b: -0.92713336, g1k: 9.886505, lb0a: 0.00223697, lb1a: 0.00300486, lb2a: 0.00223697, la1a: -1.40456695, la2a: 0.52091626, lb0b: 1.0, lb1b: -0.30111365, lb2b: 1.0, la1b: -1.39470843, la2b: 0.68395549, lb0c: 1.0, lb1c: -0.8032222, lb2c: 1.0, la1c: -1.43507501, la2c: 0.8921389};
  var H = {h_beatint: 0.0, h_prevph: 0.0, h_sbseq: 0.0, h_B: 0.0, h_wasplay: 0.0, h_jumps: 0.0, h_curgen: 0.0, h_poscmd: 0.0, h_qh: 0.0, h_fh: 0.0, h_alive: 0.0, h_act: 0.0, h_kind: 0.0, h_m: 1.0, h_i: 0.0, h_p1: 0.0, h_a: 0.0, h_k: 1.0, h_e: 1.0, h_x1: 0.0, h_x2: 0.0, h_x3: 0.0, h_v0: 0.0, h_pos: 0.0, h_v: 0.0, h_vb: 0.0, h_trph: 0.0, h_g: 0.0, h_gt: 0.0, h_sa0: 0.0, h_sb0: 0.0, h_z1a0: 0.0, h_z2a0: 0.0, h_z1b0: 0.0, h_z2b0: 0.0, h_z1c0: 0.0, h_z2c0: 0.0, h_sa1: 0.0, h_sb1: 0.0, h_z1a1: 0.0, h_z2a1: 0.0, h_z1b1: 0.0, h_z2b1: 0.0, h_z1c1: 0.0, h_z2c1: 0.0};
  function step(in1) {
    var out1 = 0, out2 = 0, QN = 0, QF = 0, FN = 0, FF = 0, SR = 0, beatint = 0, prevph = 0, B = 0, jumps = 0, curgen = 0, qh = 0, fh = 0, act = 0, kind = 0, m = 0, i = 0, p1 = 0, a = 0, sk = 0, se = 0, x1 = 0, x2 = 0, x3 = 0, v0 = 0, pos = 0, vprev = 0, vb = 0, trph = 0, g = 0, gt = 0, sa0 = 0, sb0 = 0, z1a0 = 0, z2a0 = 0, z1b0 = 0, z2b0 = 0, z1c0 = 0, z2c0 = 0, sa1 = 0, sb1 = 0, z1a1 = 0, z2a1 = 0, z1b1 = 0, z2b1 = 0, z1c1 = 0, z2c1 = 0, ph = 0, cand = 0, Bn = 0, base = 0, fbase = 0, ended = 0, u = 0, sh = 0, v = 0, kk = 0, kn = 0, tw = 0, wn = 0, env = 0, dp = 0, y0 = 0, y1 = 0, fr1 = 0, pj = 0, vj = 0, ix = 0, i0 = 0, fx = 0, x = 0, t1 = 0, t2 = 0, l1 = 0, l2 = 0, l3 = 0, fr2 = 0, fr3 = 0, fr4 = 0, frate = 0, outg = 0, alive = 0;























QN = 0; QF = 0; FN = 0; FF = 0; SR = 0; beatint = 0; prevph = 0; B = 0;
jumps = 0; curgen = 0; qh = 0; fh = 0; act = 0; kind = 0; m = 0; i = 0;
p1 = 0; a = 0; sk = 0; se = 0; x1 = 0; x2 = 0; x3 = 0; v0 = 0;
pos = 0; vprev = 0; vb = 0; trph = 0; g = 0; gt = 0; sa0 = 0; sb0 = 0;
z1a0 = 0; z2a0 = 0; z1b0 = 0; z2b0 = 0; z1c0 = 0; z2c0 = 0; sa1 = 0; sb1 = 0;
z1a1 = 0; z2a1 = 0; z1b1 = 0; z2b1 = 0; z1c1 = 0; z2c1 = 0; ph = 0; cand = 0;
Bn = 0; base = 0; fbase = 0; ended = 0; u = 0; sh = 0; v = 0; kk = 0;
kn = 0; tw = 0; wn = 0; env = 0; dp = 0; y0 = 0; y1 = 0; fr1 = 0;
pj = 0; vj = 0; ix = 0; i0 = 0; fx = 0; x = 0; t1 = 0; t2 = 0;
l1 = 0; l2 = 0; l3 = 0; fr2 = 0; fr3 = 0; fr4 = 0; frate = 0; outg = 0;
alive = 0;












QN = 256;
QF = 12;
FN = 512;
FF = 4;
SR = SRATE;
beatint = H.h_beatint; prevph = H.h_prevph; B = H.h_B; jumps = H.h_jumps; curgen = H.h_curgen;
qh = H.h_qh; fh = H.h_fh;
act = H.h_act; kind = H.h_kind; m = H.h_m; i = H.h_i; p1 = H.h_p1; a = H.h_a; sk = H.h_k; se = H.h_e;
x1 = H.h_x1; x2 = H.h_x2; x3 = H.h_x3; v0 = H.h_v0; pos = H.h_pos; vprev = H.h_v; vb = H.h_vb; trph = H.h_trph;
g = H.h_g; gt = H.h_gt;
sa0 = H.h_sa0; sb0 = H.h_sb0;
z1a0 = H.h_z1a0; z2a0 = H.h_z2a0; z1b0 = H.h_z1b0; z2b0 = H.h_z2b0; z1c0 = H.h_z1c0; z2c0 = H.h_z2c0;
sa1 = H.h_sa1; sb1 = H.h_sb1;
z1a1 = H.h_z1a1; z2a1 = H.h_z2a1; z1b1 = H.h_z1b1; z2b1 = H.h_z2b1; z1c1 = H.h_z1c1; z2c1 = H.h_z2c1;


ph = in1;
if (P.playing > 0.5) {
	if (ph < prevph - 0.5) {
		beatint = beatint + 1;
	}
	if (P.sbseq != H.h_sbseq) {
		cand = floor(P.songbeat - ph + 0.5);
		if (abs(cand - beatint) > 0.5) {
			beatint = cand;
		}
	}
	Bn = beatint + ph;
	if (H.h_wasplay < 0.5 || abs(Bn - B) > 0.05) {
		jumps = jumps + 1;
	}
	B = Bn;
} else {
	B = B + P.tempo / (60 * SR);
}
prevph = ph;


if (P.fgen != curgen) {
	curgen = P.fgen;
	act = 0;
	vb = vprev;
	gt = 0;
}


if (P.poscmd != H.h_poscmd) {
	pos = P.posset;
	act = 0;
	vb = 0;
	vprev = 0;
}


if (qh < P.qtail) {
	base = (qh - floor(qh / QN) * QN) * QF;
	if (peek(BUF.q, base + 10, 0) != curgen) {
		qh = qh + 1;
	} else {
		if (peek(BUF.q, base, 0) <= B) {
			kind = peek(BUF.q, base + 1, 0);
			m = max(1, floor(peek(BUF.q, base + 2, 0) * SR + 0.5));
			p1 = peek(BUF.q, base + 3, 0);
			a = peek(BUF.q, base + 4, 0);
			sk = peek(BUF.q, base + 5, 0);
			se = peek(BUF.q, base + 6, 0);
			x1 = peek(BUF.q, base + 7, 0);
			x2 = peek(BUF.q, base + 8, 0);
			x3 = peek(BUF.q, base + 9, 0);
			v0 = vprev;
			i = 0;
			act = 1;
			qh = qh + 1;
		}
	}
}


if (fh < P.ftail) {
	fbase = (fh - floor(fh / FN) * FN) * FF;
	if (peek(BUF.fq, fbase + 2, 0) != curgen) {
		fh = fh + 1;
	} else {
		if (peek(BUF.fq, fbase, 0) <= B) {
			gt = peek(BUF.fq, fbase + 1, 0);
			fh = fh + 1;
		}
	}
}


ended = 0;
if (act > 0.5) {
	u = (i + 0.5) / m;
	if (kind == 1) {
		sh = sin(pi * pow(u, sk));
		if (se != 1) {
			sh = pow(max(sh, 0), se);
		}
		v = a * sh;
	} else {
		if (kind == 2) {
			v = a + (v0 - a) * exp(-(i + 1) / (sk * SR));
		} else {
			if (kind == 3) {
				kk = i / m;
				kn = (i + 1) / m;
				tw = twopi * x1;
				wn = (kn + se * (1 - cos(tw * kn)) / tw) / (1 + se * (1 - cos(tw)) / tw);
				env = min(1, min(kk, 1 - kk) * 12);
				v = a * sin(twopi * sk * wn) * env - x2;
			} else {
				v = v0 * exp(-(i + 1) / (sk * SR));
			}
		}
	}
	vb = v;
	i = i + 1;
	if (i >= m) {
		act = 0;
		ended = 1;
	}
} else {
	if (abs(vb) > 0.001) {
		vb = vb * exp(-1 / (P.taustop * SR));
	} else {
		vb = 0;
	}
	v = vb + P.tremor * 0.2 * sin(trph);
	trph = trph + twopi * 8.5 / SR;
	if (trph > twopi) {
		trph = trph - twopi;
	}
}


dp = vprev / SR;
y0 = 0;
y1 = 0;
fr1 = 1 * 0.25;
pj = pos + fr1 * dp;
vj = vprev + fr1 * (v - vprev);
ix = min(max(pj * P.recsr4, 0), max(P.rlen - 2, 0));
i0 = floor(ix);
fx = ix - i0;
x = (peek(BUF.cut, i0, 0) * (1 - fx) + peek(BUF.cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa0;
sa0 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb0;
sb0 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a0;
z1a0 = P.lb1a * t2 - P.la1a * l1 + z2a0;
z2a0 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b0;
z1b0 = P.lb1b * l1 - P.la1b * l2 + z2b0;
z2b0 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c0;
z1c0 = P.lb1c * l2 - P.la1c * l3 + z2c0;
z2c0 = P.lb2c * l2 - P.la2c * l3;
y0 = l3;
x = (peek(BUF.cut, i0, 1) * (1 - fx) + peek(BUF.cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa1;
sa1 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb1;
sb1 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a1;
z1a1 = P.lb1a * t2 - P.la1a * l1 + z2a1;
z2a1 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b1;
z1b1 = P.lb1b * l1 - P.la1b * l2 + z2b1;
z2b1 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c1;
z1c1 = P.lb1c * l2 - P.la1c * l3 + z2c1;
z2c1 = P.lb2c * l2 - P.la2c * l3;
y1 = l3;
fr2 = 2 * 0.25;
pj = pos + fr2 * dp;
vj = vprev + fr2 * (v - vprev);
ix = min(max(pj * P.recsr4, 0), max(P.rlen - 2, 0));
i0 = floor(ix);
fx = ix - i0;
x = (peek(BUF.cut, i0, 0) * (1 - fx) + peek(BUF.cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa0;
sa0 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb0;
sb0 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a0;
z1a0 = P.lb1a * t2 - P.la1a * l1 + z2a0;
z2a0 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b0;
z1b0 = P.lb1b * l1 - P.la1b * l2 + z2b0;
z2b0 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c0;
z1c0 = P.lb1c * l2 - P.la1c * l3 + z2c0;
z2c0 = P.lb2c * l2 - P.la2c * l3;
y0 = l3;
x = (peek(BUF.cut, i0, 1) * (1 - fx) + peek(BUF.cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa1;
sa1 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb1;
sb1 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a1;
z1a1 = P.lb1a * t2 - P.la1a * l1 + z2a1;
z2a1 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b1;
z1b1 = P.lb1b * l1 - P.la1b * l2 + z2b1;
z2b1 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c1;
z1c1 = P.lb1c * l2 - P.la1c * l3 + z2c1;
z2c1 = P.lb2c * l2 - P.la2c * l3;
y1 = l3;
fr3 = 3 * 0.25;
pj = pos + fr3 * dp;
vj = vprev + fr3 * (v - vprev);
ix = min(max(pj * P.recsr4, 0), max(P.rlen - 2, 0));
i0 = floor(ix);
fx = ix - i0;
x = (peek(BUF.cut, i0, 0) * (1 - fx) + peek(BUF.cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa0;
sa0 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb0;
sb0 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a0;
z1a0 = P.lb1a * t2 - P.la1a * l1 + z2a0;
z2a0 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b0;
z1b0 = P.lb1b * l1 - P.la1b * l2 + z2b0;
z2b0 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c0;
z1c0 = P.lb1c * l2 - P.la1c * l3 + z2c0;
z2c0 = P.lb2c * l2 - P.la2c * l3;
y0 = l3;
x = (peek(BUF.cut, i0, 1) * (1 - fx) + peek(BUF.cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa1;
sa1 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb1;
sb1 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a1;
z1a1 = P.lb1a * t2 - P.la1a * l1 + z2a1;
z2a1 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b1;
z1b1 = P.lb1b * l1 - P.la1b * l2 + z2b1;
z2b1 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c1;
z1c1 = P.lb1c * l2 - P.la1c * l3 + z2c1;
z2c1 = P.lb2c * l2 - P.la2c * l3;
y1 = l3;
fr4 = 4 * 0.25;
pj = pos + fr4 * dp;
vj = vprev + fr4 * (v - vprev);
ix = min(max(pj * P.recsr4, 0), max(P.rlen - 2, 0));
i0 = floor(ix);
fx = ix - i0;
x = (peek(BUF.cut, i0, 0) * (1 - fx) + peek(BUF.cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa0;
sa0 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb0;
sb0 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a0;
z1a0 = P.lb1a * t2 - P.la1a * l1 + z2a0;
z2a0 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b0;
z1b0 = P.lb1b * l1 - P.la1b * l2 + z2b0;
z2b0 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c0;
z1c0 = P.lb1c * l2 - P.la1c * l3 + z2c0;
z2c0 = P.lb2c * l2 - P.la2c * l3;
y0 = l3;
x = (peek(BUF.cut, i0, 1) * (1 - fx) + peek(BUF.cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;
t1 = P.pb0a * x + sa1;
sa1 = P.pb1a * x - P.pa1a * t1;
t2 = P.pb0b * t1 + sb1;
sb1 = P.pb1b * t1 - P.pa1b * t2;
l1 = P.lb0a * t2 + z1a1;
z1a1 = P.lb1a * t2 - P.la1a * l1 + z2a1;
z2a1 = P.lb2a * t2 - P.la2a * l1;
l2 = P.lb0b * l1 + z1b1;
z1b1 = P.lb1b * l1 - P.la1b * l2 + z2b1;
z2b1 = P.lb2b * l1 - P.la2b * l2;
l3 = P.lb0c * l2 + z1c1;
z1c1 = P.lb1c * l2 - P.la1c * l3 + z2c1;
z2c1 = P.lb2c * l2 - P.la2c * l3;
y1 = l3;
pos = pos + dp;
if (ended > 0.5) {
	pos = p1 - v / SR;
}


frate = 1 / (P.fadet * SR);
if (g < gt) {
	g = min(gt, g + frate);
} else {
	g = max(gt, g - frate);
}

outg = g * P.g1k * P.level * P.haverec;
out1 = y0 * outg;
out2 = y1 * outg;


alive = H.h_alive + 1;
poke(BUF.st, B, 0, 0);
poke(BUF.st, P.playing, 1, 0);
poke(BUF.st, qh, 2, 0);
poke(BUF.st, fh, 3, 0);
poke(BUF.st, pos, 4, 0);
poke(BUF.st, v, 5, 0);
poke(BUF.st, g, 6, 0);
poke(BUF.st, alive, 7, 0);
poke(BUF.st, SR, 8, 0);
poke(BUF.st, jumps, 9, 0);
poke(BUF.st, act, 10, 0);
poke(BUF.st, curgen, 11, 0);


H.h_beatint = beatint; H.h_prevph = prevph; H.h_sbseq = P.sbseq; H.h_B = B; H.h_wasplay = P.playing; H.h_jumps = jumps;
H.h_curgen = curgen; H.h_poscmd = P.poscmd; H.h_qh = qh; H.h_fh = fh; H.h_alive = alive;
H.h_act = act; H.h_kind = kind; H.h_m = m; H.h_i = i; H.h_p1 = p1; H.h_a = a; H.h_k = sk; H.h_e = se;
H.h_x1 = x1; H.h_x2 = x2; H.h_x3 = x3; H.h_v0 = v0; H.h_pos = pos; H.h_v = v; H.h_vb = vb; H.h_trph = trph;
H.h_g = g; H.h_gt = gt;
H.h_sa0 = sa0; H.h_sb0 = sb0;
H.h_z1a0 = z1a0; H.h_z2a0 = z2a0; H.h_z1b0 = z1b0; H.h_z2b0 = z2b0; H.h_z1c0 = z1c0; H.h_z2c0 = z2c0;
H.h_sa1 = sa1; H.h_sb1 = sb1;
H.h_z1a1 = z1a1; H.h_z2a1 = z2a1; H.h_z1b1 = z1b1; H.h_z2b1 = z2b1; H.h_z1c1 = z1c1; H.h_z2c1 = z2c1;

    return [out1, out2];
  }
  return {P: P, H: H, step: step};
}
if (typeof module !== "undefined") module.exports = makeCore;