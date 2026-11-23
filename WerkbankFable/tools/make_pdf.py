#!/usr/bin/env python3
# docs/MANUAL.md (+ the dictionary from tools/proto/words.txt) -> docs/Werkbank-Manual.pdf
#   pip install markdown; needs node + playwright
import os, subprocess, markdown
HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, "..", "docs")
md = open(os.path.join(DOCS, "MANUAL.md"), encoding="utf-8").read()

# appendix: the whole dictionary
names = {"en": "English", "de": "German", "fr": "French", "es": "Spanish", "it": "Italian", "ja": "Japanese", "ru": "Russian"}
words = {}
for line in open(os.path.join(HERE, "proto", "words.txt"), encoding="utf-8"):
    line = line.strip()
    if not line or line.startswith("#"): continue
    p = line.split()
    words.setdefault(p[0], []).append((p[1], " ".join(p[2:])))
md += "\n\n## Appendix: the SPRECHER dictionary\n\nEvery word SPRECHER knows by heart, with its phonemes. `'` marks the stressed vowel.\n"
for lang in ["en", "de", "fr", "es", "it", "ja", "ru"]:
    ws = words.get(lang, [])
    md += "\n### %s (%d)\n\n| Word | Phonemes | Word | Phonemes | Word | Phonemes |\n|---|---|---|---|---|---|\n" % (names[lang], len(ws))
    rows = (len(ws) + 2) // 3
    for r in range(rows):
        cells = []
        for c in range(3):
            i = r + c * rows
            cells += [ws[i][0], ws[i][1]] if i < len(ws) else ["", ""]
        md += "| " + " | ".join(cells) + " |\n"

body = markdown.markdown(md, extensions=["tables"])
css = """body{font-family:Inter,Helvetica,Arial,sans-serif;font-size:10.5pt;line-height:1.45;color:#1b1b1b;margin:18mm 16mm}
h1{font-size:24pt;margin:0 0 4mm}h2{font-size:15pt;color:#C2392A;border-bottom:1px solid #ccc;padding-bottom:1mm;margin-top:8mm;page-break-after:avoid}
h2{page-break-before:auto}h3{font-size:12pt;margin-top:5mm;page-break-after:avoid}table{border-collapse:collapse;width:100%;margin:2mm 0;font-size:9.5pt}
td,th{border:1px solid #ddd;padding:1.2mm 2mm;vertical-align:top}th{background:#f4e1dc;text-align:left}tr{page-break-inside:avoid}
img{max-height:92mm;max-width:100%;display:block;margin:4mm auto}code{background:#f4e1dc;padding:0 1mm}"""
html = f"<html><head><meta charset='utf-8'><style>{css}</style></head><body>{body}</body></html>"
p = os.path.join(DOCS, "manual.html")
open(p, "w", encoding="utf-8").write(html)
subprocess.run(["node", os.path.join(HERE, "pdf.js"), os.path.abspath(p), os.path.abspath(os.path.join(DOCS, "Werkbank-Manual.pdf"))], check=True)
os.remove(p)
print("ok")
