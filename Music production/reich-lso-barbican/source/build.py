import svgs
CSS = """
@page { size: A4; margin: 0 }
* { box-sizing: border-box }
body { margin:0; font-family: 'Bitstream Charter', 'Charter', Georgia, serif; color:#1d1d1f; background:#fff; font-size:9.8pt; line-height:1.42 }
.page { width:210mm; height:297mm; padding:17mm 18mm 15mm; position:relative; page-break-after:always; overflow:hidden }
.page:last-child { page-break-after:auto }
h1,h2,h3,.kicker,.foot,.fact,table,.cap,.tag { font-family:'Inter', sans-serif }
.kicker { font-size:8pt; letter-spacing:.14em; text-transform:uppercase; color:#c2410c; font-weight:600; margin:0 0 3mm }
h2 { font-size:21pt; line-height:1.1; margin:0 0 4mm; font-weight:700; letter-spacing:-.01em }
h3 { font-size:11.5pt; margin:4mm 0 1.5mm; font-weight:650; break-after:avoid }
p { margin:0 0 2.6mm }
.lede { font-size:11.6pt; line-height:1.45; color:#333 }
.two { columns:2; column-gap:8mm }
.two p, .two h3 { break-inside:avoid-column }
.foot { position:absolute; bottom:8mm; left:18mm; right:18mm; font-size:7.5pt; color:#8a8a8f; display:flex; justify-content:space-between; border-top:1px solid #e6e3dc; padding-top:2mm }
.fig { background:#faf7f2; border-radius:6px; padding:3.5mm 5mm 2.5mm; margin:2.5mm 0 2.5mm }
.cap { font-size:8pt; color:#6b6b70; margin-top:1.5mm; line-height:1.35 }
.facts { display:grid; grid-template-columns:repeat(4,1fr); gap:3mm; margin:4mm 0 5mm }
.fact { background:#f3ede4; border-radius:6px; padding:3mm 3.5mm }
.fact b { display:block; font-size:16pt; font-weight:700; color:#1d1d1f; line-height:1.1 }
.fact span { font-size:7.8pt; color:#55555a; line-height:1.3; display:block; margin-top:1mm }
table { width:100%; border-collapse:collapse; font-size:8.6pt; margin:2mm 0 3mm }
th { text-align:left; font-weight:600; color:#6b6b70; font-size:7.6pt; text-transform:uppercase; letter-spacing:.06em; border-bottom:1.5px solid #1d1d1f; padding:1.5mm 2mm 1.5mm 0 }
td { border-bottom:1px solid #e6e3dc; padding:1.6mm 2mm 1.6mm 0; vertical-align:top }
.piece { display:grid; grid-template-columns: 1fr 66mm; gap:6mm; margin-bottom:4mm }
.piece h3 { margin-top:0; font-size:13.5pt }
.tag { font-size:8pt; color:#6b6b70; margin-bottom:2mm }
.note { border-left:3px solid #c2410c; padding:1mm 0 1mm 4mm; margin:3mm 0; font-style:italic; color:#333 }
.box { background:#f3ede4; border-radius:6px; padding:3.5mm 4.5mm; margin:3mm 0 }
.box h3 { margin-top:0 }
.cover { background:#15130f; color:#f5f1ea; padding:0 }
.cover .inner { padding:26mm 20mm 0 }
.cover h1 { font-size:46pt; line-height:.98; margin:0 0 6mm; font-weight:800; letter-spacing:-.025em }
.cover .sub { font-family:Inter; font-size:12pt; color:#cfc8bd; margin-bottom:12mm }
.cover .kicker { color:#f08a4b }
.prog { font-family:Inter; border-top:1px solid #3b362f; margin-top:6mm }
.prog div { display:flex; justify-content:space-between; padding:2.6mm 0; border-bottom:1px solid #3b362f; font-size:11pt }
.prog span { color:#a9a196; font-size:9.5pt }
.prog .int { color:#a9a196; font-size:9pt; justify-content:center; letter-spacing:.2em; text-transform:uppercase }
.cover .bars { position:absolute; left:0; right:0; bottom:0; height:62mm }
.cover p { color:#ddd6cb }
code, .mono { font-family:'JetBrains Mono', monospace; font-size:8.6pt }
ol, ul { margin:0 0 2.6mm; padding-left:5mm }
li { margin-bottom:1.2mm }
"""

def foot(n, title):
    return f'<div class="foot"><span>Steve Reich at 90 · LSO · Barbican · 12 November 2026</span><span>{title} · {n}</span></div>'

# cover phasing art
def cover_art():
    import math
    s=['<svg class="bars" viewBox="0 0 210 62" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg">']
    pat=svgs.CLAP
    for row in range(13):
        p=pat[row:]+pat[:row]
        for i,v in enumerate(p*2):
            if v:
                x=8+i*8.15; y=4+row*4.3
                both = (pat*2)[i]
                s.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="6.6" height="3.2" rx=".8" fill="{"#f08a4b" if both else "#3d5f7d"}" opacity="{0.35+0.05*row:.2f}"/>')
    s.append('</svg>'); return "".join(s)

pages=[]

# 1 COVER
pages.append(f'''<div class="page cover"><div class="inner">
<div class="kicker">Concert companion · Barbican Hall · Thursday 12 November 2026</div>
<h1>Steve Reich<br>at 90</h1>
<div class="sub">The composer, the London Symphony Orchestra and the Barbican, with notes on the five pieces and how Reich builds rhythm.</div>
<p style="font-family:Inter;font-size:10pt;margin:0">London Symphony Orchestra · Susanna Mälkki, conductor</p>
<div class="prog">
<div>Music for a Large Ensemble (1978)<span>17 min</span></div>
<div>Music for Pieces of Wood (1973)<span>10 min</span></div>
<div>Variations for Winds, Strings and Keyboards (1979)<span>21 min</span></div>
<div class="int">Interval</div>
<div>Three Movements (1986)<span>15 min</span></div>
<div>Music for Ensemble and Orchestra (2018)<span>20 min</span></div>
</div>
<p style="font-family:Inter;font-size:8.6pt;color:#a9a196;margin-top:6mm;max-width:150mm">Reich turned 90 on 3 October 2026. The same programme was heard on 11 November and travels to the Philharmonie de Paris on 14 November. The pattern below is the 12-pulse figure from Clapping Music, which you also heard tonight in Music for Pieces of Wood, phased through all 13 of its positions.</p>
</div>{cover_art()}</div>''')

# 2 THE EVENING + REICH
pages.append(f'''<div class="page">
<div class="kicker">1 · The composer</div>
<h2>Steve Reich: pulse, pattern and process</h2>
<p class="lede">Reich (born New York, 3 October 1936) is the best known of the composers who walked away from post-war modernism and rebuilt music around a steady pulse, clear harmony and repetition. His central idea is that the <em>process</em> that generates a piece should be audible: you hear the machine working, and the machine is the music.</p>
<div class="two">
<h3>From tape loops to ensembles</h3>
<p>He studied philosophy at Cornell, then composition at Juilliard and at Mills College in California under Luciano Berio and Darius Milhaud. The breakthrough came in San Francisco in 1965. He put two copies of the same tape loop of a street preacher on two machines. They started in unison, but the motors ran at slightly different speeds, so one loop slowly crept ahead of the other. The result, <em>It's Gonna Rain</em>, and <em>Come Out</em> (1966) were the first <strong>phase pieces</strong>.</p>
<p>Back in New York he asked whether people could do what the tape machines did. <em>Piano Phase</em> (1967) has two pianists play the same 12-note pattern; one holds tempo while the other speeds up very slightly until they are one note apart, then locks in again. <em>Drumming</em> (1970–71) followed a summer studying with Ewe master drummers in Ghana, and later study of Balinese gamelan fed the interlocking parts in his ensemble writing.</p>
<h3>From process to orchestra</h3>
<p><em>Music for 18 Musicians</em> (1974–76) was the breakout: a cycle of eleven chords, pulsing mallets, and phrases measured by the players' breath. Success brought commissions from big institutions, and the pieces tonight trace that move from his own touring group into the symphony orchestra. From the 1980s he added texts and subjects: <em>Tehillim</em> (1981), <em>The Desert Music</em> (1982–84) and <em>Different Trains</em> (1988), which won a Grammy and built its melodies from recorded speech. <em>Electric Counterpoint</em> (1987), for Pat Metheny, was later sampled by The Orb. <em>Double Sextet</em> won the Pulitzer Prize in 2009.</p>
<h3>Why electronic musicians claim him</h3>
<p>Reich treated tape machines, loops and layering as compositional tools a decade before the sampler. On "Little Fluffy Clouds" The Orb sampled <em>Electric Counterpoint</em>, and the 1999 album <em>Reich Remixed</em> had Coldcut, Howie B, Ken Ishii and others rework his pieces. Detroit and Berlin techno share his faith in a fixed grid where change happens slowly and inside the loop. Autechre's self-mutating patterns are a distant cousin of his processes, with one big difference: Reich's process is <em>audible and predictable</em>, and the drama comes from hearing it happen.</p>
</div>
<div class="facts">
<div class="fact"><b>1936</b><span>Born in New York on 3 October; 90 this autumn</span></div>
<div class="fact"><b>1965</b><span><em>It's Gonna Rain</em>: the first phase piece, made with two tape machines</span></div>
<div class="fact"><b>1990</b><span>Grammy for <em>Different Trains</em> (Kronos Quartet recording)</span></div>
<div class="fact"><b>2009</b><span>Pulitzer Prize for <em>Double Sextet</em></span></div>
</div>
<div class="note">"I am interested in perceptible processes. I want to be able to hear the process happening throughout the sounding music." Reich, <span style="font-style:normal">Music as a Gradual Process</span> (1968).</div>
{foot(2,"The composer")}</div>''')

# 3 LSO size, structure, star players
pages.append(f'''<div class="page">
<div class="kicker">2 · The orchestra</div>
<h2>The London Symphony Orchestra</h2>
<p class="lede">Founded in 1904, the LSO is owned and run by its own players. The musicians elect the board, carry the business risk and are paid per engagement rather than a salary.</p>
<div class="facts">
<div class="fact"><b>1904</b><span>Founded by about 50 players who walked out of Henry Wood's Queen's Hall Orchestra over a ban on sending deputies</span></div>
<div class="fact"><b>c. 90–100</b><span>Members at any time. The LSO doesn't publish a fixed number; freelance extras fill gaps</span></div>
<div class="fact"><b>91</b><span>People listed on stage tonight, including 5 pianists, 7 percussionists and 2 bass guitarists</span></div>
<div class="fact"><b>~70</b><span>Concerts a year at the Barbican, its home since 1982</span></div>
</div>
<div class="two">
<h3>How it is governed</h3>
<p>The LSO is a limited company and a charity. Each member holds ten £1 shares, and no dividends are paid. Nine of the board's directors are players, elected by their colleagues, and players must always hold the majority. Tonight's welcome note was signed by Sarah Quinn, who chairs the board and also plays sub-principal second violin in the orchestra on stage. Dame Kathryn McDowell ran the LSO for 21 years until July 2026; John Harte, previously chief executive of Aurora Orchestra, took over in August 2026.</p>
<h3>Conductors</h3>
<p>Sir Antonio Pappano became Chief Conductor in September 2024, following Sir Simon Rattle (2017–2023, now Conductor Emeritus). Gianandrea Noseda is Principal Guest Conductor. Tonight's conductor, Susanna Mälkki, is a guest: former chief of the Helsinki Philharmonic and, by Pierre Boulez's invitation, music director of Ensemble intercontemporain from 2006 to 2013.</p>
<h3>Beyond the concert hall</h3>
<p>Millions hear the LSO without knowing it. It recorded John Williams's <em>Star Wars</em> scores, the soundtrack to <em>Maestro</em> and music for the <em>Final Fantasy</em> games. It runs its own label, LSO Live (over 200 releases since 2000), and LSO St Luke's, a converted 18th-century church in Old Street that reopened in late 2025 after an £8m refit with new recording facilities. LSO Discovery, its education arm, reaches about 60,000 people a year.</p>
</div>
<h3>Star players (current principals)</h3>
<table>
<tr><th style="width:30%">Player</th><th style="width:27%">Role</th><th>Why they matter</th></tr>
<tr><td>Benjamin Marquise Gilmore</td><td>Leader (led tonight)</td><td>One of three leaders, with Roman Simovic (since 2010) and Andrej Power</td></tr>
<tr><td>Neil Percy</td><td>Principal Percussion</td><td>Led tonight's five-player <em>Music for Pieces of Wood</em> with David Jackson, Sam Walton, Patrick King and Aidy Spillett</td></tr>
<tr><td>Gareth Davies</td><td>Principal Flute</td><td>Also a writer; his book <em>The Show Must Go On</em> tells the story of the LSO's 1912 US tour, which nearly sailed on the Titanic</td></tr>
<tr><td>Timothy Jones · Diego Incertis Sánchez</td><td>Principal Horns</td><td>The core of the LSO's famous brass sound, heard on decades of film scores</td></tr>
<tr><td>James Fountain</td><td>Principal Trumpet</td><td>Heads the section behind the "one-breath" trumpet chords in <em>Music for a Large Ensemble</em></td></tr>
<tr><td>Chris Richards · Juliana Koch</td><td>Principal Clarinet · Oboe</td><td>Principal woodwinds, both on stage tonight</td></tr>
</table>
<p class="cap">Roles from the LSO website and tonight's programme booklet.</p>
{foot(3,"The orchestra")}</div>''')

# 4 touring and finance
pages.append(f'''<div class="page">
<div class="kicker">2 · The orchestra, continued</div>
<h2>Touring and money</h2>
<p class="lede">The LSO earns more from playing abroad than from its home concerts and its two core public grants put together.</p>
<h3>The 2026/27 touring calendar</h3>
<table>
<tr><th style="width:22%">When</th><th style="width:30%">Where</th><th>Detail</th></tr>
<tr><td>12 Sep – 3 Oct 2026</td><td>Asia</td><td>Shanghai, Beijing, Kyoto, Tokyo, Osaka, ending in Hanoi, Vietnam, with Pappano. Premiere of Dai Fujikura's <em>Amber Alchemy</em> in Japan</td></tr>
<tr><td>14 Nov 2026</td><td>Paris</td><td>Tonight's Reich programme repeated at the Philharmonie de Paris, part of a standing Paris residency</td></tr>
<tr><td>Through the season</td><td>Europe</td><td>About 20 further European concerts with Adams, Mälkki, Pappano and Rattle</td></tr>
<tr><td>Feb – Mar 2027</td><td>United States</td><td>New two-concert residencies in Palm Desert and Ann Arbor; Walt Disney Concert Hall (first visit since 2015), Boston (first since 2009), Florida and two nights at Carnegie Hall</td></tr>
<tr><td>Summer 2027</td><td>Aix-en-Provence</td><td>Annual festival residency</td></tr>
</table>
<p>For the players this means weeks away each year, often a concert a day between flights, on top of about 70 Barbican dates, recording sessions and education work. Paid per engagement, they earn more in a busy touring year.</p>
<h3>Where the money comes from</h3>
<div class="fig">{svgs.income()}</div>
<div class="two">
<p><strong>Public money is a small slice.</strong> Arts Council England (£2.0m) and the City of London Corporation (£2.1m) together give about 14% of income. Continental orchestras of the same rank typically get most of their money from the state, so the LSO has to earn the rest.</p>
<p><strong>Touring is the largest single earner</strong> at £6.3m. Add UK engagements and third-party recording sessions (film, games, albums) and "engagements" bring in £8.4m, nearly a third of the total.</p>
<p><strong>Orchestra Tax Relief</strong> (£4.2m in the "other" line) is a UK tax credit on the cost of producing concerts.</p>
<p><strong>2024/25 was flattered by a one-off.</strong> £4.4m of grants and gifts went to the "Future Ready" rebuild of LSO St Luke's. Without it, income was about £24m against £24.6m of spending, and free reserves (£2.8m) sit at the bottom of the LSO's own target range.</p>
</div>
<p class="cap">Source: LSO Ltd consolidated accounts, year to 31 July 2025. Sponsorship and LSO Discovery each combine their grant and earned lines.</p>
{foot(4,"Touring and money")}</div>''')

# 5 Barbican
pages.append(f'''<div class="page">
<div class="kicker">3 · The venue</div>
<h2>The Barbican</h2>
<p class="lede">The hall you sat in tonight sits inside one of the boldest pieces of post-war planning in Britain: a concrete city-within-a-city built on a bombsite, with an arts centre at its heart.</p>
<div class="facts">
<div class="fact"><b>1982</b><span>Arts centre opened by Queen Elizabeth II on 3 March</span></div>
<div class="fact"><b>~2,000</b><span>Seats in the Barbican Hall</span></div>
<div class="fact"><b>Grade II</b><span>Listed status for the whole complex since 2001</span></div>
<div class="fact"><b>£191m</b><span>Committed by the City in 2024 to renew the centre</span></div>
</div>
<div class="two">
<h3>From bombsite to Brutalism</h3>
<p>The Cripplegate area, just north of the old Roman and medieval wall (the "barbican" was a watchtower on that wall), was flattened in the Blitz on 29 December 1940. The City of London Corporation chose to rebuild it as housing rather than offices, and hired architects Chamberlin, Powell and Bon. The residential estate, with its three towers, terrace blocks, highwalks and lakes, went up from the 1960s to the mid-1970s. The arts centre came last and was opened by Queen Elizabeth II on 3 March 1982, who called it "one of the wonders of the modern world". The whole complex was listed Grade II in 2001.</p>
<p>It is a famously difficult building to navigate (yellow lines on the floor exist for a reason) and famously loved: rough "bush-hammered" concrete, a tropical conservatory on the roof of the fly tower, and the lakeside terrace you might have walked past at the interval.</p>
<h3>The Hall</h3>
<p>The Barbican Hall seats just under 2,000. The original acoustic was criticised as dry, and the hall has been reworked more than once, most thoroughly in 2001, when a new stage canopy and adjustable reflectors went in. The result is clear and close rather than lush and reverberant, which suits music like Reich's, where you need to hear every interlocking note of five pianos and seven percussionists at once.</p>
<h3>Who runs it and who pays</h3>
<p>The Barbican Centre is owned and mostly funded by the City of London Corporation, the local authority for the Square Mile, which is one of the largest arts funders in the UK. It is also the LSO's second core funder. In 2024 the City committed £191m to a multi-year renewal of the centre, starting with the lakeside, foyers and the conservatory.</p>
<h3>The LSO and the Barbican</h3>
<p>The LSO has been resident orchestra since the centre opened in 1982. The Barbican provides the hall; the LSO plans and promotes its own season there (tonight was "produced by the London Symphony Orchestra"). Other partners include the BBC Symphony Orchestra as associate orchestra and the Academy of Ancient Music. A plan for a new purpose-built Centre for Music on the Museum of London site, championed by Rattle, was cancelled by the City in 2021, and the money and attention moved to renewing the Barbican itself.</p>
<h3>More than concerts</h3>
<p>The centre holds two theatres, three cinemas, two galleries (the Curve and the Art Gallery), a library and the Conservatory, with the Guildhall School of Music &amp; Drama next door.</p>
</div>
{foot(5,"The venue")}</div>''')

# 6 Rhythm toolkit
pages.append(f'''<div class="page">
<div class="kicker">4 · How Reich builds rhythm</div>
<h2>Phasing, construction and the 12-pulse grid</h2>
<p class="lede">Almost everything you heard tonight grows from a handful of rhythmic techniques. Once you can spot them, the music stops sounding "repetitive" and starts sounding like something is always on the move.</p>
<h3>1 · Gradual phasing (the tape-machine idea)</h3>
<p>Two identical loops run at very slightly different speeds. At first you hear one fat sound, then a flam, then an echo, then a brand new rhythm made by the two copies interlocking, until they eventually come back into unison. Reich calls these by-products <strong>resulting patterns</strong>: rhythms nobody is playing, which your ear builds from the combination.</p>
<div class="fig">{svgs.drift()}<div class="cap">Loop B runs about 3% fast. It creeps ahead: first a flam, then an echo, then a new interlocking rhythm.</div></div>
<h3>2 · Stepped phasing (Clapping Music, Pieces of Wood)</h3>
<p>By 1972 Reich replaced the gradual drift with sudden jumps. In <em>Clapping Music</em> both players clap the same 12-pulse pattern; after a number of repeats, player 2 jumps ahead by one pulse. After twelve jumps they are back in unison. It is the same idea as rotating a step pattern on a sequencer. <em>Music for Pieces of Wood</em> uses this exact pattern as its root figure.</p>
<div class="piece" style="grid-template-columns:96mm 1fr"><div class="fig" style="margin-top:0">{svgs.clapping()}<div class="cap">Clapping Music pattern (3 + 2 + 1 + 2 claps, rests between groups). The shared-clap count shows how unison density changes as the pattern rotates.</div></div><div>
<h3 style="margin-top:0">3 · Why twelve?</h3>
<p>Twelve pulses divide into 2 × 6, 3 × 4, 4 × 3 and 6 × 2. Reich's patterns, like the West African bell patterns he studied, sit across those groupings, so your ear keeps switching between hearing them in threes and in fours. Phasing exploits this: each new position pushes a different grouping to the front. That is why the music feels like it's changing when no note has changed.</p>
<h3>4 · Augmentation and breath</h3>
<p>Reich stretches note values within a pattern (augmentation) and measures some phrases by the length of a breath instead of by bars, so the same material can feel urgent or spacious. Both appear tonight.</p>
</div></div>
{foot(6,"Rhythm and phasing")}</div>''')

# 7 Pieces 1 & 2
pages.append(f'''<div class="page">
<div class="kicker">5 · Tonight's pieces, first half</div>
<h2>Riffs, arches and five pairs of claves</h2>
<div class="piece"><div>
<h3>Music for a Large Ensemble (1978)</h3>
<div class="tag">17 minutes · commissioned by the Holland Festival · first performed 1979 by the Netherlands Wind Ensemble with Steve Reich and Musicians · about 30 players, several amplified</div>
<p>Written right after <em>Music for 18 Musicians</em> and the biggest group Reich had used so far. The engine is a carpet of short, repeating riffs on pianos, marimbas and xylophones, with strings and winds layered on top.</p>
<p><strong>The process</strong> is melodic expansion and contraction. Each of the four sections is an arch, <strong>A B C B A</strong>: short riffs get longer step by step, reach a peak, and shrink back. At each of the four peaks, four trumpets play broad sustained chords, each written "to take one comfortable breath to perform". So the long notes are timed by human lungs, not by bar lines, and they float over a machine-tight pulse.</p>
<p><strong>Rhythm to listen for:</strong> the pulsing piano and mallet syncopations go round "like circles within circles". There is no real beginning or end. The piece fades in from the grid and, in the programme's words, stops "as though the music has simply been switched off". That's a techno ending.</p>
</div><div class="fig">{svgs.arch_large()}<div class="cap">One section's arch. Riffs grow from A to the peak C (trumpet breath chords), then shrink back.</div></div></div>

<div class="piece"><div>
<h3>Music for Pieces of Wood (1973)</h3>
<div class="tag">10 minutes · five pairs of tuned claves · tonight: Neil Percy, David Jackson, Sam Walton, Patrick King, Aidy Spillett</div>
<p>A touring piece: Reich wanted music he could play anywhere without "a vanload of gear". Each player has a pair of hardwood claves tuned to a different pitch. The root figure is the <em>Clapping Music</em> pattern.</p>
<p><strong>The process is rhythmic construction.</strong> One player holds a steady pulse (a human metronome) and another plays the root figure. The others then build the same figure <em>one beat at a time</em>, replacing rests with hits until the pattern is complete. Each player enters at a different offset, so when the pattern is complete it is phased against the others and pushes a different accent to the front.</p>
<p><strong>The arc:</strong> the root figure shrinks from <strong>6 beats to 4 to 3</strong> across the three parts, so the cycles get tighter and the texture denser. The players crescendo as it fills up. Reich called it "one of the loudest I have ever composed". With five pitches sounding together, the unison fortissimo near the end is close to a drum machine with every voice on.</p>
</div><div>
<div class="fig">{svgs.ostinato()}</div>
<div class="fig">{svgs.construction()}<div class="cap">Rhythmic construction (illustrative order). A player fills in the target pattern beat by beat until it matches.</div></div>
</div></div>
{foot(7,"First half")}</div>''')

# 8 Pieces 3 & 4
pages.append(f'''<div class="page">
<div class="kicker">6 · Tonight's pieces, either side of the interval</div>
<h2>Slow chords, fast surfaces</h2>
<div class="piece"><div>
<h3>Variations for Winds, Strings and Keyboards (1979)</h3>
<div class="tag">21 minutes · first performed by the San Francisco Symphony, 1980 · Reich's first piece for full orchestra</div>
<p>Reich's take on the <strong>chaconne</strong>, the Baroque form where a repeating chord sequence carries a set of variations (Pachelbel's Canon is the famous one). Here the chord cycle is stretched to an epic scale: <strong>three passes</strong> of about six, ten and nine minutes. Each chord can last a long time.</p>
<p>On top, winds and keyboards play a constant stream of melody <strong>in canon</strong>, the same line entering at different points so it overlaps itself. That's phasing written out. The canons get more florid with each pass. Strings and brass hold the long chords underneath.</p>
<p><strong>Rhythm to listen for:</strong> two clocks at once. The surface moves in quick even pulses, the harmony moves like weather. This is the same split as a techno track with a 16th-note sequence over a pad that changes every 16 bars, and it shows how long you can hold a chord if the surface keeps moving. Max Richter, whose <em>On the Nature of Daylight</em> uses the same chaconne idea, names Reich as a major influence.</p>
</div><div class="fig">{svgs.chaconne()}</div></div>

<div class="piece"><div>
<h3>Three Movements (1986)</h3>
<div class="tag">15 minutes · written for the St Louis Symphony and Leonard Slatkin · full orchestra in three linked movements, fast–slow–fast</div>
<p>Built on the same slow chord changes as the Variations, but darker and stormier. Reich wrote that the chord changes "may suggest the changing light as clouds move slowly across the sky".</p>
<p><strong>Harmony:</strong> lots of <strong>diminished seventh chords</strong>, which contain the tritone and can resolve in several directions, so the key never feels settled. It ends on A minor, but with an added seventh ringing that keeps it open.</p>
<p><strong>The layout is part of the rhythm.</strong> Reich asks for the strings to be split into two equal groups on either side of a central "engine room" of marimbas, vibraphones and pianos. The keyboard players at the centre carry the pulse and everyone can see them. Reich calls it "one unified rhythm direction".</p>
<p><strong>The finale uses a mensuration canon:</strong> the same melody played at two speeds at once, one copy three times slower than the other. Sibelius used the same trick at the end of his Fifth Symphony. In sequencer terms, it's one track's pattern copied to another track with a clock divider of 3.</p>
</div><div class="fig">{svgs.mensuration()}<div class="cap">Mensuration canon. The lower line is the same melody at one-third speed.</div></div></div>
{foot(8,"Variations and Three Movements")}</div>''')

# 9 Piece 5 + thread
pages.append(f'''<div class="page">
<div class="kicker">7 · Tonight's pieces, finale</div>
<h2>Music for Ensemble and Orchestra (2018)</h2>
<div class="tag" style="font-family:Inter">20 minutes · five continuous movements · first performed by the Los Angeles Philharmonic under Gustavo Dudamel at Walt Disney Concert Hall, November 2018 · the LSO gave the UK premiere the same year</div>
<p class="lede">Reich's first orchestral piece in about 30 years, and the one that pulls together everything else on tonight's programme.</p>
<div class="two">
<p><strong>A modern concerto grosso.</strong> In the Baroque concerto grosso a small group of soloists trades material with the full orchestra. Reich's soloist group is 20 players: strings, winds, brass, electric bass, vibraphones and pianos. Because there are so many solo parts, the music feels like chamber music, which lets Reich, as he says, "write the intricate, detailed music that I like to write". Soloists step out of the texture with short bursts of melody that set off chain reactions in the rest of the group "like cells dividing".</p>
<p><strong>Sound world.</strong> The ostinatos are more angular than in his earlier pieces, built from unusual scales and doubled in octaves or large unisons. The electric bass with piano and vibraphone gives a sound halfway between jazz and gamelan. The harmony floats, often without a clear home key, like Three Movements.</p>
<p><strong>A new kind of pattern.</strong> In the second movement Reich writes ostinatos made from slowly <em>rising scales</em>, something he had never done before ("but they work!"). He calls it a tip of the hat to the opening of Bach's Fifth Brandenburg Concerto, which is itself a kind of concerto grosso.</p>
<p><strong>The big shape is an arch of pulse speed.</strong> The piano vamp drives all five movements, but its basic note value changes from movement to movement. The texture slows from 16th notes to 8ths to quarter notes in the centre, then speeds back up. It's the same ABCBA arch as Music for a Large Ensemble, applied to tempo feel instead of riff length. The real tempo doesn't change; only the density of the pulse does.</p>
</div>
<div class="fig">{svgs.vamp_arch()}</div>
<div class="box">
<h3>The thread through the evening</h3>
<table>
<tr><th style="width:30%">Technique</th><th>Where you heard it tonight</th></tr>
<tr><td>Phasing and canon</td><td>Pieces of Wood (staggered entries) · Variations (canons in the winds) · Three Movements (mensuration canon)</td></tr>
<tr><td>Rhythmic construction</td><td>Pieces of Wood (beats replacing rests, one at a time)</td></tr>
<tr><td>Arch forms</td><td>Large Ensemble (ABCBA riff lengths) · Ensemble and Orchestra (16 · 8 · 4 · 8 · 16 pulse)</td></tr>
<tr><td>Slow harmony, fast surface</td><td>Variations (three chord cycles) · Three Movements · Ensemble and Orchestra</td></tr>
<tr><td>Breath as a clock</td><td>Large Ensemble (one-breath trumpet chords)</td></tr>
<tr><td>Baroque models, stretched</td><td>Variations (chaconne) · Ensemble and Orchestra (concerto grosso, Brandenburg 5)</td></tr>
</table>
<p style="margin:0">The programme's own summary is the best: Reich spends a career "looking backwards to go forwards, always moving, gradually, inexorably".</p>
</div>
{foot(9,"Music for Ensemble and Orchestra")}</div>''')

# 10 Your rig
pages.append(f'''<div class="page">
<div class="kicker">8 · Taking it home</div>
<h2>Reich ideas on your rig</h2>
<p class="lede">Five techniques from tonight, set up on your modular, VCV Rack and Live. The last two also help with arrangement: Reich's arches are a way to move a track somewhere without breaking the loop.</p>
<div class="two">
<h3>1 · Clapping Music on Pam's New Workout</h3>
<p>Set two Pam's outputs to Euclidean with 12 steps and 7 hits, the West African bell pattern behind <em>Drumming</em>. Send both to two short, different voices (Zadar envelopes into Mangrove, or two Freak hits) panned apart. Leave output 1 alone. Every 8 bars, turn output 2's rotation one step on. After 12 moves you are back in unison. To automate it, put a slow stepped CV on output 2's rotation from K-Accumulator or a DROID clock divider.</p>
<h3>2 · True phasing: one clock drifts</h3>
<p>Clock one Metropolix track from Live via the disting NT as usual. Run a second voice from <strong>Marbles' internal clock</strong> (unpatch its clock input), set as close to the master as you can, with the same 12-step pattern. The tiny tempo difference does the tape-machine trick: unison, flam, echo, new pattern. In VCV, use two clock modules at 120 and 120.3 BPM driving the same sequence.</p>
<h3>3 · Rhythmic construction on Metropolix</h3>
<p>Copy your root pattern to a second track with every stage silenced (gate mode off). Switch one stage back on every 4 or 8 bars until it matches, then do it again on a third track starting at a different stage. Each new entry builds the pattern at a different offset, as in <em>Pieces of Wood</em>. Then shorten the track length from 12 to 8 to 6 to tighten the cycle, as the claves go from 6 beats to 4 to 3.</p>
<h3>4 · Mensuration canon</h3>
<p>Put the same melody on two Metropolix tracks and set the second track's clock divider to 3 (or 2 for something busier). Send the slow copy to Three Sisters or Just Friends for a pad-like line that shadows the lead. It sounds simple but the two copies never line up the same way twice.</p>
<h3>5 · Arch form for arrangement</h3>
<p>Instead of adding and removing parts, change the <em>pulse density</em> of one driving part across the track: 16ths → 8ths → quarters → 8ths → 16ths, as in <em>Music for Ensemble and Orchestra</em>. Clock-divide your main vamp by 1, 2, 4 per section from DROID or Long Form, with the drums staying at full speed. Then steal the Variations trick: take a 4- or 6-chord Hapax progression and give each chord 8 to 16 bars, so the harmony becomes the slow arc and the loops sit on top. For a breath-length swell, use a Just Friends or Cold Mac envelope triggered by hand rather than by the clock.</p>
</div>
<div class="box"><p style="margin:0"><strong>Start here:</strong> idea 1 takes ten minutes and only needs Pam's and two voices. Record 12 rotations into Live, pick the four best positions, and you have the drum skeleton for an arrangement that develops without new parts.</p></div>
<p class="cap" style="margin-top:3mm">Sources: tonight's LSO programme booklet (programme note and composer profile by Timmy Fisher; stage list); lso.co.uk (concert page, 2026/27 season announcement, players page, managing director announcement); LSO full biography, May 2026; The London Symphony Orchestra Ltd accounts, year to 31 July 2025; susannamalkki.com. Background on Reich, the Barbican and the techniques draws on standard reference sources and Reich's own writings.</p>
{foot(10,"On your rig")}</div>''')

html=f"<!doctype html><html><head><meta charset='utf-8'><title>Steve Reich at 90</title><style>{CSS}</style></head><body>{''.join(pages)}</body></html>"
open("reich.html","w").write(html)
print("ok", len(pages))
