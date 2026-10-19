#pragma once
// Web UI for the mqtt_ha example — plain HTML/CSS/JS, no external CDNs, so it
// also works on networks without internet access.
#include <pgmspace.h>

static const char STYLE_CSS[] PROGMEM = R"rawliteral(
/* Instrument panel: 4px rhythm, one blue action accent, border-based depth. */
:root{color-scheme:dark;--bg:#101720;--surface:#18222e;--inset:#121c27;--border:#2b3b4e;--text:#e4edf8;--text2:#b2c3d8;--muted:#93a9c2;--accent:#79b7ff;--s1:#79b7ff;--s2:#b9d8ff;--s3:#477db6;--good:#59d69a;--bad:#ff7c83;--warnbg:#1a2c40;--warnfg:#c9e1ff;--grid:#29394c}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.6 'Segoe UI',sans-serif;-webkit-font-smoothing:antialiased}
main{max-width:1200px;margin:auto;padding:32px 24px}header{display:flex;flex-wrap:wrap;align-items:center;gap:12px 16px;margin-bottom:24px}header h1{font-size:28px;line-height:1.2;letter-spacing:-.8px;margin:4px 0 8px;font-weight:650}header .sub{color:var(--muted);font-size:12px}.sp{flex:1}.eyebrow{font-size:11px;letter-spacing:1.6px;font-weight:700;text-transform:uppercase;color:var(--accent)}a{color:var(--accent)}.btn{text-decoration:none;display:inline-flex;align-items:center;justify-content:center}
nav{display:flex;gap:8px;border-bottom:1px solid var(--border);margin-bottom:24px;padding-bottom:12px}nav a{padding:8px 12px;text-decoration:none;border-radius:6px;color:var(--text2)}nav a[aria-current=page]{color:var(--accent);background:var(--surface);font-weight:600}
.pill{display:inline-flex;align-items:center;gap:8px;padding:4px 12px;border:1px solid var(--border);border-radius:6px;font-size:12px;background:var(--surface);color:var(--text2)}.dot{width:8px;height:8px;border-radius:50%;background:var(--muted)}.dot.ok{background:var(--good)}.dot.bad{background:var(--bad)}
.banner{background:var(--warnbg);color:var(--warnfg);border:1px solid transparent;border-radius:8px;padding:12px 16px;margin-bottom:16px}.banner strong{display:block}.banner a{color:inherit;font-weight:600}.banner.connection{background:var(--surface);border-color:var(--border);color:var(--text2)}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:24px;margin-bottom:24px}.card h2{font-size:20px;letter-spacing:-.4px;line-height:1.3;margin:0 0 16px}.card h3{font-size:14px;margin:0 0 8px}.section-head{display:flex;align-items:center;gap:16px;margin-bottom:20px}.section-head h2{margin:0}.section-head .note{margin:4px 0 0}
.tiles{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:1px;background:var(--border);border:1px solid var(--border);border-radius:12px;overflow:hidden;margin-bottom:24px}.tile{background:var(--surface);padding:20px;min-width:0}.tile .l{font-size:12px;color:var(--text2);margin-bottom:8px}.tile .v{font-size:28px;font-weight:650;line-height:1.3;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}.tile .v small{font-size:14px;font-weight:400;color:var(--muted);margin-left:4px}.tile .h{font-size:12px;color:var(--muted);min-height:20px;margin-top:8px}.tile:first-child .v,.tile:nth-child(3) .v{color:var(--accent)}
.row{display:flex;flex-wrap:wrap;align-items:center;gap:8px}.grid2{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.control-grid{display:grid;grid-template-columns:1.4fr 1fr 1fr;gap:24px}.control-grid label.f{align-content:start}.control-grid .row{align-items:stretch}.control-grid select{flex:1}.control-grid #ess{width:100%!important;font-size:28px;font-weight:650;font-variant-numeric:tabular-nums;min-height:60px}.control-grid label:first-child .row{display:grid;grid-template-columns:1fr auto}.control-grid label:first-child button{align-self:stretch}.control-grid .note{font-weight:400}
button,.btn{font:600 13px/1.4 'Segoe UI',sans-serif;min-height:44px;border:1px solid var(--border);background:var(--surface);color:var(--text);padding:12px 16px;border-radius:8px;cursor:pointer}button:hover,.btn:hover{background:var(--inset);border-color:var(--muted)}button.primary{background:var(--accent);border-color:var(--accent);color:var(--surface)}button.primary:hover{filter:brightness(.93)}button.danger{color:var(--bad)}button:active{transform:translateY(1px)}button:disabled{opacity:.5;cursor:default}:focus-visible{outline:3px solid var(--accent);outline-offset:3px}
input,select{font:inherit;color:var(--text);background:var(--inset);border:1px solid var(--border);border-radius:8px;padding:10px 12px;min-width:0;max-width:100%;min-height:44px}input:not([type=checkbox]):not([type=file]){width:100%}input[type=checkbox]{min-height:20px;width:20px;vertical-align:middle;accent-color:var(--accent)}input[type=file]{width:100%;flex:1}label.f{display:grid;gap:8px;font-size:12px;font-weight:600;color:var(--text2)}label.note{display:inline-flex;align-items:center;gap:8px;min-height:44px}.note{font-size:12px;color:var(--muted);line-height:1.7;overflow-wrap:anywhere}.hint{background:var(--inset);padding:16px;border-radius:8px;margin-top:20px}.hint p{margin:0}.hint p+p{margin-top:8px}.status-strip{background:var(--inset);border-radius:8px;padding:12px 16px;margin:20px 0;font-size:12px;color:var(--text2);font-variant-numeric:tabular-nums}.control-readings{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-bottom:24px}.reading{border-bottom:1px solid var(--border);padding-bottom:16px}.reading span{display:block;font-size:12px;color:var(--muted)}.reading strong{font-size:24px;font-weight:650;font-variant-numeric:tabular-nums}
.flow{display:grid;grid-template-columns:1fr auto 1fr auto 1fr;gap:16px;align-items:center;background:var(--inset);padding:16px;border-radius:8px;margin-bottom:24px}.flow div{text-align:center}.flow small{display:block;color:var(--muted);font-size:12px}.flow b{font-size:14px}.flow-arrow{color:var(--muted);font-size:20px}.flow-caption{margin:0 0 20px;color:var(--muted);font-size:12px}
details.card{padding:0}summary{cursor:pointer;min-height:56px;padding:20px 24px;font-weight:600;font-size:16px}summary::marker{color:var(--accent)}.detail-body{padding:0 24px 24px}details.advanced{border-top:1px solid var(--border);margin-top:20px}details.advanced summary{padding:16px 0;font-size:13px}.save-bar{display:flex;justify-content:space-between;align-items:center;gap:16px;padding:16px 24px;margin-top:8px;background:var(--surface);border:1px solid var(--border);border-radius:12px;margin-bottom:24px}.save-bar p{margin:0}.seg{display:inline-flex;border:1px solid var(--border);border-radius:8px;overflow:hidden}.seg button{border:0;border-radius:0;background:transparent;padding:8px 16px}.seg button.on{background:var(--accent);color:var(--surface)}
.chart{position:relative;margin-top:16px}.chart canvas{width:100%;height:220px;display:block;touch-action:pan-y}.chart h3{color:var(--text2);margin-bottom:8px}.chart-pair{display:grid;grid-template-columns:1fr 1fr;gap:24px}.legend{display:flex;flex-wrap:wrap;gap:16px;font-size:12px;color:var(--text2);margin-bottom:8px}.legend i{display:inline-block;width:16px;height:3px;margin-right:8px;vertical-align:middle}.tip{position:absolute;pointer-events:none;background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:8px 12px;font-size:12px;display:none;white-space:nowrap;z-index:2}.tip b{font-variant-numeric:tabular-nums}.cols{display:grid;grid-template-columns:1fr 1fr;gap:24px}table{width:100%;border-collapse:collapse;font-size:12px}td{padding:8px 0;border-bottom:1px solid var(--grid)}td:last-child{text-align:right;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}td:first-child{color:var(--text2);padding-right:16px}td[colspan]{text-align:left;padding:12px 0;font-size:14px;color:var(--text)}footer{color:var(--muted);font-size:12px}.toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);max-width:calc(100% - 32px);background:var(--text);color:var(--bg);padding:12px 20px;border-radius:8px;display:none;z-index:9}progress{width:100%;height:8px}code{font-size:11px;overflow-wrap:anywhere}
@media(max-width:800px){main{padding:24px 16px}.control-grid{grid-template-columns:1fr 1fr}.control-grid label:first-child{grid-column:1/-1}.tiles{grid-template-columns:repeat(2,minmax(0,1fr))}.chart-pair{grid-template-columns:1fr}header .sp{display:none}header h1{font-size:24px}.cols{grid-template-columns:1fr}}
@media(max-width:480px){main{padding:16px 12px}.card{padding:20px}.detail-body{padding:0 20px 20px}summary{padding:20px}.grid2,.control-grid{grid-template-columns:1fr}.control-grid label:first-child{grid-column:auto}.tile{padding:16px}.tile .v{font-size:24px}.flow{gap:8px;padding:12px}.flow b{font-size:12px}.flow small{font-size:11px}.control-readings{gap:8px}.reading strong{font-size:20px}.section-head,.save-bar{align-items:flex-start;flex-direction:column}.save-bar button{width:100%}.seg button{padding:8px 12px}}

/* One navigation path; compact status group, balanced header on small screens. */
header{gap:20px;margin-bottom:20px}header>div:first-child{min-width:0}header .sub{overflow-wrap:anywhere}.header-status{display:flex;flex-wrap:wrap;gap:8px;align-items:center}.header-status .pill{min-height:32px;padding:5px 10px;border-radius:8px}
nav{gap:4px;padding:6px;margin-bottom:24px;background:var(--inset);border:1px solid var(--border);border-radius:10px;width:fit-content;max-width:100%}nav a{min-height:44px;display:flex;align-items:center;justify-content:center;padding:10px 18px;border-radius:7px;font-weight:600}nav a:hover{background:var(--surface);color:var(--text)}nav a[aria-current=page]{background:var(--surface);color:var(--accent);box-shadow:inset 0 0 0 1px var(--border)}
@media(max-width:800px){header{align-items:flex-start}header>div:first-child{flex:1 1 100%}.header-status{width:100%}}
@media(max-width:480px){nav{width:100%}nav a{flex:1;padding:10px 8px;font-size:12px}}


.diagnostic-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px;align-items:start}.diagnostic-group{min-width:0;background:var(--inset);border:1px solid var(--border);border-radius:10px;padding:16px}.diagnostic-group h3{color:var(--accent);margin:0 0 12px;padding-bottom:12px;border-bottom:1px solid var(--border)}.diagnostic-group table{table-layout:fixed}.diagnostic-group th,.diagnostic-group td{padding:10px 0;vertical-align:top;line-height:1.5;height:auto}.diagnostic-group th{width:65%;font-weight:400;text-align:left;color:var(--text2);padding-right:12px;border-bottom:1px solid var(--grid);overflow-wrap:anywhere}.diagnostic-group td{width:35%;text-align:right}.diagnostic-group tr:last-child th,.diagnostic-group tr:last-child td{border-bottom:0}.diagnostic-group .unavailable{color:var(--muted);font-size:11px}.diagnostic-group .note{margin:12px 0 0}
@media(max-width:800px){.diagnostic-grid{grid-template-columns:1fr;gap:16px}}

@media(max-width:480px){#autoCard .control-readings{grid-template-columns:1fr;gap:12px}#autoCard .reading{display:flex;align-items:center;justify-content:space-between;gap:12px;padding-bottom:12px}#autoCard .reading span{max-width:65%}#autoCard .reading strong{white-space:nowrap}#autoCard .row button{width:100%}}
.energy-table th{font-size:11px;color:var(--muted);font-weight:600;padding:8px 0;border-bottom:1px solid var(--border);text-align:left}.energy-table td:not(:first-child),.energy-table th:not(:first-child){text-align:right}.energy-table td:first-child{white-space:nowrap}.energy-table th:first-child{width:30%}#energyCard .grid2{row-gap:20px}@media(max-width:480px){.energy-table{font-size:11px}#energyCard .reading strong{font-size:20px}}
@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}button:active{transform:none}}
)rawliteral";

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VE.Bus Dashboard</title><link rel="stylesheet" href="/style.css"></head><body><main><div class="banner" id="readOnlyNotice" style="display:none"><strong>Diagnose · Nur Lesen</strong><br>Messwerte werden abgefragt. Lade-, Entlade- und Schaltbefehle sind gesperrt.</div>
<header><div><div class="eyebrow">VE.Bus · Energiemonitor</div><h1>Dein Batteriesystem</h1><div class="sub" id="sub">&nbsp;</div></div><span class="sp"></span>
<div class="header-status" aria-label="Verbindungsstatus"><span class="pill"><span class="dot" id="dSync"></span>VE.Bus</span>
<span class="pill"><span class="dot" id="dMqtt"></span>MQTT</span>
<span class="pill"><span class="dot" id="dWifi"></span><span id="wifi">WiFi</span></span>
</div></header><nav aria-label="Hauptnavigation"><a href="/">Übersicht</a><a href="/admin/">Steuerung &amp; Einstellungen</a></nav>
<div class="banner" id="pwBanner" style="display:none">Das Admin-Passwort ist noch das Standardpasswort. <a href="/admin/">Jetzt ändern</a>.</div>
<div class="banner" id="offBanner" style="display:none">Gerät nicht erreichbar — neuer Versuch…</div>
<div class="banner connection" id="busNotice" role="status"><strong>Verbindung wird geprüft</strong>Messwerte werden geladen.</div><div class="flow"><div><small>AC-IN</small><b>Stromnetz</b></div><span class="flow-arrow">↔</span><div><small>VE.Bus</small><b>MultiPlus</b></div><span class="flow-arrow">↔</span><div><small>DC</small><b>Batterie</b></div></div><section class="tiles" id="tiles"></section>
<section class="card" id="energyCard"><div class="section-head"><div><div class="eyebrow">Batterieseite · DC</div><h2>Batterieenergie</h2><p class="note" id="energyDate">Tageswerte werden geladen.</p></div></div>
<div class="grid2"><div class="reading"><span>Heute geladen</span><strong id="energyTodayCharge">—</strong></div><div class="reading"><span>Heute entnommen</span><strong id="energyTodayDischarge">—</strong></div><div class="reading"><span>Gesamt geladen seit Aufzeichnungsbeginn</span><strong id="energyTotalCharge">—</strong></div><div class="reading"><span>Gesamt entnommen seit Aufzeichnungsbeginn</span><strong id="energyTotalDischarge">—</strong></div></div>
<p class="note" id="energyNote" role="status">Die Zähler laufen unabhängig von Batterietest und Automatik.</p>
<details class="advanced"><summary>Tagesverlauf · 32 erfasste Tage</summary><table class="energy-table"><thead><tr><th>Datum</th><th>Geladen<br>kWh</th><th>Entnommen<br>kWh</th><th>Erfasst<br>h</th></tr></thead><tbody id="energyDays"></tbody></table></details>
<details class="advanced"><summary>Dashboard &amp; MQTT</summary><p class="note">HTTP mit Tagesarchiv: <a href="/api/energy" target="_blank" rel="noopener">/api/energy</a><br>MQTT-Zusammenfassung: <code id="energyMqtt">—</code><br>Datierte Tageswerte: <code>…/energy/day/JJJJ-MM-TT</code><br>Zusätzliche Sensoren werden über Home Assistant Discovery bereitgestellt.</p><p class="note">Messbasis: Batteriespannung × Batteriestrom des MultiPlus, ohne kalibrierten Shunt. Tageswechsel: Europe/Berlin mit Sommerzeit. Speicherung alle 10 Minuten, beim Tageswechsel sowie vor Web-Update und angefordertem Neustart. Bei Stromausfall können die letzten 10 Minuten fehlen. Fehlende Messwerte werden nicht hochgerechnet.</p></details></section>
<section class="card"><div class="row"><h2 style="margin:0">Verlauf</h2><span class="sp" style="flex:1"></span>
<div class="seg" id="range"><button data-n="60">1 h</button><button data-n="360">6 h</button><button data-n="1440" class="on">24 h</button></div></div>
<div class="chart"><h3>Leistung (W)</h3><div class="legend" id="lgPower"></div><canvas id="cPower"></canvas><div class="tip"></div></div>
<div class="chart-pair"><div class="chart"><h3>Batteriespannung (V)</h3><canvas id="cBatV"></canvas><div class="tip"></div></div>
<div class="chart"><h3>Ladestand (%)</h3><canvas id="cSoc"></canvas><div class="tip"></div></div>
</div><div class="note" id="histNote"></div></section>
<details class="card"><summary>Messwerte &amp; Diagnose</summary><div class="detail-body diagnostic-grid" id="details"></div></details>
<footer id="foot"></footer></main>
<script>
const $=id=>document.getElementById(id);
const DE={ON:'Ein',OFF:'Aus',on:'Ein',off:'Aus',charger_only:'Nur Ladegerät',inverter_only:'Nur Wechselrichter',Unknown:'Unbekannt',Down:'Inaktiv',Startup:'Start',Off:'Aus',Slave:'Neben-Gerät','Invert Full':'Wechselrichter', 'Invert Half':'Wechselrichter Teillast','Invert AES':'Wechselrichter Sparmodus','Power Assist':'Leistungsunterstützung',Bypass:'Netzdurchleitung',Charge:'Laden',Init:'Initialisierung',Bulk:'Hauptladung',Absorption:'Absorptionsladung',Float:'Erhaltungsladung',Storage:'Lagerung',Equalise:'Ausgleichsladung','Bulk Stopped':'Hauptladung gestoppt','N/A':'Nicht verfügbar',ess_power:'ESS-Sollwert',switch_state:'Betriebsart',virtual_mode:'Batterieneutraler Modus',wakeup:'Einschalten',sleep:'Ausschalten',force_absorption:'Absorptionsladung',force_float:'Erhaltungsladung',force_equalise:'Ausgleichsladung'};
const de=v=>DE[v]??v;
function connection(s){const ok=s.sync==='ON';const n=$('busNotice');n.textContent=ok?'VE.Bus verbunden · Messwerte werden aktualisiert.':'Keine VE.Bus-Verbindung · Batterie-Messwerte sind nicht verfügbar. Verkabelung und Wechselrichter prüfen.';n.className='banner '+(ok?'connection':'');return ok}
document.querySelectorAll('nav a').forEach(a=>{if(a.pathname===location.pathname)a.setAttribute('aria-current','page')});


const TILES=[['output_power','Ausgangsleistung','W',0],['mains_power','Netzleistung','W',0],['bat_volt','Batteriespannung','V',2],
['bat_current','Batteriestrom','A',1],['soc','Victron-Ladestand (geschätzt)','%',0],['ess_power','ESS-Sollwert','W',0],
['device_state','Gerätezustand','',null],['temp','Temperatur','°C',1]];
const DETAILS=[['AC','ac_power','AC-Leistung','W'],['AC','mains_voltage','Netzspannung','V'],['AC','mains_current','Netzstrom','A'],
['AC','mains_freq','Netzfrequenz','Hz'],['AC','inv_voltage','Wechselrichterspannung','V'],['AC','inv_current','Wechselrichterstrom','A'],
['AC','inv_freq','Wechselrichterfrequenz','Hz'],['AC','ac_in_min','AC-Eingang Minimum','A'],['AC','ac_in_max','AC-Eingang Maximum','A'],
['AC','ac_in_actual','AC-Eingang · Stromlimit','A'],['AC','ac_in_config','AC-Konfiguration (Rohwert)',''],
['DC','bat_volt','Batteriespannung','V'],['DC','bat_current','Batteriestrom','A'],['DC','soc','Victron-Ladestand (geschätzt)','%'],['DC','temp','Temperatur','°C'],['DC','dc_current','DC-Strom (VE.Bus)','A'],['DC','dc_allows_inv','Wechselrichter freigegeben',''],['DC','battery_flow','Batteriefluss',''],
['DC','charge_sub_state','Ladephase',''],['ESS','ess_power_eff','Wirksamer ESS-Sollwert','W'],['ESS','virtual_mode','Batterieneutraler Modus',''],
['ESS','switch_state','Betriebsart',''],['Status','led_on','LED an (Bitmaske)',''],['Status','led_blink','LED blinkend (Bitmaske)',''],
['Status','ups_status','USV-Status (NUT)',''],['Status','nut_clients','NUT-Clients',''],['Status','apc_clients','apcupsd-Clients',''],['Status','checksum_faults','Prüfsummenfehler',''],['Status','firmware_version','VE.Bus-Firmware','']];
const SERIES={cPower:[{i:0,n:'Ausgang',c:'--s1'},{i:1,n:'Netz',c:'--s2'},{i:2,n:'ESS wirksam',c:'--s3'}],
cBatV:[{i:3,n:'Batterie',c:'--s1',k:.01,d:2}],cSoc:[{i:4,n:'SoC',c:'--s1',k:.1,d:1}]};
let hist=null,range=1440,failCount=0;
$('tiles').innerHTML=TILES.map(t=>`<div class="tile"><div class="l">${t[1]}</div><div class="v" id="t_${t[0]}">–</div><div class="h" id="h_${t[0]}"></div></div>`).join('');
$('lgPower').innerHTML=SERIES.cPower.map(s=>`<span><i style="background:var(${s.c})"></i>${s.n}</span>`).join('');
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return (d?d+'d ':'')+h+'h '+m+'m'}
function fmtV(v,dec){if(v===null||v===undefined||(typeof v==='number'&&!Number.isFinite(v)))return '—';return typeof v==='number'?v.toLocaleString('de-DE',{minimumFractionDigits:dec,maximumFractionDigits:dec}):de(v)}
function setDot(id,ok){const dot=$(id);dot.className='dot '+(ok?'ok':'bad');const label=id==='dSync'?'VE.Bus':id==='dMqtt'?'MQTT':'WLAN';dot.parentElement.title=label+': '+(id==='dWifi'?(ok?'guter Empfang':'schwacher Empfang'):(ok?'verbunden':'nicht verbunden'));dot.parentElement.setAttribute('aria-label',dot.parentElement.title)}
function renderDetails(s,connected){
 const current=s.bat_current;
 s={...s,battery_flow:typeof current==='number'&&Number.isFinite(current)?current>0.5?'Laden':current< -0.5?'Entladen':'Nahe 0 A':null};
 const rawFields=['ac_in_config','led_on','led_blink'];
 const decimals={bat_volt:2,mains_voltage:1,inv_voltage:1,mains_current:2,inv_current:2,bat_current:1,dc_current:1,temp:1,mains_freq:2,inv_freq:2};
 const groups={};for(const [group,key,label,unit] of DETAILS){const v=s[key],missing=v===null||v===undefined||v===''||(typeof v==='number'&&!Number.isFinite(v)),stale=!connected&&group!=='Status'&&group!=='ESS';
 let value=missing||stale?'Nicht verfügbar':rawFields.includes(key)?'0x'+Number(v).toString(16).toUpperCase().padStart(2,'0'):fmtV(v,decimals[key]??0)+(unit?' '+unit:'');
 (groups[group]??=[]).push(`<tr><th scope="row">${label}</th><td class="${missing||stale?'unavailable':''}">${value}</td></tr>`);}
 $('details').innerHTML=Object.entries(groups).map(([group,rows])=>`<section class="diagnostic-group"><h3>${{AC:'AC · Netz & Wechselrichter',DC:'DC · Batterie',ESS:'ESS · Steuerung',Status:'Verbindung & System'}[group]}</h3><table><tbody>${rows.join('')}</tbody></table>${group==='DC'?'<p class="note">Batteriefluss aus dem gemessenen Batteriestrom; bis ±0,5 A als nahe null angezeigt. Die Ladephase ist der gemeldete Gerätezustand.</p>':''}${group==='AC'&&(s.mains_freq==null||s.inv_freq==null)?'<p class="note">Frequenzwerte fehlen: Die Periodenskalierung ist noch nicht verfügbar. Es wird kein Ersatzwert angenommen.</p>':''}</section>`).join('');
}
async function poll(){
  try{const r=await fetch('/api/state',{cache:'no-store'});const s=await r.json();const connected=connection(s);$('readOnlyNotice').style.display=s.read_only?'block':'none';failCount=0;$('offBanner').style.display='none';
    for(const t of TILES){$('t_'+t[0]).innerHTML=!connected&&t[0]!=='ess_power'?'—':t[3]===null?de(s[t[0]]):`${fmtV(s[t[0]],t[3])}<small>${t[2]}</small>`}
    $('h_device_state').textContent=s.charge_sub_state!=='N/A'?de(s.charge_sub_state):'';
    $('h_ess_power').textContent=`wirksam ${s.ess_power_eff} W`+(s.virtual_mode==='ON'?' · batterieneutral':'');
    $('h_bat_volt').textContent=connected?`DC ${fmtV(s.dc_current,1)} A`:'Keine Messdaten';
    setDot('dSync',s.sync==='ON');setDot('dMqtt',s.mqtt);setDot('dWifi',s.rssi>-80);$('wifi').textContent=s.rssi+' dBm';
    $('sub').textContent=`${s.device_id} · ${s.ip} · ${s.host}.local`;
    $('pwBanner').style.display=s.default_pw?'block':'none';
    renderDetails(s,connected);
    $('foot').textContent=`Laufzeit ${dur(s.uptime)} · freier Speicher ${(s.free_heap/1024).toFixed(0)} kB · letzter Neustart: ${s.reset_reason}${s.crash_stage&&s.crash_stage!=='-'?' (in '+s.crash_stage+')':''} · MQTT-Verbindungen ${s.mqtt_reconnects} · WLAN-Neuverbindungen ${s.wifi_reconnects}`;
  }catch(e){if(++failCount>1){$('offBanner').style.display='block';setDot('dSync',false);setDot('dMqtt',false);setDot('dWifi',false);$('dWifi').parentElement.title='WLAN: Gerätestatus nicht erreichbar';$('dWifi').parentElement.setAttribute('aria-label','WLAN: Gerätestatus nicht erreichbar')}}
}
async function loadHist(){try{const r=await fetch('/api/history',{cache:'no-store'});hist=await r.json();hist.t=Date.now();drawAll()}catch(e){}}
function nice(x){const p=Math.pow(10,Math.floor(Math.log10(x))),f=x/p;return (f<=1?1:f<=2?2:f<=5?5:10)*p}
function css(v){return getComputedStyle(document.documentElement).getPropertyValue(v).trim()}
function hhmm(t){const d=new Date(t);return d.getHours().toString().padStart(2,'0')+':'+d.getMinutes().toString().padStart(2,'0')}
function drawChart(cv,hover){
  const ser=SERIES[cv.id],dpr=window.devicePixelRatio||1,W=cv.clientWidth,H=cv.clientHeight;
  cv.width=W*dpr;cv.height=H*dpr;const g=cv.getContext('2d');g.scale(dpr,dpr);g.clearRect(0,0,W,H);
  if(!hist)return;const rows=hist.data.slice(-range),n=rows.length,step=hist.interval*1000;
  const tEnd=hist.t-hist.age*1000,tStart=tEnd-(range-1)*step;
  const tOf=i=>tEnd-(n-1-i)*step,val=(r,s)=>r?r[s.i]*(s.k||1):null;
  let lo=Infinity,hi=-Infinity;for(const r of rows)if(r)for(const s of ser){const v=val(r,s);lo=Math.min(lo,v);hi=Math.max(hi,v)}
  const L=52,R=10,T=8,B=22,pw=W-L-R,ph=H-T-B;
  g.font='12px system-ui,sans-serif';g.fillStyle=css('--muted');
  if(lo===Infinity){g.fillText('Noch keine Messpunkte',L,T+ph/2);return}
  if(cv.id==='cPower'){lo=Math.min(lo,0);hi=Math.max(hi,0)}
  if(hi-lo<1e-9){hi+=1;lo-=1}
  const st=nice((hi-lo)/4);lo=Math.floor(lo/st)*st;hi=Math.ceil(hi/st)*st;
  const X=t=>L+(t-tStart)/(tEnd-tStart||1)*pw,Y=v=>T+(hi-v)/(hi-lo)*ph,dec=st<1?(st<.1?2:1):0;
  g.strokeStyle=css('--grid');g.lineWidth=1;g.textAlign='right';g.textBaseline='middle';
  for(let v=lo;v<=hi+st/2;v+=st){const y=Math.round(Y(v))+.5;g.beginPath();g.moveTo(L,y);g.lineTo(W-R,y);g.stroke();g.fillText(v.toFixed(dec),L-6,y)}
  g.textAlign='center';g.textBaseline='top';const xt=range<=60?15:range<=360?60:240;
  const first=Math.ceil(tStart/(xt*60000))*xt*60000;
  for(let t=first;t<=tEnd;t+=xt*60000)g.fillText(hhmm(t),X(t),H-B+6);
  g.lineWidth=2;g.lineJoin='round';
  for(const s of ser){g.strokeStyle=css(s.c);g.beginPath();let pen=false;
    rows.forEach((r,i)=>{const v=val(r,s);if(v===null){pen=false;return}const x=X(tOf(i)),y=Y(v);pen?g.lineTo(x,y):g.moveTo(x,y);pen=true});g.stroke()}
  const tip=cv.parentNode.querySelector('.tip');
  if(hover==null||hover<L||hover>W-R){tip.style.display='none';return}
  const i=Math.max(0,Math.min(n-1,Math.round(n-1-(tEnd-(tStart+(hover-L)/pw*(tEnd-tStart)))/step))),r=rows[i],x=X(tOf(i));
  g.strokeStyle=css('--muted');g.lineWidth=1;g.beginPath();g.moveTo(x,T);g.lineTo(x,T+ph);g.stroke();
  if(r)for(const s of ser){g.fillStyle=css(s.c);g.strokeStyle=css('--surface');g.lineWidth=2;g.beginPath();g.arc(x,Y(val(r,s)),4,0,7);g.fill();g.stroke()}
  tip.innerHTML=`<div>${hhmm(tOf(i))}</div>`+(r?ser.map(s=>`<div><i style="display:inline-block;width:8px;height:8px;border-radius:50%;background:var(${s.c});margin-right:5px"></i>${s.n} <b>${val(r,s).toFixed(s.d||0)}</b></div>`).join(''):'<div>keine Daten</div>');
  tip.style.display='block';const tw=tip.offsetWidth;tip.style.left=(x+12+tw>W?x-12-tw:x+12)+'px';tip.style.top=(cv.offsetTop+T)+'px';
}
function drawAll(){for(const id in SERIES)drawChart($(id));const n=hist?hist.data.length:0;
  $('histNote').textContent=n?`1-Minuten-Mittelwerte · ${Math.round(n/6)/10} h seit dem Start aufgezeichnet`:''}
for(const id in SERIES){const cv=$(id);const mv=e=>{const b=cv.getBoundingClientRect();drawChart(cv,e.clientX-b.left)};
  cv.addEventListener('pointermove',mv);cv.addEventListener('pointerdown',mv);cv.addEventListener('pointerleave',()=>drawChart(cv))}
$('range').onclick=e=>{const n=e.target.dataset.n;if(!n)return;range=+n;for(const b of $('range').children)b.classList.toggle('on',b===e.target);drawAll()};
addEventListener('resize',drawAll);
matchMedia('(prefers-color-scheme: dark)').addEventListener('change',drawAll);
poll();loadHist();setInterval(poll,5000);setInterval(loadHist,60000);

async function energyPoll(){try{const r=await fetch('/api/energy',{cache:'no-store'});if(!r.ok)throw Error();const a=await r.json();
 const kwh=v=>v===null||v===undefined?'—':Number(v).toLocaleString('de-DE',{minimumFractionDigits:3,maximumFractionDigits:3})+' kWh';
 for(const [id,key]of [['energyTodayCharge','today_charge_kwh'],['energyTodayDischarge','today_discharge_kwh'],['energyTotalCharge','total_charge_kwh'],['energyTotalDischarge','total_discharge_kwh']])$(id).textContent=kwh(a[key]);
 $('energyDate').textContent=a.time_valid?'Heute · '+a.date+' · Europe/Berlin':'Uhrzeit wird synchronisiert · Tageswerte noch nicht zugeordnet';
 $('energyNote').textContent=(a.fresh?'Zähler aktiv':'Keine frischen Batteriewerte · Zähler pausiert')+' · heute '+(a.today_measured_s/3600).toLocaleString('de-DE',{maximumFractionDigits:2})+' h erfasst'+(!a.storage_ok?' · Speichern fehlgeschlagen!':'')+((a.unassigned_charge_kwh+a.unassigned_discharge_kwh)>0?' · Energie ohne gültige Uhrzeit ist nur im Gesamtzähler enthalten.':'');
 $('energyMqtt').textContent=a.mqtt_topic;
 $('energyDays').innerHTML=a.days.length?[...a.days].reverse().map(d=>'<tr><td>'+String(d.date).replace(/[^0-9-]/g,'')+'</td><td>'+Number(d.charged_kwh).toLocaleString('de-DE',{minimumFractionDigits:3,maximumFractionDigits:3})+'</td><td>'+Number(d.discharged_kwh).toLocaleString('de-DE',{minimumFractionDigits:3,maximumFractionDigits:3})+'</td><td>'+(d.measured_s/3600).toLocaleString('de-DE',{maximumFractionDigits:2})+'</td></tr>').join(''):'<tr><td colspan="4">Noch keine Tagesaufzeichnung.</td></tr>';
 }catch(e){$('energyNote').textContent='Energiestatistik nicht erreichbar · erneuter Versuch folgt.';}}
energyPoll();setInterval(energyPoll,15000);
</script></body></html>)rawliteral";

static const char ADMIN_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VE.Bus Einstellungen</title><link rel="stylesheet" href="/style.css"></head><body><main><div class="banner" id="readOnlyNotice" style="display:none"><strong>Diagnose · Nur Lesen</strong><br>Messwerte werden abgefragt. Lade-, Entlade- und Schaltbefehle sind gesperrt.</div>
<header><div><div class="eyebrow">VE.Bus · Bedienkonsole</div><h1>Manuelle Steuerung</h1><div class="sub" id="sub">&nbsp;</div></div><span class="sp"></span>
</header><nav aria-label="Hauptnavigation"><a href="/">Übersicht</a><a href="/admin/">Steuerung &amp; Einstellungen</a></nav>
<div class="banner" id="pwBanner" style="display:none">Du verwendest das Standardpasswort <b>vebus</b>. Lege unten ein eigenes Passwort fest.</div>

<div class="banner connection" id="busNotice" role="status"><strong>Verbindung wird geprüft</strong>Messwerte werden geladen.</div>
<section class="card" id="autoCard"><div class="section-head"><div><div class="eyebrow">Victron zuerst</div><h2>evcc-Automatik</h2><p class="note" id="autoSummary">Lade- und Entladegrenzen werden geladen.</p></div></div>
<div class="control-readings"><div class="reading"><span>Restbedarf nach PV</span><strong id="autoDemand">—</strong></div><div class="reading"><span>BYD · Entladen + / Laden −</span><strong id="autoByd">—</strong></div><div class="reading"><span>Victron · AC-IN gemessen</span><strong id="autoVic">—</strong></div></div>
<p class="status-strip" id="autoStatus" role="status">Automatikstatus wird geladen.</p>
<div class="row"><button class="primary" id="autoOn" onclick="autoAction('on')">Automatik einschalten</button><button id="autoOff" onclick="autoAction('off')">Ausschalten · Sollwert 0 W</button></div>
<details class="advanced"><summary>Leistungsgrenzen &amp; Batterieschutz</summary><form id="autoSettings"><div class="grid2">
<label class="f" style="grid-column:1/-1;max-width:240px">Victron-Anteil am Restbedarf (%)<input id="autoSharePct" type="number" min="0" max="100" step="1" value="90" required></label>
<label class="f">Maximal laden am AC-IN (W)<input id="autoChargeLimit" type="number" min="0" max="32767" step="1" value="3000" required></label>
<label class="f">Maximal entladen am AC-IN (W)<input id="autoDischargeLimit" type="number" min="0" max="32767" step="1" value="3000" required></label></div>
<p class="note">Der Victron übernimmt den eingestellten Anteil am Verbrauch nach PV-Abzug, begrenzt durch die maximale Entladeleistung. 0 % deaktiviert die automatische Entladung; PV-Laden bleibt möglich. 0 W deaktiviert die jeweilige Leistungsrichtung.</p>
<label class="note"><input id="autoCutOn" type="checkbox" onchange="autoCutControls()"> Zusätzliche Entladesperre nach Batteriespannung</label>
<div class="grid2"><label class="f">Entladen sperren bei (V)<input id="autoCutV" type="number" min="38" max="60" step="0.1" placeholder="Spannungsgrenze deiner Batterie" disabled></label>
<label class="f">Freigeben nach Spannungsanstieg um (V)<input id="autoDeltaV" type="number" min="0.1" max="5" step="0.1" value="1" disabled></label></div>
<p class="note" id="autoGuardStatus">Spannungsschutz wird geladen.</p><p class="note">Die Spannungssperre setzt auch manuelle Entlade-Sollwerte am AC-IN auf 0 W. AC-OUT-Verbraucher können weiter Batteriestrom beziehen. Keine vollständige Geräteabschaltung; MultiPlus- und BMS-Schutz bleiben erforderlich.</p>
<button type="submit" id="autoLimitSave">Grenzen speichern</button></form></details>
<details class="advanced"><summary>So arbeitet die Regelung</summary><p class="note">Bei Restbedarf übernimmt der Victron den eingestellten Prozentanteil bis zur Entladegrenze. PV-Überschuss lädt die Batterie bis zur eingestellten Ladegrenze. 100 W Totzone, Leistungsänderung maximal 500 W je frischem evcc-Abruf. Bei entladender BYD wird nicht geladen; bei ladender BYD wird nicht entladen (150 W Toleranz). Messwertausfall setzt den AC-IN-Sollwert auf 0 W; das garantiert keinen vollständigen Batteriestopp bei AC-OUT-Verbrauchern. Die Schutzgrenzen im MultiPlus bleiben bestehen.</p>
<p class="note">Quelle: evcc unter 192.168.178.2:7070. Zuordnung: Batterie 1 = GoodWe/BYD, Batterie 2 = LILYGO/Victron. Nach jedem Neustart automatisch eingeschaltet; Start mit 0 W bis frische evcc- und VE.Bus-Werte vorliegen. Manuelles Ausschalten gilt bis zum nächsten Neustart. Manuelle Leistungsvorgaben und Batterietests sind während der Automatik gesperrt; Schalt- und Ladephasenbefehle beenden die Automatik.</p></details></section>

<section class="card" id="energyCard"><div class="section-head"><div><div class="eyebrow">Batterieseite · DC</div><h2>Batterieenergie</h2><p class="note" id="energyDate">Tageswerte werden geladen.</p></div></div>
<div class="grid2"><div class="reading"><span>Heute geladen</span><strong id="energyTodayCharge">—</strong></div><div class="reading"><span>Heute entnommen</span><strong id="energyTodayDischarge">—</strong></div><div class="reading"><span>Gesamt geladen seit Aufzeichnungsbeginn</span><strong id="energyTotalCharge">—</strong></div><div class="reading"><span>Gesamt entnommen seit Aufzeichnungsbeginn</span><strong id="energyTotalDischarge">—</strong></div></div>
<p class="note" id="energyNote" role="status">Die Zähler laufen unabhängig von Batterietest und Automatik.</p>
<details class="advanced"><summary>Tagesverlauf · 32 erfasste Tage</summary><table class="energy-table"><thead><tr><th>Datum</th><th>Geladen<br>kWh</th><th>Entnommen<br>kWh</th><th>Erfasst<br>h</th></tr></thead><tbody id="energyDays"></tbody></table></details>
<details class="advanced"><summary>Dashboard &amp; MQTT</summary><p class="note">HTTP mit Tagesarchiv: <a href="/api/energy" target="_blank" rel="noopener">/api/energy</a><br>MQTT-Zusammenfassung: <code id="energyMqtt">—</code><br>Datierte Tageswerte: <code>…/energy/day/JJJJ-MM-TT</code><br>Zusätzliche Sensoren werden über Home Assistant Discovery bereitgestellt.</p><p class="note">Messbasis: Batteriespannung × Batteriestrom des MultiPlus, ohne kalibrierten Shunt. Tageswechsel: Europe/Berlin mit Sommerzeit. Speicherung alle 10 Minuten, beim Tageswechsel sowie vor Web-Update und angefordertem Neustart. Bei Stromausfall können die letzten 10 Minuten fehlen. Fehlende Messwerte werden nicht hochgerechnet.</p></details></section>
<section class="card"><div class="section-head"><div><h2>Laden &amp; Entladen</h2><p class="note">Leistung am AC-Eingang manuell vorgeben.</p></div></div>
<div class="control-readings"><div class="reading"><span>Batteriespannung</span><strong id="liveVolt">—</strong></div><div class="reading"><span>Batteriestrom</span><strong id="liveCurrent">—</strong></div><div class="reading"><span>Gerätezustand</span><strong id="liveState">—</strong></div></div>
<div class="control-grid">
<label class="f">Leistung am AC-IN (W)<span class="row"><input type="number" id="ess" min="-32767" max="32767" step="1" style="width:120px"><button class="primary" id="essSet" onclick="cmd('ess_power',$('ess').value)">Setzen</button></span></label>
<label class="f">Betriebsart<span class="row"><select id="sw"><option value="on">Ein</option><option value="off">Aus</option><option value="charger_only">Nur Ladegerät</option><option value="inverter_only">Nur Wechselrichter</option></select><button onclick="cmd('switch_state',$('sw').value)">Übernehmen</button></span></label>
<label class="f">Batterieneutraler USV-Modus<span class="row"><select id="vm"><option value="OFF">Aus</option><option value="ON">Ein</option></select><button id="vmSet" onclick="cmd('virtual_mode',$('vm').value)">Übernehmen</button></span></label>
</div>
<p class="status-strip" id="ctlState"></p>
<div class="hint"><h3>Was bedeutet das Vorzeichen?</h3><p class="note"><b>Minus → Netzbezug / Laden</b> · <b>Plus → Einspeisen / Entladen</b><br><strong>MultiPlus-II 48/5000/70-50</strong><br>Wechselrichter: 4.000 W Dauerleistung bei 25 °C (5.000 VA). Ladegerät: maximal 70 A DC; die Ladeleistung hängt von der Batteriespannung und den eingestellten Limits ab.<br> Der Sollwert regelt AC-IN. Verbraucher an AC-OUT beeinflussen den tatsächlichen Batteriestrom. 0 W ist kein garantierter Batteriestopp.</p></div>

<div class="row"><button onclick="cmd('wakeup')">Einschalten</button><button onclick="cmd('sleep')">Ausschalten</button>
</div><details class="advanced"><summary>Erweiterte Ladefunktionen</summary><p class="note">Nur verwenden, wenn die Ladeart zur Batterie passt. Ausgleichsladung ist kein normaler Batterietest.</p><div class="row"><button onclick="cmd('force_absorption')">Absorptionsladung starten</button><button onclick="cmd('force_float')">Erhaltungsladung starten</button>
<button onclick="confirm('Ausgleichsladung starten? Nur für Batterien, die diese Ladeart ausdrücklich unterstützen.')&&cmd('force_equalise')">Ausgleichsladung starten</button></div></details>
</section>

<section class="card"><h2>Victron-Ladestand synchronisieren</h2>
<p class="note">Manuelle Referenz für den internen Victron-Batteriemonitor. Nur verwenden, wenn der tatsächliche Ladestand bekannt ist. 100 % erst nach vollständig abgeschlossener Ladung setzen. Spannung allein belegt keinen Ladestand von 10 %. Die Batterie wird dadurch nicht geladen; dies ist kein ausgelesener Huawei-BMS-Ladestand. Keine automatische Synchronisierung.</p>
<div class="row"><label class="f">Bekannter Ladestand (%)<input id="syncSoc" type="number" min="0" max="100" step="1" value="100"></label><button onclick="syncSoc()">Ladestand synchronisieren</button></div>
<p class="note">Nach dem Senden den Ladestand im Dashboard prüfen: angezeigt wird weiterhin der vom MultiPlus zurückgelesene Wert. Die ?bernahme dieses Befehls ist am Ger?t noch zu prüfen.</p></section>

<section class="card" id="acLog"><div class="section-head"><div><h2>Batterietest &amp; AC-IN-Messung</h2><p class="note">Batterieenergie in kWh messen; Netzbezug und Einspeisung zusätzlich aufzeichnen.</p></div></div>
<div class="control-readings"><div class="reading"><span>Batterie geladen · DC</span><strong id="batChargeKwh">0,000 kWh</strong></div><div class="reading"><span>Batterie entladen · DC</span><strong id="batDischargeKwh">0,000 kWh</strong></div><div class="reading"><span>Batterieleistung aktuell</span><strong id="batPower">—</strong></div></div>
<p class="status-strip" id="batResult" role="status">Noch kein Batterietest.</p>
<p class="note">Die entnommene Batterieenergie ist die gemessene nutzbare Testkapazität, wenn du von vollständig geladen bis zur zulässigen Entladegrenze ohne Zwischenladen testest. Bei Teilentladung zeigt sie nur die bisher entnommene Energie. Messbasis: Spannung und Strom des MultiPlus; kein kalibrierter Batteriemonitor. Parallelakkus werden gemeinsam erfasst. Keine automatische Abschaltung durch diesen Zähler.</p>
<div class="control-readings"><div class="reading"><span>Netzbezug</span><strong id="acImport">—</strong></div><div class="reading"><span>Einspeisung</span><strong id="acExport">—</strong></div><div class="reading"><span>AC-IN aktuell</span><strong id="acPower">—</strong></div></div>
<div class="grid2" style="margin-bottom:20px"><div class="reading"><span>Gemessener Netzbezug · AC-IN</span><strong id="acChargeW">—</strong></div><div class="reading"><span>Gemessene Einspeisung · AC-IN</span><strong id="acDischargeW">—</strong></div></div>
<details class="advanced"><summary>Lade-/Entladeleistung einstellen · optional</summary>
<label class="note"><input type="checkbox" id="acEnablePower" onchange="acPowerControls()"> Leistungsvorgabe für den laufenden Test aktivieren</label>
<div class="grid2"><label class="f">Gewünschte Richtung<select id="acDirection" disabled><option value="charge">Laden / Netzbezug</option><option value="discharge">Entladen / Einspeisen</option></select></label><label class="f">Leistungsvorgabe am AC-IN (W)<input type="number" id="acWatts" min="0" max="32767" step="1" value="100" disabled></label></div>
<div class="row" style="margin-top:16px"><button class="primary" id="acApplyPower" onclick="acSetPower(false)" disabled>Leistung übernehmen</button><button id="acZeroPower" onclick="acSetPower(true)" disabled>AC-IN-Sollwert auf 0 W</button></div>
<p class="status-strip" id="acPowerSetpoint">Sollwert wird geladen.</p>
<p class="note">Nur bei laufender Messung und frischen Messwerten verfügbar. Die Vorgabe ändert den ESS-Sollwert; die Betriebsart und der batterieneutrale Modus bleiben bestehen. Im batterieneutralen Modus wird der AC-OUT-Verbrauch zusätzlich berücksichtigt. Die gemessenen Werte oben zeigen AC-IN, nicht die direkte Batterieleistung.</p>
<p class="note">„Messung beenden“ beendet nur die Aufzeichnung. Der eingestellte Sollwert bleibt aktiv. Auch 0 W garantiert keinen Batteriestopp bei Verbrauchern an AC-OUT.</p></details>
<p class="status-strip" id="acStatus" role="status">Messlauf wird geladen.</p>
<div class="row"><button class="primary" id="acStart" onclick="acAction('start')" disabled>Messung starten</button><button id="acStop" onclick="acAction('stop')" disabled>Messung beenden</button><button id="acCsv" onclick="location.href='/admin/ac-log.csv'" disabled>CSV herunterladen</button></div>
<details class="advanced"><summary>Kapazitätstest · Messprinzip &amp; Vorzeichen</summary><p class="note">Voll laden und bis zur zulässigen Entladegrenze entladen — jeweils nach Herstellervorgaben. Die Batterie-kWh werden aus gemeinsam abgefragter Batteriespannung und Batteriestrom integriert. Positive Batterie-Ströme zählen als Laden, negative als Entladen. AC-Wh werden zusätzlich erfasst und enthalten Umwandlungseinflüsse.</p><label class="note"><input type="checkbox" id="acInvert" checked> Vorzeichen des gemessenen AC-IN-Werts umkehren</label>
<p class="note">Für deinen direkten VE.Bus-Messwert ist die Umkehrung vorausgewählt: Rohwert positiv = Einspeisung, negativ = Netzbezug. Die Umkehrung betrifft nur die AC-Zähler, niemals die Batterie-kWh. Bei anderer Hardware die Richtung vor dem Start prüfen.</p>
<p class="note">Der Zähler integriert frische WR-Leistungswerte (normalerweise etwa alle 5 Sekunden). Lücken über 10 Sekunden werden ausgelassen. CSV: ein Momentanwert pro Minute, laufende AC-Wh, Batterie-kWh und getrennte gültige Messzeiten; maximal die letzten 1440 Einträge. Zeitangaben sind Sekunden seit Messbeginn.</p>
<p class="note">Starten und Beenden steuern nur die Aufzeichnung, nicht Laden oder Entladen. Ein neuer Lauf ersetzt den vorherigen. Die Daten bleiben beim Schließen des Browsers erhalten, gehen aber bei Neustart/Stromausfall verloren — CSV vorher herunterladen. Nach dem Ende bleiben die geladenen und entnommenen Batterie-kWh sichtbar. Ein neuer Lauf setzt beide Zähler zurück.</p></details></section>

<form id="cfg"><details class="card"><summary>Home Assistant &amp; MQTT</summary><div class="detail-body"><p class="note">Anbindung und Gerätekennung. Für die manuelle Web-Steuerung ist kein MQTT-Server erforderlich.</p><div class="grid2">
<label class="f">MQTT-Server<input name="host" required></label>
<label class="f">MQTT-Port<input name="port" type="number" min="1" max="65535"></label>
<label class="f">MQTT-Benutzer<input name="user" autocomplete="off"></label>
<label class="f">MQTT-Passwort<input name="pass" type="password" autocomplete="new-password" id="mpass"></label>
<label class="f">HA-Gerätekennung<input name="device" required></label>
<label class="f">MQTT-Themenpräfix<input name="prefix" required></label>
<label class="f">ESS-Sicherheitszeit (s, 0 = aus)<input name="ess_timeout" type="number" min="0" max="65535"></label>
</div>
<p class="note">Wenn innerhalb dieser Zeit kein neuer ESS-Sollwert eingeht, wird auf 0 W zurückgestellt. 0 W regelt den Netzfluss am AC-Eingang. Verbraucher am AC-Ausgang können dabei weiterhin die Batterie entladen.</p>
<label class="note"><input type="checkbox" name="clear_pass" value="1"> gespeichertes MQTT-Passwort löschen</label>
</div></details><details class="card"><summary>USV &amp; Restlaufzeit</summary><div class="detail-body">
<div class="row"><label class="note"><input type="checkbox" name="nut" value="1"> NUT-Server (TCP 3493)</label>
<label class="note"><input type="checkbox" name="apc" value="1"> apcupsd-Server (TCP 3551)</label></div>
<div class="grid2" style="margin-top:8px">
<label class="f">USV-Name<input name="nut_ups" maxlength="15" pattern="[A-Za-z0-9_.-]+"></label>
<label class="f">NUT-Benutzer (leer = keine Anmeldung)<input name="nut_user" autocomplete="off" maxlength="31"></label>
<label class="f">NUT-Passwort<input name="nut_pass" type="password" autocomplete="new-password" id="npass" maxlength="31"></label>
<label class="f">Niedriger Ladestand (%, 0 = nur LED)<input name="low_soc" type="number" min="0" max="100"></label>
<label class="f">Nennleistung (W, für Auslastung %, 0 = aus)<input name="nominal_w" type="number" min="0" max="65535"></label>
<label class="f">Nutzbare Batteriekapazität (Wh, für Laufzeit, 0 = aus)<input name="battery_wh" type="number" min="0" max="65535"></label>
</div>
<p class="note" id="nutHint">NUT-Clients (Synology, TrueNAS, Proxmox, upsmon, Home Assistant NUT) verbinden sich mit <code>&lt;ups&gt;@&lt;device-ip&gt;</code>; apcupsd-Clients (apcaccess, apcupsd, Home Assistant "APC UPS Daemon") mit <code>&lt;device-ip&gt;:3551</code>. Die eingetragene Batteriekapazität dient zur Schätzung der Restlaufzeit, nicht zur Kapazitätsmessung. OL = Netzbetrieb, OB = Batteriebetrieb, LB = niedriger Batteriestand. Die MultiPlus-Batteriewarnung oder die SoC-Schwelle löst LB und damit die Abschaltung der Clients aus.</p>
</div></details><details class="card"><summary>Zugang &amp; Admin-Passwort</summary><div class="detail-body"><div class="grid2">
<label class="f">Neues Passwort (Benutzer: admin)<input type="password" id="ap1" autocomplete="new-password" minlength="4" maxlength="31"></label>
<label class="f">Wiederholen<input type="password" id="ap2" autocomplete="new-password"></label></div>
<p class="note">Leer lassen, um das bisherige Passwort beizubehalten. Nach dem Speichern fragt dein Browser nach dem neuen Passwort.</p>
</div></details><div class="save-bar"><p class="note">Änderungen an MQTT, USV und Zugang gemeinsam speichern.</p><button class="primary" type="submit">Einstellungen speichern</button></div></form>

<details class="card"><summary>System &amp; Firmware</summary><div class="detail-body">
<label class="f">Firmware-Datei (.bin)<span class="row"><input type="file" id="fw" accept=".bin"><button id="fwBtn" onclick="upload()">Hochladen</button></span></label>
<progress id="prog" value="0" max="100" style="display:none;margin-top:8px"></progress>
<div class="row" style="margin-top:14px"><button onclick="confirm('Jetzt neu starten?')&&post('/admin/api/reboot').then(()=>toast('Neustart…')).catch(e=>toast('Fehler: '+e.message))">Neu starten</button>
<button class="danger" onclick="confirm('WLAN-Zugangsdaten löschen? Das Gerät öffnet anschließend das WLAN VEBus-Setup.')&&post('/admin/api/wifireset').then(()=>toast('WLAN zurückgesetzt — mit VEBus-Setup verbinden')).catch(e=>toast('Fehler: '+e.message))">WLAN zurücksetzen</button></div>
<p class="note" id="sys"></p></div></details>
<div class="toast" id="toast" role="status" aria-live="polite"></div></main>
<script>
const $=id=>document.getElementById(id);
const DE={ON:'Ein',OFF:'Aus',on:'Ein',off:'Aus',charger_only:'Nur Ladegerät',inverter_only:'Nur Wechselrichter',Unknown:'Unbekannt',Down:'Inaktiv',Startup:'Start',Off:'Aus',Slave:'Neben-Gerät','Invert Full':'Wechselrichter', 'Invert Half':'Wechselrichter Teillast','Invert AES':'Wechselrichter Sparmodus','Power Assist':'Leistungsunterstützung',Bypass:'Netzdurchleitung',Charge:'Laden',Init:'Initialisierung',Bulk:'Hauptladung',Absorption:'Absorptionsladung',Float:'Erhaltungsladung',Storage:'Lagerung',Equalise:'Ausgleichsladung','Bulk Stopped':'Hauptladung gestoppt','N/A':'Nicht verfügbar',ess_power:'ESS-Sollwert',switch_state:'Betriebsart',virtual_mode:'Batterieneutraler Modus',wakeup:'Einschalten',sleep:'Ausschalten',force_absorption:'Absorptionsladung',force_float:'Erhaltungsladung',force_equalise:'Ausgleichsladung'};
const de=v=>DE[v]??v;
function connection(s){const ok=s.sync==='ON';const n=$('busNotice');n.textContent=ok?'VE.Bus verbunden · Messwerte werden aktualisiert.':'Keine VE.Bus-Verbindung · Batterie-Messwerte sind nicht verfügbar. Verkabelung und Wechselrichter prüfen.';n.className='banner '+(ok?'connection':'');return ok}
document.querySelectorAll('nav a').forEach(a=>{if(a.pathname===location.pathname)a.setAttribute('aria-current','page')});


function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',3000)}
async function post(url,body){const r=await fetch(url,{method:'POST',body:new URLSearchParams(body||{})});
  const j=await r.json().catch(()=>({ok:r.ok}));if(!r.ok||j.ok===false)throw new Error(j.error||('HTTP '+r.status));return j}
function syncSoc(){const text=$('syncSoc').value.trim(),n=Number(text);if(!/^\d{1,3}$/.test(text)||!Number.isInteger(n)||n<0||n>100){toast('Bitte eine ganze Zahl von 0 bis 100 eingeben.');return;}if(confirm('Ist der tatsächliche Ladestand bekannt? Internen Victron-Ladestand auf '+n+' % setzen? Dies kann die ESS-Regelung beeinflussen.'))cmd('sync_soc',String(n));}
async function cmd(c,v){try{await post('/admin/api/control',{cmd:c,value:v??''});toast('Befehl gesendet: '+de(c)+' (Wirkung am Gerät prüfen)');setTimeout(state,1500)}catch(e){toast('Fehler: '+e.message)}}
async function state(){try{const s=await (await fetch('/api/state',{cache:'no-store'})).json();$('readOnlyNotice').style.display=s.read_only?'block':'none';if(s.read_only){document.querySelectorAll('button[onclick]').forEach(b=>{if((b.getAttribute('onclick')||'').startsWith('cmd('))b.disabled=true});['ess','sw','vm','acEnablePower','acApplyPower','acZeroPower'].forEach(id=>{if($(id))$(id).disabled=true})}
  const connected=connection(s);autoReadOnly=!!s.read_only;$('readOnlyNotice').style.display=s.read_only?'block':'none';$('liveVolt').textContent=connected?s.bat_volt.toFixed(2)+' V':'—';$('liveCurrent').textContent=connected?s.bat_current.toFixed(1)+' A':'—';$('liveState').textContent=connected?de(s.device_state):'Nicht verbunden';$('sub').textContent=`${s.device_id} · ${s.ip}`;$('pwBanner').style.display=s.default_pw?'block':'none';
  $('ctlState').textContent=`Aktuell: Sollwert ${s.ess_power} W (wirksam ${s.ess_power_eff} W) · Betriebsart ${de(s.switch_state)} · batterieneutral ${de(s.virtual_mode)} · Zustand ${de(s.device_state)}`;
  if(document.activeElement!==$('ess')&&$('ess').value==='')$('ess').value=s.ess_power;
  if(document.activeElement!==$('sw'))$('sw').value=s.switch_state;if(document.activeElement!==$('vm'))$('vm').value=s.virtual_mode;
  $('sys').textContent=`Laufzeit ${Math.floor(s.uptime/3600)} h · freier Speicher ${(s.free_heap/1024).toFixed(0)} kB · letzter Neustart: ${s.reset_reason} · WLAN ${s.rssi} dBm`}catch(e){$('busNotice').textContent='Gerät nicht erreichbar · Verbindung wird erneut geprüft.';$('liveVolt').textContent=$('liveCurrent').textContent=$('liveState').textContent='—';}}
async function loadCfg(){const c=await (await fetch('/admin/api/config',{cache:'no-store'})).json();const f=$('cfg');
  for(const k of ['host','port','user','device','prefix','ess_timeout','nut_ups','nut_user','low_soc','nominal_w'])f[k].value=c[k];
  f.nut.checked=c.nut;f.apc.checked=c.apc;f.battery_wh.value=c.battery_wh;
  $('mpass').placeholder=c.pass_set?'•••••• (unverändert)':'(keines)';
  $('npass').placeholder=c.nut_pass_set?'•••••• (unverändert)':'(keines)';
  $('nutHint').querySelector('code').textContent=c.nut_ups+'@'+location.hostname}
$('cfg').addEventListener('invalid',e=>{const d=e.target.closest('details');if(d)d.open=true},true);
$('cfg').onsubmit=async e=>{e.preventDefault();const f=new FormData(e.target);
  if($('ap1').value!==$('ap2').value){toast('Passwörter stimmen nicht überein');return}
  if($('ap1').value)f.append('admin_pass',$('ap1').value);
  try{await post('/admin/api/config',f);toast('Gespeichert — MQTT verbindet sich neu');$('ap1').value=$('ap2').value='';f.get('clear_pass')&&(e.target.clear_pass.checked=false);loadCfg();state()}catch(err){toast('Fehler: '+err.message)}};
let autoActive=false,autoReadOnly=false,autoConfigLoaded=false;
async function autoPoll(){try{const r=await fetch('/api/auto',{cache:'no-store'});if(!r.ok)throw Error();const a=await r.json();autoActive=a.enabled;
 const w=v=>Number(v).toLocaleString('de-DE')+' W';
 $('autoSummary').textContent='Max. laden '+w(a.charge_limit_w)+' · max. entladen '+w(a.discharge_limit_w)+' · '+a.share_pct+' % Restbedarf';
 if(!autoConfigLoaded){$('autoSharePct').value=a.share_pct;$('autoChargeLimit').value=a.charge_limit_w;$('autoDischargeLimit').value=a.discharge_limit_w;$('autoCutOn').checked=a.cutoff_v>0;$('autoCutV').value=a.cutoff_v||'';$('autoDeltaV').value=a.resume_delta_v;autoConfigLoaded=true;autoCutControls();}
 $('autoGuardStatus').textContent=a.cutoff_v>0?(a.discharge_blocked?'Entladesperre aktiv / Batteriespannung prüfen. ':'Entladen freigegeben. ')+'Sperre bei '+Number(a.cutoff_v).toLocaleString('de-DE')+' V · Freigabe ab '+Number(a.cutoff_v+a.resume_delta_v).toLocaleString('de-DE')+' V':'Zusätzliche Spannungssperre ausgeschaltet. Grenze passend zur Batteriekonfiguration eintragen.';
 $('autoLimitSave').disabled=autoReadOnly;

 $('autoStatus').textContent=a.status+' · Ziel '+w(a.target_w)+' · AC-IN-Sollwert '+w(a.setpoint_w);
 for(const [id,key] of [['autoDemand','demand_w'],['autoByd','byd_w'],['autoVic','victron_w']])$(id).textContent=a.fresh?w(a[key]):'—';
 $('autoOn').disabled=autoReadOnly||a.enabled;$('autoOff').disabled=autoReadOnly||!a.enabled;
 for(const id of ['ess','essSet','vm','vmSet'])$(id).disabled=autoReadOnly||a.enabled;
 if(a.enabled)$('acStart').disabled=true;
 }catch(e){$('autoStatus').textContent='Automatikstatus nicht erreichbar';$('autoOn').disabled=true;}}
async function autoAction(action){$('autoOn').disabled=$('autoOff').disabled=true;try{await post('/admin/api/auto',{action});toast(action==='on'?'Automatik eingeschaltet':'Automatik aus · Sollwert 0 W');}catch(e){toast('Fehler: '+e.message)}await autoPoll();await state();}
function autoCutControls(){const on=$('autoCutOn').checked;$('autoCutV').disabled=$('autoDeltaV').disabled=!on||autoReadOnly;$('autoCutV').required=on;}
$('autoSettings').onsubmit=async e=>{e.preventDefault();const share=Number($('autoSharePct').value),charge=Number($('autoChargeLimit').value),discharge=Number($('autoDischargeLimit').value),cut=$('autoCutOn').checked?Number($('autoCutV').value):0,delta=Number($('autoDeltaV').value);
 if(!Number.isInteger(share)||share<0||share>100||!Number.isInteger(charge)||charge<0||charge>32767||!Number.isInteger(discharge)||discharge<0||discharge>32767||!Number.isFinite(cut)||(cut!==0&&(cut<38||cut>60))||!Number.isFinite(delta)||delta<.1||delta>5){toast('Leistungs- und Spannungsgrenzen prüfen.');return;}
 $('autoLimitSave').disabled=true;try{await post('/admin/api/auto/config',{share_pct:share,charge_limit_w:charge,discharge_limit_w:discharge,cutoff_v:cut,resume_delta_v:delta});autoConfigLoaded=false;toast('Leistungsgrenzen und Spannungsschutz gespeichert.');}catch(err){toast('Fehler: '+err.message)}await autoPoll();};
let acCanSetPower=false;
function acPowerControls(){const enabled=acCanSetPower&&$('acEnablePower').checked;for(const id of ['acDirection','acWatts','acApplyPower','acZeroPower'])$(id).disabled=!enabled;}
async function acSetPower(zero){const watts=zero?0:Number($('acWatts').value);if((!zero&&$('acWatts').value.trim()==='')||!Number.isInteger(watts)||watts<0||watts>32767){toast('Bitte eine ganze Zahl von 0 bis 32767 W eingeben.');return;}
 $('acApplyPower').disabled=$('acZeroPower').disabled=true;
 try{await post('/admin/api/ac-log',{action:'power',direction:$('acDirection').value,watts});toast('ESS-Sollwert gesendet — tatsächliche Leistung prüfen.');setTimeout(state,1500);}catch(e){toast('Fehler: '+e.message)}await acPoll();}
async function acPoll(){try{const r=await fetch('/api/ac-log',{cache:'no-store'});if(!r.ok)throw new Error('HTTP '+r.status);const a=await r.json();
 const wh=v=>Number(v).toLocaleString('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1})+' Wh';
 acCanSetPower=a.running&&a.fresh&&!a.read_only&&!autoActive;acPowerControls();$('acPowerSetpoint').textContent='Vorgabe: '+a.setpoint_w+' W · wirksam: '+a.effective_w+' W · batterieneutral: '+(a.virtual_mode?'Ein':'Aus');$('acChargeW').textContent=a.power_w===null?'—':Math.max(0,a.power_w).toLocaleString('de-DE')+' W';$('acDischargeW').textContent=a.power_w===null?'—':Math.max(0,-a.power_w).toLocaleString('de-DE')+' W';$('acImport').textContent=wh(a.import_wh);$('acExport').textContent=wh(a.export_wh);$('acPower').textContent=a.power_w===null?'—':Number(a.power_w).toLocaleString('de-DE')+' W';
 const kwh=v=>Number(v).toLocaleString('de-DE',{minimumFractionDigits:3,maximumFractionDigits:3})+' kWh';
 $('batChargeKwh').textContent=kwh(a.battery_charge_kwh);$('batDischargeKwh').textContent=kwh(a.battery_discharge_kwh);$('batPower').textContent=a.battery_power_w===null?'—':Math.abs(a.battery_power_w).toLocaleString('de-DE')+' W · '+(a.battery_power_w>0?'Laden':a.battery_power_w<0?'Entladen':'Ruhe');
 const mins=s=>(s/60).toLocaleString('de-DE',{maximumFractionDigits:1})+' min';
 $('acStatus').textContent=(a.running?(a.fresh?'Messung läuft':'Messung läuft · wartet auf frische AC-IN-Werte'):a.started?'Messung beendet':'Noch kein Messlauf')+' · Laufzeit '+mins(a.elapsed_s)+' · erfasst '+mins(a.measured_s)+' · '+a.rows+' CSV-Einträge'+(a.rows_total>a.rows?' (ältere Einträge überschrieben)':'');
 $('batResult').textContent=(a.started?(a.running?'Bisher entnommen: ':'Testergebnis · entnommene Batterieenergie: ')+kwh(a.battery_discharge_kwh)+' · geladen: '+kwh(a.battery_charge_kwh)+' · Batterie-Messzeit '+mins(a.battery_measured_s):'Noch kein Batterietest.')+(a.running&&!a.battery_fresh?' · Batteriewerte fehlen, Zähler pausiert.':'')+(a.started&&a.elapsed_s-a.battery_measured_s>15?' · Messlücken: Ergebnis unvollständig.':'')+(a.started&&!a.running?' · Als nutzbare Kapazität nur bei vollständiger Entladung ab voller Batterie bewerten.':'');
 $('acStart').disabled=autoActive||a.running||!a.fresh||!a.battery_fresh;$('acStop').disabled=!a.running;$('acCsv').disabled=!a.rows;$('acInvert').disabled=a.running;
 if(a.running)$('acInvert').checked=a.inverted;
 }catch(e){acCanSetPower=false;acPowerControls();$('acChargeW').textContent=$('acDischargeW').textContent='—';$('acStatus').textContent='Messstatus nicht erreichbar · neuer Versuch folgt.';$('acStart').disabled=$('acStop').disabled=true;$('acPower').textContent=$('batPower').textContent='—';}}
async function acAction(action){if(action==='start'&&!confirm('Neuen Batterietest mit AC-IN-Messung starten? Die bisherige Aufzeichnung wird ersetzt. Der WR-Betrieb bleibt unverändert.'))return;
 $('acStart').disabled=$('acStop').disabled=true;
 try{await post('/admin/api/ac-log',{action,invert:$('acInvert').checked?'1':'0'});toast(action==='start'?'Aufzeichnung gestartet':'Aufzeichnung beendet — WR-Betrieb unverändert');}catch(e){toast('Fehler: '+e.message)}await acPoll();}
acPoll();setInterval(acPoll,5000);autoPoll();setInterval(autoPoll,5000);

function upload(){const file=$('fw').files[0];if(!file){toast('Zuerst eine .bin-Datei auswählen');return}
  const x=new XMLHttpRequest(),fd=new FormData(),p=$('prog');fd.append('firmware',file);
  x.upload.onprogress=e=>{p.style.display='block';p.value=e.loaded/e.total*100};
  x.onload=()=>{toast(x.status==200?'Update erfolgreich — Neustart…':'Update fehlgeschlagen: '+x.responseText);$('fwBtn').disabled=false;if(x.status==200)setTimeout(()=>location.href='/',15000)};
  x.onerror=()=>{toast('Hochladen fehlgeschlagen');$('fwBtn').disabled=false};
  $('fwBtn').disabled=true;x.open('POST','/admin/update');x.send(fd)}
loadCfg().catch(()=>toast('Einstellungen konnten nicht geladen werden. Seite erneut laden.'));state();setInterval(state,5000);

async function energyPoll(){try{const r=await fetch('/api/energy',{cache:'no-store'});if(!r.ok)throw Error();const a=await r.json();
 const kwh=v=>v===null||v===undefined?'—':Number(v).toLocaleString('de-DE',{minimumFractionDigits:3,maximumFractionDigits:3})+' kWh';
 for(const [id,key]of [['energyTodayCharge','today_charge_kwh'],['energyTodayDischarge','today_discharge_kwh'],['energyTotalCharge','total_charge_kwh'],['energyTotalDischarge','total_discharge_kwh']])$(id).textContent=kwh(a[key]);
 $('energyDate').textContent=a.time_valid?'Heute · '+a.date+' · Europe/Berlin':'Uhrzeit wird synchronisiert · Tageswerte noch nicht zugeordnet';
 $('energyNote').textContent=(a.fresh?'Zähler aktiv':'Keine frischen Batteriewerte · Zähler pausiert')+' · heute '+(a.today_measured_s/3600).toLocaleString('de-DE',{maximumFractionDigits:2})+' h erfasst'+(!a.storage_ok?' · Speichern fehlgeschlagen!':'')+((a.unassigned_charge_kwh+a.unassigned_discharge_kwh)>0?' · Energie ohne gültige Uhrzeit ist nur im Gesamtzähler enthalten.':'');
 $('energyMqtt').textContent=a.mqtt_topic;
 $('energyDays').innerHTML=a.days.length?[...a.days].reverse().map(d=>'<tr><td>'+String(d.date).replace(/[^0-9-]/g,'')+'</td><td>'+Number(d.charged_kwh).toLocaleString('de-DE',{minimumFractionDigits:3,maximumFractionDigits:3})+'</td><td>'+Number(d.discharged_kwh).toLocaleString('de-DE',{minimumFractionDigits:3,maximumFractionDigits:3})+'</td><td>'+(d.measured_s/3600).toLocaleString('de-DE',{maximumFractionDigits:2})+'</td></tr>').join(''):'<tr><td colspan="4">Noch keine Tagesaufzeichnung.</td></tr>';
 }catch(e){$('energyNote').textContent='Energiestatistik nicht erreichbar · erneuter Versuch folgt.';}}
energyPoll();setInterval(energyPoll,15000);
</script></body></html>)rawliteral";
