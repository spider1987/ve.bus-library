const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('examples/mqtt_ha/web_ui.h','utf8');
const script=source.slice(source.indexOf('let autoActive=false'),source.indexOf('let acCanSetPower=false'));
(async()=>{
for(const [soc,watt,delay,sleep,cut,valid] of [[80,200,30,true,47,true],[100,100,10,true,47,true],[0,3000,600,false,0,true],[-1,200,30,true,47,false],[101,200,30,true,47,false],[80.5,200,30,true,47,false],[80,99,30,true,47,false],[80,10001,30,true,47,false],[80,200,9,true,47,false],[80,200,601,true,47,false],[80,200,30,true,0,false],[80,4000,30,true,47,false]]){
 const els={};const get=id=>els[id]??={};
 const values={autoBydMinSoc:soc,autoWakeMinW:watt,autoWakeDelay:delay,autoBydThreshold:100,autoDataTimeout:35,autoSharePct:90,autoChargeLimit:3000,autoDischargeLimit:3000,autoCutV:cut,autoDeltaV:1};
 for(const [id,value] of Object.entries(values))get(id).value=String(value);
 get('autoSleepAtCutoff').checked=sleep;get('autoCutOn').checked=cut>0;
 let posted;const a={byd_min_soc:soc,wake_min_w:watt,wake_delay_s:delay,sleep_at_cutoff:sleep,byd_soc:85,fresh:true,cutoff_v:cut,resume_delta_v:1,share_pct:90,charge_limit_w:3000,discharge_limit_w:3000,enabled:false,byd_threshold_w:100,data_timeout_s:35};
 const ctx={$:get,toast:()=>{},post:async(u,data)=>posted=data,fetch:async()=>({ok:true,json:async()=>a}),state:async()=>{}};
 vm.createContext(ctx);vm.runInContext(script,ctx);await get('autoSettings').onsubmit({preventDefault(){}});
 assert.equal(!!posted,valid,JSON.stringify([soc,watt,delay,sleep,cut]));
 if(valid){assert.equal(posted.byd_min_soc,soc);assert.equal(posted.sleep_at_cutoff,sleep?1:0);assert.equal(posted.wake_min_w,watt);assert.equal(posted.wake_delay_s,delay);assert.equal(get('autoBydMinSoc').value,soc);assert(get('autoPriorityStatus').textContent.includes(soc>0?'BYD zuerst':'BYD-Ladepriorität aus'));}
}
console.log('Priority/sleep UI: persistence, 0/100 percent, wake limits and voltage prerequisite OK');
})().catch(e=>{console.error(e);process.exitCode=1});
