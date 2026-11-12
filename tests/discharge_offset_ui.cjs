const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('examples/mqtt_ha/web_ui.h','utf8');const script=src.slice(src.indexOf('let autoActive=false'),src.indexOf('let acCanSetPower=false'));
(async()=>{for(const [offset,valid] of [[0,true],[50,true],[500,true],[-1,false],[501,false],[1.5,false]]){
 const els={},get=id=>els[id]??={value:'',checked:false,setCustomValidity(){}};
 let stored={source:'http',share_pct:100,discharge_offset_w:50,charge_limit_w:3000,discharge_limit_w:3000,cutoff_v:0,resume_delta_v:1,voltage_correction_factor:0,voltage_correction_max_v:2,hard_floor_v:0,byd_threshold_w:100,data_timeout_s:35};let posted;
 const c={$:get,toast(){},state:async()=>{},fetch:async()=>({ok:true,json:async()=>stored}),post:async(url,body)=>{posted=body;stored={...stored,...body}}};
 vm.createContext(c);vm.runInContext(script,c);await vm.runInContext('autoPoll()',c);assert.equal(get('autoDischargeOffset').value,50);
 get('autoDischargeOffset').value=String(offset);get('autoSettings').oninput();await vm.runInContext('autoPoll()',c);assert.equal(get('autoDischargeOffset').value,String(offset));
 await get('autoSettings').onsubmit({preventDefault(){}});assert.equal(!!posted,valid,String(offset));if(valid)assert.equal(posted.discharge_offset_w,offset);
 }
 for(const name of ['Leistung &amp; Netzausgleich','PV-Laden &amp; BYD-Priorität','Batterie-Boost fürs Auto','Batterieschutz &amp; Lastkorrektur','Abschalten, Wiederanlauf &amp; Neustarts','Datenquelle &amp; Verbindungsprobleme'])assert(src.includes('<summary>'+name+'</summary>'));
 for(const id of ['autoSettings','autoDischargeOffset','autoFetchStatus','autoSaveStatus','autoRestartStatus'])assert.equal((src.match(new RegExp('id="'+id+'"','g'))||[]).length,1,id);
 console.log('Offset UI: load, edited-value preservation, save/validation and grouped form IDs OK');
})().catch(e=>{console.error(e);process.exitCode=1});
