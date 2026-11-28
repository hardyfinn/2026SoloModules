#!/usr/bin/env python3
"""Builds the HTML user guide (XFORMER-Guide.html) from GUIDE.md and the device tables in build.py.

The text comes from GUIDE.md, so the two guides never disagree. The control reference for each
device (every knob, menu and button, with its range and starting value) is generated from the same
parameter tables that build the devices, so it always matches what you see in Live. What each
control does comes from control_notes.py.

    python3 m4l/make_html_guide.py <out.html>
"""
import html, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build  # noqa: E402  (only its tables are used)
import control_notes  # noqa: E402

ACCENT = {"algo": "#e0702a", "stochastic": "#c94f8a", "note": "#3f8fd0", "indexed": "#2fa58a",
          "discrete": "#8a6ad6", "phaseflux": "#d0a52a", "fractal": "#d4503f"}
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def esc(s):
    return html.escape(s, quote=False)


def inline(s):
    """Markdown inline marks: `code`, **bold**, *italic*, [text](link)."""
    parts = re.split(r"(`[^`]+`)", s)
    out = []
    for p in parts:
        if p.startswith("`") and p.endswith("`") and len(p) > 1:
            out.append("<code>%s</code>" % esc(p[1:-1]))
            continue
        p = esc(p)
        p = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", p)
        p = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<em>\1</em>", p)
        p = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', p)
        out.append(p)
    return "".join(out)


def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")


def md_blocks(lines):
    """Converts a run of Markdown lines (no headings) to HTML: lists, tables, code, paragraphs."""
    out, i = [], 0
    while i < len(lines):
        ln = lines[i]
        if not ln.strip() or ln.strip() == "---":
            i += 1
        elif ln.startswith("```"):
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            out.append('<pre class="flow">%s</pre>' % esc("\n".join(lines[i + 1:j])))
            i = j + 1
        elif ln.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            head, body = rows[0], [r for r in rows[1:] if not set("".join(r)) <= set("-: ")]
            t = ['<div class="tablewrap"><table><thead><tr>']
            t += ["<th>%s</th>" % inline(c) for c in head] + ["</tr></thead><tbody>"]
            for r in body:
                t += ["<tr>"] + ["<td>%s</td>" % inline(c) for c in r] + ["</tr>"]
            out.append("".join(t) + "</tbody></table></div>")
        elif re.match(r"\s*([-*]|\d+\.) ", ln):
            j = i
            while j < len(lines) and (re.match(r"\s*([-*]|\d+\.) ", lines[j]) or
                                      (lines[j].startswith("   ") and lines[j].strip())):
                j += 1
            out.append(md_list(lines[i:j]))
            i = j
        else:
            j = i
            while j < len(lines) and lines[j].strip() and not re.match(r"\s*([-*]|\d+\.) |\||```", lines[j]):
                j += 1
            out.append("<p>%s</p>" % inline(" ".join(l.strip() for l in lines[i:j])))
            i = j
    return "\n".join(out)


def md_list(lines):
    """Nested list by indentation (2 or 3 spaces per level)."""
    items = []  # (indent, ordered, text)
    for ln in lines:
        m = re.match(r"(\s*)([-*]|\d+\.) (.*)", ln)
        if m:
            items.append([len(m.group(1)), m.group(2)[0].isdigit(), m.group(3)])
        elif items:
            items[-1][2] += " " + ln.strip()

    def render(k, indent):
        tag = "ol" if items[k][1] else "ul"
        h = ["<%s>" % tag]
        while k < len(items) and items[k][0] >= indent:
            if items[k][0] > indent:
                sub, k = render(k, items[k][0])
                h[-1] = h[-1][:-5] + sub + "</li>"
                continue
            h.append("<li>%s</li>" % inline(items[k][2]))
            k += 1
        return "".join(h) + "</%s>" % tag, k

    return render(0, items[0][0])[0]


# ---------------------------------------------------------------------------
#  Control reference, generated from build.DEVICES
# ---------------------------------------------------------------------------
def ctl_row(ctl):
    kind = ctl[0]
    if kind == "dial":
        _, _, label, lo, hi, d, unit = ctl
        u = {"%": " %", "st": " st", None: ""}.get(unit, " " + str(unit))
        return label, "Knob", "%s to %s%s" % (lo, hi, u), "%s%s" % (d, u)
    if kind in ("menu", "tab"):
        _, _, label, items, d = ctl
        if len(items) <= 6:
            rng = " · ".join(items)
        else:
            rng = "%s … %s (%d choices)" % (items[0], items[-1], len(items))
        return label, "Menu" if kind == "menu" else "Option buttons", rng, items[d]
    if kind == "numb":
        _, _, label, lo, hi, d, style = ctl
        if style == 8:
            return label, "Note box", "MIDI note", "%s%d (%d)" % (NOTE_NAMES[d % 12], d // 12 - 2, d)
        return label, "Number", "%s to %s" % (lo, hi), str(d)
    if kind == "tog":
        return ctl[2], "Toggle", "Off · On", "On" if ctl[3] else "Off"
    if kind == "btn":
        return ctl[2], "Button", "press", "—"
    return str(ctl), "", "", ""


def ctl_table(key, title, ctls):
    if not ctls:
        return ""
    rows = []
    for c in ctls:
        label, kind, rng, start = ctl_row(c)
        what = control_notes.describe(key, c[1])
        if not what:
            print("warning: no description for", key, c[1])
        rows.append('<tr><td><strong>%s</strong><small>%s</small></td><td>%s</td><td>%s</td><td>%s</td></tr>' %
                    (esc(label), esc(kind), esc(rng), esc(start), esc(what)))
    return ('<h4>%s</h4><div class="tablewrap"><table class="ref"><thead><tr><th>Control</th><th>Range</th>'
            '<th>Starts at</th><th>What it does to the notes</th></tr></thead><tbody>%s</tbody></table></div>'
            % (title, "".join(rows)))


def controls(key):
    cfg = build.DEVICES[key]
    main = cfg["main_dials"] + cfg["main_menus"] + cfg["main_buttons"]
    ed = cfg["editor_dials"] + cfg["editor_tabs"] + cfg["editor_buttons"]
    h = ['<details class="controls" open><summary>Every control of %s, and what it does</summary>' % esc(cfg["title"])]
    h.append(ctl_table(key, "On the device", main))
    h.append(ctl_table(key, "In the editor window", ed))
    if key in control_notes.LAYERS:
        h.append('<h4>Editor grid: what each step value does</h4><div class="tablewrap"><table class="ref layers">'
                 '<thead><tr><th>Layer</th><th>What it does to the notes</th></tr></thead><tbody>%s</tbody></table></div>' %
                 "".join("<tr><td><strong>%s</strong></td><td>%s</td></tr>" % (esc(n), esc(t))
                         for n, t in control_notes.LAYERS[key]))
    h.append("</details>")
    return "".join(h)


# ---------------------------------------------------------------------------
#  Page
# ---------------------------------------------------------------------------
CSS = """
:root{--bg:#f6f5f2;--panel:#ffffff;--ink:#1d1d1f;--muted:#5b5b62;--line:#e2e0da;--code:#efede7;
--accent:#e0702a;--flow:#1f2126;--flowink:#e8e6e1;--shadow:0 1px 2px rgba(0,0,0,.05),0 4px 14px rgba(0,0,0,.04)}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#141518;--panel:#1c1d21;--ink:#ecebe7;
--muted:#a3a2a8;--line:#2e3036;--code:#26282d;--flow:#0e0f11;--flowink:#e8e6e1;--shadow:none}}
:root[data-theme="dark"]{--bg:#141518;--panel:#1c1d21;--ink:#ecebe7;--muted:#a3a2a8;--line:#2e3036;--code:#26282d;
--flow:#0e0f11;--flowink:#e8e6e1;--shadow:none}
*{box-sizing:border-box}
html{scroll-behavior:smooth;scroll-padding-top:16px}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.6 "Inter",system-ui,-apple-system,"Segoe UI",sans-serif}
a{color:var(--accent)}
.layout{display:grid;grid-template-columns:250px minmax(0,1fr);max-width:1180px;margin:0 auto;gap:40px;padding:0 24px}
nav{position:sticky;top:0;align-self:start;max-height:100vh;overflow:auto;padding:32px 0}
nav .brand{font-weight:800;letter-spacing:.04em;font-size:15px;margin-bottom:18px}
nav .brand span{color:var(--accent)}
nav a{display:block;color:var(--muted);text-decoration:none;padding:4px 10px;border-radius:6px;font-size:14px}
nav a:hover{background:var(--panel);color:var(--ink)}
nav a.dev{padding-left:22px;position:relative}
nav a.dev::before{content:"";position:absolute;left:10px;top:12px;width:6px;height:6px;border-radius:50%;background:var(--c)}
main{padding:32px 0 80px;min-width:0}
header.hero{padding:28px 0 8px}
header.hero h1{font-size:40px;line-height:1.1;margin:0 0 12px;letter-spacing:-.02em}
header.hero h1 span{color:var(--accent)}
header.hero p{color:var(--muted);font-size:18px;max-width:46em;margin:0}
h2{font-size:26px;margin:56px 0 14px;padding-top:8px;border-top:1px solid var(--line);letter-spacing:-.01em}
h3{font-size:20px;margin:0 0 10px}
h4{font-size:13px;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin:22px 0 8px}
p,li{max-width:48em}
li{margin:3px 0}
ul ul,ol ul{margin:4px 0}
code{background:var(--code);padding:1px 5px;border-radius:4px;font:.88em ui-monospace,"SF Mono",Menlo,Consolas,monospace}
.tablewrap{overflow-x:auto;margin:12px 0;border:1px solid var(--line);border-radius:10px;background:var(--panel)}
table{border-collapse:collapse;width:100%;font-size:14.5px}
th,td{text-align:left;padding:8px 12px;border-bottom:1px solid var(--line);vertical-align:top}
th{font-size:12px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);font-weight:600}
tr:last-child td{border-bottom:0}
table.ref td small{display:block;color:var(--muted);font-size:12px;font-weight:400}
table.ref td:first-child{min-width:120px}
table.ref td:nth-child(2){min-width:120px;color:var(--muted)}
table.ref td:nth-child(3){white-space:nowrap}
table.ref td:nth-child(4){min-width:260px}
table.layers td:nth-child(2){min-width:0;color:var(--ink)}
.cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:12px;margin:22px 0}
.cards a{display:block;background:var(--panel);border:1px solid var(--line);border-top:4px solid var(--c);
border-radius:10px;padding:14px 16px;text-decoration:none;color:var(--ink);box-shadow:var(--shadow)}
.cards a b{display:block;font-size:16px;margin-bottom:4px}
.cards a span{color:var(--muted);font-size:14px;line-height:1.45;display:block}
.device{background:var(--panel);border:1px solid var(--line);border-left:5px solid var(--c);border-radius:12px;
padding:22px 26px;margin:18px 0;box-shadow:var(--shadow)}
.device h3{display:flex;align-items:center;gap:10px}
.device h3::before{content:"";width:12px;height:12px;border-radius:3px;background:var(--c)}
details.controls{margin-top:16px;border-top:1px solid var(--line);padding-top:12px}
details.controls summary{cursor:pointer;font-weight:600;color:var(--c)}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chips span{background:var(--code);border-radius:999px;padding:2px 10px;font-size:13px}
pre.flow{background:var(--flow);color:var(--flowink);padding:18px 20px;border-radius:10px;overflow-x:auto;
font:12.5px/1.45 ui-monospace,"SF Mono",Menlo,Consolas,monospace}
.callout{background:var(--panel);border:1px solid var(--line);border-left:4px solid var(--accent);border-radius:8px;padding:12px 16px;margin:14px 0}
footer{color:var(--muted);font-size:13px;margin-top:60px}
.theme{position:fixed;right:16px;bottom:16px;background:var(--panel);border:1px solid var(--line);color:var(--ink);
border-radius:999px;padding:6px 12px;font-size:13px;cursor:pointer}
@media (max-width:860px){.layout{display:block;padding:0 16px}nav{position:static;max-height:none;padding:20px 0 0;
display:flex;flex-wrap:wrap;gap:4px}nav .brand{width:100%;margin-bottom:6px}nav a,nav a.dev{padding:4px 10px;
background:var(--panel);border:1px solid var(--line)}nav a.dev::before{display:none}header.hero h1{font-size:30px}
.device{padding:16px}}
@media (max-width:640px){table.ref thead{display:none}table.ref,table.ref tbody,table.ref tr,table.ref td{display:block;width:auto;min-width:0!important}
table.ref tr{padding:10px 12px;border-bottom:1px solid var(--line)}table.ref tr:last-child{border-bottom:0}
table.ref td{border:0;padding:0}table.ref td:nth-child(2),table.ref td:nth-child(3){display:inline;font-size:13px;color:var(--muted)}
table.ref:not(.layers) td:nth-child(3)::before{content:" · starts at "}table.ref td:last-child{margin-top:4px;color:var(--ink)}
table.ref td small{display:inline;margin-left:6px}}
@media print{nav,.theme{display:none}.layout{display:block}details.controls{display:block}.device{break-inside:avoid-page}}
"""


def main(out):
    md = open(os.path.join(HERE, "GUIDE.md")).read().split("\n")
    by_title = {cfg["title"]: key for key, cfg in build.DEVICES.items()}
    # split into: intro, then (level, title, lines) sections
    sections, cur = [], [0, "", []]
    for ln in md:
        m = re.match(r"(#{1,3}) (.*)", ln)
        if m:
            sections.append(cur)
            cur = [len(m.group(1)), m.group(2).strip(), []]
        else:
            cur[2].append(ln)
    sections.append(cur)

    title = sections[1][1]
    intro = sections[1][2]
    # the device table becomes cards
    table = [l for l in intro if l.startswith("|")][2:]
    text_intro = [l for l in intro if not l.startswith("|")]
    cards = []
    for row in table:
        name, engine, what = [c.strip() for c in row.strip().strip("|").split("|")]
        name = name.strip("*")
        key = by_title.get(name, "")
        cards.append('<a href="#%s" style="--c:%s"><b>%s</b><span>%s</span></a>' %
                     (slug(name), ACCENT.get(key, "var(--accent)"), esc(name), inline(what)))
    paras = md_blocks(text_intro).split("\n")

    nav, body = [], []
    for level, head, lines in sections[2:]:
        if level == 2:
            if body and body[-1] == "</div><!--dev-->":
                pass
            nav.append('<a href="#%s">%s</a>' % (slug(head), esc(head)))
            body.append('<h2 id="%s">%s</h2>' % (slug(head), esc(head)))
            body.append(md_blocks(lines))
        else:
            key = by_title.get(head)
            if key:
                c = ACCENT[key]
                nav.append('<a class="dev" style="--c:%s" href="#%s">%s</a>' % (c, slug(head), esc(head)))
                body.append('<section class="device" id="%s" style="--c:%s"><h3>%s</h3>%s%s</section>' %
                            (slug(head), c, esc(head), md_blocks(lines), controls(key)))
            else:
                body.append('<section class="device" style="--c:var(--accent)"><h3 id="%s">%s</h3>%s</section>' %
                            (slug(head), esc(head), md_blocks(lines)))

    page = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>XFORMER Devices Guide</title>
<style>%s</style></head><body>
<div class="layout">
<nav><div class="brand">X<span>|</span>FORMER for Live</div><a href="#top">Overview</a>%s</nav>
<main id="top">
<header class="hero"><h1>X<span>|</span>FORMER sequencers for Ableton Live</h1>%s</header>
<div class="cards">%s</div>
%s
%s
<footer>Seven Max for Live MIDI effects porting the X|FORMER firmware sequencer engines. Live 12 Suite, Max 9.</footer>
</main></div>
<button class="theme" type="button" onclick="toggleTheme()">Light / dark</button>
<script>
function toggleTheme(){var r=document.documentElement,d=r.getAttribute('data-theme');
var dark=d?d==='dark':matchMedia('(prefers-color-scheme: dark)').matches;r.setAttribute('data-theme',dark?'light':'dark');
try{localStorage.setItem('xf-theme',r.getAttribute('data-theme'))}catch(e){}}
try{var t=localStorage.getItem('xf-theme');if(t)document.documentElement.setAttribute('data-theme',t)}catch(e){}
window.addEventListener('beforeprint',function(){document.querySelectorAll('details').forEach(function(d){d.open=true})});
</script>
</body></html>
""" % (CSS, "".join(nav), paras[0], "".join(cards), "\n".join(paras[1:]), "\n".join(body))
    del title
    with open(out, "w") as f:
        f.write(page)
    print("wrote", out, len(page), "bytes")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "XFORMER-Guide.html"))
