const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('examples/mqtt_ha/web_ui.h','utf8');
const dashboard = source.slice(source.indexOf('static const char INDEX_HTML'),source.indexOf('static const char LOGIN_HTML'));
assert(!dashboard.includes('id="busNotice"'));
assert(!dashboard.includes('class="flow"'));
const script = dashboard.match(/<script>([\s\S]*?)<\/script>/)[1];
const logic = script.slice(0,script.indexOf('function nice('));
(async()=>{
 for(const mode of ['connected','disconnected','unreachable']) {
  const elements={};const get=id=>id==='busNotice'?null:(elements[id]??={style:{},parentElement:{setAttribute(){}},setAttribute(){}});
  const data={sync:mode==='connected'?'ON':'OFF',read_only:false,output_power:0,mains_power:0,bat_volt:49.1,battery_fresh:true,bat_current:-4.3,device_state:'Charge',soc:10,temp:20,ess_power:0,ess_power_eff:0,charge_sub_state:'Bulk',virtual_mode:'OFF',mqtt:true,rssi:-55,device_id:'test',ip:'192.0.2.1',host:'test',uptime:100,free_heap:90000,reset_reason:'Power on'};
  const context={document:{getElementById:get,querySelectorAll:()=>[]},location:{pathname:'/'},fetch:async()=>{if(mode==='unreachable')throw Error('offline');return {json:async()=>data}}};
  vm.createContext(context);vm.runInContext(logic,context);
  await vm.runInContext('poll()',context);await vm.runInContext('poll()',context);
  if(mode==='connected'){assert(get('t_bat_volt').innerHTML.includes('49,10'));assert.equal(get('dSync').className,'dot ok');assert.equal(get('t_battery_flow').innerHTML,'Entladen');
for(const [a,fresh,want] of [[4.3,true,'Laden'],[0,true,'Nahe 0 A'],[0.5,true,'Nahe 0 A'],[-0.5,true,'Nahe 0 A'],[-4.3,false,'Nicht verfügbar'],[null,true,'Nicht verfügbar']]){data.bat_current=a;data.battery_fresh=fresh;await vm.runInContext('poll()',context);assert.equal(get('t_battery_flow').innerHTML,want);}}
  else if(mode==='disconnected'){assert.equal(get('t_bat_volt').innerHTML,'—');assert.equal(get('dSync').className,'dot bad');}
  else {assert.equal(get('offBanner').style.display,'block');assert.equal(get('dSync').className,'dot bad');}
 }
 console.log('Dashboard without banner/diagram: connected, stale and unreachable states OK');
})().catch(e=>{console.error(e);process.exitCode=1});
