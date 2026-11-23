{
	"patcher": {
		"fileversion": 1,
		"appversion": {
			"major": 9,
			"minor": 0,
			"revision": 5,
			"architecture": "x64",
			"modernui": 1
		},
		"classnamespace": "box",
		"rect": [
			40.0,
			80.0,
			1400.0,
			760.0
		],
		"openrect": [
			0.0,
			0.0,
			1150.0,
			169.0
		],
		"bglocked": 0,
		"openinpresentation": 1,
		"default_fontsize": 10.0,
		"default_fontface": 0,
		"default_fontname": "Arial Bold",
		"gridonopen": 1,
		"gridsize": [
			8.0,
			8.0
		],
		"gridsnaponopen": 1,
		"objectsnaponopen": 1,
		"statusbarvisible": 2,
		"toolbarvisible": 1,
		"boxanimatetime": 500,
		"enablehscroll": 1,
		"enablevscroll": 1,
		"devicewidth": 1150.0,
		"description": "DJ Claude Skratch: a modelled record hand and fader hand scratch one sound in time with Live.",
		"digest": "",
		"tags": "",
		"style": "",
		"subpatcher_template": "",
		"boxes": [
			{
				"box": {
					"id": "obj-1",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						20.0,
						20.0,
						50.0,
						22.0
					],
					"outlettype": [
						"int",
						"int",
						"int"
					],
					"text": "notein"
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						20.0,
						50.0,
						60.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "pack 0 0"
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						20.0,
						80.0,
						80.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend note"
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						120.0,
						20.0,
						80.0,
						22.0
					],
					"outlettype": [
						"signal"
					],
					"text": "plugphasor~"
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						120.0,
						120.0,
						200.0,
						22.0
					],
					"outlettype": [
						"signal",
						"signal"
					],
					"text": "gen~",
					"patcher": {
						"fileversion": 1,
						"appversion": {
							"major": 9,
							"minor": 0,
							"revision": 5,
							"architecture": "x64",
							"modernui": 1
						},
						"classnamespace": "dsp.gen",
						"rect": [
							100.0,
							100.0,
							900.0,
							700.0
						],
						"bglocked": 0,
						"openinpresentation": 0,
						"default_fontsize": 12.0,
						"default_fontface": 0,
						"default_fontname": "Arial",
						"gridonopen": 1,
						"gridsize": [
							15.0,
							15.0
						],
						"gridsnaponopen": 1,
						"objectsnaponopen": 1,
						"statusbarvisible": 2,
						"toolbarvisible": 1,
						"boxanimatetime": 200,
						"enablehscroll": 1,
						"enablevscroll": 1,
						"devicewidth": 0.0,
						"description": "",
						"digest": "",
						"tags": "",
						"style": "",
						"subpatcher_template": "",
						"boxes": [
							{
								"box": {
									"id": "obj-1",
									"maxclass": "newobj",
									"numinlets": 0,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										20.0,
										20.0,
										30.0,
										22.0
									],
									"text": "in 1"
								}
							},
							{
								"box": {
									"id": "obj-4",
									"maxclass": "codebox",
									"numinlets": 1,
									"numoutlets": 2,
									"outlettype": [
										"",
										""
									],
									"patching_rect": [
										20.0,
										60.0,
										840.0,
										560.0
									],
									"fontface": 0,
									"fontname": "Menlo",
									"fontsize": 11.0,
									"code": "// DJ Claude Skratch: gen~ core. One modelled record, a cartridge and a fader.\n// GENERATED INTO THE DEVICE by tools/build_device.py. Edit this template, not the device.\n// #FOR blocks are written out once per value with {name} replaced (tools/genexpr.py).\n// Every History is copied to a local at the top of the sample and written back at the end.\n//\n// The planner (skratch.js) decides everything. This core only plays what it is given:\n//   skr_q  hand segments, 12 fields: tb kind dur p1 a k e x1 x2 x3 gen spare\n//          kind 1 stroke  v = a * sin(pi * u^k)^e, u = (i + 0.5) / m\n//          kind 2 play    v = a + (v0 - a) * exp(-(i + 1) / (k * SR))\n//          kind 3 wiggle  a amp, k cycles, e wobble depth, x1 wobble rate, x2 mean, x3 phase\n//          kind 4 stop    v = v0 * exp(-(i + 1) / (k * SR))\n//          At the end of a segment the record position is set to p1.\n//          With no segment the hand holds the record: v decays, plus a little tremor.\n//   skr_f  fader events, 4 fields: tb gain gen spare. The fader moves in a straight line\n//          to each new gain over FADE seconds.\n//   skr_cut  the record, upsampled 4 times and pre-emphasised (inverse phono curve).\n// Output: speed times the cut record, read 4 times per sample, through the phono curve\n// and a low-pass at 4 x SR, every 4th result kept. Then the fader.\n//\n// in1: plugphasor~ (0..1 ramp per beat).   out1, out2: audio.\n\nBuffer cut(\"skr_cut\");\nBuffer q(\"skr_q\");\nBuffer fq(\"skr_f\");\nBuffer st(\"skr_stat\");\nParam tempo(120, min=20, max=999);\nParam playing(0);\nParam songbeat(0);\nParam sbseq(0);\nParam qtail(0);\nParam ftail(0);\nParam fgen(0);\nParam taustop(0.009, min=0.0005);\nParam tremor(0.01);\nParam fadet(0.0014, min=0.0002);\nParam haverec(0);\nParam recsr4(176400, min=1);\nParam rlen(0);\nParam level(1, min=0, max=4);\nParam posset(0);\nParam poscmd(0);\nParam pb0a(0.10079887);\nParam pb1a(-0.09901778);\nParam pa1a(-0.9982189);\nParam pb0b(0.13435687);\nParam pb1b(-0.06149022);\nParam pa1b(-0.92713336);\nParam g1k(9.886505);\nParam lb0a(0.00223697);\nParam lb1a(0.00300486);\nParam lb2a(0.00223697);\nParam la1a(-1.40456695);\nParam la2a(0.52091626);\nParam lb0b(1);\nParam lb1b(-0.30111365);\nParam lb2b(1);\nParam la1b(-1.39470843);\nParam la2b(0.68395549);\nParam lb0c(1);\nParam lb1c(-0.8032222);\nParam lb2c(1);\nParam la1c(-1.43507501);\nParam la2c(0.8921389);\nHistory h_beatint(0);\nHistory h_prevph(0);\nHistory h_sbseq(0);\nHistory h_B(0);\nHistory h_wasplay(0);\nHistory h_jumps(0);\nHistory h_curgen(0);\nHistory h_poscmd(0);\nHistory h_qh(0);\nHistory h_fh(0);\nHistory h_alive(0);\nHistory h_act(0);\nHistory h_kind(0);\nHistory h_m(1);\nHistory h_i(0);\nHistory h_p1(0);\nHistory h_a(0);\nHistory h_k(1);\nHistory h_e(1);\nHistory h_x1(0);\nHistory h_x2(0);\nHistory h_x3(0);\nHistory h_v0(0);\nHistory h_pos(0);\nHistory h_v(0);\nHistory h_vb(0);\nHistory h_trph(0);\nHistory h_g(0);\nHistory h_gt(0);\nHistory h_sa0(0);\nHistory h_sb0(0);\nHistory h_z1a0(0);\nHistory h_z2a0(0);\nHistory h_z1b0(0);\nHistory h_z2b0(0);\nHistory h_z1c0(0);\nHistory h_z2c0(0);\nHistory h_sa1(0);\nHistory h_sb1(0);\nHistory h_z1a1(0);\nHistory h_z2a1(0);\nHistory h_z1b1(0);\nHistory h_z2b1(0);\nHistory h_z1c1(0);\nHistory h_z2c1(0);\n\n// every local starts at zero, so none is read before it is set\nQN = 0; QF = 0; FN = 0; FF = 0; SR = 0; beatint = 0; prevph = 0; B = 0;\njumps = 0; curgen = 0; qh = 0; fh = 0; act = 0; kind = 0; m = 0; i = 0;\np1 = 0; a = 0; sk = 0; se = 0; x1 = 0; x2 = 0; x3 = 0; v0 = 0;\npos = 0; vprev = 0; vb = 0; trph = 0; g = 0; gt = 0; sa0 = 0; sb0 = 0;\nz1a0 = 0; z2a0 = 0; z1b0 = 0; z2b0 = 0; z1c0 = 0; z2c0 = 0; sa1 = 0; sb1 = 0;\nz1a1 = 0; z2a1 = 0; z1b1 = 0; z2b1 = 0; z1c1 = 0; z2c1 = 0; ph = 0; cand = 0;\nBn = 0; base = 0; fbase = 0; ended = 0; u = 0; sh = 0; v = 0; kk = 0;\nkn = 0; tw = 0; wn = 0; env = 0; dp = 0; y0 = 0; y1 = 0; fr1 = 0;\npj = 0; vj = 0; ix = 0; i0 = 0; fx = 0; x = 0; t1 = 0; t2 = 0;\nl1 = 0; l2 = 0; l3 = 0; fr2 = 0; fr3 = 0; fr4 = 0; frate = 0; outg = 0;\nalive = 0;\n// transport, set from JS\n// queues\n// hands\n// record\n// phono playback curve at 4 x SR (two first-order sections) and its 1 kHz gain\n// low-pass at 4 x SR, three biquads\n\n// the hand\n// the fader\n// filters, per channel\n\n// ---------------- per sample ----------------\nQN = 256;\nQF = 12;\nFN = 512;\nFF = 4;\nSR = samplerate;\nbeatint = h_beatint; prevph = h_prevph; B = h_B; jumps = h_jumps; curgen = h_curgen;\nqh = h_qh; fh = h_fh;\nact = h_act; kind = h_kind; m = h_m; i = h_i; p1 = h_p1; a = h_a; sk = h_k; se = h_e;\nx1 = h_x1; x2 = h_x2; x3 = h_x3; v0 = h_v0; pos = h_pos; vprev = h_v; vb = h_vb; trph = h_trph;\ng = h_g; gt = h_gt;\nsa0 = h_sa0; sb0 = h_sb0;\nz1a0 = h_z1a0; z2a0 = h_z2a0; z1b0 = h_z1b0; z2b0 = h_z2b0; z1c0 = h_z1c0; z2c0 = h_z2c0;\nsa1 = h_sa1; sb1 = h_sb1;\nz1a1 = h_z1a1; z2a1 = h_z2a1; z1b1 = h_z1b1; z2b1 = h_z2b1; z1c1 = h_z1c1; z2c1 = h_z2c1;\n\n// ---- clock: the song position while Live plays, free running at the tempo when it stops ----\nph = in1;\nif (playing > 0.5) {\n\tif (ph < prevph - 0.5) {\n\t\tbeatint = beatint + 1;\n\t}\n\tif (sbseq != h_sbseq) {\n\t\tcand = floor(songbeat - ph + 0.5);\n\t\tif (abs(cand - beatint) > 0.5) {\n\t\t\tbeatint = cand;\n\t\t}\n\t}\n\tBn = beatint + ph;\n\tif (h_wasplay < 0.5 || abs(Bn - B) > 0.05) {\n\t\tjumps = jumps + 1;\n\t}\n\tB = Bn;\n} else {\n\tB = B + tempo / (60 * SR);\n}\nprevph = ph;\n\n// ---- flush: a new generation drops the segment in hand and closes the fader ----\nif (fgen != curgen) {\n\tcurgen = fgen;\n\tact = 0;\n\tvb = vprev;\n\tgt = 0;\n}\n\n// ---- a new record: put the needle on its cue ----\nif (poscmd != h_poscmd) {\n\tpos = posset;\n\tact = 0;\n\tvb = 0;\n\tvprev = 0;\n}\n\n// ---- next hand segment ----\nif (qh < qtail) {\n\tbase = (qh - floor(qh / QN) * QN) * QF;\n\tif (peek(q, base + 10, 0) != curgen) {\n\t\tqh = qh + 1;\n\t} else {\n\t\tif (peek(q, base, 0) <= B) {\n\t\t\tkind = peek(q, base + 1, 0);\n\t\t\tm = max(1, floor(peek(q, base + 2, 0) * SR + 0.5));\n\t\t\tp1 = peek(q, base + 3, 0);\n\t\t\ta = peek(q, base + 4, 0);\n\t\t\tsk = peek(q, base + 5, 0);\n\t\t\tse = peek(q, base + 6, 0);\n\t\t\tx1 = peek(q, base + 7, 0);\n\t\t\tx2 = peek(q, base + 8, 0);\n\t\t\tx3 = peek(q, base + 9, 0);\n\t\t\tv0 = vprev;\n\t\t\ti = 0;\n\t\t\tact = 1;\n\t\t\tqh = qh + 1;\n\t\t}\n\t}\n}\n\n// ---- next fader event ----\nif (fh < ftail) {\n\tfbase = (fh - floor(fh / FN) * FN) * FF;\n\tif (peek(fq, fbase + 2, 0) != curgen) {\n\t\tfh = fh + 1;\n\t} else {\n\t\tif (peek(fq, fbase, 0) <= B) {\n\t\t\tgt = peek(fq, fbase + 1, 0);\n\t\t\tfh = fh + 1;\n\t\t}\n\t}\n}\n\n// ---- the hand: speed for this sample ----\nended = 0;\nif (act > 0.5) {\n\tu = (i + 0.5) / m;\n\tif (kind == 1) {\n\t\tsh = sin(pi * pow(u, sk));\n\t\tif (se != 1) {\n\t\t\tsh = pow(max(sh, 0), se);\n\t\t}\n\t\tv = a * sh;\n\t} else {\n\t\tif (kind == 2) {\n\t\t\tv = a + (v0 - a) * exp(-(i + 1) / (sk * SR));\n\t\t} else {\n\t\t\tif (kind == 3) {\n\t\t\t\tkk = i / m;\n\t\t\t\tkn = (i + 1) / m;\n\t\t\t\ttw = twopi * x1;\n\t\t\t\twn = (kn + se * (1 - cos(tw * kn)) / tw) / (1 + se * (1 - cos(tw)) / tw);\n\t\t\t\tenv = min(1, min(kk, 1 - kk) * 12);\n\t\t\t\tv = a * sin(twopi * sk * wn) * env - x2;\n\t\t\t} else {\n\t\t\t\tv = v0 * exp(-(i + 1) / (sk * SR));\n\t\t\t}\n\t\t}\n\t}\n\tvb = v;\n\ti = i + 1;\n\tif (i >= m) {\n\t\tact = 0;\n\t\tended = 1;\n\t}\n} else {\n\tif (abs(vb) > 0.001) {\n\t\tvb = vb * exp(-1 / (taustop * SR));\n\t} else {\n\t\tvb = 0;\n\t}\n\tv = vb + tremor * 0.2 * sin(trph);\n\ttrph = trph + twopi * 8.5 / SR;\n\tif (trph > twopi) {\n\t\ttrph = trph - twopi;\n\t}\n}\n\n// ---- the cartridge: four sub-steps from the last sample to this one ----\ndp = vprev / SR;\ny0 = 0;\ny1 = 0;\nfr1 = 1 * 0.25;\npj = pos + fr1 * dp;\nvj = vprev + fr1 * (v - vprev);\nix = min(max(pj * recsr4, 0), max(rlen - 2, 0));\ni0 = floor(ix);\nfx = ix - i0;\nx = (peek(cut, i0, 0) * (1 - fx) + peek(cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa0;\nsa0 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb0;\nsb0 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a0;\nz1a0 = lb1a * t2 - la1a * l1 + z2a0;\nz2a0 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b0;\nz1b0 = lb1b * l1 - la1b * l2 + z2b0;\nz2b0 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c0;\nz1c0 = lb1c * l2 - la1c * l3 + z2c0;\nz2c0 = lb2c * l2 - la2c * l3;\ny0 = l3;\nx = (peek(cut, i0, 1) * (1 - fx) + peek(cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa1;\nsa1 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb1;\nsb1 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a1;\nz1a1 = lb1a * t2 - la1a * l1 + z2a1;\nz2a1 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b1;\nz1b1 = lb1b * l1 - la1b * l2 + z2b1;\nz2b1 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c1;\nz1c1 = lb1c * l2 - la1c * l3 + z2c1;\nz2c1 = lb2c * l2 - la2c * l3;\ny1 = l3;\nfr2 = 2 * 0.25;\npj = pos + fr2 * dp;\nvj = vprev + fr2 * (v - vprev);\nix = min(max(pj * recsr4, 0), max(rlen - 2, 0));\ni0 = floor(ix);\nfx = ix - i0;\nx = (peek(cut, i0, 0) * (1 - fx) + peek(cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa0;\nsa0 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb0;\nsb0 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a0;\nz1a0 = lb1a * t2 - la1a * l1 + z2a0;\nz2a0 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b0;\nz1b0 = lb1b * l1 - la1b * l2 + z2b0;\nz2b0 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c0;\nz1c0 = lb1c * l2 - la1c * l3 + z2c0;\nz2c0 = lb2c * l2 - la2c * l3;\ny0 = l3;\nx = (peek(cut, i0, 1) * (1 - fx) + peek(cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa1;\nsa1 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb1;\nsb1 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a1;\nz1a1 = lb1a * t2 - la1a * l1 + z2a1;\nz2a1 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b1;\nz1b1 = lb1b * l1 - la1b * l2 + z2b1;\nz2b1 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c1;\nz1c1 = lb1c * l2 - la1c * l3 + z2c1;\nz2c1 = lb2c * l2 - la2c * l3;\ny1 = l3;\nfr3 = 3 * 0.25;\npj = pos + fr3 * dp;\nvj = vprev + fr3 * (v - vprev);\nix = min(max(pj * recsr4, 0), max(rlen - 2, 0));\ni0 = floor(ix);\nfx = ix - i0;\nx = (peek(cut, i0, 0) * (1 - fx) + peek(cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa0;\nsa0 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb0;\nsb0 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a0;\nz1a0 = lb1a * t2 - la1a * l1 + z2a0;\nz2a0 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b0;\nz1b0 = lb1b * l1 - la1b * l2 + z2b0;\nz2b0 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c0;\nz1c0 = lb1c * l2 - la1c * l3 + z2c0;\nz2c0 = lb2c * l2 - la2c * l3;\ny0 = l3;\nx = (peek(cut, i0, 1) * (1 - fx) + peek(cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa1;\nsa1 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb1;\nsb1 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a1;\nz1a1 = lb1a * t2 - la1a * l1 + z2a1;\nz2a1 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b1;\nz1b1 = lb1b * l1 - la1b * l2 + z2b1;\nz2b1 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c1;\nz1c1 = lb1c * l2 - la1c * l3 + z2c1;\nz2c1 = lb2c * l2 - la2c * l3;\ny1 = l3;\nfr4 = 4 * 0.25;\npj = pos + fr4 * dp;\nvj = vprev + fr4 * (v - vprev);\nix = min(max(pj * recsr4, 0), max(rlen - 2, 0));\ni0 = floor(ix);\nfx = ix - i0;\nx = (peek(cut, i0, 0) * (1 - fx) + peek(cut, i0 + 1, 0) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa0;\nsa0 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb0;\nsb0 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a0;\nz1a0 = lb1a * t2 - la1a * l1 + z2a0;\nz2a0 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b0;\nz1b0 = lb1b * l1 - la1b * l2 + z2b0;\nz2b0 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c0;\nz1c0 = lb1c * l2 - la1c * l3 + z2c0;\nz2c0 = lb2c * l2 - la2c * l3;\ny0 = l3;\nx = (peek(cut, i0, 1) * (1 - fx) + peek(cut, i0 + 1, 1) * fx) * vj + 0.000000000000000001;\nt1 = pb0a * x + sa1;\nsa1 = pb1a * x - pa1a * t1;\nt2 = pb0b * t1 + sb1;\nsb1 = pb1b * t1 - pa1b * t2;\nl1 = lb0a * t2 + z1a1;\nz1a1 = lb1a * t2 - la1a * l1 + z2a1;\nz2a1 = lb2a * t2 - la2a * l1;\nl2 = lb0b * l1 + z1b1;\nz1b1 = lb1b * l1 - la1b * l2 + z2b1;\nz2b1 = lb2b * l1 - la2b * l2;\nl3 = lb0c * l2 + z1c1;\nz1c1 = lb1c * l2 - la1c * l3 + z2c1;\nz2c1 = lb2c * l2 - la2c * l3;\ny1 = l3;\npos = pos + dp;\nif (ended > 0.5) {\n\tpos = p1 - v / SR;\n}\n\n// ---- the fader ----\nfrate = 1 / (fadet * SR);\nif (g < gt) {\n\tg = min(gt, g + frate);\n} else {\n\tg = max(gt, g - frate);\n}\n\noutg = g * g1k * level * haverec;\nout1 = y0 * outg;\nout2 = y1 * outg;\n\n// ---- status for the planner ----\nalive = h_alive + 1;\npoke(st, B, 0, 0);\npoke(st, playing, 1, 0);\npoke(st, qh, 2, 0);\npoke(st, fh, 3, 0);\npoke(st, pos, 4, 0);\npoke(st, v, 5, 0);\npoke(st, g, 6, 0);\npoke(st, alive, 7, 0);\npoke(st, SR, 8, 0);\npoke(st, jumps, 9, 0);\npoke(st, act, 10, 0);\npoke(st, curgen, 11, 0);\n\n// ---- write back ----\nh_beatint = beatint; h_prevph = prevph; h_sbseq = sbseq; h_B = B; h_wasplay = playing; h_jumps = jumps;\nh_curgen = curgen; h_poscmd = poscmd; h_qh = qh; h_fh = fh; h_alive = alive;\nh_act = act; h_kind = kind; h_m = m; h_i = i; h_p1 = p1; h_a = a; h_k = sk; h_e = se;\nh_x1 = x1; h_x2 = x2; h_x3 = x3; h_v0 = v0; h_pos = pos; h_v = v; h_vb = vb; h_trph = trph;\nh_g = g; h_gt = gt;\nh_sa0 = sa0; h_sb0 = sb0;\nh_z1a0 = z1a0; h_z2a0 = z2a0; h_z1b0 = z1b0; h_z2b0 = z2b0; h_z1c0 = z1c0; h_z2c0 = z2c0;\nh_sa1 = sa1; h_sb1 = sb1;\nh_z1a1 = z1a1; h_z2a1 = z2a1; h_z1b1 = z1b1; h_z2b1 = z2b1; h_z1c1 = z1c1; h_z2c1 = z2c1;\n"
								}
							},
							{
								"box": {
									"id": "obj-5",
									"maxclass": "newobj",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										20.0,
										640.0,
										40.0,
										22.0
									],
									"text": "out 1"
								}
							},
							{
								"box": {
									"id": "obj-6",
									"maxclass": "newobj",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										120.0,
										640.0,
										40.0,
										22.0
									],
									"text": "out 2"
								}
							}
						],
						"lines": [
							{
								"patchline": {
									"source": [
										"obj-1",
										0
									],
									"destination": [
										"obj-4",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"obj-4",
										0
									],
									"destination": [
										"obj-5",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"obj-4",
										1
									],
									"destination": [
										"obj-6",
										0
									]
								}
							}
						]
					}
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 2,
					"patching_rect": [
						120.0,
						200.0,
						60.0,
						22.0
					],
					"outlettype": [
						"signal",
						"signal"
					],
					"text": "plugout~"
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
						260.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---skr_rec 20000 2"
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
						288.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---skr_cut 80000 2"
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
						316.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---skr_q 100"
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
						344.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---skr_f 100"
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
						372.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---skr_stat 10"
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						260.0,
						20.0,
						60.0,
						22.0
					],
					"outlettype": [
						"bang"
					],
					"text": "loadbang"
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						260.0,
						50.0,
						330.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "cut ---skr_cut, q ---skr_q, fq ---skr_f, st ---skr_stat"
				}
			},
			{
				"box": {
					"id": "obj-14",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						400.0,
						120.0,
						300.0,
						22.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "v8",
					"textfile": {
						"text": "// DJ Claude Skratch: the planner for the Max for Live device.\n// Turns keys, the Routine and Instinct into words, and words into what two hands do:\n// segments of record movement (skr_q) and fader moves (skr_f). gen~ plays them.\n// This is a port of class Scratch in ref/scratch_engine.py and of the Instinct in\n// ref/make_demos.py. Times inside the planner are in samples on a timeline tied to the\n// beat (b2t, t2b). The human error is decided here, so gen~ decides nothing.\n//\n// Inlet: messages from the panel and the keys.  Outlet 0: gen~.  Outlet 1: the panel.\n\nautowatch = 1;\ninlets = 1;\noutlets = 2;\n\n// ------------------------------------------------------------------ settings\nvar QN = 256, QF = 12;          // hand segment queue\nvar FN = 512, FF = 4;           // fader event queue\nvar MAXREC = 20;                // seconds of record\nvar CUTOS = 4;                  // the cartridge runs at 4 x the sample rate\nvar WORDS = ['baby', 'forward', 'stab', 'chirp', 'flare1', 'flare2', 'crab', 'tear',\n\t'scribble', 'transform', 'drag', 'tip', 'lazer', 'release', 'echo', 'reverse'];\nvar WORDLABEL = ['BABY', 'FORWARD', 'STAB', 'CHIRP', '1-FLARE', '2-FLARE', 'CRAB', 'TEAR',\n\t'SCRIBBLE', 'TRANSFORM', 'DRAG', 'TIP', 'LAZER', 'RELEASE', 'ECHO', 'REVERSE'];\nvar KEY0 = 36, CUEKEY0 = 60;    // C1 = first word, C3 = cue point 1\nvar SPEEDS = [0.5, 0.25, 1 / 6, 0.125];            // menu order: 1/8, 1/16, 1/16T, 1/32\nvar SPEEDORDER = [0.125, 1 / 6, 0.25, 0.5];        // fastest to slowest\nvar QUANTS = [0.25, 0.5, 1, 0];                    // 1/16, 1/8, 1/4, OFF\nvar PHRASEON = [1, 2, 4, 8], PHRASEOFF = [0, 1, 2, 4, 8];\n\n// low-pass after the cartridge, elliptic, 6th order, 19 kHz, at 4 x these rates (ref/device_form.py)\nvar LOWPASS = {\n\t44100: [0.002236969227, 0.003004859885, 0.002236969227, 1, -1.404566950689, 0.520916262544, 1, -0.301113647742, 1, 1, -1.39470843125, 0.683955485214, 1, -0.803222196871, 1, 1, -1.435075008069, 0.892138899374],\n\t48000: [0.001835153443, 0.002270420218, 0.001835153443, 1, -1.451515384263, 0.551060540451, 1, -0.476104569712, 1, 1, -1.456312918364, 0.703810297851, 1, -0.950091566844, 1, 1, -1.508868400447, 0.899129673083],\n\t88200: [6.445252882798e-04, 1.169251651210e-04, 6.445252882798e-04, 1, -1.695767917097, 7.278504391235e-01, 1, -1.407278316763, 1, 1, -1.743384632744, 8.223195056549e-01, 1, -1.634400393697, 1, 1, -1.819131643240, 9.408334279014e-01],\n\t96000: [5.841811313529e-04, 5.501918411655e-06, 5.841811313529e-04, 1, -1.719833139755, 7.471747779841e-01, 1, -1.489195152173, 1, 1, -1.768146095456, 8.352774586663e-01, 1, -1.687804006956, 1, 1, -1.842156074597, 9.453253040630e-01]\n};\n\n// the built-in record: 16-bit mono PCM at 44.1 kHz, base64. Filled in by tools/build_device.py.\nvar BUILTIN_B64 = 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAQACQARAB0ALAA/AFUAbwCMAK0A0gD6ACUBUwGEAbgB7AEhAlUChwK3AuUCDgMyA1ED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9P/0//T/9P/1//X/9v/2//f/9//4//n/+v/7//z//f/+////AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA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AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA';\n\n// ------------------------------------------------------------------ state\nvar S = {\n\tsr: 44100, tempo: 120, playing: 0, B: 0, jumps: -1, gen: 1,\n\tBan: 0,                       // the beat at timeline sample 0\n\tctl: {\n\t\tspeed: 0.25, quant: 0.25, mode: 0, phon: 4, phoff: 0,\n\t\tdensity: 0.5, skill: 0.75, motif: 0.6, release: 0.6, funk: 0.5, walk: 0,\n\t\ttiming: 0.5, weight: 0.5, slop: 0.5, risk: 0.5, level: 1\n\t},\n\ton: {},                       // word switches\n\theld: [],                     // keys down, in order: {w, vel}\n\tfresh: null,                  // a key pressed whose first unit has not been planned yet\n\techoN: 0, lastKeyWord: null, unitLen: 0.5,\n\tgridEnd: -1e9,                // where on the grid the last planned word ends\n\tcalls: [],                    // planned Routine or Instinct cells not yet turned into strokes\n\tgenNext: null,                // beat where the next Routine cycle or Instinct phrase goes\n\troutine: [], routineCue: [],\n\tcueIdx: 0,\n\tvelT: 1, velA: 1, swingHold: null,\n\trel: null                     // a held Release: {open, tEnd, b}\n};\nfor (var wi = 0; wi < WORDS.length; wi++) S.on[WORDS[wi]] = 1;\n\nvar R = {have: 0, sr: 44100, n: 0, L: null, R: null, cues: [], wave: [], file: '', builtin: 1, cutting: 0};\n\nvar H = null;                   // the hands, from the four dials\nvar D = null;                   // the record hand and fader hand, as the planner sees them\nvar out = {seg: [], fad: []};   // waiting to be written to the queues\nvar qtail = 0, ftail = 0;\nvar rng = Math.random;\n\n// host hooks, replaced in Max (bottom of file) or by the test harness\nvar HOST = {\n\tgen: function (name, v) {},\n\tui: function () {},\n\tstatus: function () { return null; },   // [B, playing, qh, fh, pos, v, g, alive, sr, jumps, act, gen]\n\twriteSeg: function (slot, vals) {},\n\twriteFad: function (slot, vals) {},\n\tlog: function (s) {},\n\ttrace: null                             // test harness only\n};\n\n// ------------------------------------------------------------------ helpers\nfunction gauss() { var u = 1 - rng(), v = rng(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); }\nfunction normal(sd) { return gauss() * sd; }\nfunction spbS() { return S.sr * 60 / S.tempo; }\nfunction b2t(b) { return (b - S.Ban) * spbS(); }\nfunction t2b(t) { return S.Ban + t / spbS(); }\nfunction swingOf() { return S.swingHold !== null ? S.swingHold : 0.5 + 0.16 * S.ctl.funk; }\nfunction sw(b) {\n\tvar w = Math.floor(b * 2), x = b * 2 - w, s = swingOf();\n\tx = x < 0.5 ? x * s / 0.5 : s + (x - 0.5) * (1 - s) / 0.5;\n\treturn (w + x) / 2;\n}\nfunction qUp(b, q) { if (!q) return b; return Math.ceil(b / q - 1e-9) * q; }\n\n// ------------------------------------------------------------------ the hands (class Hands)\nfunction makeHands(c) {\n\tvar T = c.timing, W = c.weight, Sl = c.slop, Rk = c.risk;\n\treturn {\n\t\tT: T, W: W, S: Sl, R: Rk,\n\t\tgrid_jit: 0.006 * T, turn_jit: 0.010 * T, drift: 0.6, fad_jit: 0.005 * T,\n\t\ttrav: 0.22 * Sl, land: 0.003 * Sl, land_per: 0.05 * Sl, lvl: 0.06 * Sl,\n\t\tfade: 0.0006 + 0.0016 * Sl, leak: 0.010 * Sl,\n\t\tskew: 0.80 - 0.22 * W, tau_rel: 0.006 + 0.022 * W, tau_stop: 0.003 + 0.010 * W,\n\t\ttremor: 0.02 * Sl, flub: 0.10 * Rk\n\t};\n}\nfunction sendHands() {\n\tH = makeHands(S.ctl);\n\tHOST.gen('taustop', H.tau_stop);\n\tHOST.gen('tremor', H.tremor);\n\tHOST.gen('fadet', H.fade);\n}\n\n// ------------------------------------------------------------------ the deck (class Deck)\nfunction newDeck(t, p, v) {\n\tvar lo = 0, hi = R.have ? R.n / R.sr - 0.002 : 1;\n\treturn {t: Math.floor(t), p: p, v: v || 0, cue: cueSec(), lo: lo, hi: hi, late: 0,\n\t\topens: [], clicks: []};\n}\nfunction cueSec() { return R.cues.length ? R.cues[Math.min(S.cueIdx, R.cues.length - 1)] : 0; }\n\nfunction seg(kind, t0, m, p1, a, k, e, x1, x2, x3) {\n\tout.seg.push([t2b(t0), kind, m / S.sr, p1, a, k, e, x1 || 0, x2 || 0, x3 || 0, S.gen, 0]);\n}\n\nfunction decaySum(v0, r, m) { return v0 * r * (1 - Math.pow(r, m)) / (1 - r); }\n\nfunction hold(t1) {\n\tvar m = Math.floor(t1) - D.t;\n\tif (m <= 0) return;\n\tvar v0 = D.v;\n\tif (Math.abs(v0) > 1e-3) {\n\t\tvar r = Math.exp(-1 / (H.tau_stop * S.sr));\n\t\tD.p += decaySum(v0, r, m) / S.sr;\n\t\tD.v = v0 * Math.pow(r, m);\n\t} else D.v = 0;\n\tD.t += m;\n}\n\nvar SHAPE = {push: 0, pull: 1, flick: 2, even: 3};\nfunction stroke(t1, travel, kind, vcap) {\n\tif (vcap === undefined) vcap = 3.0;\n\tt1 = Math.floor(t1);\n\tvar v0 = D.v, SR = S.sr;\n\tif (Math.abs(v0) > 0.05) {                                   // the hand lands on a moving record\n\t\tvar md = Math.floor(Math.min((t1 - D.t) / 4, 4 * H.tau_stop * SR));\n\t\tif (md > 0) {\n\t\t\tvar r = Math.exp(-1 / (H.tau_stop * SR)), dp = decaySum(v0, r, md) / SR;\n\t\t\tseg(4, D.t, md, D.p + dp, 0, H.tau_stop, 1);\n\t\t\tD.p += dp; D.v = v0 * Math.pow(r, md); D.t += md;\n\t\t\ttravel -= dp;\n\t\t}\n\t}\n\tvar tgt = Math.min(Math.max(D.p + travel, D.lo), D.hi);\n\ttravel = tgt - D.p;\n\tif (vcap) t1 = Math.max(t1, D.t + Math.ceil(Math.abs(travel) / vcap * SR));   // a hand has a top speed: it is late instead\n\tvar m = t1 - D.t;\n\tif (m < 8) return;\n\tvar k = kind === 'push' ? H.skew : kind === 'flick' ? H.skew * 0.75 : 1.0, e = kind === 'even' ? 0.6 : 1.0;\n\tvar sum = 0, last = 0;\n\tfor (var i = 0; i < m; i++) {\n\t\tvar sh = Math.sin(Math.PI * Math.pow((i + 0.5) / m, k));\n\t\tif (e !== 1) sh = Math.pow(Math.max(sh, 0), e);\n\t\tsum += sh; last = sh;\n\t}\n\tvar a = sum > 0 ? travel * SR / sum : 0;\n\tif (HOST.trace) HOST.trace('stroke', {tb: t2b(D.t), te: t2b(t1), p0: D.p, p1: tgt, dur: m / SR, vcap: vcap || 0});\n\tseg(1, D.t, m, tgt, a, k, e);\n\tD.p = tgt; D.v = last * a; D.t = t1;\n}\nfunction to(t1, target, kind, vcap) { stroke(t1, target - D.p, kind || 'pull', vcap); }\n\nfunction play(t1, rate) {\n\tif (rate === undefined) rate = 1.0;\n\tvar m = Math.floor(t1) - D.t;\n\tif (m <= 0) return;\n\tvar v0 = D.v, r = Math.exp(-1 / (H.tau_rel * S.sr));\n\tvar sum = rate * m + decaySum(v0 - rate, r, m);\n\tvar p1 = D.p + sum / S.sr;\n\tseg(2, D.t, m, p1, rate, H.tau_rel, 1);\n\tD.p = p1; D.v = rate + (v0 - rate) * Math.pow(r, m); D.t += m;\n}\n\nfunction wiggle(t1, f, amp) {\n\tvar m = Math.floor(t1) - D.t;\n\tif (m <= 0) return;\n\tvar cyc = Math.max(1, Math.round(m / S.sr * f)), depth = H.S * 0.15, fw = 1.3 + rng();\n\tvar tw = 2 * Math.PI * fw, norm = 1 + depth * (1 - Math.cos(tw)) / tw, sum = 0, last = 0, i, c;\n\tfunction at(i) {\n\t\tvar kk = i / m, kn = (i + 1) / m, wn = (kn + depth * (1 - Math.cos(tw * kn)) / tw) / norm;\n\t\treturn amp * Math.sin(2 * Math.PI * cyc * wn) * Math.min(1, Math.min(kk, 1 - kk) * 12);\n\t}\n\tfor (i = 0; i < m; i++) sum += at(i);\n\tvar mean = sum / m;\n\tlast = at(m - 1) - mean;\n\tseg(3, D.t, m, D.p, amp, cyc, depth, fw, mean, 0);\n\tD.v = last; D.t += m;\n}\n\nfunction open(t0, t1, amp, exact) {\n\tif (amp === undefined) amp = 1.0;\n\tif (!exact) {\n\t\tt0 += normal(H.fad_jit) * S.sr;\n\t\tt1 += (normal(H.fad_jit) + H.leak * rng()) * S.sr;\n\t}\n\tif (t1 - t0 > 0.004 * S.sr) {\n\t\tvar o = {a: Math.floor(t0), b: Math.floor(t1), amp: amp * S.velA * Math.min(1 + normal(H.lvl), 1.12), sa: 0, sb: 0};\n\t\tD.opens.push(o);\n\t\treturn o;\n\t}\n\treturn null;\n}\nfunction click(t, closed) {\n\tif (closed === undefined) closed = 0.022;\n\tif (rng() < H.flub * 1.5) return;                     // a click that does not make it\n\tt += normal(H.fad_jit) * S.sr;\n\tvar w = closed * (1 + 0.3 * H.S * gauss()) * S.sr;\n\tD.clicks.push({a: Math.floor(t - w / 2), b: Math.floor(t + w / 2), sa: 0, sb: 0});\n}\n\n// fader: turn the opens and clicks into gain events, for edges that can no longer change.\n// gain(t) = 0 inside a click, else the largest open containing t (as Deck.gain in the reference)\nfunction gainAt(t) {\n\tvar i, g = 0;\n\tfor (i = 0; i < D.clicks.length; i++) if (D.clicks[i].a <= t && t < D.clicks[i].b) return 0;\n\tfor (i = 0; i < D.opens.length; i++) { var o = D.opens[i]; if (o.a <= t && t < o.b && o.amp > g) g = o.amp; }\n\treturn g;\n}\nfunction emitFader(frontier) {\n\tvar edges = [], all = D.opens.concat(D.clicks), i;\n\tfor (i = 0; i < all.length; i++) {\n\t\tvar x = all[i];\n\t\tif (!x.sa && x.a <= frontier) { edges.push(x.a); x.sa = 1; }\n\t\tif (!x.sb && x.b <= frontier) { edges.push(x.b); x.sb = 1; }\n\t}\n\tif (!edges.length) return;\n\tedges.sort(function (p, q) { return p - q; });\n\tvar half = H.fade * S.sr / 2;\n\tfor (i = 0; i < edges.length; i++) {\n\t\tif (i && edges[i] === edges[i - 1]) continue;\n\t\tvar g1 = gainAt(edges[i]), g0 = gainAt(edges[i] - 1);\n\t\tif (g1 !== g0) out.fad.push([t2b(edges[i] - half), g1, S.gen, 0]);\n\t}\n\tvar keep = frontier - 2 * S.sr;\n\tD.opens = D.opens.filter(function (o) { return !(o.sa && o.sb && o.b < keep); });\n\tD.clicks = D.clicks.filter(function (c) { return !(c.sa && c.sb && c.b < keep); });\n}\n\n// ------------------------------------------------------------------ class Scratch: timing\nfunction at(b, jit) {\n\tvar sd = jit === 'turn' ? H.turn_jit : H.grid_jit;\n\tD.late = H.drift * D.late + (1 - H.drift) * normal(H.grid_jit * 1.5);\n\treturn Math.floor(b2t(sw(b)) + (D.late + normal(sd)) * S.sr);\n}\nfunction err(dist) {\n\tvar e = normal(H.land + H.land_per * dist);\n\tif (rng() < H.flub * 0.5) e -= 0.03 + 0.03 * rng();      // pulled back too far\n\treturn e;\n}\nfunction trav(u, sp, cap) {\n\tvar t = u * spbS() / S.sr * sp * Math.max(0.4, 1 + normal(H.trav)) * S.velT;\n\treturn cap ? Math.min(t, cap) : t;\n}\nfunction start(b) {\n\tvar tb = at(b);\n\tif (Math.abs(D.p - D.cue) > 0.08) {                     // get to the cue first, fader closed\n\t\tvar dist = Math.abs(D.p - D.cue), need = Math.floor(Math.min(0.06 + dist / 4.0, 0.40) * S.sr), ta = D.t;\n\t\tto(D.t + need, D.cue + err(dist), 'pull', null);\n\t\tD.clicks.push({a: Math.floor(ta - 0.004 * S.sr), b: D.t, sa: 0, sb: 0});\n\t\tif (HOST.trace) HOST.trace('cuemove', {tb: t2b(ta), te: t2b(D.t)});\n\t}\n\thold(tb);\n\treturn D.t;\n}\nfunction fwd(b1, travel, kind) { stroke(at(b1, 'turn'), travel, kind || 'push'); return D.t; }\nfunction back(b1, kind) { to(at(b1), D.cue + err(Math.abs(D.p - D.cue)), kind || 'pull'); return D.t; }\nfunction silentBack(b1) {\n\tvar dist = Math.abs(D.p - D.cue), t1 = at(b1);\n\tvar quick = D.t + Math.floor((0.06 + dist / 2.5) * S.sr), least = D.t + Math.floor((0.04 + dist / 5.0) * S.sr);\n\tto(Math.max(Math.min(t1, quick), least), D.cue + err(dist), 'pull', null);\n}\nvar MS2 = 0.002;\n\n// ------------------------------------------------------------------ the words\nvar W = {\n\tbaby: function (b, u, n, sp, cap, kind) {\n\t\tif (n === undefined) n = 1; if (sp === undefined) sp = 1.0; kind = kind || 'push';\n\t\tvar t0 = start(b);\n\t\tfor (var i = 0; i < n; i++) {\n\t\t\tfwd(b + (2 * i + 1) * u, trav(u, sp, cap), kind);\n\t\t\tback(b + (2 * i + 2) * u, kind === 'push' ? 'pull' : kind);\n\t\t}\n\t\topen(t0 - MS2 * S.sr, D.t);\n\t},\n\tdrag: function (b, u, n, sp) { W.baby(b, u, n || 1, sp || 0.35, null, 'even'); },\n\ttip: function (b, u, n) { W.baby(b, u, n || 2, 0.9, 0.045); },\n\tforward: function (b, snd, ret, amp, frac) {\n\t\tif (amp === undefined) amp = 1.0; if (frac === undefined) frac = 0.86;\n\t\tvar t0 = start(b); play(at(b + snd * frac)); var t1 = D.t;\n\t\topen(t0 - MS2 * S.sr, t1, amp); silentBack(b + snd + ret);\n\t},\n\treverse: function (b, snd, pre, sp) {\n\t\tstart(b - pre); stroke(at(b), trav(snd, sp || 1.0), 'push'); var t1 = D.t;\n\t\tback(b + snd); open(t1, D.t);\n\t},\n\tstab: function (b, snd, ret, sp) {\n\t\tif (snd === undefined) snd = 0.125; if (ret === undefined) ret = 0.125;\n\t\tvar t0 = start(b); fwd(b + snd, trav(snd, sp || 1.7), 'flick'); var t1 = D.t;\n\t\topen(t0 - MS2 * S.sr, t0 + 0.72 * (t1 - t0)); silentBack(b + snd + ret);\n\t},\n\tlazer: function (b, snd, ret, travel, tail) {\n\t\tif (snd === undefined) snd = 0.125; if (ret === undefined) ret = 0.375;\n\t\tif (travel === undefined) travel = 0.26; if (tail === undefined) tail = true;\n\t\tvar t0 = start(b);\n\t\tstroke(at(b + snd, 'turn'), travel * (1 + normal(H.trav)) * S.velT, 'flick', 4.5); var t1 = D.t;\n\t\tif (tail) { back(b + snd + ret, 'even'); open(t0 - MS2 * S.sr, D.t); }\n\t\telse { open(t0 - MS2 * S.sr, t1); silentBack(b + snd + ret); }\n\t},\n\tchirp: function (b, u, n, sp) {\n\t\tif (n === undefined) n = 1; if (sp === undefined) sp = 1.1;\n\t\tvar t0 = start(b), a = t0 - MS2 * S.sr;\n\t\tfor (var i = 0; i < n; i++) {\n\t\t\tvar f0 = D.t, f1 = fwd(b + (2 * i + 1) * u, trav(u, sp)), b1 = back(b + (2 * i + 2) * u);\n\t\t\topen(a, f0 + 0.52 * (f1 - f0)); a = f1 + 0.50 * (b1 - f1);\n\t\t}\n\t\topen(a, D.t);\n\t},\n\tflare: function (b, u, n, clicks, sp) {\n\t\tif (sp === undefined) sp = 1.0;\n\t\tvar t0 = start(b);\n\t\tfor (var i = 0; i < 2 * n; i++) {\n\t\t\tvar a = D.t, z = (i % 2 === 0) ? fwd(b + (i + 1) * u, trav(u, sp)) : back(b + (i + 1) * u);\n\t\t\tfor (var c = 0; c < clicks; c++) click(a + (c + 1) / (clicks + 1) * (z - a));\n\t\t}\n\t\topen(t0 - MS2 * S.sr, D.t);\n\t},\n\tcrab: function (b, u, taps, both, sp) {\n\t\tif (taps === undefined) taps = 4; if (both === undefined) both = true; if (sp === undefined) sp = 0.9;\n\t\tstart(b);\n\t\tfor (var i = 0; i < (both ? 2 : 1); i++) {\n\t\t\tvar a = D.t, z = i === 0 ? fwd(b + u, trav(u, sp)) : back(b + 2 * u);\n\t\t\tvar gap = Math.min((z - a) / taps, 0.060 * S.sr) * (1 + 0.1 * H.T * gauss());\n\t\t\tfor (var k = 0; k < taps; k++) open(a + k * gap, a + (k + 0.52) * gap);\n\t\t}\n\t\tif (!both) silentBack(b + 2 * u);\n\t},\n\ttear: function (b, u, form, sp) {\n\t\tif (sp === undefined) sp = 1.1;\n\t\tvar t0 = start(b), tr = trav(u, sp), c = D.cue;\n\t\tif ((form || 'f-bb') === 'f-bb') { fwd(b + u, tr); to(at(b + 2 * u, 'turn'), c + tr * 0.48, 'pull'); back(b + 3 * u); }\n\t\telse { fwd(b + u, tr * 0.5); fwd(b + 2 * u, tr * 0.5); back(b + 3 * u); }\n\t\topen(t0 - MS2 * S.sr, D.t);\n\t},\n\tscribble: function (b, dur, f, amp) {\n\t\tvar t0 = start(b); wiggle(at(b + dur), f || 15.0, (amp || 1.25) * S.velT); open(t0 - MS2 * S.sr, D.t);\n\t},\n\ttransform: function (b, dur, step, duty, pat, mode, sp) {\n\t\tif (duty === undefined) duty = 0.5; mode = mode || 'drag'; if (sp === undefined) sp = 0.45;\n\t\tvar t0 = start(b);\n\t\tif (mode === 'drag') { stroke(at(b + dur / 2, 'turn'), trav(dur / 2, sp), 'even'); back(b + dur, 'even'); }\n\t\telse play(at(b + dur));\n\t\tfor (var k = 0; k * step < dur - 1e-6; k++) {\n\t\t\tif (!pat || pat[k % pat.length]) {\n\t\t\t\tvar g0 = at(b + k * step), g1 = at(b + (k + duty) * step);\n\t\t\t\tif (g1 > t0) open(Math.max(g0, t0), g1);\n\t\t\t}\n\t\t}\n\t\tif (mode !== 'drag') silentBack(b + dur + 0.5);\n\t},\n\trelease: function (b, n, ret) {\n\t\tvar t0 = start(b); play(at(b + n)); open(t0 - MS2 * S.sr, D.t); silentBack(b + n + ret);\n\t},\n\techo: function (b, u, n, k) { for (var i = 0; i < n; i++) W.forward(b + i * u, u * 0.55, u * 0.45, Math.pow(k, i)); }\n};\n\n// ------------------------------------------------------------------ keys: one unit of each word\n// returns the unit length in beats; u is the SPEED in beats\nvar UNIT = {\n\tbaby: [1, function (b, u) { W.baby(b, u); return 2 * u; }],\n\tforward: [1, function (b, u) { W.forward(b, u, u); return 2 * u; }],\n\tstab: [0.5, function (b, u) { W.stab(b, u / 2, u / 2); return u; }],\n\tchirp: [1, function (b, u) { W.chirp(b, u); return 2 * u; }],\n\tflare1: [2, function (b, u) { W.flare(b, 2 * u, 1, 1); return 4 * u; }],\n\tflare2: [2, function (b, u) { W.flare(b, 2 * u, 1, 2); return 4 * u; }],\n\tcrab: [2, function (b, u) { W.crab(b, 2 * u); return 4 * u; }],\n\ttear: [1, function (b, u) { W.tear(b, u); return 4 * u; }],\n\tscribble: [2, function (b, u) { W.scribble(b, 2 * u); return 2 * u; }],\n\ttransform: [1, function (b, u) { W.transform(b, 8 * u, u, 0.5, [1, 1, 0, 1]); return 8 * u; }],\n\tdrag: [2, function (b, u) { W.drag(b, 2 * u); return 4 * u; }],\n\ttip: [0.5, function (b, u) { W.tip(b, u / 2, 2); return 2 * u; }],\n\tlazer: [0.5, function (b, u) { W.lazer(b, u / 2, 1.5 * u); return 2 * u; }],\n\techo: [1, function (b, u, i) { W.forward(b, 1.1 * u, 0.9 * u, Math.pow(0.6, i)); return 2 * u; }],\n\treverse: [1, function (b, u) { W.reverse(b + u, u, u); return 2 * u; }]\n};\nfunction speedFor(w) {\n\tvar u = S.ctl.speed, f = UNIT[w] ? UNIT[w][0] : 1, i = SPEEDORDER.indexOf(u);\n\twhile (f * u * 60 / S.tempo < 0.045 && i < SPEEDORDER.length - 1) u = SPEEDORDER[++i];\n\treturn u;\n}\n\n// ------------------------------------------------------------------ Instinct cells (make_demos.py)\n// name: [beats, weight, difficulty, words used, function]\nvar CELLS = {\n\t'rest': [1, 3, 0, [], function (b) {}],\n\t'baby8': [1, 4, 1, ['baby'], function (b) { W.baby(b, 0.5); }],\n\t'baby16': [1, 5, 2, ['baby'], function (b) { W.baby(b, 0.25); W.baby(b + 0.5, 0.25); }],\n\t'pickup': [1, 4, 1, ['baby'], function (b) { W.baby(b + 0.5, 0.25); }],\n\t'baby-rest': [1, 3, 1, ['baby'], function (b) { W.baby(b, 0.25); }],\n\t'fwd-fwd': [1, 4, 2, ['forward'], function (b) { W.forward(b, 0.25, 0.25); W.forward(b + 0.5, 0.25, 0.25); }],\n\t'fwd-baby': [1, 5, 2, ['forward', 'baby'], function (b) { W.forward(b, 0.25, 0.25); W.baby(b + 0.5, 0.25); }],\n\t'stabs': [1, 3, 3, ['stab'], function (b) { W.stab(b); W.stab(b + 0.25); W.stab(b + 0.5); }],\n\t'stab-off': [1, 3, 2, ['stab'], function (b) { W.stab(b + 0.5); W.stab(b + 0.75); }],\n\t'chirp': [1, 4, 3, ['chirp'], function (b) { W.chirp(b, 0.25, 2); }],\n\t'chirp-baby': [1, 3, 2, ['chirp', 'baby'], function (b) { W.chirp(b, 0.25, 1); W.baby(b + 0.5, 0.25); }],\n\t'flare1': [1, 3, 3, ['flare1'], function (b) { W.flare(b, 0.5, 1, 1); }],\n\t'flare2': [1, 2, 4, ['flare2'], function (b) { W.flare(b, 0.5, 1, 2); }],\n\t'crab': [1, 2, 4, ['crab'], function (b) { W.crab(b, 0.5); }],\n\t'tear': [1, 3, 3, ['tear', 'stab'], function (b) { W.tear(b, 0.25); W.stab(b + 0.75); }],\n\t'tear-trip': [1, 2, 3, ['tear'], function (b) { W.tear(b, 1 / 3); }],\n\t'scribble': [1, 2, 3, ['scribble', 'baby'], function (b) { W.scribble(b, 0.5); W.baby(b + 0.5, 0.25); }],\n\t'tips': [1, 2, 4, ['tip', 'baby'], function (b) { W.tip(b, 0.125, 2); W.baby(b + 0.5, 0.25); }],\n\t'lazer': [1, 1.5, 2, ['lazer', 'baby'], function (b) { W.lazer(b, 0.125, 0.375); W.baby(b + 0.5, 0.25); }],\n\t'transform': [2, 2, 3, ['transform'], function (b) { W.transform(b, 2, 0.25, 0.5, [1, 1, 0, 1]); }],\n\t'transform-trip': [2, 1.2, 4, ['transform'], function (b) { W.transform(b, 2, 1 / 6); }],\n\t'drag': [2, 1.5, 1, ['drag'], function (b) { W.drag(b, 1.0); }],\n\t'echo': [2, 1.2, 2, ['echo'], function (b) { W.echo(b, 0.5, 4, 0.6); }],\n\t'release': [2, 0, 1, ['release'], function (b) { W.release(b, 1.5, 0.5); }]\n};\nvar CELLNAMES = [];\nfor (var cn in CELLS) CELLNAMES.push(cn);\nvar ROUTINE_DEFAULT = ['tear-trip', 'tear-trip', 'chirp', 'fwd-fwd', 'tear-trip', 'tear-trip', 'chirp', 'fwd-fwd',\n\t'fwd-baby', 'stabs', 'rest', 'baby16', 'pickup', 'stabs', 'rest', 'baby16'];\nfor (var ri = 0; ri < 16; ri++) { S.routine.push(ROUTINE_DEFAULT[ri]); S.routineCue.push(0); }\n\nfunction cellOn(nm) {\n\tvar ws = CELLS[nm][3];\n\tfor (var i = 0; i < ws.length; i++) if (!S.on[ws[i]]) return false;\n\treturn true;\n}\nfunction cellWeight(nm) {\n\tvar c = CELLS[nm], d = S.ctl.density;\n\tif (nm === 'rest') return c[1] * (1.3 - 1.25 * d) * 1.5;\n\tif (!cellOn(nm)) return 0;\n\tif (c[2] > 1 + 3.2 * S.ctl.skill) return 0;\n\tvar w = c[1] * Math.exp(-Math.abs(c[2] - (1.6 + 2.6 * d)) * 0.6);\n\tif (nm === 'tear-trip' || nm === 'transform-trip') {\n\t\tvar f = S.ctl.funk;\n\t\tw *= f < 0.5 ? 0.3 + 1.4 * f : 1 + 2 * (f - 0.5);\n\t}\n\treturn w;\n}\nfunction pick(names, ws) {\n\tvar tot = 0, i;\n\tfor (i = 0; i < ws.length; i++) tot += ws[i];\n\tif (!(tot > 0)) return null;\n\tvar r = rng() * tot;\n\tfor (i = 0; i < ws.length; i++) { r -= ws[i]; if (r < 0) return names[i]; }\n\treturn names[names.length - 1];\n}\nfunction instBar() {\n\tvar o = [], left = 4;\n\twhile (left > 0) {\n\t\tvar ok = [], ws = [];\n\t\tfor (var i = 0; i < CELLNAMES.length; i++) {\n\t\t\tvar n = CELLNAMES[i];\n\t\t\tif (CELLS[n][0] > left || (o.length && o[o.length - 1] === 'rest' && n === 'rest')) continue;\n\t\t\tok.push(n); ws.push(cellWeight(n));\n\t\t}\n\t\tvar nm = pick(ok, ws) || 'rest';\n\t\to.push(nm); left -= CELLS[nm][0];\n\t}\n\treturn o;\n}\nfunction vary(a) {\n\ta = a.slice();\n\tvar i = Math.floor(rng() * a.length), n = CELLS[a[i]][0], ok = [], ws = [];\n\tfor (var j = 0; j < CELLNAMES.length; j++) {\n\t\tvar x = CELLNAMES[j];\n\t\tif (CELLS[x][0] === n && x !== a[i] && cellWeight(x) > 0) { ok.push(x); ws.push(cellWeight(x)); }\n\t}\n\tvar nm = pick(ok, ws);\n\tif (nm) a[i] = nm;\n\treturn a;\n}\nfunction ending(A, B) {\n\tvar roomForRelease = R.have && (D.hi - cueSec()) >= 60 / S.tempo;\n\tif (rng() < S.ctl.release && S.on.release && roomForRelease) {\n\t\tvar C = vary(A), n = 0, keep = [];\n\t\tfor (var i = 0; i < C.length; i++) if (n + CELLS[C[i]][0] <= 2) { keep.push(C[i]); n += CELLS[C[i]][0]; }\n\t\twhile (n < 2) { keep.push(S.on.baby ? 'baby16' : 'rest'); n += 1; }\n\t\treturn keep.concat(['release']);\n\t}\n\treturn vary(B);\n}\n// one Instinct phrase of nb bars (1, 2 or 4) from beat b\nfunction instPhrase(b, nb) {\n\tif (S.ctl.walk > 0 && R.cues.length > 1 && rng() < S.ctl.walk) {\n\t\tvar ci = Math.floor(rng() * R.cues.length);\n\t\tS.calls.push({b: b, len: 0, run: function () { setCue(ci); }});\n\t}\n\tvar A = instBar(), A2 = rng() < S.ctl.motif ? A : vary(A), B = instBar(), C = ending(A, B);\n\tvar bars = nb >= 4 ? [A, A2, B, C] : nb === 2 ? [A, C] : [C];\n\tif (HOST.trace) HOST.trace('phrase', {b: b, bars: bars});\n\tfor (var k = 0; k < bars.length; k++) {\n\t\tvar bb = b + 4 * k;\n\t\tfor (var i = 0; i < bars[k].length; i++) {\n\t\t\tvar nm = bars[k][i];\n\t\t\tS.calls.push({b: bb, len: CELLS[nm][0], cell: nm});\n\t\t\tbb += CELLS[nm][0];\n\t\t}\n\t}\n}\nfunction generate() {\n\tvar c = S.ctl, b = S.genNext;\n\tif (c.mode === 2) {\n\t\tvar on = c.phon, off = c.phoff;\n\t\tfor (var k = 0; k < on; k += 4) instPhrase(b + 4 * k, Math.min(4, on - k));\n\t\tS.genNext = b + 4 * (on + off);\n\t} else if (c.mode === 1) {\n\t\tvar skip = 0;\n\t\tfor (var i = 0; i < 16; i++) {\n\t\t\tvar cue = S.routineCue[i];\n\t\t\tif (cue > 0) (function (ci) { S.calls.push({b: b + i, len: 0, run: function () { setCue(ci); }}); })(cue - 1);\n\t\t\tif (skip) { skip = 0; continue; }\n\t\t\tvar nm = S.routine[i] || 'rest';\n\t\t\tif (!cellOn(nm)) nm = 'rest';\n\t\t\tif (CELLS[nm][0] === 2) { if (i === 15) nm = 'rest'; else skip = 1; }\n\t\t\tS.calls.push({b: b + i, len: CELLS[nm][0], cell: nm});\n\t\t}\n\t\tS.genNext = b + 16;\n\t}\n}\n\n// ------------------------------------------------------------------ planning\nfunction keyWord() {\n\tif (S.fresh) return S.fresh;\n\treturn S.held.length ? S.held[S.held.length - 1] : null;\n}\nfunction readStatus() {\n\tvar st = HOST.status();\n\tif (!st) return null;\n\tS.B = st[0];\n\tif (st[8] > 1000 && st[8] !== S.sr) { S.sr = st[8]; sendFilters(); if (R.have && !R.cutting) startCut(); resetPlan(true); }\n\tif (st[9] !== S.jumps) { var first = S.jumps < 0; S.jumps = st[9]; if (!first) resetPlan(true); }\n\treturn st;\n}\nfunction nowB() { var st = readStatus(); return st ? st[0] : S.B; }\n\n// start the timeline again from where gen~ is now (after a jump, a new record, a tempo change)\nfunction resetPlan(flush, p) {\n\tvar st = HOST.status(), b = st ? st[0] : S.B;\n\tif (flush) { S.gen++; HOST.gen('fgen', S.gen); }\n\tS.Ban = b;\n\tD = newDeck(0, p !== undefined ? p : st ? st[4] : cueSec(), 0);\n\tout.seg = []; out.fad = [];\n\tS.gridEnd = -1e9; S.calls = []; S.genNext = null; S.rel = null;\n}\n\nfunction execCell(c) {\n\tif (c.run) { c.run(); return; }\n\tif (D.t < b2t(nowB()) + 0.005 * S.sr) hold(b2t(nowB()) + 0.005 * S.sr);\n\tD.cue = cueSec();\n\tS.velT = 1; S.velA = 1; S.swingHold = null;\n\tif (HOST.trace) HOST.trace('cell', {cell: c.cell, b: c.b});\n\tCELLS[c.cell][4](c.b);\n\tS.gridEnd = Math.max(S.gridEnd, c.b + c.len);\n}\n\nfunction planKeyUnit(k, b) {\n\tvar w = k.w;\n\tif (D.t < b2t(nowB()) + 0.005 * S.sr) hold(b2t(nowB()) + 0.005 * S.sr);\n\tD.cue = cueSec();\n\tS.velT = 0.8 + 0.4 * k.vel / 127; S.velA = 0.7 + 0.3 * k.vel / 127;\n\tif (w === 'release') { startRelease(b); return; }\n\tvar u = speedFor(w);\n\tS.swingHold = Math.abs(u - 1 / 6) < 1e-9 ? 0.5 : null;\n\tif (w === 'echo') S.echoN = (S.lastKeyWord === 'echo') ? S.echoN + 1 : 0;\n\tif (HOST.trace) HOST.trace('unit', {w: w, b: b});\n\tvar len = UNIT[w][1](b, u, S.echoN);\n\tS.unitLen = len;\n\tS.swingHold = null; S.velT = 1; S.velA = 1;\n\tS.gridEnd = b + len;\n\tS.lastKeyWord = w;\n}\n\n// Release: let go and keep playing until the key comes up (or the record ends)\nfunction startRelease(b) {\n\tvar t0 = start(b), hi = D.hi, pS = D.p, vS = D.v;\n\tvar tEnd = D.t + Math.max(Math.floor((hi - D.p) * S.sr), 1);\n\tplay(tEnd);\n\tvar o = open(t0 - MS2 * S.sr, D.t);\n\tS.rel = {open: o, tEnd: D.t, b: b, t0: t0, pStart: pS, vStart: vS};\n\tif (HOST.trace) HOST.trace('unit', {w: 'release', b: b});\n\tS.gridEnd = t2b(D.t);\n\tS.lastKeyWord = 'release';\n\tS.velT = 1; S.velA = 1;\n}\nfunction cutRelease(bq) {\n\tvar r = S.rel; S.rel = null;\n\tif (!r) return;\n\tvar tq = Math.max(Math.floor(b2t(sw(bq))), r.t0 + Math.floor(0.03 * S.sr));\n\tif (tq < r.tEnd) {\n\t\t// where the record is at tq, from the play segment's closed form\n\t\tvar m = tq - r.t0, rr = Math.exp(-1 / (H.tau_rel * S.sr));\n\t\tD.t = tq; D.p = r.pStart + (m + decaySum(r.vStart - 1, rr, m)) / S.sr;\n\t\tD.v = 1 + (r.vStart - 1) * Math.pow(rr, m);\n\t\tif (r.open) r.open.b = Math.floor(tq + (normal(H.fad_jit) + H.leak * rng()) * S.sr);\n\t}\n\tsilentBack(t2b(D.t) + 0.25);\n\tS.gridEnd = qUp(t2b(D.t), 0.25);\n}\n\nfunction plan() {\n\tif (!R.have || R.cutting || !H || !D) return;\n\tvar B = nowB(), nowT = b2t(B), SR = S.sr;\n\tvar LA = Math.max(0.5 * spbS(), 0.15 * SR);\n\tvar k = keyWord();\n\t// a held Release ends at the record's end even if the key is still down\n\tif (S.rel && S.rel.tEnd < nowT + LA) cutRelease(t2b(S.rel.tEnd));\n\tif (k && !S.rel) {\n\t\tvar unitBeats = S.lastKeyWord === k.w ? S.unitLen : 2 * speedFor(k.w);\n\t\tvar bNext = Math.max(S.gridEnd, B + 0.012 * S.tempo / 60);\n\t\tif (bNext > S.gridEnd + 1e-6) bNext = qUp(bNext, S.ctl.quant);\n\t\tvar la = Math.max(0.5 * unitBeats * spbS(), 0.1 * SR);\n\t\tvar done = !S.fresh && S.lastKeyWord === k.w && (k.w === 'release' || (k.w === 'echo' && S.echoN >= 5));\n\t\tif (b2t(bNext) - nowT < la && !done) {\n\t\t\tS.calls = []; S.genNext = null;\n\t\t\tplanKeyUnit(k, bNext);\n\t\t\tif (S.fresh === k) S.fresh = null;\n\t\t}\n\t} else if (!k && !S.rel && S.ctl.mode > 0) {\n\t\tif (S.genNext === null) S.genNext = Math.ceil((Math.max(S.gridEnd, B) + 0.05) / 4) * 4;\n\t\twhile (!S.calls.length || S.calls[S.calls.length - 1].b < B + 4) {\n\t\t\tvar before = S.calls.length; generate();\n\t\t\tif (S.calls.length === before && S.genNext > B + 64) break;\n\t\t}\n\t\twhile (S.calls.length && b2t(S.calls[0].b) < nowT + LA) {\n\t\t\tvar c = S.calls.shift();\n\t\t\tif (c.b < B - 0.1) continue;\n\t\t\texecCell(c);\n\t\t}\n\t}\n\tif (!k && S.ctl.mode === 0 && S.lastKeyWord !== null && S.gridEnd < B) S.lastKeyWord = null;\n\t// fader events that can no longer change\n\t// (a word planned later starts after D.t, or after a key press at now + 12 ms; an edge it\n\t// adds before one already sent is applied late, by at most the margin, so keep it small)\n\tvar frontier = Math.max(D.t - 0.03 * SR, nowT + 0.06 * SR);\n\tif (S.rel) frontier = Math.max(nowT - 0.018 * SR, S.rel.open ? S.rel.open.a : 0);\n\temitFader(frontier);\n\tflushOut();\n}\n\nfunction flushOut() {\n\tvar st = HOST.status(), qh = st ? st[2] : qtail, fh = st ? st[3] : ftail;\n\tvar wrote = false;\n\twhile (out.seg.length && qtail - qh < QN - 2) { HOST.writeSeg(qtail % QN, out.seg.shift()); qtail++; wrote = true; }\n\tif (wrote) HOST.gen('qtail', qtail);\n\twrote = false;\n\twhile (out.fad.length && ftail - fh < FN - 2) { HOST.writeFad(ftail % FN, out.fad.shift()); ftail++; wrote = true; }\n\tif (wrote) HOST.gen('ftail', ftail);\n}\n\n// ------------------------------------------------------------------ keys and buttons\nfunction wordPress(w, vel) {\n\tif (WORDS.indexOf(w) < 0) return;\n\tvar i;\n\tif (vel > 0) {\n\t\tfor (i = S.held.length - 1; i >= 0; i--) if (S.held[i].w === w) S.held.splice(i, 1);\n\t\tvar k = {w: w, vel: vel};\n\t\tS.held.push(k); S.fresh = k;\n\t\tif (S.rel && w !== 'release') cutRelease(qUp(nowB() + 0.012 * S.tempo / 60, S.ctl.quant || 0.0001));\n\t\tif (S.ctl.mode > 0) { S.calls = []; S.genNext = null; }\n\t} else {\n\t\tfor (i = S.held.length - 1; i >= 0; i--) if (S.held[i].w === w) S.held.splice(i, 1);\n\t\tif (w === 'release' && S.rel) {\n\t\t\tvar B = nowB(), bq = qUp(Math.max(B + 0.012 * S.tempo / 60, S.rel.b + 0.25), S.ctl.quant || 0.0001);\n\t\t\tcutRelease(bq);\n\t\t}\n\t\tif (!S.held.length && S.ctl.mode > 0) S.genNext = null;\n\t}\n\tplan();\n}\nfunction noteIn(pitch, vel) {\n\tif (pitch >= KEY0 && pitch < KEY0 + 16) { wordPress(WORDS[pitch - KEY0], vel); return; }\n\tif (pitch >= CUEKEY0 && pitch < CUEKEY0 + 8 && vel > 0) setCue(pitch - CUEKEY0, true);\n}\nfunction setCue(i, fromKey) {\n\tif (!R.cues.length) return;\n\tS.cueIdx = Math.max(0, Math.min(i, R.cues.length - 1));\n\tHOST.ui('cue', 'set', S.cueIdx + 1);\n\tHOST.ui('strip', 'sel', S.cueIdx);\n}\n\n// ------------------------------------------------------------------ the cartridge stage\nfunction section(fz, fp, fs) {\n\tvar a = Math.tan(Math.PI * fz / fs), b = Math.tan(Math.PI * fp / fs);\n\tvar n0 = 1 + 1 / a, n1 = 1 - 1 / a, d0 = 1 + 1 / b, d1 = 1 - 1 / b;\n\treturn [n0 / d0, n1 / d0, d1 / d0];      // b0 b1 a1, a0 = 1\n}\nfunction phonoSections(fs) {\n\tvar T1 = 3180e-6, T2 = 318e-6, T3 = 75e-6, F4 = 20000.0;\n\tvar F1 = 1 / (2 * Math.PI * T1), F2 = 1 / (2 * Math.PI * T2), F3 = 1 / (2 * Math.PI * T3);\n\treturn [section(F2, F1, fs), section(F4, F3, fs)];\n}\nfunction gainAt1k(secs, fs) {\n\tvar w = 2 * Math.PI * 1000 / fs, re = Math.cos(w), im = -Math.sin(w), g = 1;\n\tfor (var i = 0; i < secs.length; i++) {\n\t\tvar s = secs[i];\n\t\tvar nr = s[0] + s[1] * re, ni = s[1] * im, dr = 1 + s[2] * re, di = s[2] * im;\n\t\tg *= Math.sqrt((nr * nr + ni * ni) / (dr * dr + di * di));\n\t}\n\treturn g;\n}\nfunction sendFilters() {\n\tvar fs = S.sr * CUTOS, ps = phonoSections(fs);\n\tHOST.gen('pb0a', ps[0][0]); HOST.gen('pb1a', ps[0][1]); HOST.gen('pa1a', ps[0][2]);\n\tHOST.gen('pb0b', ps[1][0]); HOST.gen('pb1b', ps[1][1]); HOST.gen('pa1b', ps[1][2]);\n\tHOST.gen('g1k', 1 / gainAt1k(ps, fs));\n\tvar best = 44100, bd = 1e9;\n\tfor (var r in LOWPASS) { var d = Math.abs(r - S.sr); if (d < bd) { bd = d; best = r; } }\n\tvar L = LOWPASS[best], names = ['a', 'b', 'c'];\n\tfor (var i = 0; i < 3; i++) {\n\t\tHOST.gen('lb0' + names[i], L[i * 6]); HOST.gen('lb1' + names[i], L[i * 6 + 1]); HOST.gen('lb2' + names[i], L[i * 6 + 2]);\n\t\tHOST.gen('la1' + names[i], L[i * 6 + 4]); HOST.gen('la2' + names[i], L[i * 6 + 5]);\n\t}\n}\n\n// upsample 4 times and apply the recording curve (inverse of the phono stage), in slices\nvar KER = null, KN = 64;\nfunction besselI0(x) { var s = 1, t = 1; for (var k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }\nfunction kernel() {\n\tif (KER) return KER;\n\tKER = new Float64Array(2 * KN + 1);\n\tvar beta = 9.0, i0b = besselI0(beta);\n\tfor (var i = 0; i <= 2 * KN; i++) {\n\t\tvar k = i - KN, x = k / CUTOS * 0.92, sinc = x === 0 ? 1 : Math.sin(Math.PI * x) / (Math.PI * x);\n\t\tvar r = k / KN, win = besselI0(beta * Math.sqrt(Math.max(0, 1 - r * r))) / i0b;\n\t\tKER[i] = sinc * 0.92 * win;\n\t}\n\treturn KER;\n}\nvar CUT = null, poscmd = 0;\nfunction startCut() {\n\tvar fs = R.sr * CUTOS, ps = phonoSections(fs), g = gainAt1k(ps, fs);\n\tCUT = {pos: 0, ch: [R.L, R.R], st: [[0, 0, 0, 0], [0, 0, 0, 0]], ps: ps, g: g, n: R.n};\n\tR.cutting = 1;\n\tHOST.gen('haverec', 0);\n\tHOST.cutBegin(R.n * CUTOS);\n\tHOST.ui('status', 'set', 'CUTTING');\n}\n// returns true when finished\nfunction cutSlice(maxIn) {\n\tif (!CUT) return true;\n\tvar h = kernel(), n = CUT.n, a = CUT.pos, b = Math.min(n, a + maxIn), ps = CUT.ps;\n\t// inverse sections: y = (x + a1 * xprev - b1 * yprev) / b0  (numerator and denominator swapped)\n\tvar i0a = 1 / ps[0][0], i0b = 1 / ps[1][0];\n\tfor (var c = 0; c < 2; c++) {\n\t\tvar x = CUT.ch[c], st = CUT.st[c], y = new Float32Array((b - a) * CUTOS);\n\t\tvar xa = st[0], ya = st[1], xb = st[2], yb = st[3];\n\t\tfor (var n0 = a; n0 < b; n0++) {\n\t\t\tfor (var r = 0; r < CUTOS; r++) {\n\t\t\t\tvar s = 0;\n\t\t\t\tfor (var d = -16; d <= 16; d++) {\n\t\t\t\t\tvar hi = 4 * d + r + KN;\n\t\t\t\t\tif (hi < 0 || hi > 2 * KN) continue;\n\t\t\t\t\tvar xi = n0 - d;\n\t\t\t\t\tif (xi >= 0 && xi < n) s += x[xi] * h[hi];\n\t\t\t\t}\n\t\t\t\tvar ta = (s + ps[0][2] * xa - ps[0][1] * ya) * i0a; xa = s; ya = ta;\n\t\t\t\tvar tb = (ta + ps[1][2] * xb - ps[1][1] * yb) * i0b; xb = ta; yb = tb;\n\t\t\t\ty[(n0 - a) * CUTOS + r] = tb * CUT.g;\n\t\t\t}\n\t\t}\n\t\tst[0] = xa; st[1] = ya; st[2] = xb; st[3] = yb;\n\t\tHOST.cutWrite(c, a * CUTOS, y);\n\t}\n\tCUT.pos = b;\n\tif (b >= n) {\n\t\tCUT = null; R.cutting = 0;\n\t\tHOST.gen('recsr4', R.sr * CUTOS); HOST.gen('rlen', n * CUTOS);\n\t\tHOST.gen('posset', cueSec()); HOST.gen('poscmd', ++poscmd);\n\t\tresetPlan(true, cueSec());\n\t\tHOST.gen('haverec', 1);\n\t\tshowRecord();\n\t\treturn true;\n\t}\n\treturn false;\n}\n\n// ------------------------------------------------------------------ the record\nfunction findCues(mono, sr) {\n\tvar hop = Math.round(0.010 * sr), nf = Math.floor(mono.length / hop), db = new Float64Array(nf), f, i;\n\tfor (f = 0; f < nf; f++) {\n\t\tvar s = 0;\n\t\tfor (i = f * hop; i < (f + 1) * hop; i++) s += mono[i] * mono[i];\n\t\tdb[f] = 10 * Math.log(s / hop + 1e-12) / Math.LN10;\n\t}\n\tvar first = 0;\n\tfor (i = 0; i < mono.length; i++) if (Math.abs(mono[i]) > 0.01) { first = i; break; }\n\tvar c0 = Math.max(0, first / sr - 0.005), back = 5, rises = [];\n\tfor (f = back; f < nf; f++) {\n\t\tvar r = db[f] - db[f - back];\n\t\tif (r >= 9 && db[f] > -45 && (f === back || db[f - 1] - db[f - 1 - back] < 9)) rises.push([f, r]);\n\t}\n\trises.sort(function (a, b) { return b[1] - a[1]; });\n\tvar cues = [c0], mind = 0.12;\n\tfor (i = 0; i < rises.length && cues.length < 8; i++) {\n\t\tvar t = Math.max(0, (rises[i][0] - back + 1) * hop / sr);\n\t\t// slide to where the rise starts: the first frame after the quiet one\n\t\tfor (var g = rises[i][0] - back + 1; g <= rises[i][0]; g++) if (db[g] - db[g - 1] > 3) { t = g * hop / sr; break; }\n\t\tt = Math.max(0, t - 0.005);\n\t\tvar ok = true;\n\t\tfor (var j = 0; j < cues.length; j++) if (Math.abs(cues[j] - t) < mind) ok = false;\n\t\tif (ok) cues.push(t);\n\t}\n\tcues.sort(function (a, b) { return a - b; });\n\treturn cues;\n}\nfunction overview(mono, n) {\n\tvar o = [], per = mono.length / n;\n\tfor (var k = 0; k < n; k++) {\n\t\tvar m = 0;\n\t\tfor (var i = Math.floor(k * per); i < Math.floor((k + 1) * per); i += 4) { var v = Math.abs(mono[i]); if (v > m) m = v; }\n\t\to.push(Math.round(m * 1000) / 1000);\n\t}\n\treturn o;\n}\nfunction useRecord(L, Rch, sr, cues) {\n\tvar n = Math.min(L.length, Math.floor(MAXREC * sr), HOST.cutCapacity ? Math.floor(HOST.cutCapacity() / CUTOS) : 1e12);\n\tR.L = L.subarray(0, n); R.R = Rch.subarray(0, n); R.sr = sr; R.n = n; R.have = 1;\n\tvar mono = new Float32Array(n), pk = 0, i;\n\tfor (i = 0; i < n; i++) { mono[i] = 0.5 * (R.L[i] + R.R[i]); pk = Math.max(pk, Math.abs(mono[i])); }\n\tif (pk > 0) for (i = 0; i < n; i++) mono[i] /= pk;\n\tR.cues = cues && cues.length ? cues.slice() : findCues(mono, sr);\n\tR.wave = overview(mono, 300);\n\tS.cueIdx = 0;\n\tR.trimmed = L.length > n;\n\tsendStrip();\n\tsetCue(0);\n\tstartCut();\n}\nfunction decodeBuiltin() {\n\tvar tbl = {}, chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/', i;\n\tfor (i = 0; i < 64; i++) tbl[chars.charAt(i)] = i;\n\tvar s = BUILTIN_B64, bytes = [], buf = 0, bits = 0;\n\tfor (i = 0; i < s.length; i++) {\n\t\tvar c = s.charAt(i);\n\t\tif (c === '=' || tbl[c] === undefined) continue;\n\t\tbuf = (buf << 6) | tbl[c]; bits += 6;\n\t\tif (bits >= 8) { bits -= 8; bytes.push((buf >> bits) & 255); }\n\t}\n\tvar n = bytes.length >> 1, x = new Float32Array(n);\n\tfor (i = 0; i < n; i++) { var v = bytes[2 * i] | (bytes[2 * i + 1] << 8); if (v > 32767) v -= 65536; x[i] = v / 32768; }\n\treturn x;\n}\nfunction useBuiltin() {\n\tvar x = decodeBuiltin();\n\tR.file = ''; R.builtin = 1;\n\tif (!x.length) { R.have = 0; HOST.ui('status', 'set', 'DROP A SOUND'); return; }\n\tuseRecord(x, x, 44100, null);\n}\nfunction showRecord() {\n\tif (!R.have) { HOST.ui('status', 'set', 'DROP A SOUND'); return; }\n\tvar name = R.builtin ? 'BUILT-IN' : R.file.substring(R.file.lastIndexOf('/') + 1).toUpperCase();\n\tif (name.length > 22) name = name.substring(0, 21) + '~';\n\tHOST.ui('status', 'set', name + '  ' + (R.n / R.sr).toFixed(1) + ' S' + (R.trimmed ? ' (CUT TO 20)' : '') + '  ' + R.cues.length + ' CUES');\n}\nfunction sendStrip() {\n\tif (!R.have) { HOST.ui('strip', 'clear'); return; }\n\tHOST.ui.apply(null, ['strip', 'wave'].concat(R.wave));\n\tvar c = [], len = R.n / R.sr;\n\tfor (var i = 0; i < R.cues.length; i++) c.push(Math.round(R.cues[i] / len * 100000) / 100000);\n\tHOST.ui.apply(null, ['strip', 'cues'].concat(c.length ? c : [-1]));\n\tHOST.ui('strip', 'len', len);\n\tHOST.ui('strip', 'sel', S.cueIdx);\n}\n// edits on the strip: x is 0..1 along the record\nfunction stripEdit(what, i, x) {\n\tif (!R.have) return;\n\tvar len = R.n / R.sr, t = Math.max(0, Math.min(1, x)) * len;\n\tif (what === 'pick') setCue(i);\n\telse if (what === 'move' && i >= 0 && i < R.cues.length) { R.cues[i] = t; }\n\telse if (what === 'moved') { sortCues(i); }\n\telse if (what === 'add' && R.cues.length < 8) { R.cues.push(t); sortCues(R.cues.length - 1); }\n\telse if (what === 'remove' && R.cues.length > 1 && i >= 0 && i < R.cues.length) {\n\t\tR.cues.splice(i, 1); if (S.cueIdx >= R.cues.length) S.cueIdx = R.cues.length - 1; setCue(S.cueIdx);\n\t}\n\tsendStrip();\n\tHOST.dirty();\n}\nfunction sortCues(i) {\n\tvar v = R.cues[i];\n\tR.cues.sort(function (a, b) { return a - b; });\n\tvar j = R.cues.indexOf(v);\n\tif (i === S.cueIdx) setCue(j);\n}\n\n// ------------------------------------------------------------------ panel messages\nfunction control(m, v) {\n\tvar c = S.ctl;\n\tswitch (m) {\n\tcase 'speed': c.speed = SPEEDS[v] !== undefined ? SPEEDS[v] : 0.25; return true;\n\tcase 'quant': c.quant = QUANTS[v] !== undefined ? QUANTS[v] : 0.25; return true;\n\tcase 'mode': c.mode = v; S.calls = []; S.genNext = null; if (v === 0) S.calls = []; return true;\n\tcase 'phon': c.phon = PHRASEON[v] || 4; S.calls = []; S.genNext = null; return true;\n\tcase 'phoff': c.phoff = PHRASEOFF[v] !== undefined ? PHRASEOFF[v] : 0; return true;\n\tcase 'density': case 'skill': case 'motif': case 'release': case 'funk': case 'walk':\n\t\tc[m] = v / 100; return true;\n\tcase 'timing': case 'weight': case 'slop': case 'risk':\n\t\tc[m] = v / 100; sendHands(); return true;\n\tcase 'level': HOST.gen('level', Math.pow(10, v / 20)); return true;\n\tcase 'cue': setCue(v - 1); return true;\n\t}\n\tif (m.indexOf('btn_') === 0) { wordPress(m.substr(4), v ? 100 : 0); return true; }\n\tif (m.indexOf('on_') === 0) { S.on[m.substr(3)] = v ? 1 : 0; return true; }\n\tif (m.indexOf('slot') === 0) {\n\t\tvar n = parseInt(m.substr(4), 10);\n\t\tif (n >= 1 && n <= 16) S.routine[n - 1] = CELLNAMES[v] || 'rest';\n\t\treturn true;\n\t}\n\tif (m.indexOf('scue') === 0) {\n\t\tvar k = parseInt(m.substr(4), 10);\n\t\tif (k >= 1 && k <= 16) S.routineCue[k - 1] = v;\n\t\treturn true;\n\t}\n\treturn false;\n}\n\n// ------------------------------------------------------------------ Max host\nvar IN_MAX = (typeof outlet === 'function') && (typeof post === 'function') && (typeof Task !== 'undefined');\nvar NAMES = {rec: 'skr_rec', cut: 'skr_cut', q: 'skr_q', f: 'skr_f', st: 'skr_stat'};\nvar bRec = null, bCut = null, bQ = null, bF = null, bSt = null;\nvar tick = null, cutTask = null, ready = false, restoreState = null, handsGuard = true, initAt = 0;\nvar apiPlay = null, apiTempo = null, apiTime = null, sbseq = 0;\n\nif (IN_MAX) {\n\tHOST.gen = function (n, v) { outlet(0, n, v); };\n\tHOST.ui = function () { outlet(1, arrayfromargs(arguments)); };\n\tHOST.status = function () { return bSt ? bSt.peek(1, 0, 12) : null; };\n\tHOST.writeSeg = function (slot, vals) { if (bQ) bQ.poke(1, slot * QF, vals); };\n\tHOST.writeFad = function (slot, vals) { if (bF) bF.poke(1, slot * FF, vals); };\n\tHOST.log = function (s) { post('DJ Claude Skratch: ' + s + '\\n'); };\n\tHOST.cutCapacity = function () { return bCut ? bCut.framecount() : 0; };\n\tHOST.cutBegin = function (n) {};\n\tHOST.cutWrite = function (c, off, y) {\n\t\tvar CH = 16384;\n\t\tfor (var a = 0; a < y.length; a += CH) {\n\t\t\tvar part = Array.prototype.slice.call(y.subarray(a, Math.min(y.length, a + CH)));\n\t\t\tbCut.poke(c + 1, off + a, part);\n\t\t}\n\t};\n\tHOST.dirty = function () { notifyclients(); };\n\ttick = new Task(onTick, this);\n\ttick.interval = 20;\n\tcutTask = new Task(function () {\n\t\tif (cutSlice(Math.floor(R.sr * 0.5))) { cutTask.cancel(); }\n\t}, this);\n\tcutTask.interval = 5;\n}\nHOST.dirty = HOST.dirty || function () {};\n\nfunction names(rec, cut, q, f, st) {\n\tNAMES.rec = String(rec); NAMES.cut = String(cut); NAMES.q = String(q); NAMES.f = String(f); NAMES.st = String(st);\n}\nfunction init() {\n\tbRec = new Buffer(NAMES.rec); bCut = new Buffer(NAMES.cut); bQ = new Buffer(NAMES.q);\n\tbF = new Buffer(NAMES.f); bSt = new Buffer(NAMES.st);\n\tvar st = HOST.status();\n\tif (st && st[8] > 1000) S.sr = st[8];\n\ttry {\n\t\tapiPlay = new LiveAPI(function (a) { if (a[0] === 'is_playing') setPlaying(a[1]); }, 'live_set');\n\t\tapiPlay.property = 'is_playing';\n\t\tapiTempo = new LiveAPI(function (a) { if (a[0] === 'tempo') setTempo(a[1]); }, 'live_set');\n\t\tapiTempo.property = 'tempo';\n\t\tapiTime = new LiveAPI(function (a) { if (a[0] === 'current_song_time') setSongTime(a[1]); }, 'live_set');\n\t\tapiTime.property = 'current_song_time';\n\t\tsetTempo(apiTempo.get('tempo')[0]);\n\t\tsetPlaying(apiPlay.get('is_playing')[0]);\n\t} catch (e) { HOST.log('Live API not available: ' + e); }\n\tsendHands(); sendFilters();\n\tHOST.gen('fgen', S.gen);\n\tready = true; initAt = Date.now();\n\tresetPlan(false);\n\tif (restoreState) { restore(restoreState); restoreState = null; } else useBuiltin();\n\ttick.repeat();\n}\nfunction setPlaying(v) { v = v ? 1 : 0; if (v !== S.playing) { S.playing = v; HOST.gen('playing', v); } }\nfunction setTempo(v) {\n\tif (Math.abs(v - S.tempo) < 1e-6) return;\n\tvar b = nowB();\n\tS.tempo = v; HOST.gen('tempo', v);\n\tif (D) resetPlan(true);\n}\nfunction setSongTime(v) { sbseq++; HOST.gen('songbeat', v); HOST.gen('sbseq', sbseq); }\n\nfunction onTick() {\n\tif (handsGuard && Date.now() - initAt > 1500) handsGuard = false;\n\tif (R.cutting && !cutTask.running) cutTask.repeat();\n\tplan();\n}\n\nfunction loadFile(path) {\n\tbRec.send('replace', path);\n\tvar fr = bRec.framecount(), nch = bRec.channelcount();\n\tif (!fr) { HOST.ui('status', 'set', 'FILE MISSING'); return false; }\n\tvar sr = fr / (bRec.length() / 1000), n = Math.min(fr, Math.floor(MAXREC * sr) + 1);\n\tvar L = new Float32Array(n), Rc = new Float32Array(n), CH = 65536;\n\tfor (var a = 0; a < n; a += CH) {\n\t\tvar m = Math.min(CH, n - a), x = bRec.peek(1, a, m), y = nch > 1 ? bRec.peek(2, a, m) : x;\n\t\tif (m === 1) { x = [x]; y = [y]; }\n\t\tfor (var i = 0; i < m; i++) { L[a + i] = x[i]; Rc[a + i] = y[i]; }\n\t}\n\tR.file = path; R.builtin = 0;\n\tuseRecord(L, Rc, sr, null);\n\treturn true;\n}\n\n// the record and its cue points are kept with the Live set (the v8 object is a stored parameter)\nfunction getvalueof() {\n\tif (!R.have || R.builtin) return JSON.stringify({file: '', cues: R.have ? R.cues : [], cue: S.cueIdx});\n\treturn JSON.stringify({file: R.file, cues: R.cues, cue: S.cueIdx});\n}\nfunction setvalueof(v) {\n\tif (!v || v === 0) return;\n\ttry { var o = JSON.parse(v); } catch (e) { return; }\n\tif (ready) restore(o); else restoreState = o;\n}\nfunction restore(o) {\n\tif (o.file) {\n\t\tif (!loadFile(o.file)) { useBuiltin(); HOST.ui('status', 'set', 'FILE MISSING: BUILT-IN'); return; }\n\t} else useBuiltin();\n\tif (o.cues && o.cues.length) { R.cues = o.cues.slice(); sendStrip(); }\n\tsetCue(o.cue || 0);\n}\n\nfunction anything() {\n\tvar a = arrayfromargs(arguments), v = a.length ? a[0] : 0, m = messagename;\n\tswitch (m) {\n\tcase 'init': init(); return;\n\tcase 'note': if (ready) noteIn(a[0], a[1]); return;\n\tcase 'drop': if (ready) { if (loadFile(a.join(' '))) notifyclients(); } return;\n\tcase 'clear': if (ready && v) { useBuiltin(); notifyclients(); } return;\n\tcase 'strip': if (ready) stripEdit(a[0], a[1], a[2]); return;\n\tcase 'hands':\n\t\tif (handsGuard) return;\n\t\tHOST.ui('timing', v); HOST.ui('weight', v); HOST.ui('slop', v); HOST.ui('risk', v);\n\t\treturn;\n\t}\n\tif (control(m, v)) { if (ready) plan(); return; }\n\tHOST.log('unknown message ' + m);\n}\nfunction bang() {}\nfunction list() {}\n\nif (!IN_MAX && typeof module !== 'undefined' && module) {\n\tmodule.exports = {\n\t\tS: S, R: R, HOST: HOST, W: W, CELLS: CELLS, CELLNAMES: CELLNAMES, UNIT: UNIT,\n\t\tplan: plan, control: control, wordPress: wordPress, noteIn: noteIn, setCue: setCue,\n\t\tuseRecord: useRecord, cutSlice: cutSlice, sendHands: sendHands, sendFilters: sendFilters,\n\t\tresetPlan: resetPlan, findCues: findCues, emitFader: emitFader, flushOut: flushOut,\n\t\tdeck: function () { return D; }, hands: function () { return H; }, at: at, start: start,\n\t\tsetRng: function (f) { rng = f; }, b2t: b2t, t2b: t2b, decodeBuiltin: decodeBuiltin,\n\t\tsetBuiltin: function (s) { BUILTIN_B64 = s; }, useBuiltin: useBuiltin, out: out,\n\t\tstripEdit: stripEdit, instPhrase: instPhrase, generate: generate\n\t};\n}\n",
						"filename": "none",
						"flags": 0,
						"embed": 1,
						"autowatch": 1
					},
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "DJ Claude Skratch State",
							"parameter_shortname": "State",
							"parameter_type": 3,
							"parameter_invisible": 1
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						400.0,
						20.0,
						90.0,
						22.0
					],
					"outlettype": [
						"bang",
						"int",
						"int"
					],
					"text": "live.thisdevice"
				}
			},
			{
				"box": {
					"id": "obj-16",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						400.0,
						50.0,
						330.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "names ---skr_rec ---skr_cut ---skr_q ---skr_f ---skr_stat, init"
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 8,
					"patching_rect": [
						400.0,
						170.0,
						420.0,
						22.0
					],
					"outlettype": [
						"",
						"",
						"",
						"",
						"",
						"",
						"",
						""
					],
					"text": "route status strip cue timing weight slop risk"
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						700.0,
						640.0,
						50.0,
						20.0
					],
					"presentation": 1,
					"presentation_rect": [
						2.0,
						2.0,
						300.0,
						165.0
					],
					"bgcolor": [
						0.17,
						0.17,
						0.18,
						1.0
					],
					"background": 1,
					"border": 0,
					"rounded": 4
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						700.0,
						640.0,
						50.0,
						20.0
					],
					"presentation": 1,
					"presentation_rect": [
						304.0,
						2.0,
						396.0,
						165.0
					],
					"bgcolor": [
						0.17,
						0.17,
						0.18,
						1.0
					],
					"background": 1,
					"border": 0,
					"rounded": 4
				}
			},
			{
				"box": {
					"id": "obj-20",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						700.0,
						640.0,
						50.0,
						20.0
					],
					"presentation": 1,
					"presentation_rect": [
						702.0,
						2.0,
						112.0,
						165.0
					],
					"bgcolor": [
						0.17,
						0.17,
						0.18,
						1.0
					],
					"background": 1,
					"border": 0,
					"rounded": 4
				}
			},
			{
				"box": {
					"id": "obj-21",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						700.0,
						640.0,
						50.0,
						20.0
					],
					"presentation": 1,
					"presentation_rect": [
						816.0,
						2.0,
						136.0,
						165.0
					],
					"bgcolor": [
						0.17,
						0.17,
						0.18,
						1.0
					],
					"background": 1,
					"border": 0,
					"rounded": 4
				}
			},
			{
				"box": {
					"id": "obj-22",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						700.0,
						640.0,
						50.0,
						20.0
					],
					"presentation": 1,
					"presentation_rect": [
						954.0,
						2.0,
						140.0,
						165.0
					],
					"bgcolor": [
						0.17,
						0.17,
						0.18,
						1.0
					],
					"background": 1,
					"border": 0,
					"rounded": 4
				}
			},
			{
				"box": {
					"id": "obj-23",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						700.0,
						640.0,
						50.0,
						20.0
					],
					"presentation": 1,
					"presentation_rect": [
						1096.0,
						2.0,
						52.0,
						165.0
					],
					"bgcolor": [
						0.17,
						0.17,
						0.18,
						1.0
					],
					"background": 1,
					"border": 0,
					"rounded": 4
				}
			},
			{
				"box": {
					"id": "obj-24",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						700.0,
						560.0,
						80.0,
						18.0
					],
					"text": "RECORD",
					"presentation": 1,
					"presentation_rect": [
						6.0,
						4.0,
						80.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-25",
					"maxclass": "v8ui",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						700.0,
						420.0,
						300.0,
						120.0
					],
					"outlettype": [
						""
					],
					"presentation": 1,
					"presentation_rect": [
						6.0,
						18.0,
						292.0,
						92.0
					],
					"filename": "none",
					"embed": 1,
					"textfile": {
						"text": "// DJ Claude Skratch: the record strip (v8ui). Shows the record, its cue points, the current\n// cue, and the needle (bright while the fader is open).\n// Click a cue mark to choose it, drag it to move it. Double-click to add a cue point there,\n// double-click a mark to remove it. Up to 8.\n\nmgraphics.init();\nmgraphics.relative_coords = 0;\nmgraphics.autofill = 0;\ninlets = 1;\noutlets = 1;\n\nvar STAT = 'skr_stat';\nvar bSt = null;\nvar wave = [], cues = [], sel = 0, len = 1, have = false;\nvar pos = 0, g = 0, drag = -1;\nvar COL = {\n\tbg: [0.11, 0.11, 0.12, 1], wave: [0.55, 0.57, 0.6, 1], mark: [0.85, 0.85, 0.85, 1],\n\tsel: [1.0, 0.62, 0.15, 1], needle: [1.0, 0.62, 0.15, 1], text: [0.75, 0.75, 0.78, 1], dark: [0.1, 0.1, 0.1, 1]\n};\n\nvar t = new Task(function () {\n\tif (!bSt) { try { bSt = new Buffer(STAT); } catch (e) { return; } }\n\tvar st = bSt.peek(1, 4, 3);\n\tpos = st[0]; g = st[2];\n\tmgraphics.redraw();\n}, this);\nt.interval = 33;\nt.repeat();\n\nfunction col(gr, c, a) { gr.set_source_rgba(c[0], c[1], c[2], a === undefined ? c[3] : a); }\nfunction anything() {\n\tvar a = arrayfromargs(arguments);\n\tswitch (messagename) {\n\tcase 'statname': STAT = String(a[0]); bSt = null; break;\n\tcase 'wave': wave = a; have = true; break;\n\tcase 'cues': cues = (a.length === 1 && a[0] < 0) ? [] : a; break;\n\tcase 'sel': sel = a[0]; break;\n\tcase 'len': len = a[0] > 0 ? a[0] : 1; break;\n\tcase 'clear': have = false; wave = []; cues = []; break;\n\t}\n\tmgraphics.redraw();\n}\n\nfunction size() { return [box.rect[2] - box.rect[0], box.rect[3] - box.rect[1]]; }\n\nfunction paint() {\n\tvar wh = size(), w = wh[0], h = wh[1], gr = mgraphics, i, x;\n\tvar top = 12, ht = h - top - 2, mid = top + ht / 2;\n\tcol(gr, COL.bg); gr.rectangle(0, 0, w, h); gr.fill();\n\tif (!have) {\n\t\tcol(gr, COL.text); gr.select_font_face('Arial'); gr.set_font_size(10);\n\t\tgr.move_to(8, h / 2); gr.show_text('DROP A SOUND BELOW'); return;\n\t}\n\tcol(gr, COL.wave);\n\tvar n = wave.length;\n\tfor (i = 0; i < n; i++) {\n\t\tvar v = Math.min(1, wave[i]) * ht * 0.48;\n\t\tx = w * (i + 0.5) / n;\n\t\tgr.move_to(x, mid - v); gr.line_to(x, mid + v);\n\t}\n\tgr.set_line_width(Math.max(1, w / n * 0.8)); gr.stroke();\n\t// cue marks, numbered\n\tgr.select_font_face('Arial Bold'); gr.set_font_size(9);\n\tfor (i = 0; i < cues.length; i++) {\n\t\tx = Math.round(cues[i] * w) + 0.5;\n\t\tvar c = i === sel ? COL.sel : COL.mark;\n\t\tcol(gr, c, 0.55); gr.move_to(x, top); gr.line_to(x, h - 2); gr.set_line_width(1); gr.stroke();\n\t\tcol(gr, c); gr.rectangle(x - 0.5, 0, 11, 11); gr.fill();\n\t\tcol(gr, COL.dark); gr.move_to(x + 2.5, 9); gr.show_text(String(i + 1));\n\t}\n\t// needle\n\tx = Math.max(0, Math.min(1, pos / len)) * w;\n\tcol(gr, COL.needle, 0.3 + 0.7 * Math.min(1, g));\n\tgr.set_line_width(g > 0.5 ? 2.5 : 1.5);\n\tgr.move_to(x, top); gr.line_to(x, h - 2); gr.stroke();\n}\n\nfunction nearest(x) {\n\tvar w = size()[0], best = -1, bd = 7;\n\tfor (var i = 0; i < cues.length; i++) { var d = Math.abs(cues[i] * w - x); if (d < bd) { bd = d; best = i; } }\n\treturn best;\n}\nfunction onclick(x, y, but, cmd, shift) {\n\tif (!have) return;\n\tvar i = nearest(x);\n\tdrag = i;\n\tif (i >= 0) { sel = i; outlet(0, 'pick', i, 0); mgraphics.redraw(); }\n}\nfunction ondrag(x, y, but) {\n\tif (!have || drag < 0) return;\n\tvar w = size()[0], f = Math.max(0, Math.min(1, x / w));\n\tif (but) { cues[drag] = f; outlet(0, 'move', drag, f); mgraphics.redraw(); }\n\telse { outlet(0, 'moved', drag, f); drag = -1; }\n}\nfunction ondblclick(x, y) {\n\tif (!have) return;\n\tvar i = nearest(x), w = size()[0];\n\tif (i >= 0) outlet(0, 'remove', i, 0);\n\telse outlet(0, 'add', -1, Math.max(0, Math.min(1, x / w)));\n}\n",
						"filename": "none",
						"flags": 0,
						"embed": 1,
						"autowatch": 1
					},
					"parameter_enable": 0
				}
			},
			{
				"box": {
					"id": "obj-26",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						700.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend strip"
				}
			},
			{
				"box": {
					"id": "obj-27",
					"maxclass": "live.drop",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						800.0,
						500.0,
						84.0,
						44.0
					],
					"outlettype": [
						""
					],
					"presentation": 1,
					"presentation_rect": [
						6.0,
						114.0,
						76.0,
						48.0
					]
				}
			},
			{
				"box": {
					"id": "obj-28",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						800.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend drop"
				}
			},
			{
				"box": {
					"id": "obj-29",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						900.0,
						560.0,
						80.0,
						18.0
					],
					"text": "CUE",
					"presentation": 1,
					"presentation_rect": [
						88.0,
						114.0,
						30.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-30",
					"maxclass": "live.numbox",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						900.0,
						500.0,
						44.0,
						15.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						88.0,
						128.0,
						36.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Cue Point",
							"parameter_shortname": "Cue",
							"parameter_type": 1,
							"parameter_mmin": 1.0,
							"parameter_mmax": 8.0,
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 0
						}
					},
					"varname": "Cue Point",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-31",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						900.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend cue"
				}
			},
			{
				"box": {
					"id": "obj-32",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1000.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "BUILT-IN",
					"presentation": 1,
					"presentation_rect": [
						88.0,
						147.0,
						54.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Built-In Record",
							"parameter_shortname": "BUILT-IN",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Built-In Record",
					"texton": "BUILT-IN",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-33",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1000.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend clear"
				}
			},
			{
				"box": {
					"id": "obj-34",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1100.0,
						560.0,
						160.0,
						30.0
					],
					"text": "",
					"presentation": 1,
					"presentation_rect": [
						146.0,
						114.0,
						152.0,
						22.0
					],
					"textcolor": [
						1.0,
						0.75,
						0.3,
						1.0
					],
					"linecount": 2,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-35",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1100.0,
						560.0,
						80.0,
						18.0
					],
					"text": "CLICK A MARK TO CUE IT, DRAG TO MOVE. DOUBLE-CLICK TO ADD OR REMOVE.",
					"presentation": 1,
					"presentation_rect": [
						146.0,
						138.0,
						152.0,
						26.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-36",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1100.0,
						560.0,
						80.0,
						18.0
					],
					"text": "WORDS",
					"presentation": 1,
					"presentation_rect": [
						308.0,
						4.0,
						80.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-37",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1100.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "BABY",
					"presentation": 1,
					"presentation_rect": [
						308.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Baby",
							"parameter_shortname": "BABY",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Baby",
					"texton": "BABY",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-38",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1100.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_baby"
				}
			},
			{
				"box": {
					"id": "obj-39",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1200.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						320.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Baby",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Baby",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-40",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1200.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_baby"
				}
			},
			{
				"box": {
					"id": "obj-41",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1300.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "FORWARD",
					"presentation": 1,
					"presentation_rect": [
						357.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Forward",
							"parameter_shortname": "FORWARD",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Forward",
					"texton": "FORWARD",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-42",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1300.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_forward"
				}
			},
			{
				"box": {
					"id": "obj-43",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1400.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						369.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Forward",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Forward",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-44",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1400.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_forward"
				}
			},
			{
				"box": {
					"id": "obj-45",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1500.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "STAB",
					"presentation": 1,
					"presentation_rect": [
						406.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Stab",
							"parameter_shortname": "STAB",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Stab",
					"texton": "STAB",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-46",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1500.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_stab"
				}
			},
			{
				"box": {
					"id": "obj-47",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1600.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						418.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Stab",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Stab",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-48",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1600.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_stab"
				}
			},
			{
				"box": {
					"id": "obj-49",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1700.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "CHIRP",
					"presentation": 1,
					"presentation_rect": [
						455.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Chirp",
							"parameter_shortname": "CHIRP",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Chirp",
					"texton": "CHIRP",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-50",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1700.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_chirp"
				}
			},
			{
				"box": {
					"id": "obj-51",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1800.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						467.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Chirp",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Chirp",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-52",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1800.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_chirp"
				}
			},
			{
				"box": {
					"id": "obj-53",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1900.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "1-FLARE",
					"presentation": 1,
					"presentation_rect": [
						504.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "1-Click Flare",
							"parameter_shortname": "1-FLARE",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "1-Click Flare",
					"texton": "1-FLARE",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-54",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						1900.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_flare1"
				}
			},
			{
				"box": {
					"id": "obj-55",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2000.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						516.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On 1-Click Flare",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On 1-Click Flare",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-56",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2000.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_flare1"
				}
			},
			{
				"box": {
					"id": "obj-57",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2100.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "2-FLARE",
					"presentation": 1,
					"presentation_rect": [
						553.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "2-Click Flare",
							"parameter_shortname": "2-FLARE",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "2-Click Flare",
					"texton": "2-FLARE",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-58",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2100.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_flare2"
				}
			},
			{
				"box": {
					"id": "obj-59",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2200.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						565.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On 2-Click Flare",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On 2-Click Flare",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-60",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2200.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_flare2"
				}
			},
			{
				"box": {
					"id": "obj-61",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2300.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "CRAB",
					"presentation": 1,
					"presentation_rect": [
						602.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Crab",
							"parameter_shortname": "CRAB",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Crab",
					"texton": "CRAB",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-62",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2300.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_crab"
				}
			},
			{
				"box": {
					"id": "obj-63",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2400.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						614.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Crab",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Crab",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-64",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2400.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_crab"
				}
			},
			{
				"box": {
					"id": "obj-65",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2500.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "TEAR",
					"presentation": 1,
					"presentation_rect": [
						651.0,
						20.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Tear",
							"parameter_shortname": "TEAR",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Tear",
					"texton": "TEAR",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-66",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2500.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_tear"
				}
			},
			{
				"box": {
					"id": "obj-67",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2600.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						663.0,
						43.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Tear",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Tear",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-68",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2600.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_tear"
				}
			},
			{
				"box": {
					"id": "obj-69",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2700.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "SCRIBBLE",
					"presentation": 1,
					"presentation_rect": [
						308.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Scribble",
							"parameter_shortname": "SCRIBBLE",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Scribble",
					"texton": "SCRIBBLE",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-70",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2700.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_scribble"
				}
			},
			{
				"box": {
					"id": "obj-71",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2800.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						320.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Scribble",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Scribble",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-72",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2800.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_scribble"
				}
			},
			{
				"box": {
					"id": "obj-73",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						2900.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "TRANSFRM",
					"presentation": 1,
					"presentation_rect": [
						357.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Transform",
							"parameter_shortname": "TRANSFRM",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Transform",
					"texton": "TRANSFRM",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-74",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						2900.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_transform"
				}
			},
			{
				"box": {
					"id": "obj-75",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3000.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						369.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Transform",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Transform",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-76",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3000.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_transform"
				}
			},
			{
				"box": {
					"id": "obj-77",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3100.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "DRAG",
					"presentation": 1,
					"presentation_rect": [
						406.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Drag",
							"parameter_shortname": "DRAG",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Drag",
					"texton": "DRAG",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-78",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3100.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_drag"
				}
			},
			{
				"box": {
					"id": "obj-79",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3200.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						418.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Drag",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Drag",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-80",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3200.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_drag"
				}
			},
			{
				"box": {
					"id": "obj-81",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3300.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "TIP",
					"presentation": 1,
					"presentation_rect": [
						455.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Tip",
							"parameter_shortname": "TIP",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Tip",
					"texton": "TIP",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-82",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3300.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_tip"
				}
			},
			{
				"box": {
					"id": "obj-83",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3400.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						467.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Tip",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Tip",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-84",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3400.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_tip"
				}
			},
			{
				"box": {
					"id": "obj-85",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3500.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "LAZER",
					"presentation": 1,
					"presentation_rect": [
						504.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Lazer",
							"parameter_shortname": "LAZER",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Lazer",
					"texton": "LAZER",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-86",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3500.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_lazer"
				}
			},
			{
				"box": {
					"id": "obj-87",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3600.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						516.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Lazer",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Lazer",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-88",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3600.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_lazer"
				}
			},
			{
				"box": {
					"id": "obj-89",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3700.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "RELEASE",
					"presentation": 1,
					"presentation_rect": [
						553.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Release",
							"parameter_shortname": "RELEASE",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Release",
					"texton": "RELEASE",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-90",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3700.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_release"
				}
			},
			{
				"box": {
					"id": "obj-91",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3800.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						565.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Release",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Release",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-92",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3800.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_release"
				}
			},
			{
				"box": {
					"id": "obj-93",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3900.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ECHO",
					"presentation": 1,
					"presentation_rect": [
						602.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Echo",
							"parameter_shortname": "ECHO",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Echo",
					"texton": "ECHO",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-94",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						3900.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_echo"
				}
			},
			{
				"box": {
					"id": "obj-95",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4000.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						614.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Echo",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Echo",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-96",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4000.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_echo"
				}
			},
			{
				"box": {
					"id": "obj-97",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4100.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "REVERSE",
					"presentation": 1,
					"presentation_rect": [
						651.0,
						60.0,
						47.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Reverse",
							"parameter_shortname": "REVERSE",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Reverse",
					"texton": "REVERSE",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-98",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4100.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend btn_reverse"
				}
			},
			{
				"box": {
					"id": "obj-99",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4200.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						663.0,
						83.0,
						23.0,
						11.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Word On Reverse",
							"parameter_shortname": "ON",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Word On Reverse",
					"texton": "ON",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-100",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4200.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend on_reverse"
				}
			},
			{
				"box": {
					"id": "obj-101",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4300.0,
						560.0,
						80.0,
						18.0
					],
					"text": "SPEED",
					"presentation": 1,
					"presentation_rect": [
						308.0,
						104.0,
						40.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-102",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						4300.0,
						500.0,
						70.0,
						15.0
					],
					"outlettype": [
						"",
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						308.0,
						118.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Speed",
							"parameter_shortname": "Speed",
							"parameter_type": 2,
							"parameter_enum": [
								"1/8",
								"1/16",
								"1/16T",
								"1/32"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 3
						}
					},
					"varname": "Speed",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-103",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4300.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend speed"
				}
			},
			{
				"box": {
					"id": "obj-104",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4400.0,
						560.0,
						80.0,
						18.0
					],
					"text": "QUANT",
					"presentation": 1,
					"presentation_rect": [
						360.0,
						104.0,
						40.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-105",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						4400.0,
						500.0,
						70.0,
						15.0
					],
					"outlettype": [
						"",
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						360.0,
						118.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Quantise",
							"parameter_shortname": "Quant",
							"parameter_type": 2,
							"parameter_enum": [
								"1/16",
								"1/8",
								"1/4",
								"OFF"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 3
						}
					},
					"varname": "Quantise",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-106",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4400.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend quant"
				}
			},
			{
				"box": {
					"id": "obj-107",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4500.0,
						560.0,
						80.0,
						18.0
					],
					"text": "KEYS C1 TO D#2 PLAY THE WORDS, HELD TO REPEAT. C3 TO G3 PICK CUE POINTS 1 TO 8. THE SMALL SWITCHES SAY WHICH WORDS ROUTINE AND INSTINCT MAY USE.",
					"presentation": 1,
					"presentation_rect": [
						414.0,
						102.0,
						282.0,
						40.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-108",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4500.0,
						560.0,
						80.0,
						18.0
					],
					"text": "PLAY",
					"presentation": 1,
					"presentation_rect": [
						706.0,
						4.0,
						60.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-109",
					"maxclass": "live.tab",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						4500.0,
						500.0,
						70.0,
						48.0
					],
					"outlettype": [
						"",
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						706.0,
						18.0,
						64.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Mode",
							"parameter_shortname": "Mode",
							"parameter_type": 2,
							"parameter_enum": [
								"HANDS",
								"ROUTINE",
								"INSTINCT"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 2
						}
					},
					"varname": "Mode",
					"num_lines_patching": 3,
					"num_lines_presentation": 3,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-110",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4500.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend mode"
				}
			},
			{
				"box": {
					"id": "obj-111",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4600.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "ROUTINE...",
					"presentation": 1,
					"presentation_rect": [
						706.0,
						72.0,
						104.0,
						18.0
					],
					"texton": "ROUTINE...",
					"mode": 0,
					"parameter_enable": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-112",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						4680.0,
						540.0,
						60.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "pcontrol"
				}
			},
			{
				"box": {
					"id": "obj-113",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4680.0,
						510.0,
						40.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "open"
				}
			},
			{
				"box": {
					"id": "obj-114",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4680.0,
						560.0,
						80.0,
						18.0
					],
					"text": "PHRASE",
					"presentation": 1,
					"presentation_rect": [
						706.0,
						96.0,
						60.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-115",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4680.0,
						560.0,
						80.0,
						18.0
					],
					"text": "ON",
					"presentation": 1,
					"presentation_rect": [
						706.0,
						110.0,
						20.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-116",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						4680.0,
						500.0,
						70.0,
						15.0
					],
					"outlettype": [
						"",
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						722.0,
						110.0,
						40.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Phrase On",
							"parameter_shortname": "Ph On",
							"parameter_type": 2,
							"parameter_enum": [
								"1 BAR",
								"2 BARS",
								"4 BARS",
								"8 BARS"
							],
							"parameter_initial": [
								2
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 3
						}
					},
					"varname": "Phrase On",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-117",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4680.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend phon"
				}
			},
			{
				"box": {
					"id": "obj-118",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4780.0,
						560.0,
						80.0,
						18.0
					],
					"text": "OFF",
					"presentation": 1,
					"presentation_rect": [
						706.0,
						128.0,
						20.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-119",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						4780.0,
						500.0,
						70.0,
						15.0
					],
					"outlettype": [
						"",
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						722.0,
						128.0,
						40.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Phrase Off",
							"parameter_shortname": "Ph Off",
							"parameter_type": 2,
							"parameter_enum": [
								"0",
								"1 BAR",
								"2 BARS",
								"4 BARS",
								"8 BARS"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 4
						}
					},
					"varname": "Phrase Off",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-120",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4780.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend phoff"
				}
			},
			{
				"box": {
					"id": "obj-121",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4880.0,
						560.0,
						80.0,
						18.0
					],
					"text": "BARS OF SCRATCHING, THEN OF REST",
					"presentation": 1,
					"presentation_rect": [
						766.0,
						108.0,
						46.0,
						40.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-122",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4880.0,
						560.0,
						80.0,
						18.0
					],
					"text": "INSTINCT",
					"presentation": 1,
					"presentation_rect": [
						820.0,
						4.0,
						80.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-123",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4880.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						820.0,
						18.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Density",
							"parameter_shortname": "DENSITY",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Density"
				}
			},
			{
				"box": {
					"id": "obj-124",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4880.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend density"
				}
			},
			{
				"box": {
					"id": "obj-125",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4980.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						864.0,
						18.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Skill",
							"parameter_shortname": "SKILL",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								75
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Skill"
				}
			},
			{
				"box": {
					"id": "obj-126",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4980.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend skill"
				}
			},
			{
				"box": {
					"id": "obj-127",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5080.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						908.0,
						18.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Motif",
							"parameter_shortname": "MOTIF",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								60
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Motif"
				}
			},
			{
				"box": {
					"id": "obj-128",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5080.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend motif"
				}
			},
			{
				"box": {
					"id": "obj-129",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5180.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						820.0,
						92.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Release Ending",
							"parameter_shortname": "RELEASE",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								60
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Release Ending"
				}
			},
			{
				"box": {
					"id": "obj-130",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5180.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend release"
				}
			},
			{
				"box": {
					"id": "obj-131",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5280.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						864.0,
						92.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Funk",
							"parameter_shortname": "FUNK",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Funk"
				}
			},
			{
				"box": {
					"id": "obj-132",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5280.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend funk"
				}
			},
			{
				"box": {
					"id": "obj-133",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5380.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						908.0,
						92.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Walk",
							"parameter_shortname": "WALK",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Walk"
				}
			},
			{
				"box": {
					"id": "obj-134",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5380.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend walk"
				}
			},
			{
				"box": {
					"id": "obj-135",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						5480.0,
						560.0,
						80.0,
						18.0
					],
					"text": "RELEASE: HOW OFTEN A PHRASE ENDS BY PLAYING THE SOUND. WALK: MOVES BETWEEN CUES.",
					"presentation": 1,
					"presentation_rect": [
						820.0,
						140.0,
						130.0,
						26.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-136",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						5480.0,
						560.0,
						80.0,
						18.0
					],
					"text": "HANDS",
					"presentation": 1,
					"presentation_rect": [
						958.0,
						4.0,
						60.0,
						14.0
					],
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-137",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5480.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						958.0,
						18.0,
						44.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Hands",
							"parameter_shortname": "HANDS",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Hands",
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					]
				}
			},
			{
				"box": {
					"id": "obj-138",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5480.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend hands"
				}
			},
			{
				"box": {
					"id": "obj-139",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5580.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						1004.0,
						18.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Timing",
							"parameter_shortname": "TIMING",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Timing",
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					]
				}
			},
			{
				"box": {
					"id": "obj-140",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5580.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend timing"
				}
			},
			{
				"box": {
					"id": "obj-141",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5680.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						1048.0,
						18.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Weight",
							"parameter_shortname": "WEIGHT",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Weight",
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					]
				}
			},
			{
				"box": {
					"id": "obj-142",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5680.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend weight"
				}
			},
			{
				"box": {
					"id": "obj-143",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5780.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						958.0,
						92.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Slop",
							"parameter_shortname": "SLOP",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Slop",
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					]
				}
			},
			{
				"box": {
					"id": "obj-144",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5780.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend slop"
				}
			},
			{
				"box": {
					"id": "obj-145",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5880.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						1002.0,
						92.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Risk",
							"parameter_shortname": "RISK",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Risk",
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					]
				}
			},
			{
				"box": {
					"id": "obj-146",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5880.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend risk"
				}
			},
			{
				"box": {
					"id": "obj-147",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						5980.0,
						560.0,
						80.0,
						18.0
					],
					"text": "HANDS MOVES ALL FOUR. 0 IS A MACHINE, 50 A TIGHT DJ.",
					"presentation": 1,
					"presentation_rect": [
						1046.0,
						92.0,
						46.0,
						70.0
					],
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-148",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						5980.0,
						560.0,
						80.0,
						18.0
					],
					"text": "OUT",
					"presentation": 1,
					"presentation_rect": [
						1100.0,
						4.0,
						40.0,
						14.0
					],
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-149",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5980.0,
						500.0,
						41.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						1100.0,
						18.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Level",
							"parameter_shortname": "LEVEL",
							"parameter_type": 0,
							"parameter_mmin": -70.0,
							"parameter_mmax": 6.0,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 4
						}
					},
					"varname": "Level",
					"textcolor": [
						1.0,
						1.0,
						1.0,
						1.0
					]
				}
			},
			{
				"box": {
					"id": "obj-150",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5980.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend level"
				}
			},
			{
				"box": {
					"id": "obj-151",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						6080.0,
						560.0,
						80.0,
						18.0
					],
					"text": "v1.0.0",
					"presentation": 1,
					"presentation_rect": [
						1100.0,
						150.0,
						44.0,
						14.0
					],
					"textcolor": [
						0.78,
						0.78,
						0.8,
						1.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 8.0
				}
			},
			{
				"box": {
					"id": "obj-152",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						6080.0,
						580.0,
						80.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "p routine",
					"patcher": {
						"fileversion": 1,
						"appversion": {
							"major": 9,
							"minor": 0,
							"revision": 5,
							"architecture": "x64",
							"modernui": 1
						},
						"classnamespace": "box",
						"rect": [
							200.0,
							200.0,
							760.0,
							420.0
						],
						"openinpresentation": 0,
						"default_fontsize": 10.0,
						"default_fontname": "Arial Bold",
						"gridsize": [
							8.0,
							8.0
						],
						"boxes": [
							{
								"box": {
									"id": "r-in",
									"maxclass": "inlet",
									"numinlets": 0,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										10.0,
										10.0,
										30.0,
										30.0
									]
								}
							},
							{
								"box": {
									"id": "r-out",
									"maxclass": "outlet",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										10.0,
										380.0,
										30.0,
										30.0
									]
								}
							},
							{
								"box": {
									"id": "r-title",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										60.0,
										10.0,
										640.0,
										34.0
									],
									"linecount": 2,
									"text": "DJ CLAUDE SKRATCH ROUTINE: 16 slots of one beat, four bars, looping from the bar line where MODE is set to ROUTINE. A two-beat cell takes the next slot too. CUE: SAME keeps the cue point, 1 to 8 moves to it first."
								}
							},
							{
								"box": {
									"id": "r-l0",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										60.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 1"
								}
							},
							{
								"box": {
									"id": "r-m0",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										60.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 1",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 1",
											"parameter_shortname": "S1",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												15
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c0",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										60.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 1 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 1 Cue",
											"parameter_shortname": "C1",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p0",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										60.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot1"
								}
							},
							{
								"box": {
									"id": "r-q0",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										60.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue1"
								}
							},
							{
								"box": {
									"id": "r-l1",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										142.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 2"
								}
							},
							{
								"box": {
									"id": "r-m1",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										142.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 2",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 2",
											"parameter_shortname": "S2",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												15
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c1",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										142.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 2 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 2 Cue",
											"parameter_shortname": "C2",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p1",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										142.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot2"
								}
							},
							{
								"box": {
									"id": "r-q1",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										142.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue2"
								}
							},
							{
								"box": {
									"id": "r-l2",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										224.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 3"
								}
							},
							{
								"box": {
									"id": "r-m2",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										224.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 3",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 3",
											"parameter_shortname": "S3",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												9
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c2",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										224.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 3 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 3 Cue",
											"parameter_shortname": "C3",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p2",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										224.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot3"
								}
							},
							{
								"box": {
									"id": "r-q2",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										224.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue3"
								}
							},
							{
								"box": {
									"id": "r-l3",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										306.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 4"
								}
							},
							{
								"box": {
									"id": "r-m3",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										306.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 4",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 4",
											"parameter_shortname": "S4",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												5
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c3",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										306.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 4 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 4 Cue",
											"parameter_shortname": "C4",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p3",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										306.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot4"
								}
							},
							{
								"box": {
									"id": "r-q3",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										306.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue4"
								}
							},
							{
								"box": {
									"id": "r-l4",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										388.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 5"
								}
							},
							{
								"box": {
									"id": "r-m4",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										388.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 5",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 5",
											"parameter_shortname": "S5",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												15
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c4",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										388.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 5 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 5 Cue",
											"parameter_shortname": "C5",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p4",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										388.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot5"
								}
							},
							{
								"box": {
									"id": "r-q4",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										388.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue5"
								}
							},
							{
								"box": {
									"id": "r-l5",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										470.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 6"
								}
							},
							{
								"box": {
									"id": "r-m5",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										470.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 6",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 6",
											"parameter_shortname": "S6",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												15
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c5",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										470.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 6 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 6 Cue",
											"parameter_shortname": "C6",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p5",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										470.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot6"
								}
							},
							{
								"box": {
									"id": "r-q5",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										470.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue6"
								}
							},
							{
								"box": {
									"id": "r-l6",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										552.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 7"
								}
							},
							{
								"box": {
									"id": "r-m6",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										552.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 7",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 7",
											"parameter_shortname": "S7",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												9
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c6",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										552.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 7 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 7 Cue",
											"parameter_shortname": "C7",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p6",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										552.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot7"
								}
							},
							{
								"box": {
									"id": "r-q6",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										552.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue7"
								}
							},
							{
								"box": {
									"id": "r-l7",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										634.0,
										60.0,
										78.0,
										18.0
									],
									"text": "BEAT 8"
								}
							},
							{
								"box": {
									"id": "r-m7",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										634.0,
										80.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 8",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 8",
											"parameter_shortname": "S8",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												5
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c7",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										634.0,
										100.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 8 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 8 Cue",
											"parameter_shortname": "C8",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p7",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										634.0,
										130.0,
										78.0,
										22.0
									],
									"text": "prepend slot8"
								}
							},
							{
								"box": {
									"id": "r-q7",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										634.0,
										160.0,
										78.0,
										22.0
									],
									"text": "prepend scue8"
								}
							},
							{
								"box": {
									"id": "r-l8",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										60.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 9"
								}
							},
							{
								"box": {
									"id": "r-m8",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										60.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 9",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 9",
											"parameter_shortname": "S9",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												6
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c8",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										60.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 9 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 9 Cue",
											"parameter_shortname": "C9",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p8",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										60.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot9"
								}
							},
							{
								"box": {
									"id": "r-q8",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										60.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue9"
								}
							},
							{
								"box": {
									"id": "r-l9",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										142.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 10"
								}
							},
							{
								"box": {
									"id": "r-m9",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										142.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 10",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 10",
											"parameter_shortname": "S10",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												7
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c9",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										142.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 10 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 10 Cue",
											"parameter_shortname": "C10",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p9",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										142.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot10"
								}
							},
							{
								"box": {
									"id": "r-q9",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										142.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue10"
								}
							},
							{
								"box": {
									"id": "r-l10",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										224.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 11"
								}
							},
							{
								"box": {
									"id": "r-m10",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										224.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 11",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 11",
											"parameter_shortname": "S11",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c10",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										224.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 11 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 11 Cue",
											"parameter_shortname": "C11",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p10",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										224.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot11"
								}
							},
							{
								"box": {
									"id": "r-q10",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										224.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue11"
								}
							},
							{
								"box": {
									"id": "r-l11",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										306.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 12"
								}
							},
							{
								"box": {
									"id": "r-m11",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										306.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 12",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 12",
											"parameter_shortname": "S12",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												2
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c11",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										306.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 12 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 12 Cue",
											"parameter_shortname": "C12",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p11",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										306.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot12"
								}
							},
							{
								"box": {
									"id": "r-q11",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										306.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue12"
								}
							},
							{
								"box": {
									"id": "r-l12",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										388.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 13"
								}
							},
							{
								"box": {
									"id": "r-m12",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										388.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 13",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 13",
											"parameter_shortname": "S13",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												3
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c12",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										388.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 13 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 13 Cue",
											"parameter_shortname": "C13",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p12",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										388.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot13"
								}
							},
							{
								"box": {
									"id": "r-q12",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										388.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue13"
								}
							},
							{
								"box": {
									"id": "r-l13",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										470.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 14"
								}
							},
							{
								"box": {
									"id": "r-m13",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										470.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 14",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 14",
											"parameter_shortname": "S14",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												7
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c13",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										470.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 14 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 14 Cue",
											"parameter_shortname": "C14",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p13",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										470.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot14"
								}
							},
							{
								"box": {
									"id": "r-q13",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										470.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue14"
								}
							},
							{
								"box": {
									"id": "r-l14",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										552.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 15"
								}
							},
							{
								"box": {
									"id": "r-m14",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										552.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 15",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 15",
											"parameter_shortname": "S15",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c14",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										552.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 15 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 15 Cue",
											"parameter_shortname": "C15",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p14",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										552.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot15"
								}
							},
							{
								"box": {
									"id": "r-q14",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										552.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue15"
								}
							},
							{
								"box": {
									"id": "r-l15",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										634.0,
										210.0,
										78.0,
										18.0
									],
									"text": "BEAT 16"
								}
							},
							{
								"box": {
									"id": "r-m15",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										634.0,
										230.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 16",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 16",
											"parameter_shortname": "S16",
											"parameter_type": 2,
											"parameter_enum": [
												"REST",
												"BABY8",
												"BABY16",
												"PICKUP",
												"BABY-REST",
												"FWD-FWD",
												"FWD-BABY",
												"STABS",
												"STAB-OFF",
												"CHIRP",
												"CHIRP-BABY",
												"FLARE1",
												"FLARE2",
												"CRAB",
												"TEAR",
												"TEAR-TRIP",
												"SCRIBBLE",
												"TIPS",
												"LAZER",
												"TRANSFORM",
												"TRANSFORM-TRIP",
												"DRAG",
												"ECHO",
												"RELEASE"
											],
											"parameter_initial": [
												2
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 23
										}
									}
								}
							},
							{
								"box": {
									"id": "r-c15",
									"maxclass": "live.menu",
									"numinlets": 1,
									"numoutlets": 3,
									"outlettype": [
										"",
										"",
										"float"
									],
									"patching_rect": [
										634.0,
										250.0,
										78.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot 16 Cue",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot 16 Cue",
											"parameter_shortname": "C16",
											"parameter_type": 2,
											"parameter_enum": [
												"CUE SAME",
												"CUE 1",
												"CUE 2",
												"CUE 3",
												"CUE 4",
												"CUE 5",
												"CUE 6",
												"CUE 7",
												"CUE 8"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 8
										}
									}
								}
							},
							{
								"box": {
									"id": "r-p15",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										634.0,
										280.0,
										78.0,
										22.0
									],
									"text": "prepend slot16"
								}
							},
							{
								"box": {
									"id": "r-q15",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										634.0,
										310.0,
										78.0,
										22.0
									],
									"text": "prepend scue16"
								}
							}
						],
						"lines": [
							{
								"patchline": {
									"source": [
										"r-m0",
										0
									],
									"destination": [
										"r-p0",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c0",
										0
									],
									"destination": [
										"r-q0",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p0",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q0",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m1",
										0
									],
									"destination": [
										"r-p1",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c1",
										0
									],
									"destination": [
										"r-q1",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p1",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q1",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m2",
										0
									],
									"destination": [
										"r-p2",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c2",
										0
									],
									"destination": [
										"r-q2",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p2",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q2",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m3",
										0
									],
									"destination": [
										"r-p3",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c3",
										0
									],
									"destination": [
										"r-q3",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p3",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q3",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m4",
										0
									],
									"destination": [
										"r-p4",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c4",
										0
									],
									"destination": [
										"r-q4",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p4",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q4",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m5",
										0
									],
									"destination": [
										"r-p5",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c5",
										0
									],
									"destination": [
										"r-q5",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p5",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q5",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m6",
										0
									],
									"destination": [
										"r-p6",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c6",
										0
									],
									"destination": [
										"r-q6",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p6",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q6",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m7",
										0
									],
									"destination": [
										"r-p7",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c7",
										0
									],
									"destination": [
										"r-q7",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p7",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q7",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m8",
										0
									],
									"destination": [
										"r-p8",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c8",
										0
									],
									"destination": [
										"r-q8",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p8",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q8",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m9",
										0
									],
									"destination": [
										"r-p9",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c9",
										0
									],
									"destination": [
										"r-q9",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p9",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q9",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m10",
										0
									],
									"destination": [
										"r-p10",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c10",
										0
									],
									"destination": [
										"r-q10",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p10",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q10",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m11",
										0
									],
									"destination": [
										"r-p11",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c11",
										0
									],
									"destination": [
										"r-q11",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p11",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q11",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m12",
										0
									],
									"destination": [
										"r-p12",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c12",
										0
									],
									"destination": [
										"r-q12",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p12",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q12",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m13",
										0
									],
									"destination": [
										"r-p13",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c13",
										0
									],
									"destination": [
										"r-q13",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p13",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q13",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m14",
										0
									],
									"destination": [
										"r-p14",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c14",
										0
									],
									"destination": [
										"r-q14",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p14",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q14",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-m15",
										0
									],
									"destination": [
										"r-p15",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-c15",
										0
									],
									"destination": [
										"r-q15",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-p15",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-q15",
										0
									],
									"destination": [
										"r-out",
										0
									]
								}
							}
						],
						"description": "",
						"digest": "",
						"tags": "",
						"style": "",
						"subpatcher_template": ""
					}
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"obj-1",
						0
					],
					"destination": [
						"obj-2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-1",
						1
					],
					"destination": [
						"obj-2",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-2",
						0
					],
					"destination": [
						"obj-3",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-4",
						0
					],
					"destination": [
						"obj-5",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						0
					],
					"destination": [
						"obj-6",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						1
					],
					"destination": [
						"obj-6",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-12",
						0
					],
					"destination": [
						"obj-13",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-13",
						0
					],
					"destination": [
						"obj-5",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-14",
						0
					],
					"destination": [
						"obj-5",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-3",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
						0
					],
					"destination": [
						"obj-16",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-16",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-14",
						1
					],
					"destination": [
						"obj-17",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						1
					],
					"destination": [
						"obj-25",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-25",
						0
					],
					"destination": [
						"obj-26",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-26",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-27",
						0
					],
					"destination": [
						"obj-28",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-28",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-30",
						0
					],
					"destination": [
						"obj-31",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-31",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						2
					],
					"destination": [
						"obj-30",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-32",
						0
					],
					"destination": [
						"obj-33",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-33",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						0
					],
					"destination": [
						"obj-34",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-37",
						0
					],
					"destination": [
						"obj-38",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-38",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-39",
						0
					],
					"destination": [
						"obj-40",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-40",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-41",
						0
					],
					"destination": [
						"obj-42",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-42",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-43",
						0
					],
					"destination": [
						"obj-44",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-44",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-45",
						0
					],
					"destination": [
						"obj-46",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-46",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-47",
						0
					],
					"destination": [
						"obj-48",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-48",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-49",
						0
					],
					"destination": [
						"obj-50",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-50",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-51",
						0
					],
					"destination": [
						"obj-52",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-52",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-53",
						0
					],
					"destination": [
						"obj-54",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-54",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-55",
						0
					],
					"destination": [
						"obj-56",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-56",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-57",
						0
					],
					"destination": [
						"obj-58",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-58",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-59",
						0
					],
					"destination": [
						"obj-60",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-60",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-61",
						0
					],
					"destination": [
						"obj-62",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-62",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-63",
						0
					],
					"destination": [
						"obj-64",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-64",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-65",
						0
					],
					"destination": [
						"obj-66",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-66",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-67",
						0
					],
					"destination": [
						"obj-68",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-68",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-69",
						0
					],
					"destination": [
						"obj-70",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-70",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-71",
						0
					],
					"destination": [
						"obj-72",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-72",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-73",
						0
					],
					"destination": [
						"obj-74",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-74",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-75",
						0
					],
					"destination": [
						"obj-76",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-76",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-77",
						0
					],
					"destination": [
						"obj-78",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-78",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-79",
						0
					],
					"destination": [
						"obj-80",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-80",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-81",
						0
					],
					"destination": [
						"obj-82",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-82",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-83",
						0
					],
					"destination": [
						"obj-84",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-84",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-85",
						0
					],
					"destination": [
						"obj-86",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-86",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-87",
						0
					],
					"destination": [
						"obj-88",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-88",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-89",
						0
					],
					"destination": [
						"obj-90",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-90",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-91",
						0
					],
					"destination": [
						"obj-92",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-92",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-93",
						0
					],
					"destination": [
						"obj-94",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-94",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-95",
						0
					],
					"destination": [
						"obj-96",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-96",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-97",
						0
					],
					"destination": [
						"obj-98",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-98",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-99",
						0
					],
					"destination": [
						"obj-100",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-100",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-102",
						0
					],
					"destination": [
						"obj-103",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-103",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-105",
						0
					],
					"destination": [
						"obj-106",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-106",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-109",
						0
					],
					"destination": [
						"obj-110",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-110",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-111",
						0
					],
					"destination": [
						"obj-113",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-113",
						0
					],
					"destination": [
						"obj-112",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-116",
						0
					],
					"destination": [
						"obj-117",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-117",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-119",
						0
					],
					"destination": [
						"obj-120",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-120",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-123",
						0
					],
					"destination": [
						"obj-124",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-124",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-125",
						0
					],
					"destination": [
						"obj-126",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-126",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-127",
						0
					],
					"destination": [
						"obj-128",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-128",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-129",
						0
					],
					"destination": [
						"obj-130",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-130",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-131",
						0
					],
					"destination": [
						"obj-132",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-132",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-133",
						0
					],
					"destination": [
						"obj-134",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-134",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-137",
						0
					],
					"destination": [
						"obj-138",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-138",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-139",
						0
					],
					"destination": [
						"obj-140",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-140",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-141",
						0
					],
					"destination": [
						"obj-142",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-142",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-143",
						0
					],
					"destination": [
						"obj-144",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-144",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-145",
						0
					],
					"destination": [
						"obj-146",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-146",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						3
					],
					"destination": [
						"obj-139",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						4
					],
					"destination": [
						"obj-141",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						5
					],
					"destination": [
						"obj-143",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						6
					],
					"destination": [
						"obj-145",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-149",
						0
					],
					"destination": [
						"obj-150",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-150",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-112",
						0
					],
					"destination": [
						"obj-152",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-152",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			}
		],
		"parameters": {
			"obj-14": [
				"DJ Claude Skratch State",
				"State",
				0
			],
			"obj-30": [
				"Cue Point",
				"Cue",
				0
			],
			"obj-32": [
				"Built-In Record",
				"BUILT-IN",
				0
			],
			"obj-37": [
				"Baby",
				"BABY",
				0
			],
			"obj-39": [
				"Word On Baby",
				"ON",
				0
			],
			"obj-41": [
				"Forward",
				"FORWARD",
				0
			],
			"obj-43": [
				"Word On Forward",
				"ON",
				0
			],
			"obj-45": [
				"Stab",
				"STAB",
				0
			],
			"obj-47": [
				"Word On Stab",
				"ON",
				0
			],
			"obj-49": [
				"Chirp",
				"CHIRP",
				0
			],
			"obj-51": [
				"Word On Chirp",
				"ON",
				0
			],
			"obj-53": [
				"1-Click Flare",
				"1-FLARE",
				0
			],
			"obj-55": [
				"Word On 1-Click Flare",
				"ON",
				0
			],
			"obj-57": [
				"2-Click Flare",
				"2-FLARE",
				0
			],
			"obj-59": [
				"Word On 2-Click Flare",
				"ON",
				0
			],
			"obj-61": [
				"Crab",
				"CRAB",
				0
			],
			"obj-63": [
				"Word On Crab",
				"ON",
				0
			],
			"obj-65": [
				"Tear",
				"TEAR",
				0
			],
			"obj-67": [
				"Word On Tear",
				"ON",
				0
			],
			"obj-69": [
				"Scribble",
				"SCRIBBLE",
				0
			],
			"obj-71": [
				"Word On Scribble",
				"ON",
				0
			],
			"obj-73": [
				"Transform",
				"TRANSFRM",
				0
			],
			"obj-75": [
				"Word On Transform",
				"ON",
				0
			],
			"obj-77": [
				"Drag",
				"DRAG",
				0
			],
			"obj-79": [
				"Word On Drag",
				"ON",
				0
			],
			"obj-81": [
				"Tip",
				"TIP",
				0
			],
			"obj-83": [
				"Word On Tip",
				"ON",
				0
			],
			"obj-85": [
				"Lazer",
				"LAZER",
				0
			],
			"obj-87": [
				"Word On Lazer",
				"ON",
				0
			],
			"obj-89": [
				"Release",
				"RELEASE",
				0
			],
			"obj-91": [
				"Word On Release",
				"ON",
				0
			],
			"obj-93": [
				"Echo",
				"ECHO",
				0
			],
			"obj-95": [
				"Word On Echo",
				"ON",
				0
			],
			"obj-97": [
				"Reverse",
				"REVERSE",
				0
			],
			"obj-99": [
				"Word On Reverse",
				"ON",
				0
			],
			"obj-102": [
				"Speed",
				"Speed",
				0
			],
			"obj-105": [
				"Quantise",
				"Quant",
				0
			],
			"obj-109": [
				"Mode",
				"Mode",
				0
			],
			"obj-116": [
				"Phrase On",
				"Ph On",
				0
			],
			"obj-119": [
				"Phrase Off",
				"Ph Off",
				0
			],
			"obj-123": [
				"Density",
				"DENSITY",
				0
			],
			"obj-125": [
				"Skill",
				"SKILL",
				0
			],
			"obj-127": [
				"Motif",
				"MOTIF",
				0
			],
			"obj-129": [
				"Release Ending",
				"RELEASE",
				0
			],
			"obj-131": [
				"Funk",
				"FUNK",
				0
			],
			"obj-133": [
				"Walk",
				"WALK",
				0
			],
			"obj-137": [
				"Hands",
				"HANDS",
				0
			],
			"obj-139": [
				"Timing",
				"TIMING",
				0
			],
			"obj-141": [
				"Weight",
				"WEIGHT",
				0
			],
			"obj-143": [
				"Slop",
				"SLOP",
				0
			],
			"obj-145": [
				"Risk",
				"RISK",
				0
			],
			"obj-149": [
				"Level",
				"LEVEL",
				0
			],
			"r-m0": [
				"Slot 1",
				"S1",
				0
			],
			"r-c0": [
				"Slot 1 Cue",
				"C1",
				0
			],
			"r-m1": [
				"Slot 2",
				"S2",
				0
			],
			"r-c1": [
				"Slot 2 Cue",
				"C2",
				0
			],
			"r-m2": [
				"Slot 3",
				"S3",
				0
			],
			"r-c2": [
				"Slot 3 Cue",
				"C3",
				0
			],
			"r-m3": [
				"Slot 4",
				"S4",
				0
			],
			"r-c3": [
				"Slot 4 Cue",
				"C4",
				0
			],
			"r-m4": [
				"Slot 5",
				"S5",
				0
			],
			"r-c4": [
				"Slot 5 Cue",
				"C5",
				0
			],
			"r-m5": [
				"Slot 6",
				"S6",
				0
			],
			"r-c5": [
				"Slot 6 Cue",
				"C6",
				0
			],
			"r-m6": [
				"Slot 7",
				"S7",
				0
			],
			"r-c6": [
				"Slot 7 Cue",
				"C7",
				0
			],
			"r-m7": [
				"Slot 8",
				"S8",
				0
			],
			"r-c7": [
				"Slot 8 Cue",
				"C8",
				0
			],
			"r-m8": [
				"Slot 9",
				"S9",
				0
			],
			"r-c8": [
				"Slot 9 Cue",
				"C9",
				0
			],
			"r-m9": [
				"Slot 10",
				"S10",
				0
			],
			"r-c9": [
				"Slot 10 Cue",
				"C10",
				0
			],
			"r-m10": [
				"Slot 11",
				"S11",
				0
			],
			"r-c10": [
				"Slot 11 Cue",
				"C11",
				0
			],
			"r-m11": [
				"Slot 12",
				"S12",
				0
			],
			"r-c11": [
				"Slot 12 Cue",
				"C12",
				0
			],
			"r-m12": [
				"Slot 13",
				"S13",
				0
			],
			"r-c12": [
				"Slot 13 Cue",
				"C13",
				0
			],
			"r-m13": [
				"Slot 14",
				"S14",
				0
			],
			"r-c13": [
				"Slot 14 Cue",
				"C14",
				0
			],
			"r-m14": [
				"Slot 15",
				"S15",
				0
			],
			"r-c14": [
				"Slot 15 Cue",
				"C15",
				0
			],
			"r-m15": [
				"Slot 16",
				"S16",
				0
			],
			"r-c15": [
				"Slot 16 Cue",
				"C16",
				0
			],
			"inherited_shortname": 1
		},
		"dependency_cache": [],
		"latency": 0,
		"is_mpe": 0,
		"minimum_live_version": "",
		"minimum_max_version": "",
		"platform_compatibility": 0,
		"autosave": 0,
		"project": {
			"version": 1,
			"creationdate": 3590052786,
			"modificationdate": 3590052786,
			"viewrect": [
				0.0,
				0.0,
				300.0,
				500.0
			],
			"autoorganize": 1,
			"hideprojectwindow": 1,
			"showdependencies": 1,
			"autolocalize": 0,
			"contents": {
				"patchers": {}
			},
			"layout": {},
			"searchpath": {},
			"detailsvisible": 0,
			"amxdtype": 1768515945,
			"readonly": 0,
			"devpathtype": 0,
			"devpath": ".",
			"sortmode": 0,
			"viewmode": 0,
			"includepackages": 0
		}
	}
}
