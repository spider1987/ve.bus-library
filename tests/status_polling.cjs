const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('examples/mqtt_ha/web_ui.h','utf8');
const dashboard=src.slice(src.indexOf('static const char INDEX_HTML'),src.indexOf('static const char LOGIN_HTML'));
const script=dashboard.match(/<script>([\s\S]*?)<\/script>/)[1];
const logic=script.slice(0,script.indexOf('function nice('));
let writes=0,fetches=0,pending=null,slow=false,fail=false;
const els={},get=id=>els[id]??={style:{},parentElement:{setAttribute(){}},attrs:{},getAttribute(k){return this.attrs[k]},setAttribute(k,v){this.attrs[k]=v},get textContent(){return this.text||''},set textContent(v){if(id.startsWith('operating'))writes++;this.text=v}};
const s={sync:'ON',switch_state:'on',dc_allows_inv:'ON',discharge_blocked:false,read_only:false,battery_fresh:true,bat_current:-5,bat_volt:49,output_power:0,mains_power:0,soc:50,ess_power:1000,ess_power_eff:1000,rssi:-50,uptime:100,free_heap:90000};
const a={enabled:true,fresh:true,status:'Entladen',restart_today:0,restart_daily_limit:2};
const c={document:{getElementById:get,querySelectorAll:()=>[]},location:{pathname:'/'},fetch:async url=>{fetches++;if(url==='/api/state')return {json:async()=>s};if(fail)throw Error('offline');if(slow)return new Promise(resolve=>pending=resolve);return {ok:true,json:async()=>a}}};
vm.createContext(c);vm.runInContext(logic,c);
(async()=>{
 await vm.runInContext('poll()',c);assert.equal(get('operatingAuto').textContent,'Automatik aktiv');
 const before=get('operatingDetail').textContent;writes=0;slow=true;
 const poll=vm.runInContext('poll()',c);
 for(let i=0;i<8&&!pending;i++)await Promise.resolve();assert(pending);
 assert.equal(writes,0);assert.equal(get('operatingDetail').textContent,before);
 const count=fetches;await vm.runInContext('poll()',c);assert.equal(fetches,count); // overlap suppressed
 pending({ok:true,json:async()=>a});await poll;assert.equal(writes,0); // unchanged DOM
 slow=false;fail=true;await vm.runInContext('poll()',c);assert.equal(get('operatingAuto').textContent,'Automatikstatus nicht verfügbar');
 fail=false;await vm.runInContext('poll()',c);assert.equal(get('operatingAuto').textContent,'Automatik aktiv'); // lock released after error
 console.log('Status polling: no intermediate empty state, no unchanged text writes, no overlapping requests, errors remain visible and polling resumes OK');
})().catch(e=>{console.error(e);process.exitCode=1});
