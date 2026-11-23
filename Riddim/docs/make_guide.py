#!/usr/bin/env python3
# Riddim guide PDF.  pip install reportlab ; python3 docs/make_guide.py
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, PageBreak
HERE = os.path.dirname(os.path.abspath(__file__))
P = {}
exec(open(os.path.join(HERE, "..", "tools", "make_patterns.py")).read().split("assert len(R)")[0], P)
KITN = ["CASIO", "ROLAND 808", "ROLAND PCM", "LINN / OBERHEIM", "SIMMONS", "SAMPLER", "WORKSTATION", "LIVE"]
ss = getSampleStyleSheet()
H1 = ParagraphStyle("h1", parent=ss["Title"], fontSize=24, textColor=colors.HexColor("#16171A"), alignment=0)
H2 = ParagraphStyle("h2", parent=ss["Heading2"], textColor=colors.HexColor("#B07F00"))
B = ParagraphStyle("b", parent=ss["BodyText"], fontSize=10, leading=14)
S = ParagraphStyle("s", parent=B, fontSize=8, leading=10)
doc = SimpleDocTemplate(os.path.join(HERE, "Riddim-Guide.pdf"), pagesize=A4, leftMargin=18*mm, rightMargin=18*mm, topMargin=16*mm, bottomMargin=16*mm,
                        title="Riddim guide", author="Haer")
f = []
f.append(Paragraph("Riddim", H1))
f.append(Paragraph("Dancehall riddim drum machine for VCV Rack. 30 HP, ten synthesised voices, 24 riddims, each with its own machine flavour.", B))
prev = os.path.join(HERE, "panel-preview.png")
if os.path.exists(prev):
    f.append(Spacer(1, 4*mm)); f.append(Image(prev, width=120*mm, height=120*mm * 1028 / 1219))
f.append(Paragraph("Playing it", H2))
for t in [
    "Send a <b>16th-note clock</b> to CLOCK (one pulse per step). RESET goes back to step 1 of bar 1.",
    "<b>RIDDIM</b> picks 1-24; the RIDDIM jack takes 0-10 V across the list. <b>KIT</b> 0 = the riddim's own machine; 1-8 forces CASIO, ROLAND 808, ROLAND PCM, LINN, SIMMONS, SAMPLER, WORKSTATION, LIVE. Try Dem Bow on the Simmons kit.",
    "<b>VAR</b>: A is the main groove, B adds the fill-ins and extra hits, AUTO plays B on every fourth bar. A gate on VAR B forces B.",
    "<b>FILL</b> (button or jack) plays one of four fills on the last beat of the bar. Right-click: auto fill every 2, 4 or 8 bars.",
    "<b>SWING</b> pushes the odd 16ths late (up to 60% of a step). <b>MUTATE</b> adds ghost notes and drops soft hits, fresh each step. <b>CRUSH</b> lowers bit depth and sample rate on top of the kit's own, <b>DRIVE</b> adds saturation.",
    "<b>TUNE</b> +-12 semitones, <b>DECAY</b> 0.5-2x, <b>TONE</b> brightens the noise in the snare, clap and hats, <b>LEVEL</b> sets the mix.",
    "Each voice has a level knob (left) and its own output (right). Patching an output takes that voice out of the MIX L / MIX R pair, so you can send the kick to a compressor on its own.",
    "The screen shows the riddim, year, producer, machine (red with ? where unconfirmed), and the 10 x 16 grid with the playhead. Dots at the left of the grid flash when a voice fires.",
]:
    f.append(Paragraph("&bull; " + t, B))
f.append(PageBreak())
f.append(Paragraph("The 24 riddims", H2))
f.append(Paragraph("Machine column: plain = a source names it; <b>?</b> = no source found, so it is the gear the era or producer typically used, shown as a flavour, not a claim. Patterns are by-ear reconstructions of each riddim's feel, not transcriptions. Years starting c. are approximate.", S))
rows = [["#", "Riddim", "Year", "Producer", "Machine / kit flavour"]]
for i, (name, year, prod, mach, kit, conf, A, Bd) in enumerate(P["R"]):
    rows.append([str(i + 1), name.title().replace("Mi", "Mi"), year, prod.title(), f"{mach.title()}  ({KITN[kit]})"])
t = Table(rows, colWidths=[8*mm, 48*mm, 22*mm, 40*mm, 52*mm], repeatRows=1)
t.setStyle(TableStyle([("FONTSIZE", (0, 0), (-1, -1), 8), ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#F2B731")),
                       ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F4F1E6")]),
                       ("VALIGN", (0, 0), (-1, -1), "TOP"), ("GRID", (0, 0), (-1, -1), 0.25, colors.HexColor("#BBB"))]))
f.append(t)
f.append(Spacer(1, 4*mm))
f.append(Paragraph("About the machines", H2))
for tx in [
    "<b>Under Mi Sleng Teng</b> (1985) was written on a preset of the Casio Casiotone MT-40 (Wikipedia). The Casio kit here is thin, 8-bit and crunchy to suit.",
    "<b>Rumours</b> is the riddim behind J.C. Lodge's Telephone Love (Gussie Clarke, Music Works, 1988). A 2015 rebuild of it used an Oberheim DX drum machine, Yamaha DX100 and CS01; no source I found names the gear on the original, so the Linn/Oberheim kit is a guess.",
    "For the Dave Kelly (Madhouse) and Lenky riddims no page I read names a drum machine, so they carry the workstation and sampler flavours as flavours only. If you know what they used, the label and kit are one line each in tools/make_patterns.py.",
    "Real Rock and Stalag 17 were played by bands. They are here as reference grooves in the live kit.",
]:
    f.append(Paragraph("&bull; " + tx, B))
f.append(Paragraph("Jacks", H2))
f.append(Paragraph("In: CLOCK (16ths), RESET, FILL (trigger), VAR B (gate), RIDDIM (0-10 V). Out: MIX L, MIX R, and ten voice outs (KICK, SNARE, RIM, CLAP, TOM, C HAT, O HAT, COW, PERC, SHAKE). A closed hat chokes the open hat. TOM steps carry three pitches (low, mid, high); PERC is a conga-like drum with ghost, normal and accent hits.", B))
doc.build(f)
print("wrote Riddim-Guide.pdf")
