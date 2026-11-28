# Generates inline SVG diagrams for the Reich PDF
INK="#1d1d1f"; MUTED="#6b6b70"; LINE="#d4d2cc"; ACC="#c2410c"; ACC2="#1e5a8a"; SOFT="#f3ede4"

CLAP=[1,1,1,0,1,1,0,1,0,1,1,0]

def clapping():
    cell=12; gap=2; x0=104; y0=30
    rows=13
    w=x0+12*(cell+gap)+190; h=y0+(rows+1)*(cell+gap)+40
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    s.append(f'<text x="0" y="12" fill="{MUTED}" font-size="9.5">Each row is one section. Player 1 never changes. Player 2 rotates one step left per section.</text>')
    for i in range(12):
        s.append(f'<text x="{x0+i*(cell+gap)+cell/2}" y="{y0-6}" text-anchor="middle" fill="{MUTED}" font-size="8">{i+1}</text>')
    # player 1 row
    def row(y,pat,label,col,other=None):
        s.append(f'<text x="{x0-8}" y="{y+cell-4}" text-anchor="end" fill="{INK}" font-size="9">{label}</text>')
        for i,v in enumerate(pat):
            x=x0+i*(cell+gap)
            if v:
                both = other is not None and other[i]
                fill = ACC if (other is not None and both) else col
                s.append(f'<rect x="{x}" y="{y}" width="{cell}" height="{cell}" rx="3" fill="{fill}"/>')
            else:
                s.append(f'<rect x="{x+0.5}" y="{y+0.5}" width="{cell-1}" height="{cell-1}" rx="3" fill="none" stroke="{LINE}"/>')
    row(y0,CLAP,"Player 1 (fixed)",INK)
    for k in range(rows):
        pat=CLAP[k:]+CLAP[:k]
        y=y0+(k+1)*(cell+gap)+6
        lab = "Section 1 (unison)" if k==0 else ("Section 13 (unison)" if False else f"Section {k+1}")
        row(y,pat,lab,ACC2,CLAP)
        coinc=sum(a and b for a,b in zip(CLAP,pat))
        s.append(f'<text x="{x0+12*(cell+gap)+8}" y="{y+cell-4}" fill="{MUTED}" font-size="9">{coinc} shared claps</text>')
    yl=y0+(rows+1)*(cell+gap)+22
    s.append(f'<rect x="{x0}" y="{yl-9}" width="10" height="10" rx="2" fill="{ACC}"/><text x="{x0+14}" y="{yl}" fill="{INK}" font-size="9">both clap together</text>')
    s.append(f'<rect x="{x0+120}" y="{yl-9}" width="10" height="10" rx="2" fill="{ACC2}"/><text x="{x0+134}" y="{yl}" fill="{INK}" font-size="9">player 2 alone</text>')
    s.append(f'<text x="{x0+230}" y="{yl}" fill="{MUTED}" font-size="9">Section 13 returns to unison.</text>')
    s.append('</svg>'); return "".join(s)

def drift():
    # gradual phasing: two loops, second slightly faster
    w=640; h=120; x0=70; L=w-x0-10
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    n=48
    for lane,(lab,period,col) in enumerate([("Tape / loop A",1.0,INK),("Tape / loop B",0.97,ACC2)]):
        y=30+lane*36
        s.append(f'<text x="{x0-8}" y="{y+4}" text-anchor="end" fill="{INK}" font-size="9.5">{lab}</text>')
        s.append(f'<line x1="{x0}" y1="{y}" x2="{x0+L}" y2="{y}" stroke="{LINE}"/>')
        t=0;i=0
        while t<n:
            x=x0+t/n*L
            big = (i%12==0)
            s.append(f'<line x1="{x:.1f}" y1="{y-(9 if big else 5)}" x2="{x:.1f}" y2="{y+(9 if big else 5)}" stroke="{col}" stroke-width="{2 if big else 1.2}"/>')
            t+=period; i+=1
    s.append(f'<text x="{x0}" y="{h-8}" fill="{MUTED}" font-size="9">Start: in unison</text>')
    s.append(f'<text x="{x0+L}" y="{h-8}" text-anchor="end" fill="{MUTED}" font-size="9">long marks = start of each 12-pulse loop</text>')
    s.append('</svg>'); return "".join(s)

def construction():
    # Illustrative rhythmic construction: beats replace rests one at a time
    target=CLAP
    order=[0,4,7,1,9,5,2,10]
    steps=[]
    cur=[0]*12
    for k,o in enumerate(order):
        cur=cur[:]; cur[o]=1
        if k in (0,2,4,6,7): steps.append(cur)
    cell=15; gap=2; x0=110; y0=10
    w=x0+12*(cell+gap)+10; h=y0+len(steps)*(cell+gap+4)+6
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="9">']
    labs=["Stage 1","Stage 2","Stage 3","Stage 4","Complete pattern"]
    for r,p in enumerate(steps):
        y=y0+r*(cell+gap+4)
        s.append(f'<text x="{x0-8}" y="{y+cell-4}" text-anchor="end" fill="{INK}">{labs[r]}</text>')
        for i,v in enumerate(p):
            x=x0+i*(cell+gap)
            if v: s.append(f'<rect x="{x}" y="{y}" width="{cell}" height="{cell}" rx="3" fill="{ACC2 if r<len(steps)-1 else ACC}"/>')
            else: s.append(f'<rect x="{x+0.5}" y="{y+0.5}" width="{cell-1}" height="{cell-1}" rx="3" fill="none" stroke="{LINE}"/>')
    s.append('</svg>'); return "".join(s)

def arch_large():
    # ABCBA pattern length arch
    w=300;h=120;x0=20;base=96
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    labs=["A","B","C","B","A"]; hs=[22,46,72,46,22]
    bw=40; g=14
    for i,(l,hh) in enumerate(zip(labs,hs)):
        x=x0+i*(bw+g)
        s.append(f'<rect x="{x}" y="{base-hh}" width="{bw}" height="{hh}" rx="4" fill="{ACC2 if l!="C" else ACC}"/>')
        s.append(f'<text x="{x+bw/2}" y="{base+13}" text-anchor="middle" fill="{INK}">{l}</text>')
    s.append(f'<text x="{x0}" y="12" fill="{MUTED}" font-size="9">Riff length inside one of the four sections</text>')
    s.append(f'<text x="{x0+2*(bw+g)+bw/2}" y="{base-76}" text-anchor="middle" fill="{ACC}" font-size="9">trumpet breath</text>')
    s.append('</svg>'); return "".join(s)

def ostinato():
    # Pieces of Wood root ostinato shrinking 6 -> 4 -> 3
    w=300;h=110;x0=70
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    s.append(f'<text x="0" y="12" fill="{MUTED}" font-size="9">Length of the root figure across the piece</text>')
    for r,(lab,n) in enumerate([("Part 1",6),("Part 2",4),("Part 3",3)]):
        y=26+r*26
        s.append(f'<text x="{x0-8}" y="{y+12}" text-anchor="end" fill="{INK}">{lab}</text>')
        bw=30
        for i in range(n):
            s.append(f'<rect x="{x0+i*(bw+3)}" y="{y}" width="{bw}" height="16" rx="3" fill="{ACC2}"/>')
        s.append(f'<text x="{x0+n*(bw+3)+6}" y="{y+12}" fill="{MUTED}">{n} beats</text>')
    s.append('</svg>'); return "".join(s)

def chaconne():
    w=300;h=96;x0=10
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    s.append(f'<text x="0" y="12" fill="{MUTED}" font-size="9">Three passes through the chord cycle (minutes)</text>')
    total=25; L=w-20; x=x0
    for i,m in enumerate([6,10,9]):
        bw=m/total*L
        s.append(f'<rect x="{x}" y="28" width="{bw-2}" height="26" rx="4" fill="{[ACC2,ACC,ACC2][i]}"/>')
        s.append(f'<text x="{x+bw/2}" y="45" text-anchor="middle" fill="#fff" font-size="10" font-weight="600">Cycle {i+1} · ~{m}</text>')
        x+=bw
    # melody ticks
    import random; random.seed(3)
    for i in range(90):
        xx=x0+i*(L/90)
        s.append(f'<line x1="{xx:.1f}" y1="66" x2="{xx:.1f}" y2="{70+random.randint(0,10)}" stroke="{INK}" stroke-width="1"/>')
    s.append(f'<text x="{x0}" y="{h-2}" fill="{MUTED}" font-size="9">Above it: winds and keyboards in canon, growing more florid</text>')
    s.append('</svg>'); return "".join(s)

def mensuration():
    w=300;h=118;x0=46
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    mel=[0,2,4,3,5,7,6,4]
    s.append(f'<text x="0" y="12" fill="{MUTED}" font-size="9">Same line, two speeds at once</text>')
    for lane,(lab,dur,col,reps) in enumerate([("1 : 1",30,ACC2,3),("1 : 3",90,ACC,1)]):
        ybase=58+lane*48
        s.append(f'<text x="{x0-8}" y="{ybase-10}" text-anchor="end" fill="{INK}">{lab}</text>')
        x=x0
        for rpt in range(reps):
            for n in mel:
                ww=dur/len(mel)*  (1)
                ww=(w-x0-6)/(len(mel)*reps)
                s.append(f'<rect x="{x:.1f}" y="{ybase-n*4-6}" width="{ww-1.5:.1f}" height="5" rx="2" fill="{col}"/>')
                x+=ww
    s.append('</svg>'); return "".join(s)

def vamp_arch():
    w=640;h=110;x0=20
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    vals=[("I","16ths",16),("II","8ths",8),("III","quarters",4),("IV","8ths",8),("V","16ths",16)]
    bw=(w-x0*2)/5
    for i,(m,lab,d) in enumerate(vals):
        x=x0+i*bw
        s.append(f'<rect x="{x+3}" y="22" width="{bw-6}" height="44" rx="5" fill="{SOFT}"/>')
        n=d*2//2
        for k in range(n):
            xx=x+10+k*((bw-20)/n)
            s.append(f'<line x1="{xx:.1f}" y1="32" x2="{xx:.1f}" y2="56" stroke="{ACC if d==4 else ACC2}" stroke-width="2" stroke-linecap="round"/>')
        s.append(f'<text x="{x+bw/2}" y="14" text-anchor="middle" fill="{INK}" font-weight="600">Mvt {m}</text>')
        s.append(f'<text x="{x+bw/2}" y="82" text-anchor="middle" fill="{MUTED}">piano vamp in {lab}</text>')
    s.append(f'<text x="{w/2}" y="104" text-anchor="middle" fill="{MUTED}" font-size="9">Ticks show pulse density per bar, not exact counts. The arch slows the surface, then speeds it back up.</text>')
    s.append('</svg>'); return "".join(s)

def income():
    data=[("Overseas tours & residencies",6.309),("Orchestra Tax Relief & other",4.244),("Future Ready (one-off, LSO St Luke's)",4.386),
          ("City of London Corporation",2.055),("LSO concerts at the Barbican",2.062),("Arts Council England",2.009),
          ("Sponsorship & donations",2.062),("Third-party recordings (film, games)",1.485),("LSO Discovery grants & income",1.407),
          ("LSO Live label",1.016),("Endowment Trust",0.961),("UK engagements",0.567),("Other",0.242)]
    data.sort(key=lambda d:-d[1])
    w=640; rowh=14.5; x0=230; L=w-x0-60; mx=6.5
    h=len(data)*rowh+28
    s=[f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter" font-size="10">']
    for i,(lab,v) in enumerate(data):
        y=6+i*rowh
        bw=v/mx*L
        col = ACC if lab.startswith(("Arts","City")) else ACC2
        s.append(f'<text x="{x0-8}" y="{y+11}" text-anchor="end" fill="{INK}">{lab}</text>')
        s.append(f'<rect x="{x0}" y="{y+1}" width="{bw:.1f}" height="11.5" rx="3" fill="{col}"/>')
        s.append(f'<text x="{x0+bw+6:.1f}" y="{y+11}" fill="{INK}">£{v:.1f}m</text>')
    s.append(f'<text x="{x0}" y="{h-4}" fill="{MUTED}" font-size="9">Year to 31 July 2025. Total £28.8m. Orange = core public subsidy (about 14%).</text>')
    s.append('</svg>'); return "".join(s)
