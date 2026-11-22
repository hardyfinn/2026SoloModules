// node ref/debug.js script.json fromBeat toBeat step  -> prints deck state over time
const fs=require('fs');
const script=JSON.parse(fs.readFileSync(process.argv[2]));
const a=+process.argv[3], b=+process.argv[4], st=+process.argv[5];
require('./core_sim.js');
// wrap core step to sample state
const orig=require.cache[require.resolve('./core_sim.js')].exports;
require.cache[require.resolve('./core_sim.js')].exports=function(BUF,SR,rnd){
  const c=orig(BUF,SR,rnd); const step=c.step; let n=0; let next=a;
  c.step=function(i1,i2,i3){ const o=step(i1,i2,i3); const B=BUF.st.ch[0][0];
    if(B>=next && B<b){ next+=st; const H=c.H; const f=x=>Math.round(x*1000)/1000;
      console.log(f(B), 'd0', ['m',H.h_mode0,'has',H.h_has0,'bs',f(H.h_bs0),'be',f(H.h_be0),'gt',H.h_gt0,'g',f(H.h_g0),'po',f(H.h_popen0),'pc',f(H.h_pclose0),'v',f(H.h_v0),'sl',H.h_slot0,'err',f(((H.h_exp0-H.h_pos0)%c.P.L+c.P.L*1.5)%c.P.L-c.P.L/2),'cu',H.h_culen0,'chk',H.h_cuchk0,'ok',H.h_expok0,'tr',f(H.h_trrem0)].join(' '),
       '| d1', ['m',H.h_mode1,'has',H.h_has1,'bs',f(H.h_bs1),'be',f(H.h_be1),'gt',H.h_gt1,'g',f(H.h_g1),'po',f(H.h_popen1),'pc',f(H.h_pclose1),'v',f(H.h_v1),'sl',H.h_slot1,'hd',H.h_head1,'qt',c.P.qt1].join(' '), 'gen', c.P.fgen, H.h_curgen);}
    return o;};
  return c;};
const sim=require('./sim.js'); sim.run(script);
