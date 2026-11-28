import markdown, sys

md=open(sys.argv[1]).read()
html=markdown.markdown(md, extensions=['tables'])
css="""body{font-family:-apple-system,Helvetica,Arial,sans-serif;font-size:9.5pt;color:#1d1d1f;margin:0}
h1{font-size:20pt;margin:0 0 6px}h2{font-size:13pt;margin:18px 0 6px;border-bottom:2px solid #c0392b;padding-bottom:3px;break-after:avoid}
table{border-collapse:collapse;width:100%;margin-bottom:8px}th,td{border-bottom:1px solid #ddd;padding:4px 6px;text-align:left;vertical-align:top}
th{background:#f2f0ec;font-size:8.5pt}td:nth-child(4),th:nth-child(4){text-align:center;white-space:nowrap}tr{break-inside:avoid}
a{color:#1d1d1f}code{font-size:8.5pt;background:#f4f4f4;padding:0 2px}hr{border:0}"""
doc=f"<html><head><meta charset='utf-8'><style>{css}</style></head><body>{html}</body></html>"
open(sys.argv[2],'w').write(doc)
