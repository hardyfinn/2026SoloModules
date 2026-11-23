// DJ Claude Skratch: the record strip (v8ui). Shows the record, its cue points, the current
// cue, and the needle (bright while the fader is open).
// Click a cue mark to choose it, drag it to move it. Double-click to add a cue point there,
// double-click a mark to remove it. Up to 8.

mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
inlets = 1;
outlets = 1;

var STAT = 'skr_stat';
var bSt = null;
var wave = [], cues = [], sel = 0, len = 1, have = false;
var pos = 0, g = 0, drag = -1;
var COL = {
	bg: [0.11, 0.11, 0.12, 1], wave: [0.55, 0.57, 0.6, 1], mark: [0.85, 0.85, 0.85, 1],
	sel: [1.0, 0.62, 0.15, 1], needle: [1.0, 0.62, 0.15, 1], text: [0.75, 0.75, 0.78, 1], dark: [0.1, 0.1, 0.1, 1]
};

var t = new Task(function () {
	if (!bSt) { try { bSt = new Buffer(STAT); } catch (e) { return; } }
	var st = bSt.peek(1, 4, 3);
	pos = st[0]; g = st[2];
	mgraphics.redraw();
}, this);
t.interval = 33;
t.repeat();

function col(gr, c, a) { gr.set_source_rgba(c[0], c[1], c[2], a === undefined ? c[3] : a); }
function anything() {
	var a = arrayfromargs(arguments);
	switch (messagename) {
	case 'statname': STAT = String(a[0]); bSt = null; break;
	case 'wave': wave = a; have = true; break;
	case 'cues': cues = (a.length === 1 && a[0] < 0) ? [] : a; break;
	case 'sel': sel = a[0]; break;
	case 'len': len = a[0] > 0 ? a[0] : 1; break;
	case 'clear': have = false; wave = []; cues = []; break;
	}
	mgraphics.redraw();
}

function size() { return [box.rect[2] - box.rect[0], box.rect[3] - box.rect[1]]; }

function paint() {
	var wh = size(), w = wh[0], h = wh[1], gr = mgraphics, i, x;
	var top = 12, ht = h - top - 2, mid = top + ht / 2;
	col(gr, COL.bg); gr.rectangle(0, 0, w, h); gr.fill();
	if (!have) {
		col(gr, COL.text); gr.select_font_face('Arial'); gr.set_font_size(10);
		gr.move_to(8, h / 2); gr.show_text('DROP A SOUND BELOW'); return;
	}
	col(gr, COL.wave);
	var n = wave.length;
	for (i = 0; i < n; i++) {
		var v = Math.min(1, wave[i]) * ht * 0.48;
		x = w * (i + 0.5) / n;
		gr.move_to(x, mid - v); gr.line_to(x, mid + v);
	}
	gr.set_line_width(Math.max(1, w / n * 0.8)); gr.stroke();
	// cue marks, numbered
	gr.select_font_face('Arial Bold'); gr.set_font_size(9);
	for (i = 0; i < cues.length; i++) {
		x = Math.round(cues[i] * w) + 0.5;
		var c = i === sel ? COL.sel : COL.mark;
		col(gr, c, 0.55); gr.move_to(x, top); gr.line_to(x, h - 2); gr.set_line_width(1); gr.stroke();
		col(gr, c); gr.rectangle(x - 0.5, 0, 11, 11); gr.fill();
		col(gr, COL.dark); gr.move_to(x + 2.5, 9); gr.show_text(String(i + 1));
	}
	// needle
	x = Math.max(0, Math.min(1, pos / len)) * w;
	col(gr, COL.needle, 0.3 + 0.7 * Math.min(1, g));
	gr.set_line_width(g > 0.5 ? 2.5 : 1.5);
	gr.move_to(x, top); gr.line_to(x, h - 2); gr.stroke();
}

function nearest(x) {
	var w = size()[0], best = -1, bd = 7;
	for (var i = 0; i < cues.length; i++) { var d = Math.abs(cues[i] * w - x); if (d < bd) { bd = d; best = i; } }
	return best;
}
function onclick(x, y, but, cmd, shift) {
	if (!have) return;
	var i = nearest(x);
	drag = i;
	if (i >= 0) { sel = i; outlet(0, 'pick', i, 0); mgraphics.redraw(); }
}
function ondrag(x, y, but) {
	if (!have || drag < 0) return;
	var w = size()[0], f = Math.max(0, Math.min(1, x / w));
	if (but) { cues[drag] = f; outlet(0, 'move', drag, f); mgraphics.redraw(); }
	else { outlet(0, 'moved', drag, f); drag = -1; }
}
function ondblclick(x, y) {
	if (!have) return;
	var i = nearest(x), w = size()[0];
	if (i >= 0) outlet(0, 'remove', i, 0);
	else outlet(0, 'add', -1, Math.max(0, Math.min(1, x / w)));
}
