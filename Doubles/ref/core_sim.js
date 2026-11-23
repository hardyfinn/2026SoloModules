// generated from doubles_core.gen.tmpl by tools/genexpr.py: do not edit
function makeCore(BUF, SRATE, rnd) {
  var pi = Math.PI, twopi = 2 * Math.PI;
  var floor = Math.floor, ceil = Math.ceil, abs = Math.abs, exp = Math.exp, cos = Math.cos, sin = Math.sin,
      tanh = Math.tanh, sqrt = Math.sqrt, pow = Math.pow, sign = Math.sign;
  function min(a, b) { return a < b ? a : b; }
  function max(a, b) { return a > b ? a : b; }
  function wrap(x, lo, hi) { var r = hi - lo; var y = (x - lo) % r; if (y < 0) y += r; return y + lo; }
  function noise() { return rnd() * 2 - 1; }
  function dim(b) { return b.frames; }
  function peek(b, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return 0; return b.ch[ch | 0][i]; }
  function poke(b, v, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return; b.ch[(ch | 0)][i] = v; }
  var P = {tempo: 120.0, playing: 0.0, songbeat: 0.0, sbseq: 0.0, reccmd: 0.0, recmode: 0.0, reclen: 8.0, recstop: 0.0, haverec: 0.0, L: 1.0, Lb: 8.0, anchor: 0.0, timing: 0.5, weight: 0.5, slop: 0.5, risk: 0.5, wear: 0.5, level: 1.0, strlev: 1.0, brake: 1.0, qt0: 0.0, qt1: 0.0, fgen: 0.0, fbeat: 0.0, bootb: -1000000000.0, dragseq: 0.0, dragdepth: 0.35};
  var H = {h_beatint: 0.0, h_prevph: 0.0, h_sbseq: 0.0, h_wasplay: 0.0, h_curgen: 0.0, h_flushb: 0.0, h_reccmd: 0.0, h_recstate: 0.0, h_wi: 0.0, h_recstart: 0.0, h_recb: 0.0, h_prevb: 0.0, h_statc: 0.0, h_live: 0.0, h_dragseq: 0.0, h_alive: 0.0, h_pos0: 0.0, h_v0: 0.0, h_vb0: 0.0, h_mode0: 0.0, h_k0: 0.0, h_has0: 0.0, h_bs0: 0.0, h_be0: 1000000000.0, h_p00: 0.0, h_rate0: 1.0, h_kind0: 0.0, h_gm0: 0.0, h_gd0: 1.0, h_g00: 0.0, h_amp0: 1.0, h_str0: 0.0, h_gen0: 0.0, h_cued0: 0.0, h_pdc0: 0.0, h_pa0: 0.0, h_pdv0: 0.0, h_trrem0: 0.0, h_push0: 0.0, h_rk0: 0.0, h_exp0: 0.0, h_expok0: 0.0, h_cuchk0: 0.0, h_culen0: 0.0, h_cuk0: 0.0, h_cua0: 0.0, h_spm0: 1.0, h_spa0: 0.0, h_spdv0: 0.0, h_trem0: 0.0, h_gt0: 0.0, h_g0: 0.0, h_gamp0: 1.0, h_popen0: 1000000000.0, h_pclose0: 1000000000.0, h_slot0: 0.0, h_drk0: 0.0, h_drlen0: 0.0, h_drd0: 0.0, h_late0: 0.0, h_flub0: 0.0, h_e0: 0.0, h_wl0: 0.0, h_wr0: 0.0, h_head0: 0.0, h_brk0: 0.0, h_idl0: 0.0, h_pcs0: 0.0, h_scn0: 0.0, h_scns0: 1.0, h_sctot0: 0.0, h_sctr0: 0.0, h_scoff0: 0.0, h_sck0: 0.0, h_sclen0: 0.0, h_sca0: 0.0, h_scj0: 0.0, h_pos1: 0.0, h_v1: 0.0, h_vb1: 0.0, h_mode1: 0.0, h_k1: 0.0, h_has1: 0.0, h_bs1: 0.0, h_be1: 1000000000.0, h_p01: 0.0, h_rate1: 1.0, h_kind1: 0.0, h_gm1: 0.0, h_gd1: 1.0, h_g01: 0.0, h_amp1: 1.0, h_str1: 0.0, h_gen1: 0.0, h_cued1: 0.0, h_pdc1: 0.0, h_pa1: 0.0, h_pdv1: 0.0, h_trrem1: 0.0, h_push1: 0.0, h_rk1: 0.0, h_exp1: 0.0, h_expok1: 0.0, h_cuchk1: 0.0, h_culen1: 0.0, h_cuk1: 0.0, h_cua1: 0.0, h_spm1: 1.0, h_spa1: 0.0, h_spdv1: 0.0, h_trem1: 0.0, h_gt1: 0.0, h_g1: 0.0, h_gamp1: 1.0, h_popen1: 1000000000.0, h_pclose1: 1000000000.0, h_slot1: 0.0, h_drk1: 0.0, h_drlen1: 0.0, h_drd1: 0.0, h_late1: 0.0, h_flub1: 0.0, h_e1: 0.0, h_wl1: 0.0, h_wr1: 0.0, h_head1: 0.0, h_brk1: 0.0, h_idl1: 0.0, h_pcs1: 0.0, h_scn1: 0.0, h_scns1: 1.0, h_sctot1: 0.0, h_sctr1: 0.0, h_scoff1: 0.0, h_sck1: 0.0, h_sclen1: 0.0, h_sca1: 0.0, h_scj1: 0.0};
  function step(in1, in2, in3) {
    var out1 = 0, out2 = 0, NONE = 0, SR = 0, beatint = 0, prevph = 0, curgen = 0, flushb = 0, recstate = 0, wi = 0, recstart = 0, recb = 0, prevb = 0, live = 0, pos0 = 0, v0 = 0, vb0 = 0, mode0 = 0, k0 = 0, has0 = 0, bs0 = 0, be0 = 0, p00 = 0, rate0 = 0, kind0 = 0, gm0 = 0, gd0 = 0, g00 = 0, amp0 = 0, str0 = 0, gen0 = 0, cued0 = 0, pdc0 = 0, pa0 = 0, pdv0 = 0, trrem0 = 0, push0 = 0, rk0 = 0, exp0 = 0, expok0 = 0, cuchk0 = 0, culen0 = 0, cuk0 = 0, cua0 = 0, spm0 = 0, spa0 = 0, spdv0 = 0, trem0 = 0, gt0 = 0, g0 = 0, gamp0 = 0, popen0 = 0, pclose0 = 0, slot0 = 0, drk0 = 0, drlen0 = 0, drd0 = 0, late0 = 0, flub0 = 0, e0 = 0, wl0 = 0, wr0 = 0, head0 = 0, brk0 = 0, idl0 = 0, pcs0 = 0, scn0 = 0, scns0 = 0, sctot0 = 0, sctr0 = 0, scoff0 = 0, sck0 = 0, sclen0 = 0, sca0 = 0, scj0 = 0, pos1 = 0, v1 = 0, vb1 = 0, mode1 = 0, k1 = 0, has1 = 0, bs1 = 0, be1 = 0, p01 = 0, rate1 = 0, kind1 = 0, gm1 = 0, gd1 = 0, g01 = 0, amp1 = 0, str1 = 0, gen1 = 0, cued1 = 0, pdc1 = 0, pa1 = 0, pdv1 = 0, trrem1 = 0, push1 = 0, rk1 = 0, exp1 = 0, expok1 = 0, cuchk1 = 0, culen1 = 0, cuk1 = 0, cua1 = 0, spm1 = 0, spa1 = 0, spdv1 = 0, trem1 = 0, gt1 = 0, g1 = 0, gamp1 = 0, popen1 = 0, pclose1 = 0, slot1 = 0, drk1 = 0, drlen1 = 0, drd1 = 0, late1 = 0, flub1 = 0, e1 = 0, wl1 = 0, wr1 = 0, head1 = 0, brk1 = 0, idl1 = 0, pcs1 = 0, scn1 = 0, scns1 = 0, sctot1 = 0, sctr1 = 0, scoff1 = 0, sck1 = 0, sclen1 = 0, sca1 = 0, scj1 = 0, ph = 0, jumped = 0, cand = 0, B = 0, spb = 0, started = 0, stopped = 0, tau = 0, taus = 0, dk = 0, dks = 0, pre0 = 0, pushsd = 0, vmax = 0, reach = 0, reljit = 0, rush = 0, fadjit = 0, fadearly = 0, fadlate = 0, cuebase = 0, cueper = 0, tremsd = 0, lvl = 0, fade = 0, flubp = 0, flubsd = 0, rrate = 0, recspb = 0, trema = 0, tremb = 0, done = 0, active = 0, flushed = 0, dragnew = 0, rbs = 0, boot = 0, qt = 0, need_next = 0, base = 0, e_bs = 0, e_be = 0, e_p0 = 0, e_kind = 0, e_cm = 0, e_rate = 0, e_gm = 0, e_gd = 0, e_g0 = 0, e_amp = 0, e_dist = 0, e_gen = 0, e_str = 0, e_scn = 0, e_scd = 0, keep = 0, cut = 0, gg = 0, cont = 0, oldbe = 0, latej = 0, earlyj = 0, gap = 0, j = 0, s0 = 0, pre = 0, adv = 0, d0 = 0, cerr = 0, prel = 0, fast = 0, jit = 0, tlen = 0, tr = 0, dist = 0, need = 0, ns = 0, np = 0, dc = 0, sd = 0, nb = 0, w = 0, trav = 0, u = 0, rate_e = 0, vv = 0, err = 0, vplay = 0, waitrel = 0, a = 0, gend = 0, gain0 = 0, yl0 = 0, yr0 = 0, ip = 0, fr = 0, i0 = 0, i1 = 0, i2 = 0, i3 = 0, xm = 0, x0 = 0, x1 = 0, x2 = 0, c1 = 0, c2 = 0, c3 = 0, hw = 0, ja = 0, jb = 0, wa = 0, gain1 = 0, yl1 = 0, yr1 = 0, mixl = 0, mixr = 0, kn = 0, statc = 0;














NONE = 0; SR = 0; beatint = 0; prevph = 0; curgen = 0; flushb = 0; recstate = 0; wi = 0;
recstart = 0; recb = 0; prevb = 0; live = 0; pos0 = 0; v0 = 0; vb0 = 0; mode0 = 0;
k0 = 0; has0 = 0; bs0 = 0; be0 = 0; p00 = 0; rate0 = 0; kind0 = 0; gm0 = 0;
gd0 = 0; g00 = 0; amp0 = 0; str0 = 0; gen0 = 0; cued0 = 0; pdc0 = 0; pa0 = 0;
pdv0 = 0; trrem0 = 0; push0 = 0; rk0 = 0; exp0 = 0; expok0 = 0; cuchk0 = 0; culen0 = 0;
cuk0 = 0; cua0 = 0; spm0 = 0; spa0 = 0; spdv0 = 0; trem0 = 0; gt0 = 0; g0 = 0;
gamp0 = 0; popen0 = 0; pclose0 = 0; slot0 = 0; drk0 = 0; drlen0 = 0; drd0 = 0; late0 = 0;
flub0 = 0; e0 = 0; wl0 = 0; wr0 = 0; head0 = 0; brk0 = 0; idl0 = 0; pcs0 = 0;
scn0 = 0; scns0 = 0; sctot0 = 0; sctr0 = 0; scoff0 = 0; sck0 = 0; sclen0 = 0; sca0 = 0;
scj0 = 0; pos1 = 0; v1 = 0; vb1 = 0; mode1 = 0; k1 = 0; has1 = 0; bs1 = 0;
be1 = 0; p01 = 0; rate1 = 0; kind1 = 0; gm1 = 0; gd1 = 0; g01 = 0; amp1 = 0;
str1 = 0; gen1 = 0; cued1 = 0; pdc1 = 0; pa1 = 0; pdv1 = 0; trrem1 = 0; push1 = 0;
rk1 = 0; exp1 = 0; expok1 = 0; cuchk1 = 0; culen1 = 0; cuk1 = 0; cua1 = 0; spm1 = 0;
spa1 = 0; spdv1 = 0; trem1 = 0; gt1 = 0; g1 = 0; gamp1 = 0; popen1 = 0; pclose1 = 0;
slot1 = 0; drk1 = 0; drlen1 = 0; drd1 = 0; late1 = 0; flub1 = 0; e1 = 0; wl1 = 0;
wr1 = 0; head1 = 0; brk1 = 0; idl1 = 0; pcs1 = 0; scn1 = 0; scns1 = 0; sctot1 = 0;
sctr1 = 0; scoff1 = 0; sck1 = 0; sclen1 = 0; sca1 = 0; scj1 = 0; ph = 0; jumped = 0;
cand = 0; B = 0; spb = 0; started = 0; stopped = 0; tau = 0; taus = 0; dk = 0;
dks = 0; pre0 = 0; pushsd = 0; vmax = 0; reach = 0; reljit = 0; rush = 0; fadjit = 0;
fadearly = 0; fadlate = 0; cuebase = 0; cueper = 0; tremsd = 0; lvl = 0; fade = 0; flubp = 0;
flubsd = 0; rrate = 0; recspb = 0; trema = 0; tremb = 0; done = 0; active = 0; flushed = 0;
dragnew = 0; rbs = 0; boot = 0; qt = 0; need_next = 0; base = 0; e_bs = 0; e_be = 0;
e_p0 = 0; e_kind = 0; e_cm = 0; e_rate = 0; e_gm = 0; e_gd = 0; e_g0 = 0; e_amp = 0;
e_dist = 0; e_gen = 0; e_str = 0; e_scn = 0; e_scd = 0; keep = 0; cut = 0; gg = 0;
cont = 0; oldbe = 0; latej = 0; earlyj = 0; gap = 0; j = 0; s0 = 0; pre = 0;
adv = 0; d0 = 0; cerr = 0; prel = 0; fast = 0; jit = 0; tlen = 0; tr = 0;
dist = 0; need = 0; ns = 0; np = 0; dc = 0; sd = 0; nb = 0; w = 0;
trav = 0; u = 0; rate_e = 0; vv = 0; err = 0; vplay = 0; waitrel = 0; a = 0;
gend = 0; gain0 = 0; yl0 = 0; yr0 = 0; ip = 0; fr = 0; i0 = 0; i1 = 0;
i2 = 0; i3 = 0; xm = 0; x0 = 0; x1 = 0; x2 = 0; c1 = 0; c2 = 0;
c3 = 0; hw = 0; ja = 0; jb = 0; wa = 0; gain1 = 0; yl1 = 0; yr1 = 0;
mixl = 0; mixr = 0; kn = 0; statc = 0;























NONE = 1000000000;
SR = SRATE;
beatint = H.h_beatint; prevph = H.h_prevph; curgen = H.h_curgen; flushb = H.h_flushb;
recstate = H.h_recstate; wi = H.h_wi; recstart = H.h_recstart; recb = H.h_recb; prevb = H.h_prevb;
live = H.h_live;
pos0 = H.h_pos0; v0 = H.h_v0; vb0 = H.h_vb0; mode0 = H.h_mode0; k0 = H.h_k0; has0 = H.h_has0;
bs0 = H.h_bs0; be0 = H.h_be0; p00 = H.h_p00; rate0 = H.h_rate0; kind0 = H.h_kind0; gm0 = H.h_gm0;
gd0 = H.h_gd0; g00 = H.h_g00; amp0 = H.h_amp0; str0 = H.h_str0; gen0 = H.h_gen0; cued0 = H.h_cued0;
pdc0 = H.h_pdc0; pa0 = H.h_pa0; pdv0 = H.h_pdv0; trrem0 = H.h_trrem0; push0 = H.h_push0; rk0 = H.h_rk0;
exp0 = H.h_exp0; expok0 = H.h_expok0; cuchk0 = H.h_cuchk0; culen0 = H.h_culen0; cuk0 = H.h_cuk0; cua0 = H.h_cua0;
spm0 = H.h_spm0; spa0 = H.h_spa0; spdv0 = H.h_spdv0; trem0 = H.h_trem0;
gt0 = H.h_gt0; g0 = H.h_g0; gamp0 = H.h_gamp0; popen0 = H.h_popen0; pclose0 = H.h_pclose0; slot0 = H.h_slot0;
drk0 = H.h_drk0; drlen0 = H.h_drlen0; drd0 = H.h_drd0; late0 = H.h_late0; flub0 = H.h_flub0; e0 = H.h_e0;
wl0 = H.h_wl0; wr0 = H.h_wr0; head0 = H.h_head0; brk0 = H.h_brk0; idl0 = H.h_idl0; pcs0 = H.h_pcs0;
scn0 = H.h_scn0; scns0 = H.h_scns0; sctot0 = H.h_sctot0; sctr0 = H.h_sctr0; scoff0 = H.h_scoff0;
sck0 = H.h_sck0; sclen0 = H.h_sclen0; sca0 = H.h_sca0; scj0 = H.h_scj0;
pos1 = H.h_pos1; v1 = H.h_v1; vb1 = H.h_vb1; mode1 = H.h_mode1; k1 = H.h_k1; has1 = H.h_has1;
bs1 = H.h_bs1; be1 = H.h_be1; p01 = H.h_p01; rate1 = H.h_rate1; kind1 = H.h_kind1; gm1 = H.h_gm1;
gd1 = H.h_gd1; g01 = H.h_g01; amp1 = H.h_amp1; str1 = H.h_str1; gen1 = H.h_gen1; cued1 = H.h_cued1;
pdc1 = H.h_pdc1; pa1 = H.h_pa1; pdv1 = H.h_pdv1; trrem1 = H.h_trrem1; push1 = H.h_push1; rk1 = H.h_rk1;
exp1 = H.h_exp1; expok1 = H.h_expok1; cuchk1 = H.h_cuchk1; culen1 = H.h_culen1; cuk1 = H.h_cuk1; cua1 = H.h_cua1;
spm1 = H.h_spm1; spa1 = H.h_spa1; spdv1 = H.h_spdv1; trem1 = H.h_trem1;
gt1 = H.h_gt1; g1 = H.h_g1; gamp1 = H.h_gamp1; popen1 = H.h_popen1; pclose1 = H.h_pclose1; slot1 = H.h_slot1;
drk1 = H.h_drk1; drlen1 = H.h_drlen1; drd1 = H.h_drd1; late1 = H.h_late1; flub1 = H.h_flub1; e1 = H.h_e1;
wl1 = H.h_wl1; wr1 = H.h_wr1; head1 = H.h_head1; brk1 = H.h_brk1; idl1 = H.h_idl1; pcs1 = H.h_pcs1;
scn1 = H.h_scn1; scns1 = H.h_scns1; sctot1 = H.h_sctot1; sctr1 = H.h_sctr1; scoff1 = H.h_scoff1;
sck1 = H.h_sck1; sclen1 = H.h_sclen1; sca1 = H.h_sca1; scj1 = H.h_scj1;


ph = in3;
if (P.playing > 0.5) {
	if (ph < prevph - 0.5) {
		beatint = beatint + 1;
	}
}
prevph = ph;
jumped = 0;
if (P.sbseq != H.h_sbseq) {
	cand = floor(P.songbeat - ph + 0.5);
	if (abs(cand - beatint) > 0.5) {
		if (P.playing > 0.5) {
			jumped = 1;
		}
		beatint = cand;
	}
}
B = beatint + ph;
spb = SR * 60 / P.tempo;
started = 0;
stopped = 0;
if (P.playing > 0.5 && H.h_wasplay < 0.5) {
	started = 1;
}
if (P.playing < 0.5 && H.h_wasplay > 0.5) {
	stopped = 1;
}


tau = SR * (0.002 + 0.018 * P.weight);
taus = SR * (0.003 + 0.012 * P.weight);
dk = exp(-1 / tau);
dks = exp(-1 / taus);
pre0 = tau * (3.2 - 1.2 * P.weight);
pushsd = 0.10 * P.weight;
vmax = 6 / max(P.weight, 0.05);
reach = SR * 0.045 * P.weight;
reljit = SR * 0.007 * P.timing;
rush = SR * 0.002 * P.timing;
fadjit = SR * 0.003 * P.timing;
fadearly = SR * 0.002 * P.slop;
fadlate = SR * 0.004 * P.slop;
cuebase = SR * 0.002 * P.slop;
cueper = 0.012 * P.slop;
tremsd = 0.03 * P.slop;
lvl = 0.07 * P.slop;
fade = SR * (0.0008 + 0.0012 * P.slop);
flubp = 0.05 * P.risk;
flubsd = SR * 0.035;
rrate = P.L * P.tempo / (P.Lb * 60 * SR);
recspb = P.L / P.Lb;
trema = exp(-twopi * 100 / SR);
tremb = sqrt(1 - trema * trema);


if (P.reccmd != H.h_reccmd) {
	if (P.recmode > 0.5) {
		recstate = 1;
	} else {
		recstate = 0;
	}
}
if (recstate == 1 && P.playing > 0.5) {
	if (floor(B / 4) > floor(prevb / 4) && B - prevb < 1) {
		recstate = 2;
		wi = 0;
		recstart = floor(B / 4 + 0.5) * 4;
	}
}
if (recstate == 2) {
	if (P.playing < 0.5) {
		recstate = 0;
	} else {
		poke(BUF.rec, in1, wi, 0);
		poke(BUF.rec, in2, wi, 1);
		wi = wi + 1;
		done = 0;
		if (P.reclen > 0 && B - recstart >= P.reclen) {
			done = 1;
			recb = P.reclen;
		}
		if (P.reclen <= 0 && P.recstop > 0.5 && floor(B / 4) > floor(prevb / 4) && B - recstart > 3.5) {
			done = 1;
			recb = floor((B - recstart) / 4 + 0.5) * 4;
		}
		if (wi >= dim(BUF.rec) - 1) {
			done = 1;
			recb = max(4, floor((B - recstart) / 4) * 4);
		}
		if (done > 0.5) {
			recstate = 3;
		}
	}
}
prevb = B;

active = 0;
if (P.haverec > 0.5 && recstate != 1 && recstate != 2) {
	active = 1;
}


flushed = 0;
if (P.fgen != curgen) {
	curgen = P.fgen;
	flushb = P.fbeat;
	poke(BUF.gcut, P.fbeat, wrap(curgen, 0, 256), 0);
	flushed = 1;
}


dragnew = 0;
if (P.dragseq != H.h_dragseq) {
	dragnew = 1;
}

rbs = wrap(B - P.anchor, 0, P.Lb);


if (stopped > 0.5 || jumped > 0.5 || active < 0.5) {
	head0 = P.qt0;
	has0 = 0;
	popen0 = NONE;
	pclose0 = NONE;
	culen0 = 0;
	expok0 = 0;
	scn0 = 0;
	if (stopped > 0.5 && P.brake > 0.5 && gt0 > 0.5) {
		mode0 = 6;
		brk0 = floor(0.15 * SR);
	} else {
		if (mode0 != 6) {
			mode0 = 0;
			gt0 = 0;
		}
	}
}
if (mode0 == 6) {
	brk0 = brk0 - 1;
	if (brk0 <= 0) {
		gt0 = 0;
		mode0 = 0;
	}
}


if (flushed > 0.5 && has0 > 0.5 && gen0 >= 0 && gen0 < curgen) {
	if (bs0 >= flushb) {
		has0 = 0;
		popen0 = NONE;
		scn0 = 0;
		if (mode0 == 1 || mode0 == 2) {
			mode0 = 0;
		}
		if (mode0 == 7) {
			mode0 = 0;
			pclose0 = B;
		}
	} else {
		if (be0 > flushb) {
			be0 = flushb;
			if (pclose0 > flushb + fadlate / spb) {
				pclose0 = flushb + fadlate / spb;
			}
			if (popen0 > flushb) {
				popen0 = NONE;
			}
		}
	}
}


boot = 0;
if (B < P.bootb - 0.02) {
	boot = 1;
}
if (0 == 0 && active > 0.5 && P.playing > 0.5 && has0 < 0.5 && mode0 != 1 && mode0 != 2 && ((head0 >= P.qt0 && has1 < 0.5 && head1 >= P.qt1) || boot > 0.5)) {
	has0 = 1;
	bs0 = B; be0 = NONE;
	if (boot > 0.5) {
		be0 = P.bootb;
		pclose0 = P.bootb;
	} p00 = rbs * recspb; rate0 = 1; kind0 = 0; gm0 = 0; amp0 = 1; str0 = 1; gen0 = -1;
	pos0 = rbs * recspb;
	v0 = 1; vb0 = 1; mode0 = 3; rk0 = 0; cued0 = 0;
	exp0 = pos0; expok0 = 1; cuchk0 = floor(0.35 * SR); culen0 = 0;
	gt0 = 1; popen0 = NONE; pclose0 = NONE;
	gamp0 = P.strlev;
	live = 0;
}

qt = P.qt0;
need_next = 0;
if (active > 0.5 && P.playing > 0.5) {
	if (has0 < 0.5 || B >= be0) {
		need_next = 1;
	}
}
if (need_next > 0.5) {
	if (head0 < qt) {
		base = (0 * 64 + wrap(head0, 0, 64)) * 16;
		e_bs = peek(BUF.q, base, 0);
		e_be = peek(BUF.q, base + 1, 0);
		e_p0 = peek(BUF.q, base + 2, 0);
		e_kind = peek(BUF.q, base + 3, 0);
		e_cm = peek(BUF.q, base + 4, 0);
		e_rate = peek(BUF.q, base + 5, 0);
		e_gm = peek(BUF.q, base + 6, 0);
		e_gd = peek(BUF.q, base + 7, 0);
		e_g0 = peek(BUF.q, base + 8, 0);
		e_amp = peek(BUF.q, base + 9, 0);
		e_dist = peek(BUF.q, base + 10, 0);
		e_gen = peek(BUF.q, base + 11, 0);
		e_str = peek(BUF.q, base + 12, 0);
		e_scn = peek(BUF.q, base + 13, 0);
		e_scd = peek(BUF.q, base + 14, 0);
		head0 = head0 + 1;
		keep = 1;
		if (e_gen < curgen) {
			cut = NONE;
			for (gg = max(e_gen + 1, curgen - 255); gg <= curgen; gg = gg + 1) {
				cut = min(cut, peek(BUF.gcut, wrap(gg, 0, 256), 0));
			}
			if (e_bs >= cut) {
				keep = 0;
			}
			e_be = min(e_be, cut);
		}
		if (keep > 0.5) {
			cont = 0;
			if (e_cm == 2 && has0 > 0.5 && mode0 == 3 && abs(be0 - e_bs) < 0.0001) {
				cont = 1;
			}
			oldbe = be0;
			has0 = 1;
			bs0 = e_bs; be0 = e_be; p00 = e_p0; rate0 = e_rate; kind0 = e_kind;
			gm0 = e_gm; gd0 = max(e_gd, 0.001); g00 = e_g0; amp0 = e_amp; str0 = e_str; gen0 = e_gen;
			if (kind0 == 2) {
				p00 = e_dist;
			}
			latej = (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
			earlyj = (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
			gap = max((bs0 - B) * spb, 0);
			if (cont > 0.5) {
				
				if (gm0 < 0.5) {
					if (pclose0 < NONE) {
						pclose0 = be0 + latej;
					}
					if (gt0 < 0.5 && popen0 >= NONE) {
						popen0 = B;
						slot0 = 0;
					}
				} else {
					j = floor((B - g00) / gd0 + 0.0001);
					if (wrap(j, 0, 2) == gm0 - 1) {
						slot0 = j;
						if (gt0 > 0.5) {
							pclose0 = min(be0, g00 + (j + 1) * gd0) + latej;
							pcs0 = 1;
						} else {
							popen0 = B;
						}
					} else {
						slot0 = j - 1;
						if (gt0 > 0.5) {
							pclose0 = B + latej;
							pcs0 = 1;
						} else {
							s0 = g00 + (j + 1) * gd0;
							if (s0 < be0) {
								popen0 = s0 - earlyj;
								slot0 = j + 1;
							}
						}
					}
				}
			} else {
				pcs0 = 0;
				culen0 = 0;
				expok0 = 0;
				cued0 = 0;
				scn0 = 0;
				
				if (gm0 < 0.5) {
					popen0 = bs0 - earlyj;
					slot0 = 0;
				} else {
					j = ceil((bs0 - g00) / gd0 - 0.0001);
					if (wrap(j, 0, 2) != gm0 - 1) {
						j = j + 1;
					}
					s0 = g00 + j * gd0;
					if (s0 < be0) {
						popen0 = s0 - earlyj;
					} else {
						popen0 = NONE;
					}
					slot0 = j;
				}
				if ((kind0 == 0 && e_cm != 1) || kind0 == 3) {
					
					
					
					if (kind0 == 0) {
						cued0 = 1;
					} else {
						gap = max((be0 - B) * spb, 0);
						popen0 = NONE;
					}
					pre = min(pre0, floor(gap / 2));
					if (kind0 == 3) {
						pre = 0;
					}
					adv = rate0 * rrate * (pre - tau * (1 - exp(-pre / tau)));
					d0 = abs(wrap(p00 - adv - pos0, -P.L / 2, P.L / 2));
					cerr = 0;
					if (d0 > 8) {
						cerr = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * min(cuebase + cueper * d0, 0.03 * SR);
					}
					prel = p00 - adv + cerr;
					fast = min(0.25 * SR / max(gap, 1), 2);
					e0 = 0.6 * e0 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * reljit * 0.8;
					jit = e0 * (1 + 0.5 * fast) - rush * fast;
					if (noise() * 0.5 + 0.5 < flubp) {
						jit = jit + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * flubsd;
						flub0 = flub0 + 1;
					}
					tlen = max((be0 - B) * spb, 1);
					tr = gap - pre + jit;
					tr = min(max(tr, 0), tlen - 1);
					dist = wrap(prel - pos0, -P.L / 2, P.L / 2);
					need = 0;
					if (abs(dist) > 8) {
						need = reach + abs(dist) / rrate / vmax;
					}
					if (tr < need) {
						tr = min(need, tlen - 1);
						if (kind0 == 0) {
							late0 = late0 + 1;
						}
					}
					
					if (e_scn > 0.5 && gm0 < 0.5) {
						ns = max(floor(e_scd * spb), 8);
						np = min(floor(e_scn + 0.5), floor((tr - need - 0.02 * SR) / (2 * ns)));
						if (np >= 1) {
							scn0 = 2 * np;
							scns0 = ns;
							sctot0 = 2 * np * ns;
							sctr0 = e_dist;
							scoff0 = 0;
							sck0 = 0;
							sclen0 = 0;
							popen0 = NONE;
						}
					}
					dc = 0;
					if (need > 0) {
						dc = floor(min(need, tr - sctot0 * min(scn0, 1)));
					}
					if (dc < 4) {
						pos0 = wrap(pos0 + dist, 0, P.L);
						if (abs(dist) > 8) {
							v0 = 0;
						}
						mode0 = 2;
					} else {
						sd = v0 * dks * (1 - pow(dks, dc)) / (1 - dks);
						pa0 = (dist / rrate - sd) / ((dc + 1) / 2);
						pdv0 = v0;
						pdc0 = dc;
						k0 = 0;
						mode0 = 1;
					}
					trrem0 = tr;
					push0 = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * pushsd;
					if (kind0 == 0) {
						exp0 = wrap(p00 - rate0 * rrate * gap, 0, P.L);
						expok0 = 1;
					}
				} else {
					if (kind0 == 0) {
						
						trrem0 = gap;
						mode0 = 2;
					}
					if (kind0 == 1) {
						trrem0 = gap;
						mode0 = 2;
					}
					if (kind0 == 2) {
						trrem0 = gap;
						mode0 = 2;
						spm0 = max(floor((be0 - bs0) * spb * 0.85), 16);
					}
				}
			}
		}
	} else {
		if (has0 > 0.5 && B >= be0) {
			has0 = 0;
			if (mode0 == 3) {
				mode0 = 0;
			}
		}
	}
}

if (0 == 0 && be0 >= NONE && has0 > 0.5) {
	nb = NONE;
	if (head0 < qt) {
		nb = min(nb, peek(BUF.q, (0 * 64 + wrap(head0, 0, 64)) * 16, 0));
	}
	if (head1 < P.qt1) {
		nb = min(nb, peek(BUF.q, (64 + wrap(head1, 0, 64)) * 16, 0));
	}
	if (nb < NONE) {
		be0 = max(nb, B);
		pclose0 = be0 + fadlate / spb;
	}
}


if (mode0 == 1) {
	w = 0.5 - 0.5 * cos(twopi * (k0 + 1) / (pdc0 + 1));
	pdv0 = pdv0 * dks;
	v0 = pdv0 + pa0 * w;
	k0 = k0 + 1;
	trrem0 = trrem0 - 1;
	if (k0 >= pdc0) {
		v0 = 0;
		mode0 = 2;
	}
} else {
	if (mode0 == 2) {
		v0 = v0 * dks;
		trrem0 = trrem0 - 1;
		if (scn0 > 0.5 && trrem0 <= sctot0) {
			
			mode0 = 7;
			sck0 = 0;
			sclen0 = 0;
			popen0 = B;
		}
		if (mode0 == 2 && trrem0 <= 0) {
			if (kind0 == 3) {
				mode0 = 0;
			}
			if (kind0 == 1) {
				mode0 = 4;
			}
			if (kind0 == 0 || kind0 == 2) {
				if (kind0 == 2) {
					spdv0 = v0;
					k0 = 0;
					mode0 = 5;
				} else {
					vb0 = v0;
					rk0 = 0;
					mode0 = 3;
					cuchk0 = floor(0.35 * SR);
				}
			}
		}
	}
}
if (mode0 == 7) {
	
	
	if (sck0 >= sclen0) {
		if (scn0 < 0.5) {
			v0 = 0;
			if (kind0 == 0) {
				vb0 = 0;
				rk0 = 0;
				mode0 = 3;
				cuchk0 = floor(0.35 * SR);
			} else {
				mode0 = 0;
			}
		} else {
			if (wrap(scn0, 0, 2) < 0.5) {
				
				scj0 = min(max(((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * reljit * 0.5, -0.2 * scns0), 0.2 * scns0);
				sclen0 = scns0 + floor(scj0);
				trav = sctr0 * max(1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * 0.25 * P.slop, 0.4);
			} else {
				
				sclen0 = scns0 - floor(scj0);
				trav = -scoff0 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * min(cuebase + cueper * abs(scoff0), 0.03 * SR);
			}
			scoff0 = scoff0 + trav;
			
			
			if (trav > 0) {
				sca0 = trav / rrate / (0.6560 * sclen0);
			} else {
				sca0 = trav / rrate / (0.6366 * sclen0);
			}
			sck0 = 0;
			scn0 = scn0 - 1;
		}
	}
	if (mode0 == 7) {
		u = (sck0 + 0.5) / sclen0;
		if (sca0 > 0) {
			u = pow(u, 0.75);
		}
		v0 = sca0 * sin(pi * u);
		sck0 = sck0 + 1;
		trrem0 = trrem0 - 1;
	}
}
if (mode0 == 5 && k0 == 0) {
	
	sd = spdv0 * dks * (1 - pow(dks, spm0)) / (1 - dks);
	spa0 = (p00 / rrate - sd) / (spm0 * 0.2475168);
}
if (mode0 == 5) {
	u = (k0 + 0.5) / spm0;
	spdv0 = spdv0 * dks;
	v0 = spdv0 + spa0 * exp(-3 * u) * (1 - exp(-60 * u)) * sqrt(max(1 - u, 0));
	k0 = k0 + 1;
	if (k0 >= spm0) {
		v0 = 0;
		mode0 = 0;
	}
}
if (mode0 == 4) {
	trem0 = trema * trem0 + tremb * ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678);
	v0 = v0 * dks + trem0 * tremsd;
	if (B >= be0) {
		mode0 = 0;
	}
}
if (mode0 == 0 || mode0 == 6) {
	if (mode0 == 6) {
		v0 = v0 * exp(-1 / (0.04 * SR));
	} else {
		v0 = v0 * dks;
	}
}

if (mode0 == 0 && has0 < 0.5 && g0 <= 0 && active > 0.5 && P.playing > 0.5) {
	idl0 = idl0 + 1;
	if (idl0 > 0.25 * SR) {
		pos0 = rbs * recspb;
		v0 = 1;
	}
} else {
	idl0 = 0;
}
if (mode0 == 3) {
	rate_e = rate0;
	if (0 == 1) {
		rate_e = rate0 * (1 + 0.0012 * P.wear);
	}
	vb0 = vb0 + (rate_e - vb0) * (1 - dk);
	vv = vb0;
	if (cued0 > 0.5) {
		vv = vv + rate_e * push0 * (exp(-rk0 / (2.5 * tau)) - exp(-rk0 / (0.6 * tau)));
	}
	rk0 = rk0 + 1;
	
	if (expok0 > 0.5) {
		cuchk0 = cuchk0 - 1;
		if (cuchk0 <= 0) {
			err = wrap(exp0 - pos0, -P.L / 2, P.L / 2);
			if (abs(err) > 0.0003 * SR * rrate) {
				culen0 = floor(0.6 * SR);
				cuk0 = 0;
				cua0 = 0.8 * err / rrate / ((culen0 + 1) / 2);
				cuchk0 = culen0 + floor(0.25 * SR);
			} else {
				cuchk0 = floor(2 * SR);
			}
		}
	}
	if (culen0 > 0) {
		vv = vv + cua0 * (0.5 - 0.5 * cos(twopi * (cuk0 + 1) / (culen0 + 1)));
		cuk0 = cuk0 + 1;
		if (cuk0 >= culen0) {
			culen0 = 0;
		}
	}
	v0 = vv;
}
if (expok0 > 0.5) {
	exp0 = wrap(exp0 + rate0 * rrate, 0, P.L);
}


if (dragnew > 0.5 && live == 0) {
	drlen0 = floor(0.30 * SR);
	drk0 = 0;
	drd0 = P.dragdepth;
}
vplay = v0;
if (drlen0 > 0) {
	vplay = v0 * (1 - drd0 * (0.5 - 0.5 * cos(twopi * drk0 / drlen0)));
	drk0 = drk0 + 1;
	if (drk0 >= drlen0) {
		drlen0 = 0;
	}
}
pos0 = wrap(pos0 + vplay * rrate, 0, P.L);



waitrel = 0;
if (has0 > 0.5 && cued0 > 0.5 && kind0 == 0 && (mode0 == 1 || mode0 == 2)) {
	waitrel = 1;
}
if (B >= popen0 && waitrel < 0.5) {
	popen0 = NONE;
	if (has0 > 0.5) {
		gt0 = 1;
		live = 0;
		a = amp0;
		if (str0 > 0.5) {
			a = a * P.strlev;
		}
		gamp0 = a * min(1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * lvl, 1.12);
		gend = be0;
		if (gm0 > 0.5) {
			gend = min(be0, g00 + (slot0 + 1) * gd0);
		}
		if (kind0 == 2) {
			gend = min(be0, bs0 + (be0 - bs0) * 0.85);
		}
		pclose0 = gend + (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
		pcs0 = 1;
	}
}
if (B >= pclose0) {
	pclose0 = NONE;
	gt0 = 0;
	if (gm0 > 0.5 && has0 > 0.5 && pcs0 > 0.5) {
		slot0 = slot0 + 2;
		s0 = g00 + slot0 * gd0;
		if (s0 < be0) {
			popen0 = s0 - (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
		}
	}
}
if (gt0 > 0.5) {
	g0 = min(g0 + 1 / fade, 1);
} else {
	g0 = max(g0 - 1 / fade, 0);
}
gain0 = gamp0 * (0.5 - 0.5 * cos(pi * g0));


yl0 = 0;
yr0 = 0;
if (g0 > 0 && active > 0.5) {
	ip = floor(pos0);
	fr = pos0 - ip;
	i0 = wrap(ip - 1, 0, P.L); i1 = wrap(ip, 0, P.L); i2 = wrap(ip + 1, 0, P.L); i3 = wrap(ip + 2, 0, P.L);
	xm = peek(BUF.rec, i0, 0); x0 = peek(BUF.rec, i1, 0); x1 = peek(BUF.rec, i2, 0); x2 = peek(BUF.rec, i3, 0);
	c1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);
	yl0 = ((c3 * fr + c2) * fr + c1) * fr + x0;
	xm = peek(BUF.rec, i0, 1); x0 = peek(BUF.rec, i1, 1); x1 = peek(BUF.rec, i2, 1); x2 = peek(BUF.rec, i3, 1);
	c1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);
	yr0 = ((c3 * fr + c2) * fr + c1) * fr + x0;
	if (abs(vplay * rrate) > 2) {
		
		hw = abs(vplay * rrate) * 0.5;
		ja = wrap(floor(pos0 - hw), 0, P.L); jb = wrap(floor(pos0 + hw), 0, P.L);
		yl0 = 0.5 * yl0 + 0.25 * (peek(BUF.rec, ja, 0) + peek(BUF.rec, jb, 0));
		yr0 = 0.5 * yr0 + 0.25 * (peek(BUF.rec, ja, 1) + peek(BUF.rec, jb, 1));
	}
	if (0 == 1) {
		wa = exp(-twopi * (20000 - 9000 * P.wear) / SR);
		wl0 = (1 - wa) * yl0 + wa * wl0;
		wr0 = (1 - wa) * yr0 + wa * wr0;
		yl0 = wl0 * (1 - 0.06 * P.wear);
		yr0 = wr0 * (1 - 0.06 * P.wear);
	}
}

if (stopped > 0.5 || jumped > 0.5 || active < 0.5) {
	head1 = P.qt1;
	has1 = 0;
	popen1 = NONE;
	pclose1 = NONE;
	culen1 = 0;
	expok1 = 0;
	scn1 = 0;
	if (stopped > 0.5 && P.brake > 0.5 && gt1 > 0.5) {
		mode1 = 6;
		brk1 = floor(0.15 * SR);
	} else {
		if (mode1 != 6) {
			mode1 = 0;
			gt1 = 0;
		}
	}
}
if (mode1 == 6) {
	brk1 = brk1 - 1;
	if (brk1 <= 0) {
		gt1 = 0;
		mode1 = 0;
	}
}


if (flushed > 0.5 && has1 > 0.5 && gen1 >= 0 && gen1 < curgen) {
	if (bs1 >= flushb) {
		has1 = 0;
		popen1 = NONE;
		scn1 = 0;
		if (mode1 == 1 || mode1 == 2) {
			mode1 = 0;
		}
		if (mode1 == 7) {
			mode1 = 0;
			pclose1 = B;
		}
	} else {
		if (be1 > flushb) {
			be1 = flushb;
			if (pclose1 > flushb + fadlate / spb) {
				pclose1 = flushb + fadlate / spb;
			}
			if (popen1 > flushb) {
				popen1 = NONE;
			}
		}
	}
}


boot = 0;
if (B < P.bootb - 0.02) {
	boot = 1;
}
if (1 == 0 && active > 0.5 && P.playing > 0.5 && has1 < 0.5 && mode1 != 1 && mode1 != 2 && ((head1 >= P.qt1 && has1 < 0.5 && head1 >= P.qt1) || boot > 0.5)) {
	has1 = 1;
	bs1 = B; be1 = NONE;
	if (boot > 0.5) {
		be1 = P.bootb;
		pclose1 = P.bootb;
	} p01 = rbs * recspb; rate1 = 1; kind1 = 0; gm1 = 0; amp1 = 1; str1 = 1; gen1 = -1;
	pos1 = rbs * recspb;
	v1 = 1; vb1 = 1; mode1 = 3; rk1 = 0; cued1 = 0;
	exp1 = pos1; expok1 = 1; cuchk1 = floor(0.35 * SR); culen1 = 0;
	gt1 = 1; popen1 = NONE; pclose1 = NONE;
	gamp1 = P.strlev;
	live = 0;
}

qt = P.qt1;
need_next = 0;
if (active > 0.5 && P.playing > 0.5) {
	if (has1 < 0.5 || B >= be1) {
		need_next = 1;
	}
}
if (need_next > 0.5) {
	if (head1 < qt) {
		base = (1 * 64 + wrap(head1, 0, 64)) * 16;
		e_bs = peek(BUF.q, base, 0);
		e_be = peek(BUF.q, base + 1, 0);
		e_p0 = peek(BUF.q, base + 2, 0);
		e_kind = peek(BUF.q, base + 3, 0);
		e_cm = peek(BUF.q, base + 4, 0);
		e_rate = peek(BUF.q, base + 5, 0);
		e_gm = peek(BUF.q, base + 6, 0);
		e_gd = peek(BUF.q, base + 7, 0);
		e_g0 = peek(BUF.q, base + 8, 0);
		e_amp = peek(BUF.q, base + 9, 0);
		e_dist = peek(BUF.q, base + 10, 0);
		e_gen = peek(BUF.q, base + 11, 0);
		e_str = peek(BUF.q, base + 12, 0);
		e_scn = peek(BUF.q, base + 13, 0);
		e_scd = peek(BUF.q, base + 14, 0);
		head1 = head1 + 1;
		keep = 1;
		if (e_gen < curgen) {
			cut = NONE;
			for (gg = max(e_gen + 1, curgen - 255); gg <= curgen; gg = gg + 1) {
				cut = min(cut, peek(BUF.gcut, wrap(gg, 0, 256), 0));
			}
			if (e_bs >= cut) {
				keep = 0;
			}
			e_be = min(e_be, cut);
		}
		if (keep > 0.5) {
			cont = 0;
			if (e_cm == 2 && has1 > 0.5 && mode1 == 3 && abs(be1 - e_bs) < 0.0001) {
				cont = 1;
			}
			oldbe = be1;
			has1 = 1;
			bs1 = e_bs; be1 = e_be; p01 = e_p0; rate1 = e_rate; kind1 = e_kind;
			gm1 = e_gm; gd1 = max(e_gd, 0.001); g01 = e_g0; amp1 = e_amp; str1 = e_str; gen1 = e_gen;
			if (kind1 == 2) {
				p01 = e_dist;
			}
			latej = (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
			earlyj = (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
			gap = max((bs1 - B) * spb, 0);
			if (cont > 0.5) {
				
				if (gm1 < 0.5) {
					if (pclose1 < NONE) {
						pclose1 = be1 + latej;
					}
					if (gt1 < 0.5 && popen1 >= NONE) {
						popen1 = B;
						slot1 = 0;
					}
				} else {
					j = floor((B - g01) / gd1 + 0.0001);
					if (wrap(j, 0, 2) == gm1 - 1) {
						slot1 = j;
						if (gt1 > 0.5) {
							pclose1 = min(be1, g01 + (j + 1) * gd1) + latej;
							pcs1 = 1;
						} else {
							popen1 = B;
						}
					} else {
						slot1 = j - 1;
						if (gt1 > 0.5) {
							pclose1 = B + latej;
							pcs1 = 1;
						} else {
							s0 = g01 + (j + 1) * gd1;
							if (s0 < be1) {
								popen1 = s0 - earlyj;
								slot1 = j + 1;
							}
						}
					}
				}
			} else {
				pcs1 = 0;
				culen1 = 0;
				expok1 = 0;
				cued1 = 0;
				scn1 = 0;
				
				if (gm1 < 0.5) {
					popen1 = bs1 - earlyj;
					slot1 = 0;
				} else {
					j = ceil((bs1 - g01) / gd1 - 0.0001);
					if (wrap(j, 0, 2) != gm1 - 1) {
						j = j + 1;
					}
					s0 = g01 + j * gd1;
					if (s0 < be1) {
						popen1 = s0 - earlyj;
					} else {
						popen1 = NONE;
					}
					slot1 = j;
				}
				if ((kind1 == 0 && e_cm != 1) || kind1 == 3) {
					
					
					
					if (kind1 == 0) {
						cued1 = 1;
					} else {
						gap = max((be1 - B) * spb, 0);
						popen1 = NONE;
					}
					pre = min(pre0, floor(gap / 2));
					if (kind1 == 3) {
						pre = 0;
					}
					adv = rate1 * rrate * (pre - tau * (1 - exp(-pre / tau)));
					d0 = abs(wrap(p01 - adv - pos1, -P.L / 2, P.L / 2));
					cerr = 0;
					if (d0 > 8) {
						cerr = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * min(cuebase + cueper * d0, 0.03 * SR);
					}
					prel = p01 - adv + cerr;
					fast = min(0.25 * SR / max(gap, 1), 2);
					e1 = 0.6 * e1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * reljit * 0.8;
					jit = e1 * (1 + 0.5 * fast) - rush * fast;
					if (noise() * 0.5 + 0.5 < flubp) {
						jit = jit + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * flubsd;
						flub1 = flub1 + 1;
					}
					tlen = max((be1 - B) * spb, 1);
					tr = gap - pre + jit;
					tr = min(max(tr, 0), tlen - 1);
					dist = wrap(prel - pos1, -P.L / 2, P.L / 2);
					need = 0;
					if (abs(dist) > 8) {
						need = reach + abs(dist) / rrate / vmax;
					}
					if (tr < need) {
						tr = min(need, tlen - 1);
						if (kind1 == 0) {
							late1 = late1 + 1;
						}
					}
					
					if (e_scn > 0.5 && gm1 < 0.5) {
						ns = max(floor(e_scd * spb), 8);
						np = min(floor(e_scn + 0.5), floor((tr - need - 0.02 * SR) / (2 * ns)));
						if (np >= 1) {
							scn1 = 2 * np;
							scns1 = ns;
							sctot1 = 2 * np * ns;
							sctr1 = e_dist;
							scoff1 = 0;
							sck1 = 0;
							sclen1 = 0;
							popen1 = NONE;
						}
					}
					dc = 0;
					if (need > 0) {
						dc = floor(min(need, tr - sctot1 * min(scn1, 1)));
					}
					if (dc < 4) {
						pos1 = wrap(pos1 + dist, 0, P.L);
						if (abs(dist) > 8) {
							v1 = 0;
						}
						mode1 = 2;
					} else {
						sd = v1 * dks * (1 - pow(dks, dc)) / (1 - dks);
						pa1 = (dist / rrate - sd) / ((dc + 1) / 2);
						pdv1 = v1;
						pdc1 = dc;
						k1 = 0;
						mode1 = 1;
					}
					trrem1 = tr;
					push1 = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * pushsd;
					if (kind1 == 0) {
						exp1 = wrap(p01 - rate1 * rrate * gap, 0, P.L);
						expok1 = 1;
					}
				} else {
					if (kind1 == 0) {
						
						trrem1 = gap;
						mode1 = 2;
					}
					if (kind1 == 1) {
						trrem1 = gap;
						mode1 = 2;
					}
					if (kind1 == 2) {
						trrem1 = gap;
						mode1 = 2;
						spm1 = max(floor((be1 - bs1) * spb * 0.85), 16);
					}
				}
			}
		}
	} else {
		if (has1 > 0.5 && B >= be1) {
			has1 = 0;
			if (mode1 == 3) {
				mode1 = 0;
			}
		}
	}
}

if (1 == 0 && be1 >= NONE && has1 > 0.5) {
	nb = NONE;
	if (head1 < qt) {
		nb = min(nb, peek(BUF.q, (1 * 64 + wrap(head1, 0, 64)) * 16, 0));
	}
	if (head1 < P.qt1) {
		nb = min(nb, peek(BUF.q, (64 + wrap(head1, 0, 64)) * 16, 0));
	}
	if (nb < NONE) {
		be1 = max(nb, B);
		pclose1 = be1 + fadlate / spb;
	}
}


if (mode1 == 1) {
	w = 0.5 - 0.5 * cos(twopi * (k1 + 1) / (pdc1 + 1));
	pdv1 = pdv1 * dks;
	v1 = pdv1 + pa1 * w;
	k1 = k1 + 1;
	trrem1 = trrem1 - 1;
	if (k1 >= pdc1) {
		v1 = 0;
		mode1 = 2;
	}
} else {
	if (mode1 == 2) {
		v1 = v1 * dks;
		trrem1 = trrem1 - 1;
		if (scn1 > 0.5 && trrem1 <= sctot1) {
			
			mode1 = 7;
			sck1 = 0;
			sclen1 = 0;
			popen1 = B;
		}
		if (mode1 == 2 && trrem1 <= 0) {
			if (kind1 == 3) {
				mode1 = 0;
			}
			if (kind1 == 1) {
				mode1 = 4;
			}
			if (kind1 == 0 || kind1 == 2) {
				if (kind1 == 2) {
					spdv1 = v1;
					k1 = 0;
					mode1 = 5;
				} else {
					vb1 = v1;
					rk1 = 0;
					mode1 = 3;
					cuchk1 = floor(0.35 * SR);
				}
			}
		}
	}
}
if (mode1 == 7) {
	
	
	if (sck1 >= sclen1) {
		if (scn1 < 0.5) {
			v1 = 0;
			if (kind1 == 0) {
				vb1 = 0;
				rk1 = 0;
				mode1 = 3;
				cuchk1 = floor(0.35 * SR);
			} else {
				mode1 = 0;
			}
		} else {
			if (wrap(scn1, 0, 2) < 0.5) {
				
				scj1 = min(max(((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * reljit * 0.5, -0.2 * scns1), 0.2 * scns1);
				sclen1 = scns1 + floor(scj1);
				trav = sctr1 * max(1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * 0.25 * P.slop, 0.4);
			} else {
				
				sclen1 = scns1 - floor(scj1);
				trav = -scoff1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * min(cuebase + cueper * abs(scoff1), 0.03 * SR);
			}
			scoff1 = scoff1 + trav;
			
			
			if (trav > 0) {
				sca1 = trav / rrate / (0.6560 * sclen1);
			} else {
				sca1 = trav / rrate / (0.6366 * sclen1);
			}
			sck1 = 0;
			scn1 = scn1 - 1;
		}
	}
	if (mode1 == 7) {
		u = (sck1 + 0.5) / sclen1;
		if (sca1 > 0) {
			u = pow(u, 0.75);
		}
		v1 = sca1 * sin(pi * u);
		sck1 = sck1 + 1;
		trrem1 = trrem1 - 1;
	}
}
if (mode1 == 5 && k1 == 0) {
	
	sd = spdv1 * dks * (1 - pow(dks, spm1)) / (1 - dks);
	spa1 = (p01 / rrate - sd) / (spm1 * 0.2475168);
}
if (mode1 == 5) {
	u = (k1 + 0.5) / spm1;
	spdv1 = spdv1 * dks;
	v1 = spdv1 + spa1 * exp(-3 * u) * (1 - exp(-60 * u)) * sqrt(max(1 - u, 0));
	k1 = k1 + 1;
	if (k1 >= spm1) {
		v1 = 0;
		mode1 = 0;
	}
}
if (mode1 == 4) {
	trem1 = trema * trem1 + tremb * ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678);
	v1 = v1 * dks + trem1 * tremsd;
	if (B >= be1) {
		mode1 = 0;
	}
}
if (mode1 == 0 || mode1 == 6) {
	if (mode1 == 6) {
		v1 = v1 * exp(-1 / (0.04 * SR));
	} else {
		v1 = v1 * dks;
	}
}

if (mode1 == 0 && has1 < 0.5 && g1 <= 0 && active > 0.5 && P.playing > 0.5) {
	idl1 = idl1 + 1;
	if (idl1 > 0.25 * SR) {
		pos1 = rbs * recspb;
		v1 = 1;
	}
} else {
	idl1 = 0;
}
if (mode1 == 3) {
	rate_e = rate1;
	if (1 == 1) {
		rate_e = rate1 * (1 + 0.0012 * P.wear);
	}
	vb1 = vb1 + (rate_e - vb1) * (1 - dk);
	vv = vb1;
	if (cued1 > 0.5) {
		vv = vv + rate_e * push1 * (exp(-rk1 / (2.5 * tau)) - exp(-rk1 / (0.6 * tau)));
	}
	rk1 = rk1 + 1;
	
	if (expok1 > 0.5) {
		cuchk1 = cuchk1 - 1;
		if (cuchk1 <= 0) {
			err = wrap(exp1 - pos1, -P.L / 2, P.L / 2);
			if (abs(err) > 0.0003 * SR * rrate) {
				culen1 = floor(0.6 * SR);
				cuk1 = 0;
				cua1 = 0.8 * err / rrate / ((culen1 + 1) / 2);
				cuchk1 = culen1 + floor(0.25 * SR);
			} else {
				cuchk1 = floor(2 * SR);
			}
		}
	}
	if (culen1 > 0) {
		vv = vv + cua1 * (0.5 - 0.5 * cos(twopi * (cuk1 + 1) / (culen1 + 1)));
		cuk1 = cuk1 + 1;
		if (cuk1 >= culen1) {
			culen1 = 0;
		}
	}
	v1 = vv;
}
if (expok1 > 0.5) {
	exp1 = wrap(exp1 + rate1 * rrate, 0, P.L);
}


if (dragnew > 0.5 && live == 1) {
	drlen1 = floor(0.30 * SR);
	drk1 = 0;
	drd1 = P.dragdepth;
}
vplay = v1;
if (drlen1 > 0) {
	vplay = v1 * (1 - drd1 * (0.5 - 0.5 * cos(twopi * drk1 / drlen1)));
	drk1 = drk1 + 1;
	if (drk1 >= drlen1) {
		drlen1 = 0;
	}
}
pos1 = wrap(pos1 + vplay * rrate, 0, P.L);



waitrel = 0;
if (has1 > 0.5 && cued1 > 0.5 && kind1 == 0 && (mode1 == 1 || mode1 == 2)) {
	waitrel = 1;
}
if (B >= popen1 && waitrel < 0.5) {
	popen1 = NONE;
	if (has1 > 0.5) {
		gt1 = 1;
		live = 1;
		a = amp1;
		if (str1 > 0.5) {
			a = a * P.strlev;
		}
		gamp1 = a * min(1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * lvl, 1.12);
		gend = be1;
		if (gm1 > 0.5) {
			gend = min(be1, g01 + (slot1 + 1) * gd1);
		}
		if (kind1 == 2) {
			gend = min(be1, bs1 + (be1 - bs1) * 0.85);
		}
		pclose1 = gend + (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
		pcs1 = 1;
	}
}
if (B >= pclose1) {
	pclose1 = NONE;
	gt1 = 0;
	if (gm1 > 0.5 && has1 > 0.5 && pcs1 > 0.5) {
		slot1 = slot1 + 2;
		s0 = g01 + slot1 * gd1;
		if (s0 < be1) {
			popen1 = s0 - (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;
		}
	}
}
if (gt1 > 0.5) {
	g1 = min(g1 + 1 / fade, 1);
} else {
	g1 = max(g1 - 1 / fade, 0);
}
gain1 = gamp1 * (0.5 - 0.5 * cos(pi * g1));


yl1 = 0;
yr1 = 0;
if (g1 > 0 && active > 0.5) {
	ip = floor(pos1);
	fr = pos1 - ip;
	i0 = wrap(ip - 1, 0, P.L); i1 = wrap(ip, 0, P.L); i2 = wrap(ip + 1, 0, P.L); i3 = wrap(ip + 2, 0, P.L);
	xm = peek(BUF.rec, i0, 0); x0 = peek(BUF.rec, i1, 0); x1 = peek(BUF.rec, i2, 0); x2 = peek(BUF.rec, i3, 0);
	c1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);
	yl1 = ((c3 * fr + c2) * fr + c1) * fr + x0;
	xm = peek(BUF.rec, i0, 1); x0 = peek(BUF.rec, i1, 1); x1 = peek(BUF.rec, i2, 1); x2 = peek(BUF.rec, i3, 1);
	c1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);
	yr1 = ((c3 * fr + c2) * fr + c1) * fr + x0;
	if (abs(vplay * rrate) > 2) {
		
		hw = abs(vplay * rrate) * 0.5;
		ja = wrap(floor(pos1 - hw), 0, P.L); jb = wrap(floor(pos1 + hw), 0, P.L);
		yl1 = 0.5 * yl1 + 0.25 * (peek(BUF.rec, ja, 0) + peek(BUF.rec, jb, 0));
		yr1 = 0.5 * yr1 + 0.25 * (peek(BUF.rec, ja, 1) + peek(BUF.rec, jb, 1));
	}
	if (1 == 1) {
		wa = exp(-twopi * (20000 - 9000 * P.wear) / SR);
		wl1 = (1 - wa) * yl1 + wa * wl1;
		wr1 = (1 - wa) * yr1 + wa * wr1;
		yl1 = wl1 * (1 - 0.06 * P.wear);
		yr1 = wr1 * (1 - 0.06 * P.wear);
	}
}


mixl = yl0 * gain0 + yl1 * gain1;
mixr = yr0 * gain0 + yr1 * gain1;
kn = 0.85;
if (abs(mixl) > kn) {
	mixl = sign(mixl) * (kn + (1 - kn) * tanh((abs(mixl) - kn) / (1 - kn)));
}
if (abs(mixr) > kn) {
	mixr = sign(mixr) * (kn + (1 - kn) * tanh((abs(mixr) - kn) / (1 - kn)));
}
if (active > 0.5) {
	out1 = mixl * P.level;
	out2 = mixr * P.level;
} else {
	out1 = in1;
	out2 = in2;
}


statc = H.h_statc + 1;
if (statc >= 32) {
	statc = 0;
	poke(BUF.st, B, 0, 0);
	poke(BUF.st, P.playing, 1, 0);
	poke(BUF.st, recstate, 2, 0);
	poke(BUF.st, wi, 3, 0);
	poke(BUF.st, recstart, 4, 0);
	poke(BUF.st, head0, 5, 0);
	poke(BUF.st, head1, 6, 0);
	poke(BUF.st, pos0 / P.L, 7, 0);
	poke(BUF.st, pos1 / P.L, 8, 0);
	poke(BUF.st, gain0, 9, 0);
	poke(BUF.st, gain1, 10, 0);
	poke(BUF.st, late0 + late1, 11, 0);
	poke(BUF.st, flub0 + flub1, 12, 0);
	poke(BUF.st, H.h_alive + 1, 13, 0);
	poke(BUF.st, recb, 14, 0);
	poke(BUF.st, live, 15, 0);
}


H.h_beatint = beatint; H.h_prevph = prevph; H.h_sbseq = P.sbseq; H.h_wasplay = P.playing; H.h_curgen = curgen;
H.h_flushb = flushb; H.h_reccmd = P.reccmd; H.h_recstate = recstate; H.h_wi = wi; H.h_recstart = recstart;
H.h_recb = recb; H.h_prevb = prevb; H.h_statc = statc; H.h_live = live; H.h_dragseq = P.dragseq; H.h_alive = H.h_alive + 1;
H.h_pos0 = pos0; H.h_v0 = v0; H.h_vb0 = vb0; H.h_mode0 = mode0; H.h_k0 = k0; H.h_has0 = has0;
H.h_bs0 = bs0; H.h_be0 = be0; H.h_p00 = p00; H.h_rate0 = rate0; H.h_kind0 = kind0; H.h_gm0 = gm0;
H.h_gd0 = gd0; H.h_g00 = g00; H.h_amp0 = amp0; H.h_str0 = str0; H.h_gen0 = gen0; H.h_cued0 = cued0;
H.h_pdc0 = pdc0; H.h_pa0 = pa0; H.h_pdv0 = pdv0; H.h_trrem0 = trrem0; H.h_push0 = push0; H.h_rk0 = rk0;
H.h_exp0 = exp0; H.h_expok0 = expok0; H.h_cuchk0 = cuchk0; H.h_culen0 = culen0; H.h_cuk0 = cuk0; H.h_cua0 = cua0;
H.h_spm0 = spm0; H.h_spa0 = spa0; H.h_spdv0 = spdv0; H.h_trem0 = trem0;
H.h_gt0 = gt0; H.h_g0 = g0; H.h_gamp0 = gamp0; H.h_popen0 = popen0; H.h_pclose0 = pclose0; H.h_slot0 = slot0;
H.h_drk0 = drk0; H.h_drlen0 = drlen0; H.h_drd0 = drd0; H.h_late0 = late0; H.h_flub0 = flub0; H.h_e0 = e0;
H.h_wl0 = wl0; H.h_wr0 = wr0; H.h_head0 = head0; H.h_brk0 = brk0; H.h_idl0 = idl0; H.h_pcs0 = pcs0;
H.h_scn0 = scn0; H.h_scns0 = scns0; H.h_sctot0 = sctot0; H.h_sctr0 = sctr0; H.h_scoff0 = scoff0;
H.h_sck0 = sck0; H.h_sclen0 = sclen0; H.h_sca0 = sca0; H.h_scj0 = scj0;
H.h_pos1 = pos1; H.h_v1 = v1; H.h_vb1 = vb1; H.h_mode1 = mode1; H.h_k1 = k1; H.h_has1 = has1;
H.h_bs1 = bs1; H.h_be1 = be1; H.h_p01 = p01; H.h_rate1 = rate1; H.h_kind1 = kind1; H.h_gm1 = gm1;
H.h_gd1 = gd1; H.h_g01 = g01; H.h_amp1 = amp1; H.h_str1 = str1; H.h_gen1 = gen1; H.h_cued1 = cued1;
H.h_pdc1 = pdc1; H.h_pa1 = pa1; H.h_pdv1 = pdv1; H.h_trrem1 = trrem1; H.h_push1 = push1; H.h_rk1 = rk1;
H.h_exp1 = exp1; H.h_expok1 = expok1; H.h_cuchk1 = cuchk1; H.h_culen1 = culen1; H.h_cuk1 = cuk1; H.h_cua1 = cua1;
H.h_spm1 = spm1; H.h_spa1 = spa1; H.h_spdv1 = spdv1; H.h_trem1 = trem1;
H.h_gt1 = gt1; H.h_g1 = g1; H.h_gamp1 = gamp1; H.h_popen1 = popen1; H.h_pclose1 = pclose1; H.h_slot1 = slot1;
H.h_drk1 = drk1; H.h_drlen1 = drlen1; H.h_drd1 = drd1; H.h_late1 = late1; H.h_flub1 = flub1; H.h_e1 = e1;
H.h_wl1 = wl1; H.h_wr1 = wr1; H.h_head1 = head1; H.h_brk1 = brk1; H.h_idl1 = idl1; H.h_pcs1 = pcs1;
H.h_scn1 = scn1; H.h_scns1 = scns1; H.h_sctot1 = sctot1; H.h_sctr1 = sctr1; H.h_scoff1 = scoff1;
H.h_sck1 = sck1; H.h_sclen1 = sclen1; H.h_sca1 = sca1; H.h_scj1 = scj1;

    return [out1, out2];
  }
  return {P: P, H: H, step: step};
}
if (typeof module !== "undefined") module.exports = makeCore;