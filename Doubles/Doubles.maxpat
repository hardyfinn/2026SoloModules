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
			1068.0,
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
		"devicewidth": 1068.0,
		"description": "Doubles: beat juggling on two modelled copies of a short recorded piece.",
		"digest": "",
		"tags": "",
		"style": "",
		"subpatcher_template": "",
		"boxes": [
			{
				"box": {
					"id": "obj-1",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
						20.0,
						60.0,
						22.0
					],
					"outlettype": [
						"signal",
						"signal"
					],
					"text": "plugin~"
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						100.0,
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
					"id": "obj-3",
					"maxclass": "newobj",
					"numinlets": 3,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
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
									"id": "obj-2",
									"maxclass": "newobj",
									"numinlets": 0,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										120.0,
										20.0,
										30.0,
										22.0
									],
									"text": "in 2"
								}
							},
							{
								"box": {
									"id": "obj-3",
									"maxclass": "newobj",
									"numinlets": 0,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										220.0,
										20.0,
										30.0,
										22.0
									],
									"text": "in 3"
								}
							},
							{
								"box": {
									"id": "obj-4",
									"maxclass": "codebox",
									"numinlets": 3,
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
									"code": "// Doubles: gen~ core. Two modelled turntables, a crossfader, and the record buffer.\n// GENERATED INTO THE DEVICE by tools/build_device.py. Edit this template, not the device.\n// Lines between #DECK and #ENDDECK are written out twice, with $ replaced by 0 and 1\n// and {D} by the deck number. Every History is copied to a local at the top of the\n// sample and written back at the end, so no History is read after it is written.\n//\n// in1, in2: audio in (L, R). in3: plugphasor~ (0..1 ramp per beat).\n// out1, out2: audio out.\n// Buffers: dbl_rec (the record, stereo), dbl_q (play queue), dbl_stat (status for JS).\n\nBuffer rec(\"dbl_rec\");\nBuffer q(\"dbl_q\");\nBuffer st(\"dbl_stat\");\nData gcut(256);\nParam tempo(120, min=20, max=999);\nParam playing(0);\nParam songbeat(0);\nParam sbseq(0);\nParam reccmd(0);        // counter: each change arms (recmode 1) or cancels (recmode 0)\nParam recmode(0);\nParam reclen(8, min=0, max=64);  // beats; 0 = free\nParam recstop(0);       // free mode: 1 = stop at next bar line\nParam haverec(0);\nParam L(1, min=1);      // record length, samples\nParam Lb(8, min=1);     // record length, beats\nParam anchor(0);        // song beat where the record starts (mod Lb)\nParam timing(0.5, min=0, max=1);\nParam weight(0.5, min=0, max=1);\nParam slop(0.5, min=0, max=1);\nParam risk(0.5, min=0, max=1);\nParam wear(0.5, min=0, max=1);\nParam level(1, min=0, max=2);\nParam strlev(1, min=0, max=1);\nParam brake(1);\nParam qt0(0);\nParam qt1(0);\nParam fgen(0);\nParam fbeat(0);\nParam bootb(-1000000000);   // play straight until this beat while the plan starts\nParam dragseq(0);\nParam dragdepth(0.35);\nHistory h_beatint(0);\nHistory h_prevph(0);\nHistory h_sbseq(0);\nHistory h_wasplay(0);\nHistory h_curgen(0);\nHistory h_flushb(0);\nHistory h_reccmd(0);\nHistory h_recstate(0);   // 0 idle, 1 armed, 2 recording, 3 done\nHistory h_wi(0);\nHistory h_recstart(0);\nHistory h_recb(0);\nHistory h_prevb(0);\nHistory h_statc(0);\nHistory h_live(0);\nHistory h_dragseq(0);\nHistory h_alive(0);\nHistory h_pos0(0);\nHistory h_v0(0);\nHistory h_vb0(0);\nHistory h_mode0(0);      // 0 idle, 1 cue pulse, 2 wait for release, 3 run, 4 hold, 5 spin, 6 brake\nHistory h_k0(0);\nHistory h_has0(0);\nHistory h_bs0(0);\nHistory h_be0(1000000000);\nHistory h_p00(0);\nHistory h_rate0(1);\nHistory h_kind0(0);\nHistory h_gm0(0);\nHistory h_gd0(1);\nHistory h_g00(0);\nHistory h_amp0(1);\nHistory h_str0(0);\nHistory h_gen0(0);\nHistory h_cued0(0);\nHistory h_pdc0(0);\nHistory h_pa0(0);\nHistory h_pdv0(0);\nHistory h_trrem0(0);\nHistory h_push0(0);\nHistory h_rk0(0);\nHistory h_exp0(0);\nHistory h_expok0(0);\nHistory h_cuchk0(0);\nHistory h_culen0(0);\nHistory h_cuk0(0);\nHistory h_cua0(0);\nHistory h_spm0(1);\nHistory h_spa0(0);\nHistory h_spdv0(0);\nHistory h_trem0(0);\nHistory h_gt0(0);\nHistory h_g0(0);\nHistory h_gamp0(1);\nHistory h_popen0(1000000000);\nHistory h_pclose0(1000000000);\nHistory h_slot0(0);\nHistory h_drk0(0);\nHistory h_drlen0(0);\nHistory h_drd0(0);\nHistory h_late0(0);\nHistory h_flub0(0);\nHistory h_e0(0);\nHistory h_wl0(0);\nHistory h_wr0(0);\nHistory h_head0(0);\nHistory h_brk0(0);\nHistory h_idl0(0);\nHistory h_pcs0(0);    // 1 when the pending close ends a slot of the current play\nHistory h_pos1(0);\nHistory h_v1(0);\nHistory h_vb1(0);\nHistory h_mode1(0);      // 0 idle, 1 cue pulse, 2 wait for release, 3 run, 4 hold, 5 spin, 6 brake\nHistory h_k1(0);\nHistory h_has1(0);\nHistory h_bs1(0);\nHistory h_be1(1000000000);\nHistory h_p01(0);\nHistory h_rate1(1);\nHistory h_kind1(0);\nHistory h_gm1(0);\nHistory h_gd1(1);\nHistory h_g01(0);\nHistory h_amp1(1);\nHistory h_str1(0);\nHistory h_gen1(0);\nHistory h_cued1(0);\nHistory h_pdc1(0);\nHistory h_pa1(0);\nHistory h_pdv1(0);\nHistory h_trrem1(0);\nHistory h_push1(0);\nHistory h_rk1(0);\nHistory h_exp1(0);\nHistory h_expok1(0);\nHistory h_cuchk1(0);\nHistory h_culen1(0);\nHistory h_cuk1(0);\nHistory h_cua1(0);\nHistory h_spm1(1);\nHistory h_spa1(0);\nHistory h_spdv1(0);\nHistory h_trem1(0);\nHistory h_gt1(0);\nHistory h_g1(0);\nHistory h_gamp1(1);\nHistory h_popen1(1000000000);\nHistory h_pclose1(1000000000);\nHistory h_slot1(0);\nHistory h_drk1(0);\nHistory h_drlen1(0);\nHistory h_drd1(0);\nHistory h_late1(0);\nHistory h_flub1(0);\nHistory h_e1(0);\nHistory h_wl1(0);\nHistory h_wr1(0);\nHistory h_head1(0);\nHistory h_brk1(0);\nHistory h_idl1(0);\nHistory h_pcs1(0);    // 1 when the pending close ends a slot of the current play\n\n\n\n// every local starts at zero, so none is read before it is set\nNONE = 0; SR = 0; beatint = 0; prevph = 0; curgen = 0; flushb = 0; recstate = 0; wi = 0;\nrecstart = 0; recb = 0; prevb = 0; live = 0; pos0 = 0; v0 = 0; vb0 = 0; mode0 = 0;\nk0 = 0; has0 = 0; bs0 = 0; be0 = 0; p00 = 0; rate0 = 0; kind0 = 0; gm0 = 0;\ngd0 = 0; g00 = 0; amp0 = 0; str0 = 0; gen0 = 0; cued0 = 0; pdc0 = 0; pa0 = 0;\npdv0 = 0; trrem0 = 0; push0 = 0; rk0 = 0; exp0 = 0; expok0 = 0; cuchk0 = 0; culen0 = 0;\ncuk0 = 0; cua0 = 0; spm0 = 0; spa0 = 0; spdv0 = 0; trem0 = 0; gt0 = 0; g0 = 0;\ngamp0 = 0; popen0 = 0; pclose0 = 0; slot0 = 0; drk0 = 0; drlen0 = 0; drd0 = 0; late0 = 0;\nflub0 = 0; e0 = 0; wl0 = 0; wr0 = 0; head0 = 0; brk0 = 0; idl0 = 0; pcs0 = 0;\npos1 = 0; v1 = 0; vb1 = 0; mode1 = 0; k1 = 0; has1 = 0; bs1 = 0; be1 = 0;\np01 = 0; rate1 = 0; kind1 = 0; gm1 = 0; gd1 = 0; g01 = 0; amp1 = 0; str1 = 0;\ngen1 = 0; cued1 = 0; pdc1 = 0; pa1 = 0; pdv1 = 0; trrem1 = 0; push1 = 0; rk1 = 0;\nexp1 = 0; expok1 = 0; cuchk1 = 0; culen1 = 0; cuk1 = 0; cua1 = 0; spm1 = 0; spa1 = 0;\nspdv1 = 0; trem1 = 0; gt1 = 0; g1 = 0; gamp1 = 0; popen1 = 0; pclose1 = 0; slot1 = 0;\ndrk1 = 0; drlen1 = 0; drd1 = 0; late1 = 0; flub1 = 0; e1 = 0; wl1 = 0; wr1 = 0;\nhead1 = 0; brk1 = 0; idl1 = 0; pcs1 = 0; ph = 0; jumped = 0; cand = 0; B = 0;\nspb = 0; started = 0; stopped = 0; tau = 0; taus = 0; dk = 0; dks = 0; pre0 = 0;\npushsd = 0; vmax = 0; reach = 0; reljit = 0; rush = 0; fadjit = 0; fadearly = 0; fadlate = 0;\ncuebase = 0; cueper = 0; tremsd = 0; lvl = 0; fade = 0; flubp = 0; flubsd = 0; recspb = 0;\ntrema = 0; tremb = 0; done = 0; active = 0; flushed = 0; dragnew = 0; rbs = 0; boot = 0;\nqt = 0; need_next = 0; base = 0; e_bs = 0; e_be = 0; e_p0 = 0; e_kind = 0; e_cm = 0;\ne_rate = 0; e_gm = 0; e_gd = 0; e_g0 = 0; e_amp = 0; e_dist = 0; e_gen = 0; e_str = 0;\nkeep = 0; cut = 0; gg = 0; cont = 0; oldbe = 0; latej = 0; earlyj = 0; gap = 0;\nj = 0; s0 = 0; pre = 0; adv = 0; d0 = 0; cerr = 0; prel = 0; fast = 0;\njit = 0; tlen = 0; tr = 0; dist = 0; need = 0; dc = 0; sd = 0; nb = 0;\nw = 0; u = 0; rate_e = 0; vv = 0; err = 0; vplay = 0; waitrel = 0; a = 0;\ngend = 0; gain0 = 0; yl0 = 0; yr0 = 0; ip = 0; fr = 0; i0 = 0; i1 = 0;\ni2 = 0; i3 = 0; xm = 0; x0 = 0; x1 = 0; x2 = 0; c1 = 0; c2 = 0;\nc3 = 0; hw = 0; ja = 0; jb = 0; wa = 0; gain1 = 0; yl1 = 0; yr1 = 0;\nmixl = 0; mixr = 0; kn = 0; statc = 0;\n// Doubles: gen~ core. Two modelled turntables, a crossfader, and the record buffer.\n// GENERATED INTO THE DEVICE by tools/build_device.py. Edit this template, not the device.\n// Lines between #DECK and #ENDDECK are written out twice, with $ replaced by 0 and 1\n// and {D} by the deck number. Every History is copied to a local at the top of the\n// sample and written back at the end, so no History is read after it is written.\n//\n// in1, in2: audio in (L, R). in3: plugphasor~ (0..1 ramp per beat).\n// out1, out2: audio out.\n// Buffers: dbl_rec (the record, stereo), dbl_q (play queue), dbl_stat (status for JS).\n\n\n// transport, set from JS\n// record\n// hands\n// output\n// queue\n// drag\n\n// global state\n\n\n\n// ---------------- per sample ----------------\nNONE = 1000000000;\nSR = samplerate;\nbeatint = h_beatint; prevph = h_prevph; curgen = h_curgen; flushb = h_flushb;\nrecstate = h_recstate; wi = h_wi; recstart = h_recstart; recb = h_recb; prevb = h_prevb;\nlive = h_live;\npos0 = h_pos0; v0 = h_v0; vb0 = h_vb0; mode0 = h_mode0; k0 = h_k0; has0 = h_has0;\nbs0 = h_bs0; be0 = h_be0; p00 = h_p00; rate0 = h_rate0; kind0 = h_kind0; gm0 = h_gm0;\ngd0 = h_gd0; g00 = h_g00; amp0 = h_amp0; str0 = h_str0; gen0 = h_gen0; cued0 = h_cued0;\npdc0 = h_pdc0; pa0 = h_pa0; pdv0 = h_pdv0; trrem0 = h_trrem0; push0 = h_push0; rk0 = h_rk0;\nexp0 = h_exp0; expok0 = h_expok0; cuchk0 = h_cuchk0; culen0 = h_culen0; cuk0 = h_cuk0; cua0 = h_cua0;\nspm0 = h_spm0; spa0 = h_spa0; spdv0 = h_spdv0; trem0 = h_trem0;\ngt0 = h_gt0; g0 = h_g0; gamp0 = h_gamp0; popen0 = h_popen0; pclose0 = h_pclose0; slot0 = h_slot0;\ndrk0 = h_drk0; drlen0 = h_drlen0; drd0 = h_drd0; late0 = h_late0; flub0 = h_flub0; e0 = h_e0;\nwl0 = h_wl0; wr0 = h_wr0; head0 = h_head0; brk0 = h_brk0; idl0 = h_idl0; pcs0 = h_pcs0;\npos1 = h_pos1; v1 = h_v1; vb1 = h_vb1; mode1 = h_mode1; k1 = h_k1; has1 = h_has1;\nbs1 = h_bs1; be1 = h_be1; p01 = h_p01; rate1 = h_rate1; kind1 = h_kind1; gm1 = h_gm1;\ngd1 = h_gd1; g01 = h_g01; amp1 = h_amp1; str1 = h_str1; gen1 = h_gen1; cued1 = h_cued1;\npdc1 = h_pdc1; pa1 = h_pa1; pdv1 = h_pdv1; trrem1 = h_trrem1; push1 = h_push1; rk1 = h_rk1;\nexp1 = h_exp1; expok1 = h_expok1; cuchk1 = h_cuchk1; culen1 = h_culen1; cuk1 = h_cuk1; cua1 = h_cua1;\nspm1 = h_spm1; spa1 = h_spa1; spdv1 = h_spdv1; trem1 = h_trem1;\ngt1 = h_gt1; g1 = h_g1; gamp1 = h_gamp1; popen1 = h_popen1; pclose1 = h_pclose1; slot1 = h_slot1;\ndrk1 = h_drk1; drlen1 = h_drlen1; drd1 = h_drd1; late1 = h_late1; flub1 = h_flub1; e1 = h_e1;\nwl1 = h_wl1; wr1 = h_wr1; head1 = h_head1; brk1 = h_brk1; idl1 = h_idl1; pcs1 = h_pcs1;\n\n// ---- song position ----\nph = in3;\nif (playing > 0.5) {\n\tif (ph < prevph - 0.5) {\n\t\tbeatint = beatint + 1;\n\t}\n}\nprevph = ph;\njumped = 0;\nif (sbseq != h_sbseq) {\n\tcand = floor(songbeat - ph + 0.5);\n\tif (abs(cand - beatint) > 0.5) {\n\t\tif (playing > 0.5) {\n\t\t\tjumped = 1;\n\t\t}\n\t\tbeatint = cand;\n\t}\n}\nB = beatint + ph;\nspb = SR * 60 / tempo;\nstarted = 0;\nstopped = 0;\nif (playing > 0.5 && h_wasplay < 0.5) {\n\tstarted = 1;\n}\nif (playing < 0.5 && h_wasplay > 0.5) {\n\tstopped = 1;\n}\n\n// ---- hands ----\ntau = SR * (0.002 + 0.018 * weight);\ntaus = SR * (0.003 + 0.012 * weight);\ndk = exp(-1 / tau);\ndks = exp(-1 / taus);\npre0 = tau * (3.2 - 1.2 * weight);\npushsd = 0.10 * weight;\nvmax = 6 / max(weight, 0.05);\nreach = SR * 0.045 * weight;\nreljit = SR * 0.007 * timing;\nrush = SR * 0.002 * timing;\nfadjit = SR * 0.003 * timing;\nfadearly = SR * 0.002 * slop;\nfadlate = SR * 0.004 * slop;\ncuebase = SR * 0.002 * slop;\ncueper = 0.012 * slop;\ntremsd = 0.03 * slop;\nlvl = 0.07 * slop;\nfade = SR * (0.0008 + 0.0012 * slop);\nflubp = 0.05 * risk;\nflubsd = SR * 0.035;\nrate0 = L * tempo / (Lb * 60 * SR);\nrecspb = L / Lb;\ntrema = exp(-twopi * 100 / SR);\ntremb = sqrt(1 - trema * trema);\n\n// ---- recording ----\nif (reccmd != h_reccmd) {\n\tif (recmode > 0.5) {\n\t\trecstate = 1;\n\t} else {\n\t\trecstate = 0;\n\t}\n}\nif (recstate == 1 && playing > 0.5) {\n\tif (floor(B / 4) > floor(prevb / 4) && B - prevb < 1) {\n\t\trecstate = 2;\n\t\twi = 0;\n\t\trecstart = floor(B / 4 + 0.5) * 4;\n\t}\n}\nif (recstate == 2) {\n\tif (playing < 0.5) {\n\t\trecstate = 0;\n\t} else {\n\t\tpoke(rec, in1, wi, 0);\n\t\tpoke(rec, in2, wi, 1);\n\t\twi = wi + 1;\n\t\tdone = 0;\n\t\tif (reclen > 0 && B - recstart >= reclen) {\n\t\t\tdone = 1;\n\t\t\trecb = reclen;\n\t\t}\n\t\tif (reclen <= 0 && recstop > 0.5 && floor(B / 4) > floor(prevb / 4) && B - recstart > 3.5) {\n\t\t\tdone = 1;\n\t\t\trecb = floor((B - recstart) / 4 + 0.5) * 4;\n\t\t}\n\t\tif (wi >= dim(rec) - 1) {\n\t\t\tdone = 1;\n\t\t\trecb = max(4, floor((B - recstart) / 4) * 4);\n\t\t}\n\t\tif (done > 0.5) {\n\t\t\trecstate = 3;\n\t\t}\n\t}\n}\nprevb = B;\n\nactive = 0;\nif (haverec > 0.5 && recstate != 1 && recstate != 2) {\n\tactive = 1;\n}\n\n// ---- flush ----\nflushed = 0;\nif (fgen != curgen) {\n\tcurgen = fgen;\n\tflushb = fbeat;\n\tpoke(gcut, fbeat, wrap(curgen, 0, 256), 0);\n\tflushed = 1;\n}\n\n// ---- drag ----\ndragnew = 0;\nif (dragseq != h_dragseq) {\n\tdragnew = 1;\n}\n\nrbs = wrap(B - anchor, 0, Lb);\n\n// ======== deck 0 ========\nif (stopped > 0.5 || jumped > 0.5 || active < 0.5) {\n\thead0 = qt0;\n\thas0 = 0;\n\tpopen0 = NONE;\n\tpclose0 = NONE;\n\tculen0 = 0;\n\texpok0 = 0;\n\tif (stopped > 0.5 && brake > 0.5 && gt0 > 0.5) {\n\t\tmode0 = 6;\n\t\tbrk0 = floor(0.15 * SR);\n\t} else {\n\t\tif (mode0 != 6) {\n\t\t\tmode0 = 0;\n\t\t\tgt0 = 0;\n\t\t}\n\t}\n}\nif (mode0 == 6) {\n\tbrk0 = brk0 - 1;\n\tif (brk0 <= 0) {\n\t\tgt0 = 0;\n\t\tmode0 = 0;\n\t}\n}\n\n// a flush cuts the current play\nif (flushed > 0.5 && has0 > 0.5 && gen0 >= 0 && gen0 < curgen) {\n\tif (bs0 >= flushb) {\n\t\thas0 = 0;\n\t\tpopen0 = NONE;\n\t\tif (mode0 == 1 || mode0 == 2) {\n\t\t\tmode0 = 0;\n\t\t}\n\t} else {\n\t\tif (be0 > flushb) {\n\t\t\tbe0 = flushb;\n\t\t\tif (pclose0 > flushb + fadlate / spb) {\n\t\t\t\tpclose0 = flushb + fadlate / spb;\n\t\t\t}\n\t\t\tif (popen0 > flushb) {\n\t\t\t\tpopen0 = NONE;\n\t\t\t}\n\t\t}\n\t}\n}\n\n// the safety net: if nothing is planned, play straight\nboot = 0;\nif (B < bootb - 0.02) {\n\tboot = 1;\n}\nif (0 == 0 && active > 0.5 && playing > 0.5 && has0 < 0.5 && mode0 != 1 && mode0 != 2 && ((head0 >= qt0 && has1 < 0.5 && head1 >= qt1) || boot > 0.5)) {\n\thas0 = 1;\n\tbs0 = B; be0 = NONE;\n\tif (boot > 0.5) {\n\t\tbe0 = bootb;\n\t\tpclose0 = bootb;\n\t} p00 = rbs * recspb; rate0 = 1; kind0 = 0; gm0 = 0; amp0 = 1; str0 = 1; gen0 = -1;\n\tpos0 = rbs * recspb;\n\tv0 = 1; vb0 = 1; mode0 = 3; rk0 = 0; cued0 = 0;\n\texp0 = pos0; expok0 = 1; cuchk0 = floor(0.35 * SR); culen0 = 0;\n\tgt0 = 1; popen0 = NONE; pclose0 = NONE;\n\tgamp0 = strlev;\n\tlive = 0;\n}\n// next play\nqt = qt0;\nneed_next = 0;\nif (active > 0.5 && playing > 0.5) {\n\tif (has0 < 0.5 || B >= be0) {\n\t\tneed_next = 1;\n\t}\n}\nif (need_next > 0.5) {\n\tif (head0 < qt) {\n\t\tbase = (0 * 64 + wrap(head0, 0, 64)) * 16;\n\t\te_bs = peek(q, base, 0);\n\t\te_be = peek(q, base + 1, 0);\n\t\te_p0 = peek(q, base + 2, 0);\n\t\te_kind = peek(q, base + 3, 0);\n\t\te_cm = peek(q, base + 4, 0);\n\t\te_rate = peek(q, base + 5, 0);\n\t\te_gm = peek(q, base + 6, 0);\n\t\te_gd = peek(q, base + 7, 0);\n\t\te_g0 = peek(q, base + 8, 0);\n\t\te_amp = peek(q, base + 9, 0);\n\t\te_dist = peek(q, base + 10, 0);\n\t\te_gen = peek(q, base + 11, 0);\n\t\te_str = peek(q, base + 12, 0);\n\t\thead0 = head0 + 1;\n\t\tkeep = 1;\n\t\tif (e_gen < curgen) {\n\t\t\tcut = NONE;\n\t\t\tfor (gg = max(e_gen + 1, curgen - 255); gg <= curgen; gg = gg + 1) {\n\t\t\t\tcut = min(cut, peek(gcut, wrap(gg, 0, 256), 0));\n\t\t\t}\n\t\t\tif (e_bs >= cut) {\n\t\t\t\tkeep = 0;\n\t\t\t}\n\t\t\te_be = min(e_be, cut);\n\t\t}\n\t\tif (keep > 0.5) {\n\t\t\tcont = 0;\n\t\t\tif (e_cm == 2 && has0 > 0.5 && mode0 == 3 && abs(be0 - e_bs) < 0.0001) {\n\t\t\t\tcont = 1;\n\t\t\t}\n\t\t\toldbe = be0;\n\t\t\thas0 = 1;\n\t\t\tbs0 = e_bs; be0 = e_be; p00 = e_p0; rate0 = e_rate; kind0 = e_kind;\n\t\t\tgm0 = e_gm; gd0 = max(e_gd, 0.001); g00 = e_g0; amp0 = e_amp; str0 = e_str; gen0 = e_gen;\n\t\t\tif (kind0 == 2) {\n\t\t\t\tp00 = e_dist;\n\t\t\t}\n\t\t\tlatej = (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\t\tearlyj = (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\t\tgap = max((bs0 - B) * spb, 0);\n\t\t\tif (cont > 0.5) {\n\t\t\t\t// carry on: no cue; the fader follows the new play from where it is\n\t\t\t\tif (gm0 < 0.5) {\n\t\t\t\t\tif (pclose0 < NONE) {\n\t\t\t\t\t\tpclose0 = be0 + latej;\n\t\t\t\t\t}\n\t\t\t\t\tif (gt0 < 0.5 && popen0 >= NONE) {\n\t\t\t\t\t\tpopen0 = B;\n\t\t\t\t\t\tslot0 = 0;\n\t\t\t\t\t}\n\t\t\t\t} else {\n\t\t\t\t\tj = floor((B - g00) / gd0 + 0.0001);\n\t\t\t\t\tif (wrap(j, 0, 2) == gm0 - 1) {\n\t\t\t\t\t\tslot0 = j;\n\t\t\t\t\t\tif (gt0 > 0.5) {\n\t\t\t\t\t\t\tpclose0 = min(be0, g00 + (j + 1) * gd0) + latej;\n\t\t\t\t\t\t\tpcs0 = 1;\n\t\t\t\t\t\t} else {\n\t\t\t\t\t\t\tpopen0 = B;\n\t\t\t\t\t\t}\n\t\t\t\t\t} else {\n\t\t\t\t\t\tslot0 = j - 1;\n\t\t\t\t\t\tif (gt0 > 0.5) {\n\t\t\t\t\t\t\tpclose0 = B + latej;\n\t\t\t\t\t\t\tpcs0 = 1;\n\t\t\t\t\t\t} else {\n\t\t\t\t\t\t\ts0 = g00 + (j + 1) * gd0;\n\t\t\t\t\t\t\tif (s0 < be0) {\n\t\t\t\t\t\t\t\tpopen0 = s0 - earlyj;\n\t\t\t\t\t\t\t\tslot0 = j + 1;\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t} else {\n\t\t\t\tpcs0 = 0;\n\t\t\t\tculen0 = 0;\n\t\t\t\texpok0 = 0;\n\t\t\t\tcued0 = 0;\n\t\t\t\t// the gate: first opening of this play\n\t\t\t\tif (gm0 < 0.5) {\n\t\t\t\t\tpopen0 = bs0 - earlyj;\n\t\t\t\t\tslot0 = 0;\n\t\t\t\t} else {\n\t\t\t\t\tj = ceil((bs0 - g00) / gd0 - 0.0001);\n\t\t\t\t\tif (wrap(j, 0, 2) != gm0 - 1) {\n\t\t\t\t\t\tj = j + 1;\n\t\t\t\t\t}\n\t\t\t\t\ts0 = g00 + j * gd0;\n\t\t\t\t\tif (s0 < be0) {\n\t\t\t\t\t\tpopen0 = s0 - earlyj;\n\t\t\t\t\t} else {\n\t\t\t\t\t\tpopen0 = NONE;\n\t\t\t\t\t}\n\t\t\t\t\tslot0 = j;\n\t\t\t\t}\n\t\t\t\tif (kind0 == 0 && e_cm != 1) {\n\t\t\t\t\t// cued play: the hand pulls the record back to its cue and lets go early\n\t\t\t\t\tcued0 = 1;\n\t\t\t\t\tpre = min(pre0, floor(gap / 2));\n\t\t\t\t\tadv = rate0 * rate0 * (pre - tau * (1 - exp(-pre / tau)));\n\t\t\t\t\td0 = abs(wrap(p00 - adv - pos0, -L / 2, L / 2));\n\t\t\t\t\tcerr = 0;\n\t\t\t\t\tif (d0 > 8) {\n\t\t\t\t\t\tcerr = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * min(cuebase + cueper * d0, 0.03 * SR);\n\t\t\t\t\t}\n\t\t\t\t\tprel = p00 - adv + cerr;\n\t\t\t\t\tfast = min(0.25 * SR / max(gap, 1), 2);\n\t\t\t\t\te0 = 0.6 * e0 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * reljit * 0.8;\n\t\t\t\t\tjit = e0 * (1 + 0.5 * fast) - rush * fast;\n\t\t\t\t\tif (noise() * 0.5 + 0.5 < flubp) {\n\t\t\t\t\t\tjit = jit + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * flubsd;\n\t\t\t\t\t\tflub0 = flub0 + 1;\n\t\t\t\t\t}\n\t\t\t\t\ttlen = max((be0 - B) * spb, 1);\n\t\t\t\t\ttr = gap - pre + jit;\n\t\t\t\t\ttr = min(max(tr, 0), tlen - 1);\n\t\t\t\t\tdist = wrap(prel - pos0, -L / 2, L / 2);\n\t\t\t\t\tneed = 0;\n\t\t\t\t\tif (abs(dist) > 8) {\n\t\t\t\t\t\tneed = reach + abs(dist) / rate0 / vmax;\n\t\t\t\t\t}\n\t\t\t\t\tif (tr < need) {\n\t\t\t\t\t\ttr = min(need, tlen - 1);\n\t\t\t\t\t\tlate0 = late0 + 1;\n\t\t\t\t\t}\n\t\t\t\t\tdc = 0;\n\t\t\t\t\tif (need > 0) {\n\t\t\t\t\t\tdc = floor(min(need, tr));\n\t\t\t\t\t}\n\t\t\t\t\tif (dc < 4) {\n\t\t\t\t\t\tpos0 = wrap(pos0 + dist, 0, L);\n\t\t\t\t\t\tif (abs(dist) > 8) {\n\t\t\t\t\t\t\tv0 = 0;\n\t\t\t\t\t\t}\n\t\t\t\t\t\tmode0 = 2;\n\t\t\t\t\t} else {\n\t\t\t\t\t\tsd = v0 * dks * (1 - pow(dks, dc)) / (1 - dks);\n\t\t\t\t\t\tpa0 = (dist / rate0 - sd) / ((dc + 1) / 2);\n\t\t\t\t\t\tpdv0 = v0;\n\t\t\t\t\t\tpdc0 = dc;\n\t\t\t\t\t\tk0 = 0;\n\t\t\t\t\t\tmode0 = 1;\n\t\t\t\t\t}\n\t\t\t\t\ttrrem0 = tr;\n\t\t\t\t\tpush0 = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * pushsd;\n\t\t\t\t\texp0 = wrap(p00 - rate0 * rate0 * gap, 0, L);\n\t\t\t\t\texpok0 = 1;\n\t\t\t\t} else {\n\t\t\t\t\tif (kind0 == 0) {\n\t\t\t\t\t\t// let go from wherever it is\n\t\t\t\t\t\ttrrem0 = gap;\n\t\t\t\t\t\tmode0 = 2;\n\t\t\t\t\t}\n\t\t\t\t\tif (kind0 == 1) {\n\t\t\t\t\t\ttrrem0 = gap;\n\t\t\t\t\t\tmode0 = 2;\n\t\t\t\t\t}\n\t\t\t\t\tif (kind0 == 2) {\n\t\t\t\t\t\ttrrem0 = gap;\n\t\t\t\t\t\tmode0 = 2;\n\t\t\t\t\t\tspm0 = max(floor((be0 - bs0) * spb * 0.85), 16);\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t} else {\n\t\tif (has0 > 0.5 && B >= be0) {\n\t\t\thas0 = 0;\n\t\t\tif (mode0 == 3) {\n\t\t\t\tmode0 = 0;\n\t\t\t}\n\t\t}\n\t}\n}\n\nif (0 == 0 && be0 >= NONE && has0 > 0.5) {\n\tnb = NONE;\n\tif (head0 < qt) {\n\t\tnb = min(nb, peek(q, (0 * 64 + wrap(head0, 0, 64)) * 16, 0));\n\t}\n\tif (head1 < qt1) {\n\t\tnb = min(nb, peek(q, (64 + wrap(head1, 0, 64)) * 16, 0));\n\t}\n\tif (nb < NONE) {\n\t\tbe0 = max(nb, B);\n\t\tpclose0 = be0 + fadlate / spb;\n\t}\n}\n\n// ---- platter ----\nif (mode0 == 1) {\n\tw = 0.5 - 0.5 * cos(twopi * (k0 + 1) / (pdc0 + 1));\n\tpdv0 = pdv0 * dks;\n\tv0 = pdv0 + pa0 * w;\n\tk0 = k0 + 1;\n\ttrrem0 = trrem0 - 1;\n\tif (k0 >= pdc0) {\n\t\tv0 = 0;\n\t\tmode0 = 2;\n\t}\n} else {\n\tif (mode0 == 2) {\n\t\tv0 = v0 * dks;\n\t\ttrrem0 = trrem0 - 1;\n\t\tif (trrem0 <= 0) {\n\t\t\tif (kind0 == 1) {\n\t\t\t\tmode0 = 4;\n\t\t\t} else {\n\t\t\t\tif (kind0 == 2) {\n\t\t\t\t\tspdv0 = v0;\n\t\t\t\t\tk0 = 0;\n\t\t\t\t\tmode0 = 5;\n\t\t\t\t} else {\n\t\t\t\t\tvb0 = v0;\n\t\t\t\t\trk0 = 0;\n\t\t\t\t\tmode0 = 3;\n\t\t\t\t\tcuchk0 = floor(0.35 * SR);\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\nif (mode0 == 5 && k0 == 0) {\n\t// spin distance lives in p0 for a spin entry\n\tsd = spdv0 * dks * (1 - pow(dks, spm0)) / (1 - dks);\n\tspa0 = (p00 / rate0 - sd) / (spm0 * 0.2475168);\n}\nif (mode0 == 5) {\n\tu = (k0 + 0.5) / spm0;\n\tspdv0 = spdv0 * dks;\n\tv0 = spdv0 + spa0 * exp(-3 * u) * (1 - exp(-60 * u)) * sqrt(max(1 - u, 0));\n\tk0 = k0 + 1;\n\tif (k0 >= spm0) {\n\t\tv0 = 0;\n\t\tmode0 = 0;\n\t}\n}\nif (mode0 == 4) {\n\ttrem0 = trema * trem0 + tremb * ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678);\n\tv0 = v0 * dks + trem0 * tremsd;\n\tif (B >= be0) {\n\t\tmode0 = 0;\n\t}\n}\nif (mode0 == 0 || mode0 == 6) {\n\tif (mode0 == 6) {\n\t\tv0 = v0 * exp(-1 / (0.04 * SR));\n\t} else {\n\t\tv0 = v0 * dks;\n\t}\n}\n// a deck left alone runs muted in time with the song, ready to be caught\nif (mode0 == 0 && has0 < 0.5 && g0 <= 0 && active > 0.5 && playing > 0.5) {\n\tidl0 = idl0 + 1;\n\tif (idl0 > 0.25 * SR) {\n\t\tpos0 = rbs * recspb;\n\t\tv0 = 1;\n\t}\n} else {\n\tidl0 = 0;\n}\nif (mode0 == 3) {\n\trate_e = rate0;\n\tif (0 == 1) {\n\t\trate_e = rate0 * (1 + 0.0012 * wear);\n\t}\n\tvb0 = vb0 + (rate_e - vb0) * (1 - dk);\n\tvv = vb0;\n\tif (cued0 > 0.5) {\n\t\tvv = vv + rate_e * push0 * (exp(-rk0 / (2.5 * tau)) - exp(-rk0 / (0.6 * tau)));\n\t}\n\trk0 = rk0 + 1;\n\t// catching up: the DJ hears the record is off and nudges it back\n\tif (expok0 > 0.5) {\n\t\tcuchk0 = cuchk0 - 1;\n\t\tif (cuchk0 <= 0) {\n\t\t\terr = wrap(exp0 - pos0, -L / 2, L / 2);\n\t\t\tif (abs(err) > 0.0003 * SR * rate0) {\n\t\t\t\tculen0 = floor(0.6 * SR);\n\t\t\t\tcuk0 = 0;\n\t\t\t\tcua0 = 0.8 * err / rate0 / ((culen0 + 1) / 2);\n\t\t\t\tcuchk0 = culen0 + floor(0.25 * SR);\n\t\t\t} else {\n\t\t\t\tcuchk0 = floor(2 * SR);\n\t\t\t}\n\t\t}\n\t}\n\tif (culen0 > 0) {\n\t\tvv = vv + cua0 * (0.5 - 0.5 * cos(twopi * (cuk0 + 1) / (culen0 + 1)));\n\t\tcuk0 = cuk0 + 1;\n\t\tif (cuk0 >= culen0) {\n\t\t\tculen0 = 0;\n\t\t}\n\t}\n\tv0 = vv;\n}\nif (expok0 > 0.5) {\n\texp0 = wrap(exp0 + rate0 * rate0, 0, L);\n}\n\n// a finger on the platter\nif (dragnew > 0.5 && live == 0) {\n\tdrlen0 = floor(0.30 * SR);\n\tdrk0 = 0;\n\tdrd0 = dragdepth;\n}\nvplay = v0;\nif (drlen0 > 0) {\n\tvplay = v0 * (1 - drd0 * (0.5 - 0.5 * cos(twopi * drk0 / drlen0)));\n\tdrk0 = drk0 + 1;\n\tif (drk0 >= drlen0) {\n\t\tdrlen0 = 0;\n\t}\n}\npos0 = wrap(pos0 + vplay * rate0, 0, L);\n\n// ---- fader ----\n// a late hand: the fader waits until the record has been let go\nwaitrel = 0;\nif (has0 > 0.5 && cued0 > 0.5 && kind0 == 0 && (mode0 == 1 || mode0 == 2)) {\n\twaitrel = 1;\n}\nif (B >= popen0 && waitrel < 0.5) {\n\tpopen0 = NONE;\n\tif (has0 > 0.5) {\n\t\tgt0 = 1;\n\t\tlive = 0;\n\t\ta = amp0;\n\t\tif (str0 > 0.5) {\n\t\t\ta = a * strlev;\n\t\t}\n\t\tgamp0 = a * min(1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * lvl, 1.12);\n\t\tgend = be0;\n\t\tif (gm0 > 0.5) {\n\t\t\tgend = min(be0, g00 + (slot0 + 1) * gd0);\n\t\t}\n\t\tif (kind0 == 2) {\n\t\t\tgend = min(be0, bs0 + (be0 - bs0) * 0.85);\n\t\t}\n\t\tpclose0 = gend + (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\tpcs0 = 1;\n\t}\n}\nif (B >= pclose0) {\n\tpclose0 = NONE;\n\tgt0 = 0;\n\tif (gm0 > 0.5 && has0 > 0.5 && pcs0 > 0.5) {\n\t\tslot0 = slot0 + 2;\n\t\ts0 = g00 + slot0 * gd0;\n\t\tif (s0 < be0) {\n\t\t\tpopen0 = s0 - (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\t}\n\t}\n}\nif (gt0 > 0.5) {\n\tg0 = min(g0 + 1 / fade, 1);\n} else {\n\tg0 = max(g0 - 1 / fade, 0);\n}\ngain0 = gamp0 * (0.5 - 0.5 * cos(pi * g0));\n\n// ---- read the record (4-point Hermite) ----\nyl0 = 0;\nyr0 = 0;\nif (g0 > 0 && active > 0.5) {\n\tip = floor(pos0);\n\tfr = pos0 - ip;\n\ti0 = wrap(ip - 1, 0, L); i1 = wrap(ip, 0, L); i2 = wrap(ip + 1, 0, L); i3 = wrap(ip + 2, 0, L);\n\txm = peek(rec, i0, 0); x0 = peek(rec, i1, 0); x1 = peek(rec, i2, 0); x2 = peek(rec, i3, 0);\n\tc1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);\n\tyl0 = ((c3 * fr + c2) * fr + c1) * fr + x0;\n\txm = peek(rec, i0, 1); x0 = peek(rec, i1, 1); x1 = peek(rec, i2, 1); x2 = peek(rec, i3, 1);\n\tc1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);\n\tyr0 = ((c3 * fr + c2) * fr + c1) * fr + x0;\n\tif (abs(vplay * rate0) > 2) {\n\t\t// fast spin: average a little either side to tame aliasing\n\t\thw = abs(vplay * rate0) * 0.5;\n\t\tja = wrap(floor(pos0 - hw), 0, L); jb = wrap(floor(pos0 + hw), 0, L);\n\t\tyl0 = 0.5 * yl0 + 0.25 * (peek(rec, ja, 0) + peek(rec, jb, 0));\n\t\tyr0 = 0.5 * yr0 + 0.25 * (peek(rec, ja, 1) + peek(rec, jb, 1));\n\t}\n\tif (0 == 1) {\n\t\twa = exp(-twopi * (20000 - 9000 * wear) / SR);\n\t\twl0 = (1 - wa) * yl0 + wa * wl0;\n\t\twr0 = (1 - wa) * yr0 + wa * wr0;\n\t\tyl0 = wl0 * (1 - 0.06 * wear);\n\t\tyr0 = wr0 * (1 - 0.06 * wear);\n\t}\n}\n// ======== deck 1 ========\nif (stopped > 0.5 || jumped > 0.5 || active < 0.5) {\n\thead1 = qt1;\n\thas1 = 0;\n\tpopen1 = NONE;\n\tpclose1 = NONE;\n\tculen1 = 0;\n\texpok1 = 0;\n\tif (stopped > 0.5 && brake > 0.5 && gt1 > 0.5) {\n\t\tmode1 = 6;\n\t\tbrk1 = floor(0.15 * SR);\n\t} else {\n\t\tif (mode1 != 6) {\n\t\t\tmode1 = 0;\n\t\t\tgt1 = 0;\n\t\t}\n\t}\n}\nif (mode1 == 6) {\n\tbrk1 = brk1 - 1;\n\tif (brk1 <= 0) {\n\t\tgt1 = 0;\n\t\tmode1 = 0;\n\t}\n}\n\n// a flush cuts the current play\nif (flushed > 0.5 && has1 > 0.5 && gen1 >= 0 && gen1 < curgen) {\n\tif (bs1 >= flushb) {\n\t\thas1 = 0;\n\t\tpopen1 = NONE;\n\t\tif (mode1 == 1 || mode1 == 2) {\n\t\t\tmode1 = 0;\n\t\t}\n\t} else {\n\t\tif (be1 > flushb) {\n\t\t\tbe1 = flushb;\n\t\t\tif (pclose1 > flushb + fadlate / spb) {\n\t\t\t\tpclose1 = flushb + fadlate / spb;\n\t\t\t}\n\t\t\tif (popen1 > flushb) {\n\t\t\t\tpopen1 = NONE;\n\t\t\t}\n\t\t}\n\t}\n}\n\n// the safety net: if nothing is planned, play straight\nboot = 0;\nif (B < bootb - 0.02) {\n\tboot = 1;\n}\nif (1 == 0 && active > 0.5 && playing > 0.5 && has1 < 0.5 && mode1 != 1 && mode1 != 2 && ((head1 >= qt1 && has1 < 0.5 && head1 >= qt1) || boot > 0.5)) {\n\thas1 = 1;\n\tbs1 = B; be1 = NONE;\n\tif (boot > 0.5) {\n\t\tbe1 = bootb;\n\t\tpclose1 = bootb;\n\t} p01 = rbs * recspb; rate1 = 1; kind1 = 0; gm1 = 0; amp1 = 1; str1 = 1; gen1 = -1;\n\tpos1 = rbs * recspb;\n\tv1 = 1; vb1 = 1; mode1 = 3; rk1 = 0; cued1 = 0;\n\texp1 = pos1; expok1 = 1; cuchk1 = floor(0.35 * SR); culen1 = 0;\n\tgt1 = 1; popen1 = NONE; pclose1 = NONE;\n\tgamp1 = strlev;\n\tlive = 0;\n}\n// next play\nqt = qt1;\nneed_next = 0;\nif (active > 0.5 && playing > 0.5) {\n\tif (has1 < 0.5 || B >= be1) {\n\t\tneed_next = 1;\n\t}\n}\nif (need_next > 0.5) {\n\tif (head1 < qt) {\n\t\tbase = (1 * 64 + wrap(head1, 0, 64)) * 16;\n\t\te_bs = peek(q, base, 0);\n\t\te_be = peek(q, base + 1, 0);\n\t\te_p0 = peek(q, base + 2, 0);\n\t\te_kind = peek(q, base + 3, 0);\n\t\te_cm = peek(q, base + 4, 0);\n\t\te_rate = peek(q, base + 5, 0);\n\t\te_gm = peek(q, base + 6, 0);\n\t\te_gd = peek(q, base + 7, 0);\n\t\te_g0 = peek(q, base + 8, 0);\n\t\te_amp = peek(q, base + 9, 0);\n\t\te_dist = peek(q, base + 10, 0);\n\t\te_gen = peek(q, base + 11, 0);\n\t\te_str = peek(q, base + 12, 0);\n\t\thead1 = head1 + 1;\n\t\tkeep = 1;\n\t\tif (e_gen < curgen) {\n\t\t\tcut = NONE;\n\t\t\tfor (gg = max(e_gen + 1, curgen - 255); gg <= curgen; gg = gg + 1) {\n\t\t\t\tcut = min(cut, peek(gcut, wrap(gg, 0, 256), 0));\n\t\t\t}\n\t\t\tif (e_bs >= cut) {\n\t\t\t\tkeep = 0;\n\t\t\t}\n\t\t\te_be = min(e_be, cut);\n\t\t}\n\t\tif (keep > 0.5) {\n\t\t\tcont = 0;\n\t\t\tif (e_cm == 2 && has1 > 0.5 && mode1 == 3 && abs(be1 - e_bs) < 0.0001) {\n\t\t\t\tcont = 1;\n\t\t\t}\n\t\t\toldbe = be1;\n\t\t\thas1 = 1;\n\t\t\tbs1 = e_bs; be1 = e_be; p01 = e_p0; rate1 = e_rate; kind1 = e_kind;\n\t\t\tgm1 = e_gm; gd1 = max(e_gd, 0.001); g01 = e_g0; amp1 = e_amp; str1 = e_str; gen1 = e_gen;\n\t\t\tif (kind1 == 2) {\n\t\t\t\tp01 = e_dist;\n\t\t\t}\n\t\t\tlatej = (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\t\tearlyj = (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\t\tgap = max((bs1 - B) * spb, 0);\n\t\t\tif (cont > 0.5) {\n\t\t\t\t// carry on: no cue; the fader follows the new play from where it is\n\t\t\t\tif (gm1 < 0.5) {\n\t\t\t\t\tif (pclose1 < NONE) {\n\t\t\t\t\t\tpclose1 = be1 + latej;\n\t\t\t\t\t}\n\t\t\t\t\tif (gt1 < 0.5 && popen1 >= NONE) {\n\t\t\t\t\t\tpopen1 = B;\n\t\t\t\t\t\tslot1 = 0;\n\t\t\t\t\t}\n\t\t\t\t} else {\n\t\t\t\t\tj = floor((B - g01) / gd1 + 0.0001);\n\t\t\t\t\tif (wrap(j, 0, 2) == gm1 - 1) {\n\t\t\t\t\t\tslot1 = j;\n\t\t\t\t\t\tif (gt1 > 0.5) {\n\t\t\t\t\t\t\tpclose1 = min(be1, g01 + (j + 1) * gd1) + latej;\n\t\t\t\t\t\t\tpcs1 = 1;\n\t\t\t\t\t\t} else {\n\t\t\t\t\t\t\tpopen1 = B;\n\t\t\t\t\t\t}\n\t\t\t\t\t} else {\n\t\t\t\t\t\tslot1 = j - 1;\n\t\t\t\t\t\tif (gt1 > 0.5) {\n\t\t\t\t\t\t\tpclose1 = B + latej;\n\t\t\t\t\t\t\tpcs1 = 1;\n\t\t\t\t\t\t} else {\n\t\t\t\t\t\t\ts0 = g01 + (j + 1) * gd1;\n\t\t\t\t\t\t\tif (s0 < be1) {\n\t\t\t\t\t\t\t\tpopen1 = s0 - earlyj;\n\t\t\t\t\t\t\t\tslot1 = j + 1;\n\t\t\t\t\t\t\t}\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t} else {\n\t\t\t\tpcs1 = 0;\n\t\t\t\tculen1 = 0;\n\t\t\t\texpok1 = 0;\n\t\t\t\tcued1 = 0;\n\t\t\t\t// the gate: first opening of this play\n\t\t\t\tif (gm1 < 0.5) {\n\t\t\t\t\tpopen1 = bs1 - earlyj;\n\t\t\t\t\tslot1 = 0;\n\t\t\t\t} else {\n\t\t\t\t\tj = ceil((bs1 - g01) / gd1 - 0.0001);\n\t\t\t\t\tif (wrap(j, 0, 2) != gm1 - 1) {\n\t\t\t\t\t\tj = j + 1;\n\t\t\t\t\t}\n\t\t\t\t\ts0 = g01 + j * gd1;\n\t\t\t\t\tif (s0 < be1) {\n\t\t\t\t\t\tpopen1 = s0 - earlyj;\n\t\t\t\t\t} else {\n\t\t\t\t\t\tpopen1 = NONE;\n\t\t\t\t\t}\n\t\t\t\t\tslot1 = j;\n\t\t\t\t}\n\t\t\t\tif (kind1 == 0 && e_cm != 1) {\n\t\t\t\t\t// cued play: the hand pulls the record back to its cue and lets go early\n\t\t\t\t\tcued1 = 1;\n\t\t\t\t\tpre = min(pre0, floor(gap / 2));\n\t\t\t\t\tadv = rate1 * rate0 * (pre - tau * (1 - exp(-pre / tau)));\n\t\t\t\t\td0 = abs(wrap(p01 - adv - pos1, -L / 2, L / 2));\n\t\t\t\t\tcerr = 0;\n\t\t\t\t\tif (d0 > 8) {\n\t\t\t\t\t\tcerr = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * min(cuebase + cueper * d0, 0.03 * SR);\n\t\t\t\t\t}\n\t\t\t\t\tprel = p01 - adv + cerr;\n\t\t\t\t\tfast = min(0.25 * SR / max(gap, 1), 2);\n\t\t\t\t\te1 = 0.6 * e1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * reljit * 0.8;\n\t\t\t\t\tjit = e1 * (1 + 0.5 * fast) - rush * fast;\n\t\t\t\t\tif (noise() * 0.5 + 0.5 < flubp) {\n\t\t\t\t\t\tjit = jit + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * flubsd;\n\t\t\t\t\t\tflub1 = flub1 + 1;\n\t\t\t\t\t}\n\t\t\t\t\ttlen = max((be1 - B) * spb, 1);\n\t\t\t\t\ttr = gap - pre + jit;\n\t\t\t\t\ttr = min(max(tr, 0), tlen - 1);\n\t\t\t\t\tdist = wrap(prel - pos1, -L / 2, L / 2);\n\t\t\t\t\tneed = 0;\n\t\t\t\t\tif (abs(dist) > 8) {\n\t\t\t\t\t\tneed = reach + abs(dist) / rate0 / vmax;\n\t\t\t\t\t}\n\t\t\t\t\tif (tr < need) {\n\t\t\t\t\t\ttr = min(need, tlen - 1);\n\t\t\t\t\t\tlate1 = late1 + 1;\n\t\t\t\t\t}\n\t\t\t\t\tdc = 0;\n\t\t\t\t\tif (need > 0) {\n\t\t\t\t\t\tdc = floor(min(need, tr));\n\t\t\t\t\t}\n\t\t\t\t\tif (dc < 4) {\n\t\t\t\t\t\tpos1 = wrap(pos1 + dist, 0, L);\n\t\t\t\t\t\tif (abs(dist) > 8) {\n\t\t\t\t\t\t\tv1 = 0;\n\t\t\t\t\t\t}\n\t\t\t\t\t\tmode1 = 2;\n\t\t\t\t\t} else {\n\t\t\t\t\t\tsd = v1 * dks * (1 - pow(dks, dc)) / (1 - dks);\n\t\t\t\t\t\tpa1 = (dist / rate0 - sd) / ((dc + 1) / 2);\n\t\t\t\t\t\tpdv1 = v1;\n\t\t\t\t\t\tpdc1 = dc;\n\t\t\t\t\t\tk1 = 0;\n\t\t\t\t\t\tmode1 = 1;\n\t\t\t\t\t}\n\t\t\t\t\ttrrem1 = tr;\n\t\t\t\t\tpush1 = ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * pushsd;\n\t\t\t\t\texp1 = wrap(p01 - rate1 * rate0 * gap, 0, L);\n\t\t\t\t\texpok1 = 1;\n\t\t\t\t} else {\n\t\t\t\t\tif (kind1 == 0) {\n\t\t\t\t\t\t// let go from wherever it is\n\t\t\t\t\t\ttrrem1 = gap;\n\t\t\t\t\t\tmode1 = 2;\n\t\t\t\t\t}\n\t\t\t\t\tif (kind1 == 1) {\n\t\t\t\t\t\ttrrem1 = gap;\n\t\t\t\t\t\tmode1 = 2;\n\t\t\t\t\t}\n\t\t\t\t\tif (kind1 == 2) {\n\t\t\t\t\t\ttrrem1 = gap;\n\t\t\t\t\t\tmode1 = 2;\n\t\t\t\t\t\tspm1 = max(floor((be1 - bs1) * spb * 0.85), 16);\n\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t} else {\n\t\tif (has1 > 0.5 && B >= be1) {\n\t\t\thas1 = 0;\n\t\t\tif (mode1 == 3) {\n\t\t\t\tmode1 = 0;\n\t\t\t}\n\t\t}\n\t}\n}\n\nif (1 == 0 && be1 >= NONE && has1 > 0.5) {\n\tnb = NONE;\n\tif (head1 < qt) {\n\t\tnb = min(nb, peek(q, (1 * 64 + wrap(head1, 0, 64)) * 16, 0));\n\t}\n\tif (head1 < qt1) {\n\t\tnb = min(nb, peek(q, (64 + wrap(head1, 0, 64)) * 16, 0));\n\t}\n\tif (nb < NONE) {\n\t\tbe1 = max(nb, B);\n\t\tpclose1 = be1 + fadlate / spb;\n\t}\n}\n\n// ---- platter ----\nif (mode1 == 1) {\n\tw = 0.5 - 0.5 * cos(twopi * (k1 + 1) / (pdc1 + 1));\n\tpdv1 = pdv1 * dks;\n\tv1 = pdv1 + pa1 * w;\n\tk1 = k1 + 1;\n\ttrrem1 = trrem1 - 1;\n\tif (k1 >= pdc1) {\n\t\tv1 = 0;\n\t\tmode1 = 2;\n\t}\n} else {\n\tif (mode1 == 2) {\n\t\tv1 = v1 * dks;\n\t\ttrrem1 = trrem1 - 1;\n\t\tif (trrem1 <= 0) {\n\t\t\tif (kind1 == 1) {\n\t\t\t\tmode1 = 4;\n\t\t\t} else {\n\t\t\t\tif (kind1 == 2) {\n\t\t\t\t\tspdv1 = v1;\n\t\t\t\t\tk1 = 0;\n\t\t\t\t\tmode1 = 5;\n\t\t\t\t} else {\n\t\t\t\t\tvb1 = v1;\n\t\t\t\t\trk1 = 0;\n\t\t\t\t\tmode1 = 3;\n\t\t\t\t\tcuchk1 = floor(0.35 * SR);\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\nif (mode1 == 5 && k1 == 0) {\n\t// spin distance lives in p0 for a spin entry\n\tsd = spdv1 * dks * (1 - pow(dks, spm1)) / (1 - dks);\n\tspa1 = (p01 / rate0 - sd) / (spm1 * 0.2475168);\n}\nif (mode1 == 5) {\n\tu = (k1 + 0.5) / spm1;\n\tspdv1 = spdv1 * dks;\n\tv1 = spdv1 + spa1 * exp(-3 * u) * (1 - exp(-60 * u)) * sqrt(max(1 - u, 0));\n\tk1 = k1 + 1;\n\tif (k1 >= spm1) {\n\t\tv1 = 0;\n\t\tmode1 = 0;\n\t}\n}\nif (mode1 == 4) {\n\ttrem1 = trema * trem1 + tremb * ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678);\n\tv1 = v1 * dks + trem1 * tremsd;\n\tif (B >= be1) {\n\t\tmode1 = 0;\n\t}\n}\nif (mode1 == 0 || mode1 == 6) {\n\tif (mode1 == 6) {\n\t\tv1 = v1 * exp(-1 / (0.04 * SR));\n\t} else {\n\t\tv1 = v1 * dks;\n\t}\n}\n// a deck left alone runs muted in time with the song, ready to be caught\nif (mode1 == 0 && has1 < 0.5 && g1 <= 0 && active > 0.5 && playing > 0.5) {\n\tidl1 = idl1 + 1;\n\tif (idl1 > 0.25 * SR) {\n\t\tpos1 = rbs * recspb;\n\t\tv1 = 1;\n\t}\n} else {\n\tidl1 = 0;\n}\nif (mode1 == 3) {\n\trate_e = rate1;\n\tif (1 == 1) {\n\t\trate_e = rate1 * (1 + 0.0012 * wear);\n\t}\n\tvb1 = vb1 + (rate_e - vb1) * (1 - dk);\n\tvv = vb1;\n\tif (cued1 > 0.5) {\n\t\tvv = vv + rate_e * push1 * (exp(-rk1 / (2.5 * tau)) - exp(-rk1 / (0.6 * tau)));\n\t}\n\trk1 = rk1 + 1;\n\t// catching up: the DJ hears the record is off and nudges it back\n\tif (expok1 > 0.5) {\n\t\tcuchk1 = cuchk1 - 1;\n\t\tif (cuchk1 <= 0) {\n\t\t\terr = wrap(exp1 - pos1, -L / 2, L / 2);\n\t\t\tif (abs(err) > 0.0003 * SR * rate0) {\n\t\t\t\tculen1 = floor(0.6 * SR);\n\t\t\t\tcuk1 = 0;\n\t\t\t\tcua1 = 0.8 * err / rate0 / ((culen1 + 1) / 2);\n\t\t\t\tcuchk1 = culen1 + floor(0.25 * SR);\n\t\t\t} else {\n\t\t\t\tcuchk1 = floor(2 * SR);\n\t\t\t}\n\t\t}\n\t}\n\tif (culen1 > 0) {\n\t\tvv = vv + cua1 * (0.5 - 0.5 * cos(twopi * (cuk1 + 1) / (culen1 + 1)));\n\t\tcuk1 = cuk1 + 1;\n\t\tif (cuk1 >= culen1) {\n\t\t\tculen1 = 0;\n\t\t}\n\t}\n\tv1 = vv;\n}\nif (expok1 > 0.5) {\n\texp1 = wrap(exp1 + rate1 * rate0, 0, L);\n}\n\n// a finger on the platter\nif (dragnew > 0.5 && live == 1) {\n\tdrlen1 = floor(0.30 * SR);\n\tdrk1 = 0;\n\tdrd1 = dragdepth;\n}\nvplay = v1;\nif (drlen1 > 0) {\n\tvplay = v1 * (1 - drd1 * (0.5 - 0.5 * cos(twopi * drk1 / drlen1)));\n\tdrk1 = drk1 + 1;\n\tif (drk1 >= drlen1) {\n\t\tdrlen1 = 0;\n\t}\n}\npos1 = wrap(pos1 + vplay * rate0, 0, L);\n\n// ---- fader ----\n// a late hand: the fader waits until the record has been let go\nwaitrel = 0;\nif (has1 > 0.5 && cued1 > 0.5 && kind1 == 0 && (mode1 == 1 || mode1 == 2)) {\n\twaitrel = 1;\n}\nif (B >= popen1 && waitrel < 0.5) {\n\tpopen1 = NONE;\n\tif (has1 > 0.5) {\n\t\tgt1 = 1;\n\t\tlive = 1;\n\t\ta = amp1;\n\t\tif (str1 > 0.5) {\n\t\t\ta = a * strlev;\n\t\t}\n\t\tgamp1 = a * min(1 + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * lvl, 1.12);\n\t\tgend = be1;\n\t\tif (gm1 > 0.5) {\n\t\t\tgend = min(be1, g01 + (slot1 + 1) * gd1);\n\t\t}\n\t\tif (kind1 == 2) {\n\t\t\tgend = min(be1, bs1 + (be1 - bs1) * 0.85);\n\t\t}\n\t\tpclose1 = gend + (fadlate + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\tpcs1 = 1;\n\t}\n}\nif (B >= pclose1) {\n\tpclose1 = NONE;\n\tgt1 = 0;\n\tif (gm1 > 0.5 && has1 > 0.5 && pcs1 > 0.5) {\n\t\tslot1 = slot1 + 2;\n\t\ts0 = g01 + slot1 * gd1;\n\t\tif (s0 < be1) {\n\t\t\tpopen1 = s0 - (fadearly + ((noise() + noise() + noise() + noise() + noise() + noise()) * 0.70710678) * fadjit) / spb;\n\t\t}\n\t}\n}\nif (gt1 > 0.5) {\n\tg1 = min(g1 + 1 / fade, 1);\n} else {\n\tg1 = max(g1 - 1 / fade, 0);\n}\ngain1 = gamp1 * (0.5 - 0.5 * cos(pi * g1));\n\n// ---- read the record (4-point Hermite) ----\nyl1 = 0;\nyr1 = 0;\nif (g1 > 0 && active > 0.5) {\n\tip = floor(pos1);\n\tfr = pos1 - ip;\n\ti0 = wrap(ip - 1, 0, L); i1 = wrap(ip, 0, L); i2 = wrap(ip + 1, 0, L); i3 = wrap(ip + 2, 0, L);\n\txm = peek(rec, i0, 0); x0 = peek(rec, i1, 0); x1 = peek(rec, i2, 0); x2 = peek(rec, i3, 0);\n\tc1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);\n\tyl1 = ((c3 * fr + c2) * fr + c1) * fr + x0;\n\txm = peek(rec, i0, 1); x0 = peek(rec, i1, 1); x1 = peek(rec, i2, 1); x2 = peek(rec, i3, 1);\n\tc1 = 0.5 * (x1 - xm); c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2; c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);\n\tyr1 = ((c3 * fr + c2) * fr + c1) * fr + x0;\n\tif (abs(vplay * rate0) > 2) {\n\t\t// fast spin: average a little either side to tame aliasing\n\t\thw = abs(vplay * rate0) * 0.5;\n\t\tja = wrap(floor(pos1 - hw), 0, L); jb = wrap(floor(pos1 + hw), 0, L);\n\t\tyl1 = 0.5 * yl1 + 0.25 * (peek(rec, ja, 0) + peek(rec, jb, 0));\n\t\tyr1 = 0.5 * yr1 + 0.25 * (peek(rec, ja, 1) + peek(rec, jb, 1));\n\t}\n\tif (1 == 1) {\n\t\twa = exp(-twopi * (20000 - 9000 * wear) / SR);\n\t\twl1 = (1 - wa) * yl1 + wa * wl1;\n\t\twr1 = (1 - wa) * yr1 + wa * wr1;\n\t\tyl1 = wl1 * (1 - 0.06 * wear);\n\t\tyr1 = wr1 * (1 - 0.06 * wear);\n\t}\n}\n\n// ---- mix ----\nmixl = yl0 * gain0 + yl1 * gain1;\nmixr = yr0 * gain0 + yr1 * gain1;\nkn = 0.85;\nif (abs(mixl) > kn) {\n\tmixl = sign(mixl) * (kn + (1 - kn) * tanh((abs(mixl) - kn) / (1 - kn)));\n}\nif (abs(mixr) > kn) {\n\tmixr = sign(mixr) * (kn + (1 - kn) * tanh((abs(mixr) - kn) / (1 - kn)));\n}\nif (active > 0.5) {\n\tout1 = mixl * level;\n\tout2 = mixr * level;\n} else {\n\tout1 = in1;\n\tout2 = in2;\n}\n\n// ---- status for JS, every 32 samples ----\nstatc = h_statc + 1;\nif (statc >= 32) {\n\tstatc = 0;\n\tpoke(st, B, 0, 0);\n\tpoke(st, playing, 1, 0);\n\tpoke(st, recstate, 2, 0);\n\tpoke(st, wi, 3, 0);\n\tpoke(st, recstart, 4, 0);\n\tpoke(st, head0, 5, 0);\n\tpoke(st, head1, 6, 0);\n\tpoke(st, pos0 / L, 7, 0);\n\tpoke(st, pos1 / L, 8, 0);\n\tpoke(st, gain0, 9, 0);\n\tpoke(st, gain1, 10, 0);\n\tpoke(st, late0 + late1, 11, 0);\n\tpoke(st, flub0 + flub1, 12, 0);\n\tpoke(st, h_alive + 1, 13, 0);\n\tpoke(st, recb, 14, 0);\n\tpoke(st, live, 15, 0);\n}\n\n// ---- write back ----\nh_beatint = beatint; h_prevph = prevph; h_sbseq = sbseq; h_wasplay = playing; h_curgen = curgen;\nh_flushb = flushb; h_reccmd = reccmd; h_recstate = recstate; h_wi = wi; h_recstart = recstart;\nh_recb = recb; h_prevb = prevb; h_statc = statc; h_live = live; h_dragseq = dragseq; h_alive = h_alive + 1;\nh_pos0 = pos0; h_v0 = v0; h_vb0 = vb0; h_mode0 = mode0; h_k0 = k0; h_has0 = has0;\nh_bs0 = bs0; h_be0 = be0; h_p00 = p00; h_rate0 = rate0; h_kind0 = kind0; h_gm0 = gm0;\nh_gd0 = gd0; h_g00 = g00; h_amp0 = amp0; h_str0 = str0; h_gen0 = gen0; h_cued0 = cued0;\nh_pdc0 = pdc0; h_pa0 = pa0; h_pdv0 = pdv0; h_trrem0 = trrem0; h_push0 = push0; h_rk0 = rk0;\nh_exp0 = exp0; h_expok0 = expok0; h_cuchk0 = cuchk0; h_culen0 = culen0; h_cuk0 = cuk0; h_cua0 = cua0;\nh_spm0 = spm0; h_spa0 = spa0; h_spdv0 = spdv0; h_trem0 = trem0;\nh_gt0 = gt0; h_g0 = g0; h_gamp0 = gamp0; h_popen0 = popen0; h_pclose0 = pclose0; h_slot0 = slot0;\nh_drk0 = drk0; h_drlen0 = drlen0; h_drd0 = drd0; h_late0 = late0; h_flub0 = flub0; h_e0 = e0;\nh_wl0 = wl0; h_wr0 = wr0; h_head0 = head0; h_brk0 = brk0; h_idl0 = idl0; h_pcs0 = pcs0;\nh_pos1 = pos1; h_v1 = v1; h_vb1 = vb1; h_mode1 = mode1; h_k1 = k1; h_has1 = has1;\nh_bs1 = bs1; h_be1 = be1; h_p01 = p01; h_rate1 = rate1; h_kind1 = kind1; h_gm1 = gm1;\nh_gd1 = gd1; h_g01 = g01; h_amp1 = amp1; h_str1 = str1; h_gen1 = gen1; h_cued1 = cued1;\nh_pdc1 = pdc1; h_pa1 = pa1; h_pdv1 = pdv1; h_trrem1 = trrem1; h_push1 = push1; h_rk1 = rk1;\nh_exp1 = exp1; h_expok1 = expok1; h_cuchk1 = cuchk1; h_culen1 = culen1; h_cuk1 = cuk1; h_cua1 = cua1;\nh_spm1 = spm1; h_spa1 = spa1; h_spdv1 = spdv1; h_trem1 = trem1;\nh_gt1 = gt1; h_g1 = g1; h_gamp1 = gamp1; h_popen1 = popen1; h_pclose1 = pclose1; h_slot1 = slot1;\nh_drk1 = drk1; h_drlen1 = drlen1; h_drd1 = drd1; h_late1 = late1; h_flub1 = flub1; h_e1 = e1;\nh_wl1 = wl1; h_wr1 = wr1; h_head1 = head1; h_brk1 = brk1; h_idl1 = idl1; h_pcs1 = pcs1;\n"
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
										"obj-2",
										0
									],
									"destination": [
										"obj-4",
										1
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
										"obj-4",
										2
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
					"id": "obj-4",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 2,
					"patching_rect": [
						20.0,
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
					"id": "obj-5",
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
					"text": "buffer~ ---dbl_rec 100000 2"
				}
			},
			{
				"box": {
					"id": "obj-6",
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
					"text": "buffer~ ---dbl_q 100"
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
						316.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---dbl_stat 10"
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
						344.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---dbl_save 10 2"
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
						372.0,
						180.0,
						22.0
					],
					"outlettype": [
						"float",
						"bang"
					],
					"text": "buffer~ ---dbl_load 10 2"
				}
			},
			{
				"box": {
					"id": "obj-10",
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
					"id": "obj-11",
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
					"text": "rec ---dbl_rec, q ---dbl_q, st ---dbl_stat"
				}
			},
			{
				"box": {
					"id": "obj-12",
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
						"text": "// Doubles: logic for the Max for Live device.\n// Plans the juggle (which deck plays what, when), handles the record, finds the hits\n// (\"stickers\"), runs the Routine and Instinct modes, and keeps the record with the set.\n// Sample-accurate timing lives in gen~; this file only decides things ahead of time.\n//\n// Arguments: rec-buffer queue-buffer status-buffer save-buffer load-buffer\n// Inlet: messages \"name value\" from the panel, \"init\", \"strip ...\" from the strip.\n// Outlet 0: parameter messages for gen~.  Outlet 1: messages for the panel.\n\nautowatch = 1;\ninlets = 1;\noutlets = 2;\n\n// ------------------------------------------------------------------ settings\nvar LOOKAHEAD = 1.5;   // beats of plays sent to gen~ ahead of now\nvar GENAHEAD = 2.5;    // beats of plan generated ahead of now\nvar QN = 64;           // queue slots per deck\nvar QF = 16;           // fields per queue entry\n\nvar FILLS = [\n\t'K.KS.K.SK.KS.SSS', 'KK.S.KKS.K.SKSKS', 'K..SK.S.KKSS.KSS', 'KSKKS.KSKSS.SSSS',\n\t'K.K.S..KK.S.KSSS', 'K...S.K.K.S.SKSK', 'KKSSKKSSK.S.KSSS', 'K.S.K.S.KSKSKSSS'\n];\nvar MOVES = ['straight', 'loop', 'double', 'triple', 'chase', 'shuffle', 'pause', 'fill', 'spin', 'phase'];\n\n// ------------------------------------------------------------------ state\nvar S = {\n\t// record\n\tL: 0, Lb: 8, anchor: 0, sr: 44100, have: 0, recSR: 44100,\n\tstk: [], stkOff: {}, stkAdd: [], K: -1, S: -1, padStk: [], barK: [], barS: [],\n\tfile: '',\n\t// transport\n\ttempo: 120, playing: 0, B: 0,\n\t// controls\n\tctl: {\n\t\tlooplen: 1, doubledist: 0.5, chaseunit: 1, shuffle: 0.5, pauselen: 0.5, fillpat: 1, spinback: 4,\n\t\tquant: 1, latch: 0, mode: 0, fillk: 0, fills: 0, density: 0.6, funk: 0.5, arc: 16, slotlen: 4,\n\t\ttiming: 0.5, weight: 0.5, slop: 0.5, risk: 0.5, copy: 0.5, length: 8\n\t},\n\theld: null,          // {move, b} the move button in force\n\tpad: null,\n\troutine: [],\n\troutineAnchor: 0,\n\tinstinctBar0: 0,\n\tlastFill: -1,\n\tprevBarMove: null\n};\nfor (var i = 0; i < 16; i++) S.routine.push('straight');\n\n// planner\nvar TL = [[], []];       // timeline of plays per deck\nvar sentIdx = [0, 0];    // plays already written to the queue\nvar tail = [0, 0];       // queue write counters\nvar gen = 0;             // flush generation\nvar planEnd = null;      // beat up to which the plan has been generated\nvar seg = null;          // the segment being generated\nvar rng = Math.random;\n\n// host hooks, replaced in Max (bottom of file) or by the test harness\nvar HOST = {\n\tgen: function (name, v) {},\n\tui: function () {},\n\tqueueWrite: function (d, slot, vals) {},\n\theads: function () { return [0, 0]; },\n\tlog: function (s) {}\n};\n\n// ------------------------------------------------------------------ helpers\nfunction mod(x, m) { var y = x % m; return y < 0 ? y + m : y; }\nfunction wrapc(x, m) { return mod(x + m / 2, m) - m / 2; }\nfunction spbRec() { return S.L / S.Lb; }                 // record samples per record beat\nfunction rbS(b) { return mod(b - S.anchor, S.Lb); }       // straight position at song beat b\nfunction gauss() { return (rng() + rng() + rng() + rng() + rng() + rng() - 3) * 1.41421356; }\n\nfunction snapSamples(rb) {\n\t// record beats -> record samples, snapped to a sticker within 40 ms, cued 2 ms early\n\tvar p = mod(rb, S.Lb) * spbRec();\n\tvar lim = 0.040 * S.recSR, best = null, bd = 1e30;\n\tfor (var i = 0; i < S.stk.length; i++) {\n\t\tvar d = wrapc(S.stk[i] - p, S.L);\n\t\tif (Math.abs(d) < bd) { bd = Math.abs(d); best = d; }\n\t}\n\tif (best !== null && bd < lim) p = p + best;\n\treturn mod(p - 0.002 * S.recSR, S.L);\n}\n\n// ------------------------------------------------------------------ plays\n// play: {bs, be, rb (record beats or null), p0 (samples), kind 0 play 1 hold 2 spin,\n//        cm 0 cue 1 let-go 2 continuous, rate, gm, gd, g0, amp, dist, str}\nfunction lastPlay(d, b) {\n\tvar t = TL[d];\n\tfor (var i = t.length - 1; i >= 0; i--) if (t[i].bs < b - 1e-9) return t[i];\n\treturn null;\n}\nfunction deckEnd(d, b) { var p = lastPlay(d, b); return p ? Math.min(p.be, b) : -1e9; }\nfunction deckRbEnd(d, b) {\n\tvar p = lastPlay(d, b);\n\tif (!p || p.rb === null || p.kind !== 0) return null;\n\treturn mod(p.rb + (Math.min(p.be, b) - p.bs) * p.rate, S.Lb);\n}\nvar liveDeck = 0;\nfunction freeDeck(b, rb) {\n\tvar l = liveDeck, o = 1 - l, ds = [l, o];\n\tif (rb !== null && rb !== undefined) {\n\t\tfor (var i = 0; i < 2; i++) {\n\t\t\tvar d = ds[i];\n\t\t\tif (Math.abs(deckEnd(d, b) - b) < 1e-9) {\n\t\t\t\tvar re = deckRbEnd(d, b);\n\t\t\t\tif (re !== null && Math.abs(wrapc(re - rb, S.Lb)) < 1e-6) return d;\n\t\t\t}\n\t\t}\n\t}\n\treturn deckEnd(o, b) < deckEnd(l, b) ? o : l;\n}\n// The deck that can be at record beat rb by song beat b with the most time to spare.\n// A deck that can simply carry on always wins.\nfunction readyDeck(b, rb) {\n\tvar c = freeDeck(b, rb);\n\tif (Math.abs(deckEnd(c, b) - b) < 1e-9) {\n\t\tvar re = deckRbEnd(c, b);\n\t\tif (re !== null && Math.abs(wrapc(re - rb, S.Lb)) < 1e-6) return c;\n\t}\n\tvar w = S.ctl.weight, vmax = 6 / Math.max(w, 0.05), best = c, bslack = -1e9;\n\tfor (var d = 0; d < 2; d++) {\n\t\tvar e = deckEnd(d, b), lp = lastPlay(d, b), at;\n\t\tif (!lp) return d;                                  // never used: it is running in time\n\t\tif (b - e > 0.5) return d;                          // idle a while: running in time, muted\n\t\tat = lp.rb === null ? rbS(e) : mod(lp.rb + (Math.min(lp.be, b) - lp.bs) * lp.rate, S.Lb);\n\t\tvar dist = Math.abs(wrapc(rb - at, S.Lb)) * 60 / S.tempo;     // seconds of record\n\t\tvar need = 0.045 * w + dist / vmax;\n\t\tvar slack = (b - e) * 60 / S.tempo - need;\n\t\tif (slack > bslack) { bslack = slack; best = d; }\n\t}\n\treturn best;\n}\n\nfunction addPlay(d, p) {\n\tvar prev = lastPlay(d, p.bs + 1e-9);\n\tp.d = d;\n\tif (p.kind === undefined) p.kind = 0;\n\tif (p.rate === undefined) p.rate = 1;\n\tif (p.gm === undefined) p.gm = 0;\n\tif (p.gd === undefined) p.gd = 1;\n\tif (p.g0 === undefined) p.g0 = 0;\n\tif (p.amp === undefined) p.amp = 1;\n\tif (p.dist === undefined) p.dist = 0;\n\tif (p.str === undefined) p.str = 0;\n\tif (p.cm === undefined) {\n\t\tp.cm = 0;\n\t\tif (p.kind === 0 && p.rb !== null && prev && prev.kind === 0 && prev.rb !== null &&\n\t\t\tMath.abs(prev.be - p.bs) < 1e-9 && prev.rate === p.rate &&\n\t\t\tMath.abs(wrapc(mod(prev.rb + (prev.be - prev.bs) * prev.rate, S.Lb) - p.rb, S.Lb)) < 1e-6) p.cm = 2;\n\t}\n\tif (p.p0 === undefined) {\n\t\tif (p.rb === null) p.p0 = 0;\n\t\telse if (p.cm === 2 || p.nosnap) p.p0 = mod(p.rb, S.Lb) * spbRec();\n\t\telse p.p0 = snapSamples(p.rb);\n\t}\n\tTL[d].push(p);\n\treturn p;\n}\n\n// ------------------------------------------------------------------ moves\n// Each move gets a segment {move, b0, b1, ...} and emits its plays for [from, to).\nfunction segInit(sg) {\n\tvar b0 = sg.b0;\n\tsg.rb0 = rbS(b0);\n\tif (sg.move === 'straight' || sg.move === 'double' || sg.move === 'phase') sg.first = readyDeck(b0, sg.rb0);\n\telse if (sg.move === 'fill') sg.first = freeDeck(b0, null);\n\telse if (sg.move === 'pause' || sg.move === 'spin') sg.first = liveDeck;\n\telse sg.first = freeDeck(b0, sg.rb0);\n\tsg.done = b0;\n}\n\nfunction emit(sg, to) {\n\tvar from = sg.done, b0 = sg.b0, b1 = sg.b1, m = sg.move, k, b, d, u;\n\tto = Math.min(to, b1);\n\tif (to <= from + 1e-9) return;\n\tif (m === 'straight') {\n\t\tif (from <= b0 + 1e-9) { addPlay(sg.first, {bs: b0, be: b1, rb: sg.rb0, str: 1, nosnap: 1}); liveDeck = sg.first; }\n\t} else if (m === 'loop' || m === 'triple') {\n\t\tu = m === 'triple' ? 1 / 3 : sg.unit;\n\t\tk = Math.max(0, Math.ceil((from - b0) / u - 1e-9));\n\t\tfor (; b0 + k * u < to - 1e-9; k++) {\n\t\t\tb = b0 + k * u; d = (sg.first + k) % 2;\n\t\t\taddPlay(d, {bs: b, be: Math.min(b + u, b1), rb: sg.rb0}); liveDeck = d;\n\t\t}\n\t} else if (m === 'double') {\n\t\tif (from <= b0 + 1e-9) {\n\t\t\tvar X = sg.first, Y = 1 - X, dd = sg.dist;\n\t\t\taddPlay(X, {bs: b0, be: b1, rb: sg.rb0, gm: 1, gd: dd, g0: b0, nosnap: 1});\n\t\t\taddPlay(Y, {bs: b0 + dd, be: b1, rb: sg.rb0, gm: 2, gd: dd, g0: b0, nosnap: 1});\n\t\t\tliveDeck = X;\n\t\t}\n\t} else if (m === 'chase' || m === 'shuffle') {\n\t\tu = sg.unit; var sh = m === 'shuffle' ? sg.shuffle : 0;\n\t\tif (sg.ck === undefined) { sg.ck = 0; sg.cb = b0; }\n\t\twhile (sg.cb < to - 1e-9) {\n\t\t\tvar dur = (sg.ck % 2 === 0) ? u * (1 + sh) : u * (1 - sh);\n\t\t\td = (sg.first + sg.ck) % 2;\n\t\t\taddPlay(d, {bs: sg.cb, be: Math.min(sg.cb + dur, b1), rb: mod(sg.rb0 + Math.floor(sg.ck / 2) * u, S.Lb)});\n\t\t\tliveDeck = d; sg.cb += dur; sg.ck++;\n\t\t}\n\t} else if (m === 'pause') {\n\t\tif (from <= b0 + 1e-9) {\n\t\t\td = sg.first;\n\t\t\taddPlay(d, {bs: b0, be: b0 + sg.hold, rb: null, kind: 1, cm: 1});\n\t\t\taddPlay(d, {bs: b0 + sg.hold, be: b1, rb: null, kind: 0, cm: 1});\n\t\t\tliveDeck = d;\n\t\t}\n\t} else if (m === 'fill') {\n\t\tvar pat = sg.pattern, step = sg.step, X2 = sg.first;\n\t\tif (!sg.ks) sg.ks = fillStickers(b0);\n\t\tvar kdeck = X2, sdeck = 1 - X2;\n\t\tif (pat.charAt(0) === 'S') { sdeck = X2; kdeck = 1 - X2; }\n\t\tvar hits = [];\n\t\tfor (var i = 0; i < pat.length; i++) if (pat.charAt(i) !== '.') hits.push(i);\n\t\tfor (var j = 0; j < hits.length; j++) {\n\t\t\tvar i2 = hits[j], c = pat.charAt(i2), isK = c === 'K';\n\t\t\tvar bs = b0 + i2 * step;\n\t\t\tif (bs < from - 1e-9 || bs >= to - 1e-9) continue;\n\t\t\tvar nxt = j + 1 < hits.length ? hits[j + 1] : pat.length;\n\t\t\tvar durh = Math.min(nxt - i2, 2) * step;\n\t\t\tfor (var q = j + 1; q < hits.length; q++) {\n\t\t\t\tif ((pat.charAt(hits[q]) === 'K') === isK) {\n\t\t\t\t\tif (hits[q] - i2 <= 2) durh = Math.min(durh, (hits[q] - i2) * step * 0.45);\n\t\t\t\t\tbreak;\n\t\t\t\t}\n\t\t\t}\n\t\t\td = isK ? kdeck : sdeck;\n\t\t\tvar p0 = mod((isK ? sg.ks[0] : sg.ks[1]) - 0.002 * S.recSR, S.L);\n\t\t\taddPlay(d, {bs: bs, be: Math.min(bs + durh, b1), rb: (p0 + 0.002 * S.recSR) / spbRec(), p0: p0, cm: 0});\n\t\t\tliveDeck = d;\n\t\t}\n\t} else if (m === 'spin') {\n\t\tif (from <= b0 + 1e-9) {\n\t\t\td = sg.first;\n\t\t\taddPlay(d, {bs: b0, be: b1, rb: null, kind: 2, cm: 1, dist: -sg.back * spbRec()});\n\t\t\tliveDeck = d;\n\t\t}\n\t} else if (m === 'phase') {\n\t\tif (from <= b0 + 1e-9) {\n\t\t\tvar X3 = sg.first, Y3 = 1 - X3;\n\t\t\taddPlay(X3, {bs: b0, be: b1, rb: sg.rb0, amp: 0.72, nosnap: 1});\n\t\t\taddPlay(Y3, {bs: b0, be: b1, rb: mod(sg.rb0 - 0.009 * S.tempo / 60, S.Lb), amp: 0.72, rate: 1.0018, nosnap: 1});\n\t\t\tliveDeck = X3;\n\t\t}\n\t}\n\tsg.done = to;\n}\n\nfunction makeSeg(move, b0, b1, opts) {\n\tvar c = S.ctl, sg = {move: move, b0: b0, b1: b1};\n\tsg.unit = c.looplen; sg.dist = c.doubledist; sg.shuffle = c.shuffle; sg.hold = c.pauselen;\n\tsg.back = c.spinback; sg.step = 0.25; sg.pattern = FILLS[(c.fillpat - 1) % 8];\n\tif (move === 'chase' || move === 'shuffle') sg.unit = c.chaseunit;\n\tif (opts) for (var k in opts) sg[k] = opts[k];\n\treturn sg;\n}\n\n// ------------------------------------------------------------------ where plans come from\nfunction barOf(b) { return Math.floor(b / 4 + 1e-9); }\n\nfunction nextSegments(b) {\n\t// returns a list of segments starting at b\n\tvar c = S.ctl, nb = (barOf(b) + 1) * 4;\n\tif (S.held) return heldSegments(b);\n\tif (c.mode === 1) return routineSegments(b);\n\tif (c.mode === 2) return instinctSegments(b);\n\treturn [makeSeg('straight', b, nb)];\n}\n\nfunction heldSegments(b) {\n\tvar h = S.held, m = h.move, nb = (barOf(b) + 1) * 4;\n\tif (m === 'triple') { S.held = null; return [makeSeg('triple', b, b + 1)]; }\n\tif (m === 'spin') {\n\t\tvar sb = nb - 1;\n\t\tif (sb < b - 1e-9) { nb += 4; sb = nb - 1; }\n\t\tS.held = null;\n\t\tvar out = [];\n\t\tif (sb > b + 1e-9) out.push(makeSeg('straight', b, sb));\n\t\tout.push(makeSeg('spin', sb, nb));\n\t\treturn out;\n\t}\n\tif (m === 'pause') {\n\t\tvar end = nb;\n\t\tif (end - b < S.ctl.pauselen + 0.25) end += 4;\n\t\tS.held = null;\n\t\treturn [makeSeg('pause', b, end)];\n\t}\n\tif (m === 'fill') return [makeSeg('fill', b, b + 4)];\n\treturn [makeSeg(m, b, b + 256)];\n}\n\nfunction slotRender(name, b0, b1) {\n\tvar out = [], c = S.ctl, i;\n\tif (name.indexOf('fill') === 0) {\n\t\tvar n = parseInt(name.substr(4), 10) || 1, pat = FILLS[(n - 1) % 8];\n\t\tif (b1 - b0 < 4 - 1e-9) pat = pat.substr(16 - Math.round((b1 - b0) * 4));\n\t\treturn [makeSeg('fill', b0, b1, {pattern: pat})];\n\t}\n\tif (name === 'triple') {\n\t\tfor (i = b0; i < b1 - 1e-9; i += 1) {\n\t\t\tvar beatInBar = mod(Math.round(i), 4);\n\t\t\tout.push(makeSeg(beatInBar === 1 || beatInBar === 3 ? 'triple' : 'straight', i, i + 1));\n\t\t}\n\t\treturn out;\n\t}\n\tif (name === 'pause') {\n\t\tvar pre = b1 - b0 >= 4 ? 2 : 1;\n\t\treturn [makeSeg('straight', b0, b0 + pre), makeSeg('pause', b0 + pre, b1)];\n\t}\n\tif (name === 'spin') {\n\t\tif (b1 - b0 > 1) out.push(makeSeg('straight', b0, b1 - 1));\n\t\tout.push(makeSeg('spin', b1 - 1, b1));\n\t\treturn out;\n\t}\n\treturn [makeSeg(name, b0, b1)];\n}\n\nfunction routineSegments(b) {\n\tvar sl = S.ctl.slotlen, k = Math.floor((b - S.routineAnchor) / sl + 1e-9);\n\tvar s0 = S.routineAnchor + k * sl, s1 = s0 + sl;\n\tvar name = S.routine[mod(k, 16)];\n\tif (b > s0 + 1e-9) return [makeSeg('straight', b, s1)];   // joined mid-slot\n\treturn slotRender(name, s0, s1);\n}\n\nfunction wpick(w, dens) {\n\tvar keys = [], ps = [], tot = 0;\n\tfor (var k in w) {\n\t\tvar p = w[k] * (k === 'straight' ? 1 : 2 * dens);\n\t\tkeys.push(k); ps.push(p); tot += p;\n\t}\n\tvar r = rng() * tot;\n\tfor (var i = 0; i < keys.length; i++) { r -= ps[i]; if (r <= 0) return keys[i]; }\n\treturn keys[keys.length - 1];\n}\nfunction choose(a) { return a[Math.floor(rng() * a.length)]; }\n\nfunction instinctMove(m, b0, b1) {\n\tif (m === 'loop') return makeSeg('loop', b0, b1, {unit: b1 - b0 >= 2 ? choose([0.5, 1, 1, 2]) : 0.5});\n\tif (m === 'double') return makeSeg('double', b0, b1, {dist: choose([0.25, 0.5, 0.5, 0.5])});\n\tif (m === 'chase') return makeSeg('chase', b0, b1, {unit: choose([0.5, 1])});\n\tif (m === 'shuffle') return makeSeg('shuffle', b0, b1, {unit: 0.5, shuffle: choose([1 / 3, 0.5])});\n\treturn makeSeg('straight', b0, b1);\n}\n\nfunction instinctSegments(b) {\n\tvar c = S.ctl, bar = barOf(b), b0 = bar * 4;\n\tif (b > b0 + 1e-9) return [makeSeg('straight', b, b0 + 4)];\n\tvar n = bar - S.instinctBar0, ph = mod(n, 4);\n\tvar prog = mod(n, c.arc) / Math.max(c.arc - 1, 1);\n\tvar dens = c.density * (0.6 + 0.8 * prog), f = c.funk;\n\tvar W = [\n\t\t{straight: 6, loop: 2.5, double: 1.5},\n\t\t{straight: 4, double: 3, loop: 1.5, chase: 1.5},\n\t\t{straight: 2.5, double: 2.5, chase: 2.5, loop: 1.5, pause: 1, shuffle: 3 * f},\n\t\t{straight: 1, double: 2, chase: 2, shuffle: 4 * f, loop: 1}\n\t];\n\tvar E = {fill: 4, triple: 2.5, spin: 2, chase8: 1.5, straight: 1};\n\tvar out = [], m;\n\tif (ph < 3) {\n\t\tvar prev = S.prevBarMove;\n\t\tif ((ph === 1 || ph === 2) && prev && prev !== 'straight' && prev !== 'pause' && rng() < 0.45) m = prev;\n\t\telse m = wpick(W[ph], dens);\n\t\tif (m === 'pause') {\n\t\t\tout.push(makeSeg('straight', b0, b0 + 2));\n\t\t\tout.push(makeSeg('pause', b0 + 2, b0 + 4, {hold: choose([0.25, 0.5])}));\n\t\t} else out.push(instinctMove(m, b0, b0 + 4));\n\t\tS.prevBarMove = m;\n\t} else {\n\t\tm = wpick(W[3], dens);\n\t\tout.push(instinctMove(m === 'pause' ? 'straight' : m, b0, b0 + 2));\n\t\tvar e = wpick(E, dens);\n\t\tif (e === 'fill') {\n\t\t\tvar fi;\n\t\t\tdo { fi = Math.floor(rng() * 5); } while (fi === S.lastFill);\n\t\t\tS.lastFill = fi;\n\t\t\tout.push(makeSeg('fill', b0 + 2, b0 + 4, {pattern: FILLS[fi].substr(8)}));\n\t\t} else if (e === 'triple') {\n\t\t\tout.push(makeSeg('triple', b0 + 2, b0 + 3)); out.push(makeSeg('triple', b0 + 3, b0 + 4));\n\t\t} else if (e === 'spin') {\n\t\t\tout.push(makeSeg('straight', b0 + 2, b0 + 3)); out.push(makeSeg('spin', b0 + 3, b0 + 4));\n\t\t} else if (e === 'chase8') {\n\t\t\tout.push(makeSeg('chase', b0 + 2, b0 + 4, {unit: 0.25}));\n\t\t} else out.push(makeSeg('straight', b0 + 2, b0 + 4));\n\t\tS.prevBarMove = null;\n\t}\n\treturn out;\n}\n\n// ------------------------------------------------------------------ the planner\nvar segQ = [];\nfunction plan(now) {\n\tif (!S.have || !S.playing) return;\n\tif (planEnd === null) {\n\t\tplanEnd = Math.ceil(now + 0.5);\n\t\tresetTimeline(planEnd);\n\t\tHOST.gen('bootb', planEnd);\n\t}\n\tvar guard = 0;\n\twhile (guard++ < 64) {\n\t\tif (!seg) {\n\t\t\tif (!segQ.length) {\n\t\t\t\tif (planEnd > now + GENAHEAD) break;\n\t\t\t\tsegQ = nextSegments(planEnd);\n\t\t\t}\n\t\t\tseg = segQ.shift();\n\t\t\tsegInit(seg);\n\t\t}\n\t\temit(seg, Math.min(seg.b1, now + GENAHEAD + 1));\n\t\tif (seg.done >= seg.b1 - 1e-9) { planEnd = seg.b1; seg = null; }\n\t\telse break;\n\t}\n\tsend(now);\n}\n\nfunction send(now) {\n\tvar heads = HOST.heads();\n\tfor (var d = 0; d < 2; d++) {\n\t\tvar t = TL[d];\n\t\twhile (sentIdx[d] < t.length && t[sentIdx[d]].bs < now + LOOKAHEAD) {\n\t\t\tif (tail[d] - heads[d] >= QN - 1) break;\n\t\t\tvar p = t[sentIdx[d]];\n\t\t\tHOST.queueWrite(d, mod(tail[d], QN), [p.bs, p.be, p.p0, p.kind, p.cm, p.rate, p.gm, p.gd, p.g0,\n\t\t\t\tp.amp, p.dist, gen, p.str, 0, 0, tail[d]]);\n\t\t\ttail[d]++;\n\t\t\tsentIdx[d]++;\n\t\t}\n\t\tHOST.gen('qt' + d, tail[d]);\n\t}\n}\n\n// Cut everything planned from beat b on, in the timeline and in gen~.\nfunction cutAt(b) {\n\tfor (var d = 0; d < 2; d++) {\n\t\tvar t = TL[d], keep = [];\n\t\tfor (var i = 0; i < t.length; i++) {\n\t\t\tif (t[i].bs >= b - 1e-9) continue;\n\t\t\tif (t[i].be > b) t[i].be = b;\n\t\t\tkeep.push(t[i]);\n\t\t}\n\t\tTL[d] = keep;\n\t\tsentIdx[d] = Math.min(sentIdx[d], keep.length);\n\t}\n\tgen++;\n\tHOST.gen('fbeat', b);\n\tHOST.gen('fgen', gen);\n\tseg = null; segQ = [];\n\tplanEnd = b;\n\t// who was live at b\n\tvar best = -1e9;\n\tfor (var d2 = 0; d2 < 2; d2++) {\n\t\tvar p = lastPlay(d2, b);\n\t\tif (p && p.bs > best) { best = p.bs; liveDeck = d2; }\n\t}\n}\n\nfunction resetTimeline(b) {\n\tTL = [[], []]; sentIdx = [0, 0]; seg = null; segQ = [];\n\tgen++;\n\tHOST.gen('fbeat', b === undefined ? -1e9 : b);\n\tHOST.gen('fgen', gen);\n}\n\nfunction quantNext(now, q) {\n\treturn Math.ceil((now + 0.02) / q - 1e-9) * q;\n}\n\n// ------------------------------------------------------------------ panel events\nvar QUANTS = [0.25, 0.5, 1, 4];\nfunction movePress(m, on) {\n\tvar now = S.B, c = S.ctl;\n\tif (!S.have) return;\n\tif (m === 'drag') { if (on) dragPulse(); return; }\n\tvar b = S.playing ? quantNext(now, c.quant) : now;\n\tif (on) {\n\t\tS.held = {move: m, b: b};\n\t\tif (S.playing) cutAt(b);\n\t} else {\n\t\tif (S.held && S.held.move === m) {\n\t\t\tS.held = null;\n\t\t\tif (S.playing) cutAt(b);\n\t\t}\n\t}\n\tplan(now);\n}\nvar dragN = 0;\nfunction dragPulse() { dragN++; HOST.gen('dragseq', dragN); }\n\nfunction padPress(n, on) {\n\t// a pad plays its sticker now, on its own deck, until the next hit or for one beat\n\tif (!S.have || !S.playing || !on) return;\n\tvar now = S.B, b = now + 0.02;\n\tcutAt(b);\n\tvar d = n < 4 ? 0 : 1, idx = S.padStk[n % 4];\n\tvar p0 = idx !== undefined && idx >= 0 && idx < S.stk.length ? mod(S.stk[idx] - 0.002 * S.recSR, S.L) : 0;\n\taddPlay(d, {bs: b, be: b + 1, rb: (p0 + 0.002 * S.recSR) / spbRec(), p0: p0, cm: 0});\n\tliveDeck = d;\n\tplanEnd = b + 1;\n\tsend(now);\n}\n\n// ------------------------------------------------------------------ hands\nfunction setHands(v) {\n\tHOST.ui('timing', v); HOST.ui('weight', v); HOST.ui('slop', v); HOST.ui('risk', v);\n}\n\n// ------------------------------------------------------------------ record analysis\n// Stickers: positive jumps in short-time energy (port of find_stickers in doubles_engine.py).\nfunction findStickers(mono, maxMarks) {\n\tvar hop = 128, n = mono.length, nfr = Math.floor(n / hop);\n\tvar e = new Float64Array(nfr), e2 = new Float64Array(nfr), prev = 0, i, f, s, s2, x;\n\tfor (f = 0; f < nfr; f++) {\n\t\ts = 0; s2 = 0;\n\t\tfor (i = f * hop; i < f * hop + hop; i++) {\n\t\t\tx = mono[i] - (i > 0 ? mono[i - 1] : 0);\n\t\t\ts += x * x; s2 += mono[i] * mono[i];\n\t\t}\n\t\te[f] = Math.log(1 + 200 * Math.sqrt(s / hop));\n\t\te2[f] = Math.log(1 + 200 * Math.sqrt(s2 / hop));\n\t}\n\tvar flux = new Float64Array(nfr), mx = 0;\n\tfor (f = 0; f < nfr; f++) {\n\t\tvar a = e[f] - e[f === 0 ? nfr - 1 : f - 1], b = e2[f] - e2[f === 0 ? nfr - 1 : f - 1];\n\t\tflux[f] = (a > 0 ? a : 0) + (b > 0 ? b : 0);\n\t\tif (flux[f] > mx) mx = flux[f];\n\t}\n\tvar thr = 0.25 * mx, mind = Math.floor(0.07 * S.recSR / hop);\n\tvar order = [];\n\tfor (f = 0; f < nfr; f++) if (flux[f] >= thr) order.push(f);\n\torder.sort(function (p, q) { return flux[q] - flux[p]; });\n\tvar marks = [];\n\tfor (var k = 0; k < order.length && marks.length < maxMarks; k++) {\n\t\tvar fi = order[k], ok = true;\n\t\tfor (var j = 0; j < marks.length; j++) {\n\t\t\tvar dd = Math.abs(fi - marks[j]);\n\t\t\tif (Math.min(dd, nfr - dd) < mind) { ok = false; break; }\n\t\t}\n\t\tif (ok) marks.push(fi);\n\t}\n\tmarks.sort(function (p, q) { return p - q; });\n\tvar out = [];\n\tfor (k = 0; k < marks.length; k++) {\n\t\tvar a0 = Math.max(marks[k] * hop - hop, 1), seg2 = [], m2 = 0;\n\t\tfor (i = a0; i < Math.min(a0 + 3 * hop, n); i++) {\n\t\t\tvar v = Math.abs(mono[i] - mono[i - 1]);\n\t\t\tseg2.push(v); if (v > m2) m2 = v;\n\t\t}\n\t\tvar at = 0;\n\t\tfor (i = 0; i < seg2.length; i++) if (seg2[i] > 0.35 * m2) { at = i; break; }\n\t\tout.push(a0 + at);\n\t}\n\treturn out;\n}\n\n// Kick and snare for fills: band energy over 60 ms after each sticker.\nfunction biquadBand(x, f0, q, type) {\n\tvar w0 = 2 * Math.PI * f0 / S.recSR, al = Math.sin(w0) / (2 * q), cw = Math.cos(w0), b0, b1, b2;\n\tif (type === 'lp') { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; }\n\telse if (type === 'hp') { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; }\n\telse { b0 = al; b1 = 0; b2 = -al; }\n\tvar a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;\n\tvar y = new Float64Array(x.length), x1 = 0, x2 = 0, y1 = 0, y2 = 0;\n\tfor (var i = 0; i < x.length; i++) {\n\t\tvar o = (b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;\n\t\tx2 = x1; x1 = x[i]; y2 = y1; y1 = o; y[i] = o;\n\t}\n\treturn y;\n}\nfunction classify(mono) {\n\t// For each bar of the record: the kick nearest its first beat with the most low end,\n\t// and the snare on its beat 2 or 4 with the most mid and top. Fills use the bar\n\t// they start in, so the hands never have far to go.\n\tvar lo = biquadBand(mono, 110, 0.707, 'lp');\n\tvar mid = biquadBand(mono, 245, 1.0, 'bp');\n\tvar hi = biquadBand(mono, 3500, 0.6, 'bp');\n\tvar w = Math.floor(0.06 * S.recSR), beat = spbRec(), tol = 0.040 * S.recSR;\n\tvar nbar = Math.max(1, Math.round(S.Lb / 4)), K = [], Sn = [];\n\tfor (var r = 0; r < nbar; r++) { K.push([-1, -1]); Sn.push([-1, -1]); }\n\tfor (var k = 0; k < S.stk.length; k++) {\n\t\tvar p = S.stk[k], l = 0, m = 0, h = 0;\n\t\tfor (var i = p; i < Math.min(p + w, mono.length); i++) { l += lo[i] * lo[i]; m += mid[i] * mid[i]; h += hi[i] * hi[i]; }\n\t\tvar nb = Math.round(p / beat), off = Math.abs(p - nb * beat);\n\t\tif (off > tol) continue;\n\t\tvar bar = Math.floor(nb / 4) % nbar, inbar = mod(nb, 4);\n\t\tif (inbar === 0 && l > K[bar][1]) K[bar] = [k, l];\n\t\tif ((inbar === 1 || inbar === 3) && m * h > Sn[bar][1]) Sn[bar] = [k, m * h];\n\t}\n\t// bars without a clear hit borrow from the nearest bar that has one\n\tvar outK = [], outS = [];\n\tfor (r = 0; r < nbar; r++) {\n\t\tvar kk = -1, ss = -1;\n\t\tfor (var dd = 0; dd < nbar && (kk < 0 || ss < 0); dd++) {\n\t\t\tvar r1 = mod(r + dd, nbar), r2 = mod(r - dd, nbar);\n\t\t\tif (kk < 0) kk = K[r1][0] >= 0 ? K[r1][0] : K[r2][0];\n\t\t\tif (ss < 0) ss = Sn[r1][0] >= 0 ? Sn[r1][0] : Sn[r2][0];\n\t\t}\n\t\toutK.push(kk); outS.push(ss);\n\t}\n\treturn [outK, outS];\n}\nfunction fillStickers(b0) {\n\t// sample positions of the kick and snare a fill starting at song beat b0 should use\n\tvar c = S.ctl, k, s;\n\tif (c.fillk > 0 && c.fillk <= S.stk.length) k = S.stk[c.fillk - 1];\n\telse {\n\t\tvar bar = Math.floor(rbS(b0) / 4) % Math.max(S.barK.length, 1), ki = S.barK[bar];\n\t\tk = ki >= 0 ? S.stk[ki] : 0;\n\t}\n\tif (c.fills > 0 && c.fills <= S.stk.length) s = S.stk[c.fills - 1];\n\telse {\n\t\tvar bar2 = Math.floor(rbS(b0) / 4) % Math.max(S.barS.length, 1), si = S.barS[bar2];\n\t\ts = si >= 0 ? S.stk[si] : spbRec();\n\t}\n\treturn [k, s];\n}\n\n// ------------------------------------------------------------------ test entry point\nfunction _setRecord(L, Lb, anchor, recSR, mono) {\n\tS.L = L; S.Lb = Lb; S.anchor = mod(anchor, Lb); S.recSR = recSR; S.have = 1;\n\tS.stk = findStickers(mono, Math.min(256, 4 * Lb));\n\tvar ks = classify(mono);\n\tS.barK = ks[0]; S.barS = ks[1];\n\tS.K = S.barK[0] >= 0 ? S.stk[S.barK[0]] : -1; S.S = S.barS[0] >= 0 ? S.stk[S.barS[0]] : -1;\n\tS.padStk = [S.barK[0], S.barS[0], S.barK[1 % S.barK.length], S.barS[1 % S.barS.length]];\n\tplanEnd = null;\n}\n\n\n// ====================================================================== Max glue\n// Everything below runs only inside Max.\nvar NAMES = {rec: 'dbl_rec', q: 'dbl_q', st: 'dbl_stat', save: 'dbl_save', load: 'dbl_load'};\nif (typeof jsarguments !== 'undefined' && jsarguments.length > 5) {\n\tNAMES.rec = jsarguments[1]; NAMES.q = jsarguments[2]; NAMES.st = jsarguments[3];\n\tNAMES.save = jsarguments[4]; NAMES.load = jsarguments[5];\n}\nvar bRec = null, bQ = null, bSt = null, bSave = null, bLoad = null;\nvar ready = false, initAt = 0, handsGuard = true;\nvar reccmd = 0, sbseq = 0, recArmed = false, lastRecState = 0;\nvar apiPlay = null, apiTempo = null, apiTime = null;\nvar restoreState = null;\nvar IN_MAX = (typeof outlet === 'function') && (typeof post === 'function') && (typeof Task !== 'undefined');\nvar tick = null;\nif (IN_MAX) {\n\ttick = new Task(onTick, this);\n\ttick.interval = 20;\n}\n\nvar LENGTHS = [1, 2, 4, 8, 16, 0];\nvar LOOPLENS = [0.25, 0.5, 1, 2, 4];\nvar DDISTS = [0.25, 0.5, 1];\nvar CUNITS = [0.5, 1];\nvar PLENS = [0.25, 0.5, 0.75];\nvar SBACKS = [1, 2, 4, 8];\nvar ARCS = [8, 16, 32];\nvar SLOTLENS = [4, 2];\nvar HELD = ['loop', 'double', 'chase', 'shuffle', 'fill', 'phase'];\nvar SLOTNAMES = ['straight', 'loop', 'double', 'triple', 'chase', 'shuffle', 'pause',\n\t'fill1', 'fill2', 'fill3', 'fill4', 'fill5', 'fill6', 'fill7', 'fill8', 'spin', 'phase'];\n\nif (IN_MAX) {\n\tHOST.gen = function (n, v) { outlet(0, n, v); };\n\tHOST.ui = function () { outlet(1, arrayfromargs(arguments)); };\n\tHOST.queueWrite = function (d, slot, vals) { if (bQ) bQ.poke(1, (d * QN + slot) * QF, vals); };\n\tHOST.heads = function () {\n\t\tif (!bSt) return [0, 0];\n\t\tvar h = bSt.peek(1, 5, 2);\n\t\treturn [h[0], h[1]];\n\t};\n\tHOST.log = function (s) { post('Doubles: ' + s + '\\n'); };\n}\n\nfunction buffers() {\n\tbRec = new Buffer(NAMES.rec); bQ = new Buffer(NAMES.q); bSt = new Buffer(NAMES.st);\n\tbSave = new Buffer(NAMES.save); bLoad = new Buffer(NAMES.load);\n}\n\nfunction status(t) { outlet(1, 'status', 'set', t); }\nfunction showRecord() {\n\tif (!S.have) { status('EMPTY'); return; }\n\tstatus((S.Lb / 4) + (S.Lb === 4 ? ' BAR  ' : ' BARS  ') + S.tempo.toFixed(1) + ' BPM');\n}\n\n// --- start up\nfunction names(rec, q, st, save, load) {\n\tNAMES.rec = String(rec); NAMES.q = String(q); NAMES.st = String(st);\n\tNAMES.save = String(save); NAMES.load = String(load);\n}\nfunction init() {\n\tbuffers();\n\tS.sr = 44100;\n\ttry {\n\t\tapiPlay = new LiveAPI(function (a) { if (a[0] === 'is_playing') setPlaying(a[1]); }, 'live_set');\n\t\tapiPlay.property = 'is_playing';\n\t\tapiTempo = new LiveAPI(function (a) { if (a[0] === 'tempo') setTempo(a[1]); }, 'live_set');\n\t\tapiTempo.property = 'tempo';\n\t\tapiTime = new LiveAPI(function (a) { if (a[0] === 'current_song_time') setSongTime(a[1]); }, 'live_set');\n\t\tapiTime.property = 'current_song_time';\n\t\tsetTempo(apiTempo.get('tempo')[0]);\n\t\tsetPlaying(apiPlay.get('is_playing')[0]);\n\t} catch (e) { HOST.log('Live API not available: ' + e); }\n\tready = true;\n\tinitAt = Date.now();\n\ttick.repeat();\n\tif (restoreState) { restore(restoreState); restoreState = null; }\n\tshowRecord();\n\toutlet(1, 'strip', 'statname', NAMES.st);\n\tsendStrip();\n}\n\nfunction setPlaying(v) {\n\tv = v ? 1 : 0;\n\tif (v !== S.playing) {\n\t\tS.playing = v;\n\t\toutlet(0, 'playing', v);\n\t\tplanEnd = null; TL = [[], []]; sentIdx = [0, 0]; seg = null; segQ = [];\n\t}\n}\nfunction setTempo(v) { S.tempo = v; outlet(0, 'tempo', v); }\nfunction setSongTime(v) { sbseq++; outlet(0, 'songbeat', v); outlet(0, 'sbseq', sbseq); }\n\n// --- the 20 ms tick\nfunction onTick() {\n\tif (!bSt) return;\n\tvar st = bSt.peek(1, 0, 16);\n\tS.B = st[0];\n\tvar rs = st[2];\n\tif (rs !== lastRecState) {\n\t\tif (rs === 1) status('ARMED');\n\t\tif (rs === 2) status('RECORDING');\n\t\tif (rs === 3) finishRecording(st[3], st[4], st[14]);\n\t\tif (rs === 0 && lastRecState === 2) { status('STOPPED'); outlet(1, 'record', 'set', 0); }\n\t\tlastRecState = rs;\n\t}\n\tif (handsGuard && Date.now() - initAt > 1500) handsGuard = false;\n\tif (S.have && S.playing) plan(S.B);\n\tstripTick(st);\n}\n\n// --- record\nfunction arm(on) {\n\treccmd++;\n\tif (on) {\n\t\tvar len = LENGTHS[S.ctl.length] === undefined ? 8 : LENGTHS[S.ctl.length];\n\t\tvar secs = (len || 16) * 4 * 60 / S.tempo;\n\t\tif (secs > bRec.framecount() / S.sr) { status('TOO LONG AT THIS TEMPO'); outlet(1, 'record', 'set', 0); return; }\n\t\toutlet(0, 'reclen', len * 4);\n\t\toutlet(0, 'recstop', 0);\n\t\toutlet(0, 'recmode', 1);\n\t\toutlet(0, 'reccmd', reccmd);\n\t\trecArmed = true;\n\t\tstatus('ARMED');\n\t} else {\n\t\tif (LENGTHS[S.ctl.length] === 0 && lastRecState === 2) { outlet(0, 'recstop', 1); return; }\n\t\toutlet(0, 'recmode', 0);\n\t\toutlet(0, 'reccmd', reccmd);\n\t\trecArmed = false;\n\t\tshowRecord();\n\t}\n}\n\nfunction finishRecording(L, recstart, recb) {\n\treccmd++;\n\toutlet(0, 'recmode', 0);\n\toutlet(0, 'reccmd', reccmd);\n\toutlet(1, 'record', 'set', 0);\n\trecArmed = false;\n\tvar Lb = recb > 0 ? recb : 8;\n\tuseRecord(L, Lb, mod(recstart, Lb), S.sr);\n\tsaveRecord();\n}\n\nfunction monoOf(buf, L) {\n\tvar m = new Float32Array(L), CH = 65536, nch = Math.min(buf.channelcount(), 2);\n\tfor (var a = 0; a < L; a += CH) {\n\t\tvar n = Math.min(CH, L - a), x = buf.peek(1, a, n), y = nch > 1 ? buf.peek(2, a, n) : x;\n\t\tif (n === 1) { x = [x]; y = [y]; }\n\t\tfor (var i = 0; i < n; i++) m[a + i] = 0.5 * (x[i] + y[i]);\n\t}\n\treturn m;\n}\n\nfunction useRecord(L, Lb, anchor, recSR) {\n\toutlet(0, 'haverec', 0);\n\toutlet(0, 'L', L); outlet(0, 'Lb', Lb); outlet(0, 'anchor', anchor);\n\tvar mono = monoOf(bRec, L);\n\tS.stkOff = {}; S.stkAdd = [];\n\t_setRecord(L, Lb, anchor, recSR, mono);\n\tS.wave = overview(mono, 300);\n\tplanEnd = null; TL = [[], []]; sentIdx = [0, 0]; seg = null; segQ = [];\n\toutlet(0, 'haverec', 1);\n\tshowRecord();\n\tsendStrip();\n}\n\nfunction overview(mono, n) {\n\tvar out = [], per = mono.length / n;\n\tfor (var k = 0; k < n; k++) {\n\t\tvar m = 0;\n\t\tfor (var i = Math.floor(k * per); i < Math.floor((k + 1) * per); i += 8) { var v = Math.abs(mono[i]); if (v > m) m = v; }\n\t\tout.push(Math.round(m * 1000) / 1000);\n\t}\n\treturn out;\n}\n\nfunction deviceFolder() {\n\tvar p = this.patcher ? this.patcher.filepath : '';\n\tif (!p) return '';\n\treturn p.substring(0, p.lastIndexOf('/') + 1);\n}\n\nfunction stamp() {\n\tvar d = new Date();\n\tfunction z(n) { return n < 10 ? '0' + n : '' + n; }\n\treturn d.getFullYear() + z(d.getMonth() + 1) + z(d.getDate()) + '-' + z(d.getHours()) + z(d.getMinutes()) + z(d.getSeconds());\n}\n\nfunction saveRecord() {\n\tvar dir = deviceFolder();\n\tif (!dir) { HOST.log('device folder unknown, record not saved to disk'); return; }\n\tvar L = S.L, CH = 65536;\n\tbSave.send('sizeinsamps', L, 2);\n\tfor (var a = 0; a < L; a += CH) {\n\t\tvar n = Math.min(CH, L - a);\n\t\tfor (var c = 1; c <= 2; c++) {\n\t\t\tvar x = bRec.peek(c, a, n);\n\t\t\tbSave.poke(c, a, n === 1 ? [x] : x);\n\t\t}\n\t}\n\tS.file = dir + 'doubles-' + stamp() + '.wav';\n\tbSave.send('writewave', S.file);\n\tHOST.log('record saved to ' + S.file);\n\tnotifyclients();\n}\n\nfunction loadFile(path, Lb, anchor) {\n\tbLoad.send('replace', path);\n\tvar fr = bLoad.framecount(), nch = bLoad.channelcount();\n\tif (!fr) { status('RECORD MISSING'); return false; }\n\tvar L = Math.min(fr, bRec.framecount() - 2), CH = 65536;\n\tfor (var a = 0; a < L; a += CH) {\n\t\tvar n = Math.min(CH, L - a);\n\t\tfor (var c = 1; c <= 2; c++) {\n\t\t\tvar x = bLoad.peek(Math.min(c, nch), a, n);\n\t\t\tbRec.poke(c, a, n === 1 ? [x] : x);\n\t\t}\n\t}\n\tvar fileSR = bLoad.framecount() / (bLoad.length() / 1000);\n\tif (!Lb) {\n\t\tvar bars = fr / fileSR * S.tempo / 240, b2 = 1;\n\t\twhile (b2 * 2 <= 16 && Math.abs(b2 * 2 - bars) < Math.abs(b2 - bars)) b2 *= 2;\n\t\tLb = b2 * 4;\n\t}\n\tuseRecord(L, Lb, anchor || 0, fileSR);\n\treturn true;\n}\n\n// --- keeping the record with the Live set (js is a stored parameter)\nfunction getvalueof() {\n\tif (!S.have) return 'empty';\n\tvar off = [];\n\tfor (var k in S.stkOff) off.push(k);\n\treturn JSON.stringify({file: S.file, Lb: S.Lb, anchor: S.anchor, off: off, add: S.stkAdd});\n}\nfunction setvalueof(v) {\n\tif (!v || v === 'empty' || v === 0) return;\n\ttry { var o = JSON.parse(v); } catch (e) { return; }\n\tif (ready) restore(o); else restoreState = o;\n}\nfunction restore(o) {\n\tif (!o.file) return;\n\tif (loadFile(o.file, o.Lb, o.anchor)) {\n\t\tS.file = o.file;\n\t\tfor (var i = 0; o.off && i < o.off.length; i++) S.stkOff[o.off[i]] = 1;\n\t\tapplyStickerEdits();\n\t}\n}\n\n// --- stickers edited on the strip\nfunction applyStickerEdits() {\n\tvar base = findStickers(monoOf(bRec, S.L), Math.min(256, 4 * S.Lb)), out = [];\n\tfor (var i = 0; i < base.length; i++) if (!S.stkOff[base[i]]) out.push(base[i]);\n\tfor (i = 0; i < S.stkAdd.length; i++) out.push(S.stkAdd[i]);\n\tout.sort(function (a, b) { return a - b; });\n\tS.stk = out;\n\tsendStrip();\n}\nfunction stripClick(x, shift) {\n\tif (!S.have) return;\n\tvar p = Math.round(x * S.L);\n\tif (shift) { S.stkAdd.push(p); applyStickerEdits(); notifyclients(); return; }\n\tvar best = -1, bd = 1e30;\n\tfor (var i = 0; i < S.stk.length; i++) { var d = Math.abs(S.stk[i] - p); if (d < bd) { bd = d; best = i; } }\n\tif (best >= 0 && bd < 0.02 * S.L) {\n\t\tvar s = S.stk[best], ai = S.stkAdd.indexOf(s);\n\t\tif (ai >= 0) S.stkAdd.splice(ai, 1); else S.stkOff[s] = 1;\n\t\tapplyStickerEdits();\n\t\tnotifyclients();\n\t}\n}\n\n// --- strip display\nfunction sendStrip() {\n\tif (!S.have) { outlet(1, 'strip', 'clear'); return; }\n\tif (S.wave) outlet(1, ['strip', 'wave'].concat(S.wave));\n\tvar st = [];\n\tfor (var i = 0; i < S.stk.length; i++) st.push(Math.round(S.stk[i] / S.L * 10000) / 10000);\n\toutlet(1, ['strip', 'stk'].concat(st.length ? st : [-1]));\n\tvar ks = fillStickers(S.B || 0);\n\toutlet(1, 'strip', 'ks', ks[0] / S.L, ks[1] / S.L);\n\toutlet(1, 'strip', 'bars', S.Lb / 4);\n}\nvar lastLate = 0;\nfunction stripTick(st) {\n\tif (st[11] !== lastLate) { lastLate = st[11]; outlet(1, 'strip', 'late'); }\n}\n\n// --- panel messages\nfunction anything() {\n\tvar a = arrayfromargs(arguments), v = a.length ? a[0] : 0, m = messagename, c = S.ctl;\n\tswitch (m) {\n\tcase 'init': init(); return;\n\tcase 'record': if (ready) arm(v); return;\n\tcase 'length': c.length = v; return;\n\tcase 'clear':\n\t\tif (!v) return;\n\t\tS.have = 0; S.file = ''; outlet(0, 'haverec', 0); planEnd = null; TL = [[], []];\n\t\tshowRecord(); sendStrip(); notifyclients(); return;\n\tcase 'drop': if (ready) { if (loadFile(a.join(' '))) { S.file = a.join(' '); notifyclients(); } } return;\n\tcase 'strip': if (a[0] === 'click') stripClick(a[1], a[2]); return;\n\t\tcase 'quant': c.quant = QUANTS[v] || 1; return;\n\tcase 'looplen': c.looplen = LOOPLENS[v] || 1; return;\n\tcase 'doubledist': c.doubledist = DDISTS[v] || 0.5; return;\n\tcase 'chaseunit': c.chaseunit = CUNITS[v] || 1; return;\n\tcase 'shuffleamt': c.shuffle = v / 100; return;\n\tcase 'pauselen': c.pauselen = PLENS[v] || 0.5; return;\n\tcase 'fillpat': c.fillpat = v; return;\n\tcase 'spinback': c.spinback = SBACKS[v] || 4; return;\n\tcase 'fillk': c.fillk = v; sendStrip(); return;\n\tcase 'fills': c.fills = v; sendStrip(); return;\n\tcase 'hands': if (!handsGuard) setHands(v); return;\n\tcase 'timing': case 'weight': case 'slop': case 'risk':\n\t\tc[m] = v; outlet(0, m, v); return;\n\tcase 'copy': c.copy = v; outlet(0, 'wear', v); return;\n\tcase 'mode':\n\t\tc.mode = v;\n\t\tif (v === 1) S.routineAnchor = Math.ceil((S.B + 0.25) / 4) * 4;\n\t\tif (v === 2) S.instinctBar0 = Math.ceil((S.B + 0.25) / 4);\n\t\tif (S.playing && S.have) cutAt(Math.ceil((S.B + 0.25) / 4) * 4);\n\t\treturn;\n\tcase 'density': c.density = v / 100; return;\n\tcase 'funk': c.funk = v / 100; return;\n\tcase 'arc': c.arc = ARCS[v] || 16; return;\n\tcase 'slotlen': c.slotlen = SLOTLENS[v] || 4; return;\n\tcase 'reset': if (v) { S.routineAnchor = Math.ceil((S.B + 0.25) / 4) * 4; if (S.playing && S.have && c.mode === 1) cutAt(S.routineAnchor); } return;\n\tcase 'straight': outlet(0, 'strlev', v / 100); return;\n\tcase 'level': outlet(0, 'level', Math.pow(10, v / 20)); return;\n\tcase 'brake': outlet(0, 'brake', v); return;\n\t}\n\tif (m.indexOf('slot') === 0) {\n\t\tvar n = parseInt(m.substr(4), 10);\n\t\tif (n >= 1 && n <= 16) S.routine[n - 1] = SLOTNAMES[v] || 'straight';\n\t\treturn;\n\t}\n\tif (m.indexOf('pad') === 0) { padPress(parseInt(m.substr(3), 10) - 1, v ? 1 : 0); return; }\n\tif (MOVES.indexOf(m) >= 0 || m === 'drag') { movePress(m, v ? 1 : 0); return; }\n\tHOST.log('unknown message ' + m);\n}\nfunction bang() {}\n\n\nif (!IN_MAX && typeof module !== 'undefined' && module) {\n\tmodule.exports = {\n\t\tS: S, HOST: HOST, plan: plan, movePress: movePress, padPress: padPress, cutAt: cutAt,\n\t\tsetRecord: _setRecord, findStickers: findStickers, TL: function () { return TL; },\n\t\tsetRng: function (f) { rng = f; }, reset: function () { planEnd = null; resetTimeline(); },\n\t\tFILLS: FILLS\n\t};\n}\n",
						"filename": "none",
						"flags": 0,
						"embed": 1,
						"autowatch": 1
					},
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Doubles State",
							"parameter_shortname": "State",
							"parameter_type": 3,
							"parameter_invisible": 1
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-13",
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
					"id": "obj-14",
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
					"text": "names ---dbl_rec ---dbl_q ---dbl_stat ---dbl_save ---dbl_load, init"
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 9,
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
						"",
						""
					],
					"text": "route status record strip btn timing weight slop risk"
				}
			},
			{
				"box": {
					"id": "obj-16",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 7,
					"patching_rect": [
						560.0,
						200.0,
						300.0,
						22.0
					],
					"outlettype": [
						"",
						"",
						"",
						"",
						"",
						"",
						""
					],
					"text": "route loop double chase shuffle fill phase"
				}
			},
			{
				"box": {
					"id": "obj-17",
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
						92.0,
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
						96.0,
						2.0,
						302.0,
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
						400.0,
						2.0,
						254.0,
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
						656.0,
						2.0,
						102.0,
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
						760.0,
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
						898.0,
						2.0,
						114.0,
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
						1014.0,
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
						84.0,
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
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						700.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "RECORD",
					"presentation": 1,
					"presentation_rect": [
						6.0,
						20.0,
						84.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Record",
							"parameter_shortname": "RECORD",
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
					"varname": "Record",
					"texton": "ARMED",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
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
					"text": "prepend record"
				}
			},
			{
				"box": {
					"id": "obj-27",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						800.0,
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
						6.0,
						44.0,
						84.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Length",
							"parameter_shortname": "Length",
							"parameter_type": 2,
							"parameter_enum": [
								"1 BAR",
								"2 BARS",
								"4 BARS",
								"8 BARS",
								"16 BARS",
								"FREE"
							],
							"parameter_initial": [
								3
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 5
						}
					},
					"varname": "Length",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
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
					"text": "prepend length"
				}
			},
			{
				"box": {
					"id": "obj-29",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						900.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "CLEAR",
					"presentation": 1,
					"presentation_rect": [
						6.0,
						63.0,
						84.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Clear",
							"parameter_shortname": "CLEAR",
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
					"varname": "Clear",
					"texton": "CLEAR",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-30",
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
					"text": "prepend clear"
				}
			},
			{
				"box": {
					"id": "obj-31",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1000.0,
						560.0,
						84.0,
						30.0
					],
					"text": "EMPTY",
					"presentation": 1,
					"presentation_rect": [
						6.0,
						82.0,
						86.0,
						28.0
					],
					"textcolor": [
						1.0,
						0.75,
						0.3,
						1.0
					],
					"linecount": 2,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-32",
					"maxclass": "live.drop",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						1000.0,
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
						84.0,
						48.0
					]
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
					"text": "prepend drop"
				}
			},
			{
				"box": {
					"id": "obj-34",
					"maxclass": "v8ui",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						1100.0,
						420.0,
						300.0,
						120.0
					],
					"outlettype": [
						""
					],
					"presentation": 1,
					"presentation_rect": [
						100.0,
						6.0,
						294.0,
						126.0
					],
					"filename": "none",
					"embed": 1,
					"textfile": {
						"text": "// Doubles: the waveform strip (jsui). Shows the record, its stickers, the kick and snare\n// a fill will use, and where each deck's needle is and whether its fader is open.\n// Argument: the status buffer name. Click a sticker to switch it off; shift-click to add one.\n\nmgraphics.init();\nmgraphics.relative_coords = 0;\nmgraphics.autofill = 0;\ninlets = 1;\noutlets = 1;\n\nvar STAT = (typeof jsarguments !== 'undefined' && jsarguments.length > 1) ? jsarguments[1] : 'dbl_stat';\nvar bSt = null;\nvar wave = [], stk = [], ks = [-1, -1], bars = 0, have = false, lateFlash = 0;\nvar pa = 0, pb = 0, ga = 0, gb = 0, live = 0;\nvar COL = {\n\tbg: [0.11, 0.11, 0.12, 1], wave: [0.55, 0.57, 0.6, 1], bar: [0.3, 0.3, 0.32, 1],\n\tstk: [0.85, 0.85, 0.85, 1], a: [1.0, 0.55, 0.1, 1], b: [0.25, 0.65, 1.0, 1],\n\tk: [1.0, 0.85, 0.2, 1], s: [0.95, 0.35, 0.45, 1], text: [0.75, 0.75, 0.78, 1], late: [1, 0.2, 0.2, 1]\n};\n\nvar t = new Task(function () {\n\tif (!bSt) { try { bSt = new Buffer(STAT); } catch (e) { return; } }\n\tvar st = bSt.peek(1, 7, 9);\n\tpa = st[0]; pb = st[1]; ga = st[2]; gb = st[3]; live = st[8];\n\tif (lateFlash > 0) lateFlash--;\n\tmgraphics.redraw();\n}, this);\nt.interval = 33;\nt.repeat();\n\nfunction col(g, c) { g.set_source_rgba(c[0], c[1], c[2], c[3]); }\nfunction clear() { have = false; wave = []; stk = []; mgraphics.redraw(); }\nfunction anything() {\n\tvar a = arrayfromargs(arguments);\n\tif (messagename === 'statname') { STAT = a[0]; bSt = null; }\n\telse if (messagename === 'wave') { wave = a; have = true; }\n\telse if (messagename === 'stk') { stk = (a.length === 1 && a[0] < 0) ? [] : a; }\n\telse if (messagename === 'ks') { ks = a; }\n\telse if (messagename === 'bars') { bars = a[0]; }\n\telse if (messagename === 'late') { lateFlash = 6; }\n\tmgraphics.redraw();\n}\n\nfunction paint() {\n\tvar w = box.rect[2] - box.rect[0], h = box.rect[3] - box.rect[1], g = mgraphics, i, x;\n\tvar top = 2, wh = h - 16;\n\tcol(g, COL.bg); g.rectangle(0, 0, w, h); g.fill();\n\tif (!have) {\n\t\tcol(g, COL.text); g.select_font_face('Arial'); g.set_font_size(10);\n\t\tg.move_to(8, h / 2); g.show_text('RECORD A PIECE, OR DROP AN AUDIO FILE HERE'); return;\n\t}\n\t// bar lines\n\tcol(g, COL.bar);\n\tfor (i = 1; i < bars; i++) { x = Math.round(w * i / bars) + 0.5; g.move_to(x, top); g.line_to(x, top + wh); }\n\tg.set_line_width(1); g.stroke();\n\t// waveform\n\tcol(g, COL.wave);\n\tvar n = wave.length, mid = top + wh / 2;\n\tfor (i = 0; i < n; i++) {\n\t\tvar v = Math.min(1, wave[i]) * wh * 0.48;\n\t\tx = w * (i + 0.5) / n;\n\t\tg.move_to(x, mid - v); g.line_to(x, mid + v);\n\t}\n\tg.set_line_width(Math.max(1, w / n * 0.8)); g.stroke();\n\t// stickers\n\tcol(g, COL.stk);\n\tfor (i = 0; i < stk.length; i++) { x = Math.round(stk[i] * w) + 0.5; g.move_to(x, top); g.line_to(x, top + 5); }\n\tg.set_line_width(1); g.stroke();\n\tif (ks[0] >= 0) { col(g, COL.k); x = ks[0] * w; g.rectangle(x - 3, top, 6, 6); g.fill(); }\n\tif (ks[1] >= 0) { col(g, COL.s); x = ks[1] * w; g.rectangle(x - 3, top, 6, 6); g.fill(); }\n\t// needles\n\tg.set_line_width(2);\n\tg.set_source_rgba(COL.a[0], COL.a[1], COL.a[2], 0.35 + 0.65 * Math.min(1, ga));\n\tx = pa * w; g.move_to(x, top); g.line_to(x, top + wh); g.stroke();\n\tg.set_source_rgba(COL.b[0], COL.b[1], COL.b[2], 0.35 + 0.65 * Math.min(1, gb));\n\tx = pb * w; g.move_to(x, top); g.line_to(x, top + wh); g.stroke();\n\t// fader bar: which deck is open\n\tvar fy = h - 12;\n\tcol(g, COL.bar); g.rectangle(0, fy, w, 10); g.fill();\n\tcol(g, COL.a); g.rectangle(0, fy, w / 2 * Math.min(1, ga), 10); g.fill();\n\tcol(g, COL.b); g.rectangle(w - w / 2 * Math.min(1, gb), fy, w / 2 * Math.min(1, gb), 10); g.fill();\n\tcol(g, COL.text); g.select_font_face('Arial'); g.set_font_size(9);\n\tg.move_to(4, fy + 8.5); g.show_text('A'); g.move_to(w - 10, fy + 8.5); g.show_text('B');\n\tif (lateFlash > 0) { col(g, COL.late); g.move_to(w / 2 - 12, fy + 8.5); g.show_text('LATE'); }\n}\n\nfunction onclick(x, y, but, cmd, shift) {\n\tvar w = box.rect[2] - box.rect[0];\n\tif (!have) return;\n\toutlet(0, 'click', Math.max(0, Math.min(1, x / w)), shift ? 1 : 0);\n}\n",
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
					"id": "obj-35",
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
					"text": "prepend strip"
				}
			},
			{
				"box": {
					"id": "obj-36",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1200.0,
						560.0,
						80.0,
						18.0
					],
					"text": "FILL KICK",
					"presentation": 1,
					"presentation_rect": [
						100.0,
						136.0,
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
					"id": "obj-37",
					"maxclass": "live.numbox",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1200.0,
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
						100.0,
						150.0,
						44.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Fill Kick",
							"parameter_shortname": "Fill K",
							"parameter_type": 1,
							"parameter_mmin": 0.0,
							"parameter_mmax": 256.0,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 0
						}
					},
					"varname": "Fill Kick",
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
						1200.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend fillk"
				}
			},
			{
				"box": {
					"id": "obj-39",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1300.0,
						560.0,
						80.0,
						18.0
					],
					"text": "FILL SNARE",
					"presentation": 1,
					"presentation_rect": [
						160.0,
						136.0,
						64.0,
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
					"id": "obj-40",
					"maxclass": "live.numbox",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						1300.0,
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
						160.0,
						150.0,
						44.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Fill Snare",
							"parameter_shortname": "Fill S",
							"parameter_type": 1,
							"parameter_mmin": 0.0,
							"parameter_mmax": 256.0,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 0
						}
					},
					"varname": "Fill Snare",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-41",
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
					"text": "prepend fills"
				}
			},
			{
				"box": {
					"id": "obj-42",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1400.0,
						560.0,
						80.0,
						18.0
					],
					"text": "0 = FIND FROM EACH BAR.  CLICK A TICK TO REMOVE IT, SHIFT-CLICK TO ADD.",
					"presentation": 1,
					"presentation_rect": [
						226.0,
						134.0,
						170.0,
						30.0
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
					"id": "obj-43",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1400.0,
						560.0,
						80.0,
						18.0
					],
					"text": "MOVES",
					"presentation": 1,
					"presentation_rect": [
						404.0,
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
					"id": "obj-44",
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
					"text": "LOOP",
					"presentation": 1,
					"presentation_rect": [
						404.0,
						20.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Loop",
							"parameter_shortname": "LOOP",
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
					"varname": "Loop",
					"texton": "LOOP",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-45",
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
					"text": "prepend loop"
				}
			},
			{
				"box": {
					"id": "obj-46",
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
					"text": "DOUBLE",
					"presentation": 1,
					"presentation_rect": [
						445.0,
						20.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Double",
							"parameter_shortname": "DOUBLE",
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
					"varname": "Double",
					"texton": "DOUBLE",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-47",
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
					"text": "prepend double"
				}
			},
			{
				"box": {
					"id": "obj-48",
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
					"text": "CHASE",
					"presentation": 1,
					"presentation_rect": [
						486.0,
						20.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Chase",
							"parameter_shortname": "CHASE",
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
					"varname": "Chase",
					"texton": "CHASE",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-49",
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
					"text": "prepend chase"
				}
			},
			{
				"box": {
					"id": "obj-50",
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
					"text": "SHUFFLE",
					"presentation": 1,
					"presentation_rect": [
						527.0,
						20.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Shuffle",
							"parameter_shortname": "SHUFFLE",
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
					"varname": "Shuffle",
					"texton": "SHUFFLE",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-51",
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
					"text": "prepend shuffle"
				}
			},
			{
				"box": {
					"id": "obj-52",
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
					"text": "FILL",
					"presentation": 1,
					"presentation_rect": [
						568.0,
						20.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Fill",
							"parameter_shortname": "FILL",
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
					"varname": "Fill",
					"texton": "FILL",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-53",
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
					"text": "prepend fill"
				}
			},
			{
				"box": {
					"id": "obj-54",
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
					"text": "PHASE",
					"presentation": 1,
					"presentation_rect": [
						609.0,
						20.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Phase",
							"parameter_shortname": "PHASE",
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
					"varname": "Phase",
					"texton": "PHASE",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-55",
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
					"text": "prepend phase"
				}
			},
			{
				"box": {
					"id": "obj-56",
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
					"text": "TRIPLE",
					"presentation": 1,
					"presentation_rect": [
						404.0,
						44.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Triple",
							"parameter_shortname": "TRIPLE",
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
					"varname": "Triple",
					"texton": "TRIPLE",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-57",
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
					"text": "prepend triple"
				}
			},
			{
				"box": {
					"id": "obj-58",
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
					"text": "PAUSE",
					"presentation": 1,
					"presentation_rect": [
						445.0,
						44.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pause",
							"parameter_shortname": "PAUSE",
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
					"varname": "Pause",
					"texton": "PAUSE",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-59",
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
					"text": "prepend pause"
				}
			},
			{
				"box": {
					"id": "obj-60",
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
					"text": "SPIN",
					"presentation": 1,
					"presentation_rect": [
						486.0,
						44.0,
						39.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Spin",
							"parameter_shortname": "SPIN",
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
					"varname": "Spin",
					"texton": "SPIN",
					"mode": 0,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-61",
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
					"text": "prepend spin"
				}
			},
			{
				"box": {
					"id": "obj-62",
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
					"text": "DRAG",
					"presentation": 1,
					"presentation_rect": [
						527.0,
						44.0,
						39.0,
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
					"id": "obj-63",
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
					"text": "prepend drag"
				}
			},
			{
				"box": {
					"id": "obj-64",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						2400.0,
						560.0,
						80.0,
						18.0
					],
					"text": "QUANT",
					"presentation": 1,
					"presentation_rect": [
						570.0,
						44.0,
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
					"id": "obj-65",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						2400.0,
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
						608.0,
						46.0,
						42.0,
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
								"BAR"
							],
							"parameter_initial": [
								2
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
					"id": "obj-66",
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
					"text": "prepend quant"
				}
			},
			{
				"box": {
					"id": "obj-67",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						2500.0,
						560.0,
						80.0,
						18.0
					],
					"text": "LOOP",
					"presentation": 1,
					"presentation_rect": [
						404.0,
						70.0,
						48.0,
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
					"id": "obj-68",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						2500.0,
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
						404.0,
						84.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Loop Length",
							"parameter_shortname": "Loop",
							"parameter_type": 2,
							"parameter_enum": [
								"1/16",
								"1/8",
								"1/4",
								"1/2",
								"BAR"
							],
							"parameter_initial": [
								2
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 4
						}
					},
					"varname": "Loop Length",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-69",
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
					"text": "prepend looplen"
				}
			},
			{
				"box": {
					"id": "obj-70",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						2600.0,
						560.0,
						80.0,
						18.0
					],
					"text": "DOUBLE",
					"presentation": 1,
					"presentation_rect": [
						454.0,
						70.0,
						48.0,
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
					"id": "obj-71",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						2600.0,
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
						454.0,
						84.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Double Distance",
							"parameter_shortname": "Double",
							"parameter_type": 2,
							"parameter_enum": [
								"1/16",
								"1/8",
								"1/4"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 2
						}
					},
					"varname": "Double Distance",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-72",
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
					"text": "prepend doubledist"
				}
			},
			{
				"box": {
					"id": "obj-73",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						2700.0,
						560.0,
						80.0,
						18.0
					],
					"text": "CHASE",
					"presentation": 1,
					"presentation_rect": [
						504.0,
						70.0,
						48.0,
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
					"id": "obj-74",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						2700.0,
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
						504.0,
						84.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Chase Unit",
							"parameter_shortname": "Chase",
							"parameter_type": 2,
							"parameter_enum": [
								"1/8",
								"1/4"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 1
						}
					},
					"varname": "Chase Unit",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-75",
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
					"text": "prepend chaseunit"
				}
			},
			{
				"box": {
					"id": "obj-76",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						2800.0,
						560.0,
						80.0,
						18.0
					],
					"text": "PAUSE",
					"presentation": 1,
					"presentation_rect": [
						554.0,
						70.0,
						48.0,
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
					"id": "obj-77",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						2800.0,
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
						554.0,
						84.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pause Length",
							"parameter_shortname": "Pause",
							"parameter_type": 2,
							"parameter_enum": [
								"1/16",
								"1/8",
								"3/16"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 2
						}
					},
					"varname": "Pause Length",
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
						2800.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend pauselen"
				}
			},
			{
				"box": {
					"id": "obj-79",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						2900.0,
						560.0,
						80.0,
						18.0
					],
					"text": "SPIN",
					"presentation": 1,
					"presentation_rect": [
						604.0,
						70.0,
						48.0,
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
					"id": "obj-80",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						2900.0,
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
						604.0,
						84.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Spin Back",
							"parameter_shortname": "Spin",
							"parameter_type": 2,
							"parameter_enum": [
								"1 BEAT",
								"2 BEATS",
								"1 BAR",
								"2 BARS"
							],
							"parameter_initial": [
								2
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 3
						}
					},
					"varname": "Spin Back",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-81",
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
					"text": "prepend spinback"
				}
			},
			{
				"box": {
					"id": "obj-82",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						3000.0,
						560.0,
						80.0,
						18.0
					],
					"text": "SHUFFLE",
					"presentation": 1,
					"presentation_rect": [
						404.0,
						106.0,
						48.0,
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
					"id": "obj-83",
					"maxclass": "live.numbox",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3000.0,
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
						404.0,
						120.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Shuffle Amount",
							"parameter_shortname": "Shuffle",
							"parameter_type": 1,
							"parameter_mmin": 0.0,
							"parameter_mmax": 60.0,
							"parameter_initial": [
								50
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Shuffle Amount",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-84",
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
					"text": "prepend shuffleamt"
				}
			},
			{
				"box": {
					"id": "obj-85",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						3100.0,
						560.0,
						80.0,
						18.0
					],
					"text": "FILL PATTERN",
					"presentation": 1,
					"presentation_rect": [
						458.0,
						106.0,
						70.0,
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
					"id": "obj-86",
					"maxclass": "live.numbox",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						3100.0,
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
						458.0,
						120.0,
						46.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Fill Pattern",
							"parameter_shortname": "Fill Pat",
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
					"varname": "Fill Pattern",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-87",
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
					"text": "prepend fillpat"
				}
			},
			{
				"box": {
					"id": "obj-88",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						3200.0,
						560.0,
						80.0,
						18.0
					],
					"text": "ON/OFF: LOOP TO PHASE.  ONE-SHOT: TRIPLE TO DRAG.",
					"presentation": 1,
					"presentation_rect": [
						404.0,
						140.0,
						246.0,
						24.0
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
					"id": "obj-89",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						3200.0,
						560.0,
						80.0,
						18.0
					],
					"text": "PADS  A / B",
					"presentation": 1,
					"presentation_rect": [
						660.0,
						4.0,
						90.0,
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
					"id": "obj-90",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3200.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						662.0,
						22.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 1",
							"parameter_shortname": "Pad 1",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 1"
				}
			},
			{
				"box": {
					"id": "obj-91",
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
					"text": "prepend pad1"
				}
			},
			{
				"box": {
					"id": "obj-92",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3300.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						686.0,
						22.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 2",
							"parameter_shortname": "Pad 2",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 2"
				}
			},
			{
				"box": {
					"id": "obj-93",
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
					"text": "prepend pad2"
				}
			},
			{
				"box": {
					"id": "obj-94",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3400.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						710.0,
						22.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 3",
							"parameter_shortname": "Pad 3",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 3"
				}
			},
			{
				"box": {
					"id": "obj-95",
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
					"text": "prepend pad3"
				}
			},
			{
				"box": {
					"id": "obj-96",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3500.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						734.0,
						22.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 4",
							"parameter_shortname": "Pad 4",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 4"
				}
			},
			{
				"box": {
					"id": "obj-97",
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
					"text": "prepend pad4"
				}
			},
			{
				"box": {
					"id": "obj-98",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3600.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						662.0,
						52.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 5",
							"parameter_shortname": "Pad 5",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 5"
				}
			},
			{
				"box": {
					"id": "obj-99",
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
					"text": "prepend pad5"
				}
			},
			{
				"box": {
					"id": "obj-100",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3700.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						686.0,
						52.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 6",
							"parameter_shortname": "Pad 6",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 6"
				}
			},
			{
				"box": {
					"id": "obj-101",
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
					"text": "prepend pad6"
				}
			},
			{
				"box": {
					"id": "obj-102",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3800.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						710.0,
						52.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 7",
							"parameter_shortname": "Pad 7",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 7"
				}
			},
			{
				"box": {
					"id": "obj-103",
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
					"text": "prepend pad7"
				}
			},
			{
				"box": {
					"id": "obj-104",
					"maxclass": "live.button",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						3900.0,
						500.0,
						15.0,
						15.0
					],
					"outlettype": [
						"bang"
					],
					"presentation": 1,
					"presentation_rect": [
						734.0,
						52.0,
						20.0,
						20.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Pad 8",
							"parameter_shortname": "Pad 8",
							"parameter_type": 2,
							"parameter_enum": [
								"off",
								"on"
							],
							"parameter_mmax": 1
						}
					},
					"varname": "Pad 8"
				}
			},
			{
				"box": {
					"id": "obj-105",
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
					"text": "prepend pad8"
				}
			},
			{
				"box": {
					"id": "obj-106",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4000.0,
						560.0,
						80.0,
						18.0
					],
					"text": "A: KICK, SNARE, KICK 2, SNARE 2.  B: THE SAME ON THE OTHER COPY.",
					"presentation": 1,
					"presentation_rect": [
						660.0,
						84.0,
						96.0,
						60.0
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
					"id": "obj-107",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4000.0,
						560.0,
						80.0,
						18.0
					],
					"text": "HANDS",
					"presentation": 1,
					"presentation_rect": [
						764.0,
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
					"id": "obj-108",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4000.0,
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
						764.0,
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
					"varname": "Hands"
				}
			},
			{
				"box": {
					"id": "obj-109",
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
					"text": "prepend hands"
				}
			},
			{
				"box": {
					"id": "obj-110",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4100.0,
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
						810.0,
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
					"varname": "Timing"
				}
			},
			{
				"box": {
					"id": "obj-111",
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
					"text": "prepend timing"
				}
			},
			{
				"box": {
					"id": "obj-112",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4200.0,
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
						852.0,
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
					"varname": "Weight"
				}
			},
			{
				"box": {
					"id": "obj-113",
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
					"text": "prepend weight"
				}
			},
			{
				"box": {
					"id": "obj-114",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4300.0,
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
						764.0,
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
					"varname": "Slop"
				}
			},
			{
				"box": {
					"id": "obj-115",
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
					"text": "prepend slop"
				}
			},
			{
				"box": {
					"id": "obj-116",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4400.0,
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
						808.0,
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
					"varname": "Risk"
				}
			},
			{
				"box": {
					"id": "obj-117",
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
					"text": "prepend risk"
				}
			},
			{
				"box": {
					"id": "obj-118",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4500.0,
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
						852.0,
						92.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Copy",
							"parameter_shortname": "COPY",
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
					"varname": "Copy"
				}
			},
			{
				"box": {
					"id": "obj-119",
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
					"text": "prepend copy"
				}
			},
			{
				"box": {
					"id": "obj-120",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4600.0,
						560.0,
						80.0,
						18.0
					],
					"text": "HANDS MOVES ALL FOUR.  COPY: WEAR ON DECK B.",
					"presentation": 1,
					"presentation_rect": [
						764.0,
						142.0,
						130.0,
						24.0
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
					"id": "obj-121",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4600.0,
						560.0,
						80.0,
						18.0
					],
					"text": "PLAY",
					"presentation": 1,
					"presentation_rect": [
						902.0,
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
					"id": "obj-122",
					"maxclass": "live.tab",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						4600.0,
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
						902.0,
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
								"HANDS-ON",
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
					"id": "obj-123",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4600.0,
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
					"id": "obj-124",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4700.0,
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
						968.0,
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
								60
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
					"id": "obj-125",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4700.0,
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
					"id": "obj-126",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						4800.0,
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
						902.0,
						72.0,
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
					"id": "obj-127",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4800.0,
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
					"id": "obj-128",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						4900.0,
						560.0,
						80.0,
						18.0
					],
					"text": "ARC",
					"presentation": 1,
					"presentation_rect": [
						948.0,
						76.0,
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
					"id": "obj-129",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						4900.0,
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
						948.0,
						90.0,
						58.0,
						15.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Arc",
							"parameter_shortname": "Arc",
							"parameter_type": 2,
							"parameter_enum": [
								"8 BARS",
								"16 BARS",
								"32 BARS"
							],
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_mmax": 2
						}
					},
					"varname": "Arc",
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-130",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						4900.0,
						600.0,
						90.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend arc"
				}
			},
			{
				"box": {
					"id": "obj-131",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5000.0,
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
						902.0,
						126.0,
						106.0,
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
					"id": "obj-132",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						5080.0,
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
					"id": "obj-133",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						5080.0,
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
					"id": "obj-134",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						5080.0,
						560.0,
						80.0,
						18.0
					],
					"text": "OUT",
					"presentation": 1,
					"presentation_rect": [
						1018.0,
						4.0,
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
					"id": "obj-135",
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
						1018.0,
						18.0,
						41.0,
						48.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Straight Level",
							"parameter_shortname": "STRAIGHT",
							"parameter_type": 0,
							"parameter_mmin": 0.0,
							"parameter_mmax": 100.0,
							"parameter_initial": [
								100
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					},
					"varname": "Straight Level"
				}
			},
			{
				"box": {
					"id": "obj-136",
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
					"text": "prepend straight"
				}
			},
			{
				"box": {
					"id": "obj-137",
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
						1018.0,
						72.0,
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
					"varname": "Level"
				}
			},
			{
				"box": {
					"id": "obj-138",
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
					"text": "prepend level"
				}
			},
			{
				"box": {
					"id": "obj-139",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						5280.0,
						500.0,
						60.0,
						20.0
					],
					"outlettype": [
						"",
						""
					],
					"text": "BRAKE",
					"presentation": 1,
					"presentation_rect": [
						1018.0,
						126.0,
						44.0,
						18.0
					],
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Brake",
							"parameter_shortname": "BRAKE",
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
					"varname": "Brake",
					"texton": "BRAKE",
					"mode": 1,
					"fontname": "Ableton Sans Medium",
					"fontsize": 9.0
				}
			},
			{
				"box": {
					"id": "obj-140",
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
					"text": "prepend brake"
				}
			},
			{
				"box": {
					"id": "obj-141",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						5380.0,
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
							680.0,
							380.0
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
										330.0,
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
										520.0,
										20.0
									],
									"text": "DOUBLES ROUTINE: one move per slot. Set MODE to ROUTINE to play it. RESET starts it at the next bar."
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
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 1"
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
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot1"
								}
							},
							{
								"box": {
									"id": "r-l1",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										135.0,
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 2"
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
										135.0,
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												1
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										135.0,
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot2"
								}
							},
							{
								"box": {
									"id": "r-l2",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										210.0,
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 3"
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
										210.0,
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										210.0,
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot3"
								}
							},
							{
								"box": {
									"id": "r-l3",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										285.0,
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 4"
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
										285.0,
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												3
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										285.0,
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot4"
								}
							},
							{
								"box": {
									"id": "r-l4",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										360.0,
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 5"
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
										360.0,
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												2
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										360.0,
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot5"
								}
							},
							{
								"box": {
									"id": "r-l5",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										435.0,
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 6"
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
										435.0,
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										435.0,
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot6"
								}
							},
							{
								"box": {
									"id": "r-l6",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										510.0,
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 7"
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
										510.0,
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												4
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										510.0,
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot7"
								}
							},
							{
								"box": {
									"id": "r-l7",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										585.0,
										50.0,
										70.0,
										18.0
									],
									"text": "SLOT 8"
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
										585.0,
										70.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												7
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										585.0,
										95.0,
										70.0,
										22.0
									],
									"text": "prepend slot8"
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
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 9"
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
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot9"
								}
							},
							{
								"box": {
									"id": "r-l9",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										135.0,
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 10"
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
										135.0,
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												1
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										135.0,
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot10"
								}
							},
							{
								"box": {
									"id": "r-l10",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										210.0,
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 11"
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
										210.0,
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												5
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										210.0,
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot11"
								}
							},
							{
								"box": {
									"id": "r-l11",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										285.0,
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 12"
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
										285.0,
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												8
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										285.0,
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot12"
								}
							},
							{
								"box": {
									"id": "r-l12",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										360.0,
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 13"
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
										360.0,
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												2
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										360.0,
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot13"
								}
							},
							{
								"box": {
									"id": "r-l13",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										435.0,
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 14"
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
										435.0,
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										435.0,
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot14"
								}
							},
							{
								"box": {
									"id": "r-l14",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										510.0,
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 15"
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
										510.0,
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												6
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										510.0,
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot15"
								}
							},
							{
								"box": {
									"id": "r-l15",
									"maxclass": "comment",
									"numinlets": 1,
									"numoutlets": 0,
									"patching_rect": [
										585.0,
										140.0,
										70.0,
										18.0
									],
									"text": "SLOT 16"
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
										585.0,
										160.0,
										70.0,
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
												"STRAIGHT",
												"LOOP",
												"DOUBLE",
												"TRIPLE",
												"CHASE",
												"SHUFFLE",
												"PAUSE",
												"FILL 1",
												"FILL 2",
												"FILL 3",
												"FILL 4",
												"FILL 5",
												"FILL 6",
												"FILL 7",
												"FILL 8",
												"SPIN",
												"PHASE"
											],
											"parameter_initial": [
												15
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 16
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
										585.0,
										185.0,
										70.0,
										22.0
									],
									"text": "prepend slot16"
								}
							},
							{
								"box": {
									"id": "r-sl",
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
										80.0,
										15.0
									],
									"parameter_enable": 1,
									"varname": "Slot Length",
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Slot Length",
											"parameter_shortname": "Slot Len",
											"parameter_type": 2,
											"parameter_enum": [
												"BAR",
												"HALF BAR"
											],
											"parameter_initial": [
												0
											],
											"parameter_initial_enable": 1,
											"parameter_mmax": 1
										}
									}
								}
							},
							{
								"box": {
									"id": "r-slp",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										60.0,
										280.0,
										90.0,
										22.0
									],
									"text": "prepend slotlen"
								}
							},
							{
								"box": {
									"id": "r-rs",
									"maxclass": "live.text",
									"numinlets": 1,
									"numoutlets": 2,
									"outlettype": [
										"",
										""
									],
									"patching_rect": [
										160.0,
										250.0,
										60.0,
										18.0
									],
									"parameter_enable": 1,
									"varname": "Routine Reset",
									"text": "RESET",
									"texton": "RESET",
									"mode": 0,
									"saved_attribute_attributes": {
										"valueof": {
											"parameter_longname": "Routine Reset",
											"parameter_shortname": "Reset",
											"parameter_type": 2,
											"parameter_enum": [
												"off",
												"on"
											],
											"parameter_mmax": 1
										}
									}
								}
							},
							{
								"box": {
									"id": "r-rsp",
									"maxclass": "newobj",
									"numinlets": 2,
									"numoutlets": 1,
									"outlettype": [
										""
									],
									"patching_rect": [
										160.0,
										280.0,
										80.0,
										22.0
									],
									"text": "prepend reset"
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
										"r-sl",
										0
									],
									"destination": [
										"r-slp",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-slp",
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
										"r-rs",
										0
									],
									"destination": [
										"r-rsp",
										0
									]
								}
							},
							{
								"patchline": {
									"source": [
										"r-rsp",
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
						"obj-3",
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
						"obj-3",
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
						2
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
						"obj-4",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-3",
						1
					],
					"destination": [
						"obj-4",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-10",
						0
					],
					"destination": [
						"obj-11",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-11",
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
						"obj-12",
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
						"obj-13",
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
						0
					],
					"destination": [
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-12",
						1
					],
					"destination": [
						"obj-15",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
						3
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-29",
						0
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
						"obj-30",
						0
					],
					"destination": [
						"obj-12",
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
						"obj-31",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
						2
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
						"obj-34",
						0
					],
					"destination": [
						"obj-35",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-35",
						0
					],
					"destination": [
						"obj-12",
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
						"obj-12",
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
						"obj-41",
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
						"obj-12",
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
						"obj-45",
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
						"obj-12",
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
						"obj-44",
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
						"obj-47",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-16",
						1
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
						"obj-48",
						0
					],
					"destination": [
						"obj-49",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-16",
						2
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
						"obj-50",
						0
					],
					"destination": [
						"obj-51",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-16",
						3
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
						"obj-52",
						0
					],
					"destination": [
						"obj-53",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-16",
						4
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
						"obj-54",
						0
					],
					"destination": [
						"obj-55",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-16",
						5
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
						"obj-56",
						0
					],
					"destination": [
						"obj-57",
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
						"obj-12",
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
						"obj-59",
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
						"obj-12",
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
						"obj-61",
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
						"obj-12",
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
						"obj-63",
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
						"obj-12",
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
						"obj-12",
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
						"obj-69",
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
						"obj-12",
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
						"obj-12",
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
						"obj-75",
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
						"obj-12",
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
						"obj-12",
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
						"obj-81",
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
						"obj-12",
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
						"obj-12",
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
						"obj-87",
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
						"obj-12",
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
						"obj-91",
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
						"obj-12",
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
						"obj-93",
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
						"obj-12",
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
						"obj-95",
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
						"obj-12",
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
						"obj-97",
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
						"obj-12",
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
						"obj-99",
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
						"obj-12",
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
						"obj-101",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-101",
						0
					],
					"destination": [
						"obj-12",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-104",
						0
					],
					"destination": [
						"obj-105",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-108",
						0
					],
					"destination": [
						"obj-109",
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
						"obj-12",
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
						"obj-111",
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
						"obj-12",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-114",
						0
					],
					"destination": [
						"obj-115",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-115",
						0
					],
					"destination": [
						"obj-12",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-118",
						0
					],
					"destination": [
						"obj-119",
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
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
						4
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
						"obj-15",
						5
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
						"obj-15",
						6
					],
					"destination": [
						"obj-114",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
						7
					],
					"destination": [
						"obj-116",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-122",
						0
					],
					"destination": [
						"obj-123",
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
						"obj-12",
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
						"obj-125",
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
						"obj-12",
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
						"obj-127",
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
						"obj-12",
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
						"obj-12",
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
						"obj-133",
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
						"obj-132",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-135",
						0
					],
					"destination": [
						"obj-136",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-136",
						0
					],
					"destination": [
						"obj-12",
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
						"obj-12",
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
						"obj-12",
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
						"obj-141",
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
						"obj-12",
						0
					]
				}
			}
		],
		"parameters": {
			"obj-12": [
				"Doubles State",
				"State",
				0
			],
			"obj-25": [
				"Record",
				"RECORD",
				0
			],
			"obj-27": [
				"Length",
				"Length",
				0
			],
			"obj-29": [
				"Clear",
				"CLEAR",
				0
			],
			"obj-37": [
				"Fill Kick",
				"Fill K",
				0
			],
			"obj-40": [
				"Fill Snare",
				"Fill S",
				0
			],
			"obj-44": [
				"Loop",
				"LOOP",
				0
			],
			"obj-46": [
				"Double",
				"DOUBLE",
				0
			],
			"obj-48": [
				"Chase",
				"CHASE",
				0
			],
			"obj-50": [
				"Shuffle",
				"SHUFFLE",
				0
			],
			"obj-52": [
				"Fill",
				"FILL",
				0
			],
			"obj-54": [
				"Phase",
				"PHASE",
				0
			],
			"obj-56": [
				"Triple",
				"TRIPLE",
				0
			],
			"obj-58": [
				"Pause",
				"PAUSE",
				0
			],
			"obj-60": [
				"Spin",
				"SPIN",
				0
			],
			"obj-62": [
				"Drag",
				"DRAG",
				0
			],
			"obj-65": [
				"Quantise",
				"Quant",
				0
			],
			"obj-68": [
				"Loop Length",
				"Loop",
				0
			],
			"obj-71": [
				"Double Distance",
				"Double",
				0
			],
			"obj-74": [
				"Chase Unit",
				"Chase",
				0
			],
			"obj-77": [
				"Pause Length",
				"Pause",
				0
			],
			"obj-80": [
				"Spin Back",
				"Spin",
				0
			],
			"obj-83": [
				"Shuffle Amount",
				"Shuffle",
				0
			],
			"obj-86": [
				"Fill Pattern",
				"Fill Pat",
				0
			],
			"obj-90": [
				"Pad 1",
				"Pad 1",
				0
			],
			"obj-92": [
				"Pad 2",
				"Pad 2",
				0
			],
			"obj-94": [
				"Pad 3",
				"Pad 3",
				0
			],
			"obj-96": [
				"Pad 4",
				"Pad 4",
				0
			],
			"obj-98": [
				"Pad 5",
				"Pad 5",
				0
			],
			"obj-100": [
				"Pad 6",
				"Pad 6",
				0
			],
			"obj-102": [
				"Pad 7",
				"Pad 7",
				0
			],
			"obj-104": [
				"Pad 8",
				"Pad 8",
				0
			],
			"obj-108": [
				"Hands",
				"HANDS",
				0
			],
			"obj-110": [
				"Timing",
				"TIMING",
				0
			],
			"obj-112": [
				"Weight",
				"WEIGHT",
				0
			],
			"obj-114": [
				"Slop",
				"SLOP",
				0
			],
			"obj-116": [
				"Risk",
				"RISK",
				0
			],
			"obj-118": [
				"Copy",
				"COPY",
				0
			],
			"obj-122": [
				"Mode",
				"Mode",
				0
			],
			"obj-124": [
				"Density",
				"DENSITY",
				0
			],
			"obj-126": [
				"Funk",
				"FUNK",
				0
			],
			"obj-129": [
				"Arc",
				"Arc",
				0
			],
			"obj-135": [
				"Straight Level",
				"STRAIGHT",
				0
			],
			"obj-137": [
				"Level",
				"LEVEL",
				0
			],
			"obj-139": [
				"Brake",
				"BRAKE",
				0
			],
			"r-m0": [
				"Slot 1",
				"S1",
				0
			],
			"r-m1": [
				"Slot 2",
				"S2",
				0
			],
			"r-m2": [
				"Slot 3",
				"S3",
				0
			],
			"r-m3": [
				"Slot 4",
				"S4",
				0
			],
			"r-m4": [
				"Slot 5",
				"S5",
				0
			],
			"r-m5": [
				"Slot 6",
				"S6",
				0
			],
			"r-m6": [
				"Slot 7",
				"S7",
				0
			],
			"r-m7": [
				"Slot 8",
				"S8",
				0
			],
			"r-m8": [
				"Slot 9",
				"S9",
				0
			],
			"r-m9": [
				"Slot 10",
				"S10",
				0
			],
			"r-m10": [
				"Slot 11",
				"S11",
				0
			],
			"r-m11": [
				"Slot 12",
				"S12",
				0
			],
			"r-m12": [
				"Slot 13",
				"S13",
				0
			],
			"r-m13": [
				"Slot 14",
				"S14",
				0
			],
			"r-m14": [
				"Slot 15",
				"S15",
				0
			],
			"r-m15": [
				"Slot 16",
				"S16",
				0
			],
			"r-sl": [
				"Slot Length",
				"Slot Len",
				0
			],
			"r-rs": [
				"Routine Reset",
				"Reset",
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
			"amxdtype": 1633771873,
			"readonly": 0,
			"devpathtype": 0,
			"devpath": ".",
			"sortmode": 0,
			"viewmode": 0,
			"includepackages": 0
		}
	}
}
