#!/usr/bin/env python3
# docs/MANUAL.md -> docs/Essphor-Manual.pdf   (pip install markdown; needs node + playwright)
import os, subprocess, markdown
HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, "..", "docs")
md = open(os.path.join(DOCS, "MANUAL.md")).read()
body = markdown.markdown(md, extensions=["tables","fenced_code"])
css = """body{font-family:Inter,Helvetica,Arial,sans-serif;font-size:10.5pt;line-height:1.45;color:#1b1b1b;margin:18mm 16mm}
h1{font-size:24pt;margin:0 0 4mm}h2{font-size:15pt;color:#B07A16;border-bottom:1px solid #ccc;padding-bottom:1mm;margin-top:8mm;page-break-after:avoid}
h3{font-size:12pt;margin-top:5mm;page-break-after:avoid}table{border-collapse:collapse;width:100%;margin:2mm 0;font-size:9.5pt}
td,th{border:1px solid #ddd;padding:1.2mm 2mm;vertical-align:top}th{background:#f3ead6;text-align:left}tr{page-break-inside:avoid}
img{width:100%;display:block;margin:4mm auto}code{background:#f3ead6;padding:0 1mm}pre{background:#f3ead6;padding:3mm;font-size:8.5pt;line-height:1.25}pre code{padding:0}"""
html = f"<html><head><meta charset='utf-8'><style>{css}</style></head><body>{body}</body></html>"
p = os.path.join(DOCS, "manual.html")
open(p, "w").write(html)
subprocess.run(["node", os.path.join(HERE, "pdf.js"), os.path.abspath(p), os.path.abspath(os.path.join(DOCS, "Essphor-Manual.pdf"))], check=True)
os.remove(p)
print("ok")
