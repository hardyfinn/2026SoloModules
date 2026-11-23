// Doubles: the waveform strip (jsui). Shows the record, its stickers, the kick and snare
// a fill will use, and where each deck's needle is and whether its fader is open.
// Argument: the status buffer name. Click a sticker to switch it off; shift-click to add one.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
inlets = 1;
outlets = 1;

var STAT = (typeof jsarguments !== 'undefined' && jsarguments.length > 1) ? jsarguments[1] : 'dbl_stat';
var bSt = null;
var wave = [], stk = [], ks = [-1, -1], bars = 0, have = false, lateFlash = 0;
var pa = 0, pb = 0, ga = 0, gb = 0, live = 0;
var COL = {
	bg: [0.11, 0.11, 0.12, 1], wave: [0.55, 0.57, 0.6, 1], bar: [0.3, 0.3, 0.32, 1],
	stk: [0.85, 0.85, 0.85, 1], a: [1.0, 0.55, 0.1, 1], b: [0.25, 0.65, 1.0, 1],
	k: [1.0, 0.85, 0.2, 1], s: [0.95, 0.35, 0.45, 1], text: [0.75, 0.75, 0.78, 1], late: [1, 0.2, 0.2, 1]
};

var t = new Task(function () {
	if (!bSt) { try { bSt = new Buffer(STAT); } catch (e) { return; } }
	var st = bSt.peek(1, 7, 9);
	pa = st[0]; pb = st[1]; ga = st[2]; gb = st[3]; live = st[8];
	if (lateFlash > 0) lateFlash--;
	mgraphics.redraw();
}, this);
t.interval = 33;
t.repeat();

function col(g, c) { g.set_source_rgba(c[0], c[1], c[2], c[3]); }
function clear() { have = false; wave = []; stk = []; mgraphics.redraw(); }
function anything() {
	var a = arrayfromargs(arguments);
	if (messagename === 'statname') { STAT = a[0]; bSt = null; }
	else if (messagename === 'wave') { wave = a; have = true; }
	else if (messagename === 'stk') { stk = (a.length === 1 && a[0] < 0) ? [] : a; }
	else if (messagename === 'ks') { ks = a; }
	else if (messagename === 'bars') { bars = a[0]; }
	else if (messagename === 'late') { lateFlash = 6; }
	mgraphics.redraw();
}

function paint() {
	var w = box.rect[2] - box.rect[0], h = box.rect[3] - box.rect[1], g = mgraphics, i, x;
	var top = 2, wh = h - 16;
	col(g, COL.bg); g.rectangle(0, 0, w, h); g.fill();
	if (!have) {
		col(g, COL.text); g.select_font_face('Arial'); g.set_font_size(10);
		g.move_to(8, h / 2); g.show_text('RECORD A PIECE, OR DROP AN AUDIO FILE HERE'); return;
	}
	// bar lines
	col(g, COL.bar);
	for (i = 1; i < bars; i++) { x = Math.round(w * i / bars) + 0.5; g.move_to(x, top); g.line_to(x, top + wh); }
	g.set_line_width(1); g.stroke();
	// waveform
	col(g, COL.wave);
	var n = wave.length, mid = top + wh / 2;
	for (i = 0; i < n; i++) {
		var v = Math.min(1, wave[i]) * wh * 0.48;
		x = w * (i + 0.5) / n;
		g.move_to(x, mid - v); g.line_to(x, mid + v);
	}
	g.set_line_width(Math.max(1, w / n * 0.8)); g.stroke();
	// stickers
	col(g, COL.stk);
	for (i = 0; i < stk.length; i++) { x = Math.round(stk[i] * w) + 0.5; g.move_to(x, top); g.line_to(x, top + 5); }
	g.set_line_width(1); g.stroke();
	if (ks[0] >= 0) { col(g, COL.k); x = ks[0] * w; g.rectangle(x - 3, top, 6, 6); g.fill(); }
	if (ks[1] >= 0) { col(g, COL.s); x = ks[1] * w; g.rectangle(x - 3, top, 6, 6); g.fill(); }
	// needles
	g.set_line_width(2);
	g.set_source_rgba(COL.a[0], COL.a[1], COL.a[2], 0.35 + 0.65 * Math.min(1, ga));
	x = pa * w; g.move_to(x, top); g.line_to(x, top + wh); g.stroke();
	g.set_source_rgba(COL.b[0], COL.b[1], COL.b[2], 0.35 + 0.65 * Math.min(1, gb));
	x = pb * w; g.move_to(x, top); g.line_to(x, top + wh); g.stroke();
	// fader bar: which deck is open
	var fy = h - 12;
	col(g, COL.bar); g.rectangle(0, fy, w, 10); g.fill();
	col(g, COL.a); g.rectangle(0, fy, w / 2 * Math.min(1, ga), 10); g.fill();
	col(g, COL.b); g.rectangle(w - w / 2 * Math.min(1, gb), fy, w / 2 * Math.min(1, gb), 10); g.fill();
	col(g, COL.text); g.select_font_face('Arial'); g.set_font_size(9);
	g.move_to(4, fy + 8.5); g.show_text('A'); g.move_to(w - 10, fy + 8.5); g.show_text('B');
	if (lateFlash > 0) { col(g, COL.late); g.move_to(w / 2 - 12, fy + 8.5); g.show_text('LATE'); }
}

function onclick(x, y, but, cmd, shift) {
	var w = box.rect[2] - box.rect[0];
	if (!have) return;
	outlet(0, 'click', Math.max(0, Math.min(1, x / w)), shift ? 1 : 0);
}
