const {chromium}=require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({deviceScaleFactor:1});
for(const f of process.argv.slice(2)){await p.goto('file://'+__dirname+'/'+f+'.html');await p.waitForTimeout(600);
const bad=await p.evaluate(()=>window.check());if(!f.startsWith('annot'))console.log(JSON.stringify(bad));
await p.locator('#p').screenshot({path:__dirname+'/'+f+'.png'});}
await b.close();})();
