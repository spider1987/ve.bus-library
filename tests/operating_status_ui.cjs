const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('examples/mqtt_ha/web_ui.h','utf8');
const start=source.indexOf('function renderOperating('),end=source.indexOf('async function poll()',start);
const els={},get=id=>els[id]??={setAttribute(k,v){this[k]=v}};
const c={$:get};vm.createContext(c);vm.runInContext(source.slice(start,end),c);
const s={sync:'ON',switch_state:'on',dc_allows_inv:'ON',discharge_blocked:false,battery_fresh:true};
c.renderOperating(s,{enabled:true,fresh:true,status:'Entladen'});
assert.equal(get('operatingLocks').textContent,'Keine erkennbare Sperre');
assert(get('operatingMode').textContent.includes('Ladegerät + Wechselrichter'));
c.renderOperating({...s,switch_state:'charger_only'},null);
assert(get('operatingLocks').textContent.includes('Entladung durch Betriebsart gesperrt'));
c.renderOperating({...s,dc_allows_inv:'OFF',discharge_blocked:true,auto_low_voltage_lock:true},{enabled:true,fresh:false,ess_recovery_exhausted:true});
for(const t of ['DC-Freigabe','Software-Entladesperre','Unterspannungs','evcc-Daten','ausgeschöpft'])assert(get('operatingLocks').textContent.includes(t));
c.renderOperating({...s,switch_state:undefined,dc_allows_inv:undefined},null);
assert.equal(get('operatingLocks').textContent,'Freigaben nicht vollständig verfügbar');
c.renderOperating(null,null);assert.equal(get('operatingMode').textContent,'Nicht verfügbar');assert.equal(get('operatingDetail').textContent,'');
assert(source.includes('Interne ESS-Sperren'));
console.log('Operating status: mode, simultaneous locks, missing data and unknown internal ESS state OK');

c.renderOperating(s,{enabled:true,fresh:true,status:'Entladen',restart_today:2,restart_daily_limit:2,restart_limit_status:'Tageslimit erreicht',restart_cooldown_s:120});
assert(get('operatingRestarts').textContent==='2 von 2');
assert(get('operatingRestartInfo').textContent.includes('Tageslimit erreicht'));
assert(get('operatingRestartInfo').textContent.includes('in 2 min'));

c.renderOperating({...s,mains_power:475},{enabled:true,fresh:true,status:'Entladen · eingestellter Victron-Anteil',share_pct:90,demand_w:582,target_w:523,discharge_limit_w:4000});
assert.equal(get('operatingShare').textContent,'90 %');
assert.equal(get('operatingDemand').textContent,'582 W');
assert(get('operatingTarget').textContent==='523 W');
assert.equal(get('operatingMeasured').textContent,'475 W');
assert(get('operatingDetail').textContent.startsWith('Entladeanforderung'));
c.renderOperating(s,{enabled:true,fresh:false,share_pct:0,target_w:0});
assert.equal(get('operatingShare').textContent,'0 %');assert.equal(get('operatingDemand').textContent,'—');assert.equal(get('operatingTarget').textContent,'0 W');
c.renderOperating(s,{boost_supporting:true,share_pct:80,target_w:4000,discharge_limit_w:4000});
assert.equal(get('operatingShare').textContent,'100 %');
c.renderOperating(null,null);assert.equal(get('operatingShare').textContent,'—');assert.equal(get('operatingMeasured').textContent,'—');

assert(source.includes('Wechselrichter-Neustarts heute'));assert(source.includes('/style.css?v=status-layout-3'));
