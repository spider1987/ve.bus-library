const fs=require('node:fs'),assert=require('node:assert/strict');
const s=fs.readFileSync('examples/mqtt_ha/main.cpp','utf8');
const loop=s.slice(s.indexOf('void loop()'));
assert(loop.indexOf('now = millis();',loop.indexOf('autoLoop(millis());'))<loop.indexOf('EssTimeout::expired(now,g_lastEssCmdMs,cfg.essTimeoutS)'));
const auto=s.slice(s.indexOf('static void autoLoop'),s.indexOf('static void autoLoop')+18000);
assert(!auto.includes('autoReason="Pausiert · MQTT / evcc offline"'));
assert(auto.includes('AutoPower::holdEvcc(autoHaveSample,now-autoSample.atMs,autoCfg.dataTimeoutS)'));
assert(auto.includes('if(!acFresh(now)||!batFresh(now)||!vebus.isTxEnabled())'));
console.log('ESS integration: current watchdog time, configured MQTT hold, immediate local protection OK');
