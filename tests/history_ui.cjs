const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('examples/mqtt_ha/web_ui.h','utf8');const from=src.indexOf('function historySeriesValue('),to=src.indexOf('function nice(',from);const c={};vm.createContext(c);vm.runInContext(src.slice(from,to),c);
const series={i:4,k:.1};assert.equal(c.historySeriesValue([0,0,0,4950,570],series,'cSoc'),57);assert.equal(c.historySeriesValue([0,0,0,4950,30000],series,'cSoc'),null);assert.equal(c.historySeriesValue([0,0,0,4950,null],series,'cSoc'),null);assert.equal(c.historySeriesValue(null,series,'cSoc'),null);
const end=1800000000;const h={clock_valid:true,now:end+120,end,interval:60,data:[[1],null,[3]],t:Date.now(),age:0};let frame=c.historyFrame(h,6);assert.equal(frame.rows.length,6);assert.equal(frame.rows[1][0],1);assert.equal(frame.rows[2],null);assert.equal(frame.rows[3][0],3);assert.equal(frame.rows[4],null);assert.equal(frame.rows[5],null);assert.equal(frame.end,(end+120)*1000);
frame=c.historyFrame({...h,now:end+3*86400},1440);assert(frame.rows.every(r=>r===null)); // old flash data never slides to today
const legacy=c.historyFrame({t:6000000,age:0,interval:60,data:[[1],[2]]},60);assert.equal(legacy.rows[59][0],2);
assert(c.historyNote({data:[],clock_valid:true,storage:{status:'Flash-Aufzeichnung aktiv',dropped:2,read_errors:1}}).includes('Queue-Verluste: 2'));assert(c.historyNote({data:[],retention_h:24,clock_valid:false,storage:{status:'warte auf Zeitabgleich'}}).includes('Ohne Zeitabgleich'));
console.log('History UI: UTC gaps/restarts, 24 hours, expiry, legacy RAM and invalid SoC values OK');

assert(!src.includes('data-n="2880"'));assert(src.includes('24-h-Fenster'));

c.SERIES={cPower:[{i:0},{i:1},{i:2}],cBatV:[{i:3,k:.01}],cSoc:[{i:4,k:.1}]};
const latest={clock_valid:true,end,now:end,interval:60,data:[[1,2,3,4950,570],null,[0,-5,null,4960,null]]};
let last=c.latestHistory(latest,'cPower');assert.equal(last.at,end*1000);assert.equal(last.values[0],0);assert.equal(last.values[1],-5);assert.equal(last.values[2],null);
last=c.latestHistory(latest,'cSoc');assert.equal(last.values[0],57);assert.equal(last.at,(end-120)*1000);
assert(c.latestHistory({...latest,now:end+86400},'cPower').values.every(v=>v===null));
assert.equal(c.latestHistory({...latest,clock_valid:false},'cBatV').at,null);
assert(src.indexOf('<canvas id="cPower"')<src.indexOf('id="lgPower"'));
