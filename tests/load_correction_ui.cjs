const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('examples/mqtt_ha/web_ui.h','utf8');const script=src.slice(src.indexOf('let autoActive=false'),src.indexOf('let acCanSetPower=false'));
(async()=>{
for(const [factor,cap,floorV,valid] of [[0,2,0,true],[.0002,2,46,true],[.0002,2,0,false],[.0002,2,49,false],[.0002,0,46,false],[-.001,2,46,false],[.006,2,46,false],[.005,5,46,true]]){
const els={};const get=id=>els[id]??={};let posted;
get("autoDataSource").value="0";get("autoBoostOn").checked=true;get("autoBoostSoc").value="10";
const values={autoVoltFactor:factor,autoVoltCap:cap,autoVoltFloor:floorV,autoBydMinSoc:80,autoWakeMinW:200,autoWakeDelay:30,autoBydThreshold:100,autoDataTimeout:35,autoSharePct:90,autoChargeLimit:3000,autoDischargeLimit:3000,autoCutV:48,autoDeltaV:.5};for(const[id,value]of Object.entries(values))get(id).value=String(value);get('autoCutOn').checked=true;get('autoSleepAtCutoff').checked=true;
const a={voltage_correction_factor:factor,voltage_correction_max_v:cap,hard_floor_v:floorV,battery_voltage_v:47.5,voltage_corrected_v:48.45,battery_discharge_w:4750,cutoff_v:48,resume_delta_v:.5,enabled:false,byd_min_soc:80,wake_min_w:200,wake_delay_s:30,charge_limit_w:3000,discharge_limit_w:3000,share_pct:90};
const c={$:get,toast:()=>{},post:async(u,data)=>posted=data,fetch:async()=>({ok:true,json:async()=>a}),state:async()=>{}};vm.createContext(c);vm.runInContext(script,c);await get('autoSettings').onsubmit({preventDefault(){}});assert.equal(!!posted,valid,JSON.stringify([factor,cap,floorV]));
if(valid){assert.equal(posted.voltage_correction_factor,factor);assert.equal(posted.hard_floor_v,floorV);assert.equal(get('autoVoltFactor').value,factor);assert(get('autoVoltageStatus').textContent.includes('47,5 V'));assert(get('autoVoltageStatus').textContent.includes('48,45 V'));}
}
console.log('Load correction UI: persistence, raw/corrected display, mandatory floor and parameter bounds OK');
})().catch(e=>{console.error(e);process.exitCode=1});
