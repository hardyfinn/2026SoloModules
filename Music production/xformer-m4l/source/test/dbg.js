const load = require('./load'); const fs = require('fs');
const C = load(['common.js', 'tuesday.js']);
const eng = new C.TuesdayEngine({ gate(o){ console.log('  gate', o, T); }, stepInfo(){} });
let T=0;
for (const line of fs.readFileSync(process.argv[2], 'utf8').split('\n')) { const t=line.trim().split(/\s+/); if(!t[0]||t[0]=='engine'||t[0]=='ticks')continue; eng.setParam(t[0], +t[1]); }
eng.reset();
let acc=0, tick=0;
for (let ms=0; tick<40; ms++){ acc+=0.096; while(acc>=1){acc-=1; T=tick; eng.tick(tick); if(tick>=33) console.log('tick',tick,'armed',eng._retriggerArmed,'gt',eng._gateTicks,'q',JSON.stringify(eng._microGateQueue.items.map(e=>[e.tick,e.gate]))); tick++;} eng.update(); }
