/**
 * mqtt_ha — VEBus → MQTT → Home Assistant auto-discovery + web dashboard
 *
 * - Publishes all Multiplus data as one JSON state message; HA entities are
 *   created via MQTT discovery (sensors, ESS number, switch-state select,
 *   battery-neutral switch, buttons, diagnostics).
 * - Built-in web UI on http://<device-ip>/ (or http://<hostname>.local/):
 *     /        live dashboard + 24 h history charts (public, read-only)
 *     /admin/  controls, settings, firmware update (web login/session cookie,
 *              user "admin", default password "vebus" — change it!)
 * - Self-healing: task watchdog on the main loop, WiFi supervisor, MQTT
 *   reconnect with short timeouts, optional ESS setpoint fail-safe.
 * - NUT server (Network UPS Tools, TCP 3493): NAS/servers/HA can monitor
 *   the Multiplus as a UPS and shut down on low battery.
 * - apcupsd network server (TCP 3551): apcaccess, apcupsd slaves and the
 *   Home Assistant "APC UPS Daemon" integration see it as an APC UPS.
 *
 * Hardware: LilyGo T-CAN485 (ESP32, MAX13487E RS485 transceiver)
 *
 * Source:  https://github.com/mkrasselt1/ve.bus-library
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <stdarg.h>
#include <VEBus.h>
#include "web_ui.h"
#include "AcEnergy.h"
#include "BatteryEnergy.h"
#include "StatusLed.h"
#include "AutoPower.h"
#include "EnergyBook.h"
#include "WiFiRecovery.h"
#include <sys/time.h>
#include <mbedtls/sha256.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <VEBusScaler.h>           // optional: raw RAM values → real units
#include <VEBusUps.h>              // optional UPS add-ons of the library
#include <VEBusNutServer.h>
#include <VEBusApcupsdServer.h>
#include <time.h>

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// =======================================================================
// Defaults — used the first time the device boots (no NVS entry yet) and
// pre-fill the captive portal form. Afterwards everything is editable at
// http://<device-ip>/admin/.
// =======================================================================
#define DEFAULT_MQTT_HOST     "192.0.2.100"
#define DEFAULT_MQTT_PORT     1883
#define DEFAULT_MQTT_USER     ""
#define DEFAULT_MQTT_PASS     ""
#define DEFAULT_DEVICE_ID     "vebus_multiplus"
#define DEFAULT_TOPIC_PREFIX  "vebus/multiplus"
#define DEFAULT_ADMIN_PASS    "vebus"
#define ADMIN_USER            "admin"
#define DEFAULT_NUT_UPS       "multiplus"
#define DEFAULT_LOW_SOC       20      // % - below this NUT reports LB (low battery)
#define AP_SSID               "VEBus-Setup"
#define AP_PASS               ""    // open AP

// T-CAN485 pins
#define VEBUS_PIN_RX   21
#define VEBUS_PIN_TX   22
#define VEBUS_PIN_RE   17
#define VEBUS_PIN_SHDN 19
#define VEBUS_PIN_POWER 16  // ME2107: RS485/CAN transceiver supply enable

// T-CAN485 WS2812B, GPIO4; library uses ESP32 RMT to preserve UART timing.
static Adafruit_NeoPixel statusPixel(1, 4, NEO_GRB + NEO_KHZ800);
static LedStatus ledStatus = LedStatus::Startup;
static uint32_t ledSince = 0, ledLastRgb = UINT32_MAX;
static bool ledUpdating = false;
static void showStatusLed(LedStatus state, uint32_t now) {
    if (state != ledStatus) { ledStatus = state; ledSince = now; }
    const uint32_t rgb = statusLedRGB(state, now - ledSince);
    if (rgb == ledLastRgb) return; // only transmit to WS2812 on a visible change
    statusPixel.setPixelColor(0, rgb);
    statusPixel.show();
    ledLastRgb = rgb;
}

// Timing
#define PUBLISH_INTERVAL_MS  5000
#define ESS_INTERVAL_MS      5000
#define RAM_OFFSET_MS         500
#define EXT_RAM1_OFFSET_MS   1000   // 1st extended RAM read
#define EXT_RAM2_OFFSET_MS   2000   // 2nd extended RAM read
#define STATE_OFFSET_MS      3000   // device state request
#define MQTT_RECONNECT_MS    5000
#define WAKEUP_RETRY_MS      3000

// Robustness
#define WDT_TIMEOUT_S          30        // main loop must check in this often
// WiFi recovery timings and checks are defined in WiFiRecovery.h.

// History (for the dashboard charts): one averaged sample per minute, 24 h
#define HIST_INTERVAL_MS    60000UL
#define HIST_LEN             1440
#define HIST_GAP            INT16_MIN

// =======================================================================
// Globals
// =======================================================================
struct Config {
    char     mqttHost[64];
    uint16_t mqttPort;
    char     mqttUser[32];
    char     mqttPass[64];
    char     deviceId[32];
    char     prefix[64];
    char     adminPass[32];
    uint16_t essTimeoutS;       // 0 = off; else setpoint falls back to 0 W
    bool     nutEnabled;
    char     nutUps[16];        // UPS name clients use, e.g. multiplus@<ip>
    char     nutUser[32];       // empty = no authentication
    char     nutPass[32];
    uint8_t  lowSoc;            // % SoC that triggers LB (0 = LED only)
    uint16_t nominalW;          // for ups.load (0 = don't report)
    bool     apcEnabled;        // apcupsd NIS server on port 3551
    uint16_t batteryWh;         // usable capacity for runtime estimate (0 = unknown)
} cfg;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
VEBus        vebus;
WiFiManager  wm;
Preferences  prefs;
WebServer    server(80);
static bool wifiPortalOwnsHttp = false;
VEBusScaler        scaler(vebus); // RAM scale/offset, queried from the Multiplus
VEBusUps           ups(vebus);   // UPS view: status, runtime, statistics
VEBusNutServer     nut(ups);     // NUT, TCP 3493
VEBusApcupsdServer apc(ups);     // apcupsd, TCP 3551

char hostName[32];              // deviceId sanitised for DHCP / mDNS

// Live values from the extended RAM reads (real units) — stored in the UPS
// helper so NUT/apcupsd see them too.
VEBusUpsData &live = ups.data;

// Manual AC-IN measurement. RAM only; browser disconnect does not stop it.
static AcEnergy acEnergy;
static BatteryEnergy batEnergy;
static bool batHaveSample = false;
static uint32_t batSampleMs = 0;
static float batSampleV = 0, batSampleA = 0;
static bool acRunning = false, acStarted = false, acInvert = false;
static bool acHaveMains = false;
static uint32_t acMainsMs = 0, acStartMs = 0, acStopMs = 0, acRowMs = 0;
struct AcRow { uint32_t elapsedS, measuredS; float watts, importWh, exportWh;
    uint32_t batMeasuredS; float batV, batA, batW, batChargeWh, batDischargeWh; };
static const uint16_t AC_ROWS = 1440;
static AcRow *acRows = nullptr; // large CSV ring belongs in heap, not static DRAM
static uint16_t acHead = 0, acCount = 0;
static uint32_t acRowsTotal = 0;
static bool acFresh(uint32_t now) {
    return acHaveMains && !vebus.hasNoSync() && now - acMainsMs <= 10000;
}
static bool batFresh(uint32_t now) {
    return batHaveSample && !vebus.hasNoSync() && now - batSampleMs <= 10000;
}
static void acRecord(uint32_t now) {
    if (!acRows) return;
    acRows[acHead] = { (now - acStartMs) / 1000, acEnergy.measuredMs / 1000,
                      acFresh(now) ? live.mainsW * (acInvert ? -1 : 1) : NAN,
                      (float)acEnergy.importWh, (float)acEnergy.exportWh,
                      batEnergy.measuredMs / 1000,
                      batFresh(now) ? batSampleV : NAN, batFresh(now) ? batSampleA : NAN,
                      batFresh(now) ? batSampleV * batSampleA : NAN,
                      (float)batEnergy.importWh, (float)batEnergy.exportWh };
    acHead = (acHead + 1) % AC_ROWS;
    if (acCount < AC_ROWS) ++acCount;
    ++acRowsTotal;
    acRowMs = now;
}


int16_t  g_essPower       = 0;
uint32_t g_lastEssCmdMs   = 0;
int      g_devState       = -1;
int      g_devSubState    = -1;
char     g_fwVersion[16]  = "";
uint32_t g_mqttReconnects = 0;
uint32_t g_wifiReconnects = 0;
bool     g_publishNow     = false;
uint8_t  g_discIdx        = 0xFF;    // next discovery entity (0xFF = done)
uint32_t g_restartAtMs    = 0;
bool     g_otaAuthed      = false;
bool     g_essDirty       = false;   // ESS setpoint / virtual mode not yet in NVS
uint32_t g_essSavedMs     = 0;

struct AutoSettings { uint16_t chargeW=3000, dischargeW=3000; bool sleepAtCutoff=false; uint8_t bydMinSoc=0; uint16_t wakeMinW=200, wakeDelayS=30; uint8_t sharePct=90; uint16_t bydThresholdW=100, dataTimeoutS=35; float cutoffV=0, resumeDeltaV=1; } autoCfg;
static bool autoLowLock=false, autoSleeping=false;
static uint32_t autoSwitchMs=0, autoWakeMs=0;
static AutoPower::SurplusTimer autoSurplusTimer;
static void saveAutoSleep(bool locked) {
    if(autoLowLock==locked)return;
    autoLowLock=locked;
    Preferences sp;sp.begin("vebus_mqtt",false);sp.putBool("autolow",locked);sp.end();
}
static bool voltageBlocked = false;
static bool dischargeBlocked() { return autoLowLock || (autoCfg.cutoffV>0 && (voltageBlocked || !batFresh(millis()))); }
static void updateVoltageGuard() {
    if (autoCfg.cutoffV<=0) voltageBlocked=false;
    else if(batFresh(millis())) voltageBlocked=AutoPower::voltageLatch(voltageBlocked,batSampleV,autoCfg.cutoffV,autoCfg.resumeDeltaV);
    if(dischargeBlocked() && (g_essPower>0 || vebus.getEffectiveESSPower()>0)) {
        g_essPower=0;vebus.enableVirtualSetpointMode(false);vebus.setESSPower(0);g_publishNow=true;
    }
}

static EnergyBook energyBook;
static bool energyDirty=false, energyHaveSample=false, energyStorageOk=true, energyRollover=false;
static uint32_t energySavedMs=0, energyLastMs=0, energyLastDate=0;
static uint64_t energyLastEpochMs=0;
static double energyLastW=0;
static uint32_t localDate(time_t seconds) {
    if(seconds<1704067200 || seconds>4102444799LL) return 0;
    struct tm local;localtime_r(&seconds,&local);
    return (local.tm_year+1900)*10000+(local.tm_mon+1)*100+local.tm_mday;
}
static void dateText(uint32_t date,char *out,size_t size) {
    if(!date){if(size)out[0]=0;return;}
    snprintf(out,size,"%04u-%02u-%02u",date/10000,(date/100)%100,date%100);
}
static uint64_t epochMillis() { struct timeval tv;gettimeofday(&tv,nullptr);return (uint64_t)tv.tv_sec*1000+tv.tv_usec/1000; }
static void loadEnergy() {
    Preferences ep;if(!ep.begin("energy_v1",true))return;
    if(ep.getBytesLength("book")==sizeof(EnergyBook)) {
        EnergyBook candidate;
        if(ep.getBytes("book",&candidate,sizeof(candidate))==sizeof(candidate) && candidate.valid())energyBook=candidate;
        else energyStorageOk=false;
    }
    ep.end();energySavedMs=millis();
}
static void saveEnergy(bool force=false) {
    if(!energyDirty || (!force && !energyRollover && millis()-energySavedMs<600000))return;
    Preferences ep;
    if(ep.begin("energy_v1",false)) {
        energyStorageOk=ep.putBytes("book",&energyBook,sizeof(energyBook))==sizeof(energyBook);ep.end();
    } else energyStorageOk=false;
    energySavedMs=millis();if(energyStorageOk){energyDirty=false;energyRollover=false;}
}
static void sampleEnergy(uint32_t now,float volts,float amps) {
    const uint64_t epoch=epochMillis();const uint32_t date=localDate(epoch/1000);
    const double watts=(double)volts*amps;
    if(!isfinite(watts) || volts<=0){energyHaveSample=false;return;}
    if(date && !energyBook.find(date)){energyBook.day(date);energyDirty=true;energyRollover=true;}
    if(!energyHaveSample && energyLastMs && now-energyLastMs>10000){energyBook.gapMs+=now-energyLastMs;energyDirty=true;}
    if(energyHaveSample) {
        const uint32_t dt=now-energyLastMs;
        if(dt && dt<=10000) {
            const int64_t clockDt=(int64_t)epoch-(int64_t)energyLastEpochMs;
            uint32_t previousDate=energyLastDate,currentDate=date;double split=-1;
            if(!previousDate || !currentDate || llabs(clockDt-(int64_t)dt)>2000) {previousDate=0;currentDate=0;}
            else if(previousDate!=currentDate && clockDt>0) {
                time_t seconds=epoch/1000;struct tm midnight;localtime_r(&seconds,&midnight);
                midnight.tm_hour=midnight.tm_min=midnight.tm_sec=0;midnight.tm_isdst=-1;
                const uint64_t boundary=(uint64_t)mktime(&midnight)*1000;
                split=((double)boundary-(double)energyLastEpochMs)/(double)clockDt;
            }
            energyBook.add(previousDate,currentDate,split,dt,energyLastW,watts);energyDirty=true;
        } else if(dt>10000){energyBook.gapMs+=dt;energyDirty=true;}
    }
    energyLastMs=now;energyLastEpochMs=epoch;energyLastDate=date;energyLastW=watts;energyHaveSample=true;
}
static void dayStartText(char *out,size_t size) {
    time_t now=time(nullptr);if(!localDate(now)){if(size)out[0]=0;return;}
    struct tm midnight;localtime_r(&now,&midnight);midnight.tm_hour=midnight.tm_min=midnight.tm_sec=0;midnight.tm_isdst=-1;
    time_t start=mktime(&midnight);struct tm utc;gmtime_r(&start,&utc);strftime(out,size,"%Y-%m-%dT%H:%M:%SZ",&utc);
}
static const EnergyDay *todayEnergy() { return energyBook.find(localDate(time(nullptr))); }
static const EnergyDay *yesterdayEnergy() {
    time_t now=time(nullptr);if(!localDate(now))return nullptr;
    struct tm yesterday;localtime_r(&now,&yesterday);yesterday.tm_mday--;yesterday.tm_hour=12;yesterday.tm_isdst=-1;
    return energyBook.find(localDate(mktime(&yesterday)));
}

// evcc snapshots are read on a separate task: HTTP never blocks VE.Bus/UI.
struct AutoSnapshot { bool valid, solarValid, bydSocValid; float bydSoc; uint32_t atMs; int gridW, otherW, victronW, demandW, solarResidualW; };
static QueueHandle_t autoQueue = nullptr;
static bool autoEnabled = false, autoHaveSample = false;
static AutoSnapshot autoSample = {};
static const char *autoReason = "Aus";
static int autoWanted = 0;
static const char *AUTO_URL = "http://192.168.178.2:7070/api/state";

static void autoWorker(void *) {
    for (;;) {
        AutoSnapshot snap = {};
        if (WiFi.status() == WL_CONNECTED) {
            WiFiClient client;
            HTTPClient http;
            http.useHTTP10(true);
            http.setConnectTimeout(1500);
            http.setTimeout(2000);
            if (http.begin(client, AUTO_URL)) {
                if (http.GET() == 200 && http.getSize() <= 65536) {
                    StaticJsonDocument<512> filter;
                    filter["grid"]["power"] = true;
                    filter["homePower"] = true; filter["pvPower"] = true;
                    filter["loadpoints"][0]["chargePower"] = true;
                    filter["battery"]["devices"][0]["power"] = true;
                    filter["battery"]["devices"][0]["soc"] = true;
                    DynamicJsonDocument doc(4096);
                    const DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
                    JsonArray devices = doc["battery"]["devices"].as<JsonArray>();
                    // Verified installation: device 1 = GoodWe/BYD, device 2 = LilyGo.
                    if (!err && devices.size() == 2 && doc["grid"]["power"].is<float>() &&
                        devices[0]["power"].is<float>() && devices[1]["power"].is<float>()) {
                        const float grid = doc["grid"]["power"], other = devices[0]["power"], vic = devices[1]["power"];
                        if (isfinite(grid) && isfinite(other) && isfinite(vic) &&
                            fabsf(grid) < 100000 && fabsf(other) < 100000 && fabsf(vic) < 100000) {
                            snap.gridW = lroundf(grid); snap.otherW = lroundf(other); snap.victronW = lroundf(vic);
                            // Grid + all batteries = consumption minus PV (includes EV loads).
                            snap.demandW = snap.gridW + snap.otherW + snap.victronW;
                            snap.valid = true;
                            if(devices[0]["soc"].is<float>()) {
                                const float soc=devices[0]["soc"];
                                snap.bydSocValid=isfinite(soc)&&soc>=0&&soc<=100;
                                if(snap.bydSocValid)snap.bydSoc=soc;
                            }
                            // Explicit solar gate: include EV consumption, never substitute missing values.
                            if(doc["homePower"].is<float>() && doc["pvPower"].is<float>() && doc["loadpoints"].is<JsonArray>()) {
                                float home=doc["homePower"], pv=doc["pvPower"], ev=0;
                                bool ok=isfinite(home) && isfinite(pv) && home>=0 && pv>=0 && home<100000 && pv<100000;
                                for(JsonObject lp: doc["loadpoints"].as<JsonArray>()) {
                                    if(!lp["chargePower"].is<float>()){ok=false;break;}
                                    float w=lp["chargePower"]; if(!isfinite(w) || w<0 || w>=100000){ok=false;break;} ev+=w;
                                }
                                snap.solarValid=ok && ev<100000;
                                if(snap.solarValid)snap.solarResidualW=lroundf(home+ev-pv);
                            }
                        }
                    }
                }
                http.end();
            }
        }
        snap.atMs = millis();
        xQueueOverwrite(autoQueue, &snap);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void autoSetPower(int watts) {
    g_essPower = (int16_t)AutoPower::bounded(watts,autoCfg.chargeW,autoCfg.dischargeW);
    g_lastEssCmdMs = millis();
    vebus.setESSPower(g_essPower);
    g_publishNow = true;
}
static void autoStop() {
    autoEnabled = false; autoWanted = 0; autoReason = "Aus · Sollwert 0 W";
    autoSetPower(0);
}
// Only an automatic low-voltage shutdown is automatically reversed.
static void autoSleep(uint32_t now) {
    autoWanted=0;autoSetPower(0);
    saveAutoSleep(true);autoSleeping=true;autoSwitchMs=now;
    autoSurplusTimer.reset();autoWakeMs=now;
    vebus.setSwitchState(VEBUS_SWITCH_STATE_OFF);
    autoReason="Mindestspannungs-Abschaltung angefordert · warte auf PV und BYD-Freigabe";
}
static void autoLoop(uint32_t now) {
    AutoSnapshot next;
    const bool updated=autoQueue && xQueueReceive(autoQueue,&next,0)==pdTRUE;
    const bool complete=updated && next.valid && next.solarValid;
    if(complete){autoSample=next;autoHaveSample=true;}
    if(!autoEnabled)return;
    // Fresh voltage, not a stale/no-data guard, may trigger a device shutdown.
    if(autoCfg.sleepAtCutoff && autoCfg.cutoffV>0 && !autoLowLock && batFresh(now) &&
       batSampleV<=autoCfg.cutoffV && (vebus.getSwitchRegister()&(VEBUS_SWITCH_CHARGE|VEBUS_SWITCH_INVERT)) && vebus.isTxEnabled()) { autoSleep(now);return; }
    if(autoLowLock && autoSleeping) {
        autoWanted=0;if(g_essPower)autoSetPower(0);
        // OFF may stop AC/voltage reports. Wake uses fresh external PV balance only.
        const bool ready=AutoPower::wakeReady(autoEnabled,complete,complete?now-next.atMs:0,
            complete&&next.solarValid,complete?next.demandW:0,complete?next.solarResidualW:0,
            complete?next.otherW:0,autoCfg.bydThresholdW,autoCfg.bydMinSoc,
            complete&&next.bydSocValid,complete?next.bydSoc:0,autoCfg.wakeMinW,autoCfg.chargeW);
        if(updated && !ready)autoSurplusTimer.reset();
        if(!autoHaveSample || now-autoSample.atMs>7500)autoSurplusTimer.reset();
        const bool stable=ready && autoSurplusTimer.observe(true,now,autoCfg.wakeDelayS);
        autoReason=autoSurplusTimer.tracking?"Warte · PV-Überschuss stabilisieren":"Mindestspannungs-Abschaltung · warte auf PV / BYD-Mindestladestand";
        if(stable &&
           now-autoWakeMs>=30000 && vebus.isTxEnabled()) {
            autoSetPower(0);vebus.enableVirtualSetpointMode(false);
            vebus.setSwitchState(VEBUS_SWITCH_STATE_ON);
            autoSleeping=false;autoWakeMs=now;autoSurplusTimer.reset();
            autoReason="Einschalten angefordert · warte auf frische VE.Bus-Werte";
        } else if(vebus.isTxEnabled() && now-autoSwitchMs>=15000) {
            vebus.setSwitchState(VEBUS_SWITCH_STATE_OFF);autoSwitchMs=now;
        }
        return;
    }
    if(autoLowLock && ((int32_t)(batSampleMs-autoWakeMs)<=0 || (int32_t)(acMainsMs-autoWakeMs)<=0 ||
       (vebus.getSwitchRegister() & (VEBUS_SWITCH_CHARGE|VEBUS_SWITCH_INVERT))!=(VEBUS_SWITCH_CHARGE|VEBUS_SWITCH_INVERT))) {
        autoWanted=0;if(g_essPower)autoSetPower(0);
        autoReason="Einschalten angefordert · warte auf Bestätigung und neue Messwerte";
        if(now-autoWakeMs>=20000 && vebus.isTxEnabled())autoSleep(now);
        return;
    }
    // Recover only after actual charging has raised battery voltage; no voltage rebound wake.
    if(autoLowLock && batFresh(now) && batSampleA>0.5f && batSampleV>=autoCfg.cutoffV+autoCfg.resumeDeltaV)
        saveAutoSleep(false);
    if(!acFresh(now)||!batFresh(now)||!vebus.isTxEnabled()) {
        autoWanted=0;if(g_essPower)autoSetPower(0);
        autoReason="Pausiert · VE.Bus-Messwerte fehlen";
        if(autoLowLock && now-autoWakeMs>=20000 && vebus.isTxEnabled())autoSleep(now);
        return;
    }
    if(!AutoPower::holdEvcc(autoHaveSample,now-autoSample.atMs,autoCfg.dataTimeoutS)) {
        autoWanted=0;if(g_essPower)autoSetPower(0);
        autoReason="Pausiert · evcc-Datenzeit abgelaufen";
        if(autoLowLock)autoSleep(now);
        return;
    }
    // A fresh BYD power/SoC rejection reduces charge immediately, even if other fields are missing.
    if(updated && next.valid && g_essPower<0 &&
       (next.otherW>autoCfg.bydThresholdW || !AutoPower::priorityReady(autoCfg.bydMinSoc,next.bydSocValid,next.bydSoc))) {
        autoWanted=0;autoSetPower(0);autoReason="Pausiert · BYD-Ladepriorität / Entladung";
        if(autoLowLock)autoSleep(now);
        return;
    }
    if(!complete){g_lastEssCmdMs=now;if(now-autoSample.atMs>7500)autoReason="Warte auf evcc · letzten Sollwert halten";return;}
    int wanted=AutoPower::target(autoSample.demandW,autoSample.otherW,autoCfg.chargeW,autoCfg.dischargeW,autoCfg.sharePct,autoCfg.bydThresholdW);
    wanted=AutoPower::solarGate(wanted,autoSample.solarValid,autoSample.solarResidualW,autoSample.otherW,autoCfg.chargeW,autoCfg.bydThresholdW);
    const bool priorityStop=wanted<0 && !AutoPower::priorityReady(autoCfg.bydMinSoc,autoSample.bydSocValid,autoSample.bydSoc);
    wanted=AutoPower::chargePriority(wanted,autoCfg.bydMinSoc,autoSample.bydSocValid,autoSample.bydSoc);
    const bool voltageStop=wanted>0 && (dischargeBlocked()||autoLowLock);
    autoWanted=voltageStop?0:wanted;
    // A low-voltage wake is for solar charging only; stop again when usable surplus disappears.
    if(autoLowLock && autoWanted>=0){autoSleep(now);return;}
    autoReason=priorityStop?"Warte · BYD-Mindestladestand nicht erreicht / fehlt":voltageStop?"Entladesperre · Batteriespannung":
        autoWanted>0?"Entladen · eingestellter Victron-Anteil":autoWanted<0?"Laden · PV-Überschuss":
        autoSample.otherW>autoCfg.bydThresholdW?"Pausiert · BYD entlädt":"Bereit · kein freigegebener Bedarf / Überschuss";
    autoSetPower(AutoPower::approach(g_essPower,autoWanted));
}

// Breadcrumb: which part of loop() was running. Survives a watchdog reset
// (RTC memory) so the dashboard can show where the firmware got stuck.
enum Stage : uint8_t { ST_NONE, ST_WIFI, ST_WEB, ST_MQTT, ST_DISCOVERY, ST_UPS,
                       ST_VEBUS, ST_PUBLISH, ST_HISTORY, ST_SERIAL, ST_OTA };
static const char *STAGE_NAMES[] = { "-", "wifi", "web server", "mqtt", "ha discovery",
                                     "nut/apcupsd", "ve.bus", "mqtt publish", "history",
                                     "serial", "ota" };
RTC_NOINIT_ATTR static uint32_t g_stageMagic;
RTC_NOINIT_ATTR static uint8_t  g_stage;
static uint8_t g_crashStage = ST_NONE;      // stage before a watchdog/panic reset
#define STAGE(s) (g_stage = (s))

char     payloadBuf[3072];

static void busQuiet();
static void busResume();

// History ring buffer
struct HistSample { int16_t out, mains, ess, batv, soc, bata; };
HistSample hist[HIST_LEN];
uint16_t   histHead  = 0;   // next write index
uint16_t   histCount = 0;
uint32_t   histLastMs = 0;
int32_t    histAcc[6] = {0};
uint16_t   histAccN   = 0;

// =======================================================================
// Small helpers
// =======================================================================

// Bounded JSON/text builder — never writes past the buffer.
struct Buf {
    char  *b;
    size_t cap, len;
    Buf(char *buf, size_t n) : b(buf), cap(n), len(0) { b[0] = '\0'; }
    void add(const char *fmt, ...)
    {
        if (len >= cap - 1) return;
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(b + len, cap - len, fmt, ap);
        va_end(ap);
        if (n > 0) len = min(cap - 1, len + (size_t)n);
    }
    void str(const char *s)     // JSON-escaped string incl. quotes
    {
        add("\"");
        for (; *s && len < cap - 2; s++) {
            if (*s == '"' || *s == '\\') add("\\%c", *s);
            else if ((uint8_t)*s < 0x20) add(" ");
            else { b[len++] = *s; b[len] = '\0'; }
        }
        add("\"");
    }
};

static const char *deviceStateName(int state)
{
    switch (state) {
    case VEBUS_STATE_DOWN:         return "Down";
    case VEBUS_STATE_STARTUP:      return "Startup";
    case VEBUS_STATE_OFF:          return "Off";
    case VEBUS_STATE_SLAVE:        return "Slave";
    case VEBUS_STATE_INVERT_FULL:  return "Invert Full";
    case VEBUS_STATE_INVERT_HALF:  return "Invert Half";
    case VEBUS_STATE_INVERT_AES:   return "Invert AES";
    case VEBUS_STATE_POWER_ASSIST: return "Power Assist";
    case VEBUS_STATE_BYPASS:       return "Bypass";
    case VEBUS_STATE_CHARGE:       return "Charge";
    default:                       return "Unknown";
    }
}

static const char *chargeSubStateName(int sub)
{
    switch (sub) {
    case VEBUS_CHARGE_INIT:                return "Init";
    case VEBUS_CHARGE_BULK:                return "Bulk";
    case VEBUS_CHARGE_ABSORPTION:          return "Absorption";
    case VEBUS_CHARGE_FLOAT:               return "Float";
    case VEBUS_CHARGE_STORAGE:             return "Storage";
    case VEBUS_CHARGE_REPEATED_ABSORPTION: return "Repeat Abs";
    case VEBUS_CHARGE_FORCED_ABSORPTION:   return "Forced Abs";
    case VEBUS_CHARGE_EQUALISE:            return "Equalise";
    case VEBUS_CHARGE_BULK_STOPPED:        return "Bulk Stopped";
    default:                               return "Unknown";
    }
}

static const char *resetReasonName()
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power-on";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "panic/exception";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "other watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP:return "deep sleep";
    case ESP_RST_EXT:      return "external pin";
    default:               return "unknown";
    }
}

static const char *switchStateName()
{
    byte sw = vebus.getSwitchRegister();
    bool chg = sw & VEBUS_SWITCH_CHARGE;
    bool inv = sw & VEBUS_SWITCH_INVERT;
    if (chg && inv) return "on";
    if (chg)        return "charger_only";
    if (inv)        return "inverter_only";
    return "off";
}


static bool isDefaultPassword() { return strcmp(cfg.adminPass, DEFAULT_ADMIN_PASS) == 0; }

// =======================================================================
// Config persistence (NVS via Preferences)
// =======================================================================
static void loadConfig()
{
    prefs.begin("vebus_mqtt", true);
    strlcpy(cfg.mqttHost,  prefs.getString("host",   DEFAULT_MQTT_HOST).c_str(),    sizeof(cfg.mqttHost));
    cfg.mqttPort =         prefs.getUShort("port",   DEFAULT_MQTT_PORT);
    strlcpy(cfg.mqttUser,  prefs.getString("user",   DEFAULT_MQTT_USER).c_str(),    sizeof(cfg.mqttUser));
    strlcpy(cfg.mqttPass,  prefs.getString("pass",   DEFAULT_MQTT_PASS).c_str(),    sizeof(cfg.mqttPass));
    strlcpy(cfg.deviceId,  prefs.getString("device", DEFAULT_DEVICE_ID).c_str(),    sizeof(cfg.deviceId));
    strlcpy(cfg.prefix,    prefs.getString("prefix", DEFAULT_TOPIC_PREFIX).c_str(), sizeof(cfg.prefix));
    strlcpy(cfg.adminPass, prefs.getString("admin",  DEFAULT_ADMIN_PASS).c_str(),   sizeof(cfg.adminPass));
    cfg.essTimeoutS =      prefs.getUShort("esstmo", 0);
    cfg.nutEnabled  =      prefs.getBool("nut", true);
    strlcpy(cfg.nutUps,    prefs.getString("nutups",  DEFAULT_NUT_UPS).c_str(), sizeof(cfg.nutUps));
    strlcpy(cfg.nutUser,   prefs.getString("nutuser", "").c_str(),              sizeof(cfg.nutUser));
    strlcpy(cfg.nutPass,   prefs.getString("nutpass", "").c_str(),              sizeof(cfg.nutPass));
    cfg.lowSoc      =      prefs.getUChar("lowsoc",   DEFAULT_LOW_SOC);
    cfg.nominalW    =      prefs.getUShort("nomw",    0);
    cfg.apcEnabled  =      prefs.getBool("apc",       true);
    cfg.batteryWh   =      prefs.getUShort("batwh",   0);
    autoCfg.sleepAtCutoff=prefs.getBool("autosleep",false);
    autoCfg.bydMinSoc=prefs.getUChar("autobydsoc",0);if(autoCfg.bydMinSoc>100)autoCfg.bydMinSoc=0;
    autoCfg.wakeMinW=prefs.getUShort("autowakew",200);if(autoCfg.wakeMinW<100||autoCfg.wakeMinW>10000)autoCfg.wakeMinW=200;
    autoCfg.wakeDelayS=prefs.getUShort("autowakes",30);if(autoCfg.wakeDelayS<10||autoCfg.wakeDelayS>600)autoCfg.wakeDelayS=30;
    autoLowLock=prefs.getBool("autolow",false);autoSleeping=autoLowLock;
    autoCfg.bydThresholdW=prefs.getUShort("autobyd",100);
    autoCfg.dataTimeoutS=prefs.getUShort("autowait",35);
    if(autoCfg.bydThresholdW>1000)autoCfg.bydThresholdW=100;
    if(autoCfg.dataTimeoutS<5 || autoCfg.dataTimeoutS>120)autoCfg.dataTimeoutS=35;
    autoCfg.sharePct=prefs.getUChar("autoshare",90);
    if(autoCfg.sharePct>100)autoCfg.sharePct=90;
    autoCfg.chargeW=prefs.getUShort("autocw",3000);
    autoCfg.dischargeW=prefs.getUShort("autodw",3000);
    autoCfg.cutoffV=prefs.getFloat("autocut",0);
    autoCfg.resumeDeltaV=prefs.getFloat("autodelta",1);
    if(autoCfg.chargeW>32767)autoCfg.chargeW=3000;
    if(autoCfg.dischargeW>32767)autoCfg.dischargeW=3000;
    if(!isfinite(autoCfg.cutoffV) || (autoCfg.cutoffV!=0 && (autoCfg.cutoffV<38 || autoCfg.cutoffV>60)))autoCfg.cutoffV=0;
    if(!isfinite(autoCfg.resumeDeltaV) || autoCfg.resumeDeltaV<.1f || autoCfg.resumeDeltaV>5)autoCfg.resumeDeltaV=1;
    prefs.end();
    if (!cfg.nutUps[0]) strlcpy(cfg.nutUps, DEFAULT_NUT_UPS, sizeof(cfg.nutUps));
    if (cfg.mqttPort == 0) cfg.mqttPort = DEFAULT_MQTT_PORT;
    if (!cfg.adminPass[0]) strlcpy(cfg.adminPass, DEFAULT_ADMIN_PASS, sizeof(cfg.adminPass));
}

static void saveConfig()
{
    prefs.begin("vebus_mqtt", false);
    prefs.putString("host",   cfg.mqttHost);
    prefs.putUShort("port",   cfg.mqttPort);
    prefs.putString("user",   cfg.mqttUser);
    prefs.putString("pass",   cfg.mqttPass);
    prefs.putString("device", cfg.deviceId);
    prefs.putString("prefix", cfg.prefix);
    prefs.putString("admin",  cfg.adminPass);
    prefs.putUShort("esstmo", cfg.essTimeoutS);
    prefs.putBool("nut",       cfg.nutEnabled);
    prefs.putString("nutups",  cfg.nutUps);
    prefs.putString("nutuser", cfg.nutUser);
    prefs.putString("nutpass", cfg.nutPass);
    prefs.putUChar("lowsoc",   cfg.lowSoc);
    prefs.putUShort("nomw",    cfg.nominalW);
    prefs.putBool("apc",       cfg.apcEnabled);
    prefs.putUShort("batwh",   cfg.batteryWh);
    prefs.putBool("autosleep",autoCfg.sleepAtCutoff);prefs.putUChar("autobydsoc",autoCfg.bydMinSoc);
    prefs.putUShort("autowakew",autoCfg.wakeMinW);prefs.putUShort("autowakes",autoCfg.wakeDelayS);
    prefs.putUShort("autobyd",autoCfg.bydThresholdW);prefs.putUShort("autowait",autoCfg.dataTimeoutS);
    prefs.putUChar("autoshare",autoCfg.sharePct);
    prefs.putUShort("autocw",autoCfg.chargeW);prefs.putUShort("autodw",autoCfg.dischargeW);
    prefs.putFloat("autocut",autoCfg.cutoffV);prefs.putFloat("autodelta",autoCfg.resumeDeltaV);
    prefs.end();
}

static void makeHostName()
{
    size_t j = 0;
    for (const char *s = cfg.deviceId; *s && j < sizeof(hostName) - 1; s++) {
        char c = *s;
        if (isalnum((unsigned char)c)) hostName[j++] = tolower(c);
        else if (j && hostName[j - 1] != '-') hostName[j++] = '-';
    }
    hostName[j] = '\0';
    if (!j) strlcpy(hostName, "vebus", sizeof(hostName));
}

// Drop the current MQTT session so the next connect uses the new settings.
static void applyMqttConfig()
{
    mqtt.disconnect();
    wifiClient.stop();
    mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
}

// =======================================================================
// Captive portal: WiFi only; application credentials stay behind web login.
// =======================================================================
static void setupWiFiPortal() {
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(0); // remain available until a WiFi connection succeeds
    wm.setConnectTimeout(15);
    wm.setSaveConnectTimeout(5);
    wm.setConnectRetries(1);
    wm.setCaptivePortalEnable(true);
    wm.setHostname(hostName);
    wm.setTitle("VE.Bus WLAN-Einrichtung");
    wm.setClass("invert");
    static const char *menu[]={"wifi"};
    wm.setMenu(menu,1);
    // WiFiManager calls this before adding its own handlers. The first
    // matching route wins, keeping the unauthenticated fallback WiFi-only.
    wm.setWebServerCallback([] {
        wm.server->on("/",HTTP_GET,[] {
            wm.server->sendHeader("Cache-Control","no-store");
            wm.server->sendHeader("Location",String("http://")+WiFi.softAPIP().toString()+"/wifi");
            wm.server->send(302,"text/plain","");
        });
        for(const char *path:{"/update","/u","/erase","/restart","/param","/paramsave","/exit","/close"}) {
            wm.server->on(path,HTTP_ANY,[] {wm.server->send(403,"text/plain","Nur WLAN-Einrichtung. Weitere Einstellungen in der angemeldeten Web-UI.");});
        }
    });
}

// =======================================================================
// Commands — shared by MQTT, web UI and serial console
// =======================================================================
static bool handleCommand(const char *cmd, const char *arg, const char *src)
{
#ifdef VEBUS_READ_ONLY
    Serial.printf("[read-only] blocked control from %s: %s\n", src, cmd);
    return false;
#endif
    if (autoEnabled) {
        if (!strcmp(cmd, "ess_power") || !strcmp(cmd, "virtual_mode")) return false;
        if (strcmp(cmd, "sync_soc")) autoStop();
    }
    if((!strcmp(cmd,"switch_state") && (!strcmp(arg,"on")||!strcmp(arg,"off")||!strcmp(arg,"charger_only")||!strcmp(arg,"inverter_only")))||!strcmp(cmd,"wakeup")||!strcmp(cmd,"sleep")) {
        saveAutoSleep(false);autoSleeping=false;autoSurplusTimer.reset();
    }
    if (!strcmp(cmd, "ess_power")) {
        if (!arg || !*arg) return false;
        const bool negative = arg[0] == '-';
        const char *digits = (negative || arg[0] == '+') ? arg + 1 : arg;
        int16_t w;
        if (!acPowerValue(digits, negative, w) || (w>0 && dischargeBlocked())) return false;
        if (w != g_essPower) g_essDirty = true;
        g_essPower = w;
        g_lastEssCmdMs = millis();
        vebus.setESSPower(g_essPower);
        Serial.printf("[%s] ESS → %d W\n", src, g_essPower);
    }
    else if (!strcmp(cmd, "switch_state")) {
        if      (!strcmp(arg, "on"))            vebus.setSwitchState(VEBUS_SWITCH_STATE_ON);
        else if (!strcmp(arg, "off"))           vebus.setSwitchState(VEBUS_SWITCH_STATE_OFF);
        else if (!strcmp(arg, "charger_only"))  vebus.setSwitchState(VEBUS_SWITCH_STATE_CHARGER_ONLY);
        else if (!strcmp(arg, "inverter_only")) vebus.setSwitchState(VEBUS_SWITCH_STATE_INVERTER_ONLY);
        else return false;
        Serial.printf("[%s] Switch → %s\n", src, arg);
    }
    else if (!strcmp(cmd, "virtual_mode")) {
        bool on = (strcasecmp(arg, "ON") == 0 || atoi(arg) != 0);
        if (on != vebus.isVirtualSetpointMode()) g_essDirty = true;
        vebus.enableVirtualSetpointMode(on);
        Serial.printf("[%s] Virtual mode → %s\n", src, on ? "ON" : "OFF");
    }
    else if (!strcmp(cmd, "sync_soc")) {
        // Manual known-reference SoC only; never run on boot/reconnect.
        // RAM 13 is SoC, scaled at 0.5 percent per raw unit.
        if (strcmp(src, "web") || !batFresh(millis()) || !arg[0]) return false;
        for (const char *p = arg; *p; ++p) if (*p < '0' || *p > '9') return false;
        if (strlen(arg) > 3) return false;
        const unsigned percent = (unsigned)atoi(arg);
        if (percent > 100) return false;
        const float raw = ((float)percent - scaler.info(VEBUS_RAM_CHARGE_STATE).offset) /
                          scaler.info(VEBUS_RAM_CHARGE_STATE).scale;
        if (!isfinite(raw) || raw < 0 || raw > 65535) return false;
        vebus.writeRAMVar(VEBUS_RAM_CHARGE_STATE, (uint16_t)lroundf(raw));
        // Do not overwrite live.soc: the UI must show device readback.
        const uint8_t id = VEBUS_RAM_CHARGE_STATE;
        vebus.readRAMVars(&id, 1);
    }
    else if (!strcmp(cmd, "wakeup"))           vebus.setSwitchState(VEBUS_SWITCH_STATE_ON);
    else if (!strcmp(cmd, "sleep"))            vebus.setSwitchState(VEBUS_SWITCH_STATE_OFF);
    else if (!strcmp(cmd, "force_absorption")) vebus.forceDeviceState(VEBUS_FORCE_ABSORPTION);
    else if (!strcmp(cmd, "force_float"))      vebus.forceDeviceState(VEBUS_FORCE_FLOAT);
    else if (!strcmp(cmd, "force_equalise"))   vebus.forceDeviceState(VEBUS_FORCE_EQUALISE);
    else return false;

    if (strcmp(cmd, "ess_power") && strcmp(cmd, "switch_state") && strcmp(cmd, "virtual_mode"))
        Serial.printf("[%s] %s sent\n", src, cmd);
    g_publishNow = true;
    return true;
}

// =======================================================================
// UPS add-ons (NUT / apcupsd) — configuration from the settings
// =======================================================================
static void applyUpsConfig()
{
    ups.lowSocPct = cfg.lowSoc;
    ups.nominalW  = cfg.nominalW;
    ups.batteryWh = cfg.batteryWh;
    nut.upsName   = cfg.nutUps;
    nut.user      = cfg.nutUser;
    nut.pass      = cfg.nutPass;
    apc.upsName   = cfg.nutUps;
    apc.hostName  = hostName;
    if (!cfg.nutEnabled) nut.end();   // started from loop() once WiFi is up
    if (!cfg.apcEnabled) apc.end();
}

// =======================================================================
// State JSON — published to MQTT and served to the dashboard
// =======================================================================
static void buildStateJson(Buf &j)
{
#ifdef VEBUS_READ_ONLY
    j.add("{\"read_only\":true,");
#else
    j.add("{\"read_only\":false,");
#endif
    j.add("\"bat_volt\":%.2f,\"ac_power\":%d,\"dc_current\":%.1f,",
          vebus.getBatVolt(), (int)vebus.getACPower(), vebus.getDCCurrent());
    j.add("\"temp\":null,\"temp_available\":false,");
    j.add("\"charger_status\":%u,\"ess_power\":%d,\"ess_power_eff\":%d,\"virtual_mode\":\"%s\",",
          (unsigned)vebus.getChargerStatus(), (int)g_essPower,
          (int)vebus.getEffectiveESSPower(), vebus.isVirtualSetpointMode() ? "ON" : "OFF");
    j.add("\"mains_voltage\":%.1f,\"mains_current\":%.2f,\"inv_voltage\":%.1f,\"inv_current\":%.2f,"
          "\"output_power\":%.0f,\"mains_power\":%.0f,",
          live.mainsV, live.mainsA, live.invV, live.invA, live.outW, live.mainsW);
    j.add("\"bat_current\":%.1f,\"soc\":%.1f,", live.batA, live.soc);
    j.add("\"battery_fresh\":%s,", batFresh(millis()) ? "true" : "false");
    if (live.mainsHz > 0) j.add("\"mains_freq\":%.2f,", live.mainsHz); else j.add("\"mains_freq\":null,");
    if (live.invHz > 0)   j.add("\"inv_freq\":%.2f,", live.invHz);     else j.add("\"inv_freq\":null,");
    j.add("\"led_on\":%u,\"led_blink\":%u,\"ac_in_min\":%.1f,\"ac_in_max\":%.1f,"
          "\"ac_in_actual\":%.1f,\"ac_in_config\":%u,",
          (unsigned)vebus.getLEDon(), (unsigned)vebus.getLEDblink(),
          vebus.getMinInputCurrentLimit(), vebus.getMaxInputCurrentLimit(),
          vebus.getActInputCurrentLimit(), (unsigned)vebus.getAcInputConfiguration());
    j.add("\"device_state\":\"%s\",\"charge_sub_state\":\"%s\",\"checksum_faults\":%lu,",
          g_devState < 0 ? "Unknown" : deviceStateName(g_devState),
          g_devState == VEBUS_STATE_CHARGE ? chargeSubStateName(g_devSubState) : "N/A",
          (unsigned long)vebus.getChecksumFaults());
    j.add("\"sync\":\"%s\",\"dc_allows_inv\":\"%s\",\"switch_state\":\"%s\",",
          vebus.hasNoSync() ? "OFF" : "ON",
          vebus.dcLevelAllowsInverting() ? "ON" : "OFF", switchStateName());
    char upsSt[32];
    ups.nutStatus(upsSt, sizeof(upsSt));
    j.add("\"ups_status\":\"%s\",\"nut_clients\":%u,\"apc_clients\":%u,", upsSt,
          (unsigned)nut.clientCount(), (unsigned)apc.clientCount());
    j.add("\"firmware_version\":\"%s\",\"rssi\":%d,\"uptime\":%lu,\"free_heap\":%u,",
          g_fwVersion, (int)WiFi.RSSI(), (unsigned long)(millis() / 1000),
          (unsigned)ESP.getFreeHeap());
    const bool timeValid=localDate(time(nullptr))!=0;
    const EnergyDay *today=todayEnergy(),*yesterday=yesterdayEnergy();
    char dayStart[32];dayStartText(dayStart,sizeof(dayStart));j.add("\"energy_day_start\":");if(timeValid)j.str(dayStart);else j.add("null");j.add(",");
    j.add("\"auto_byd_threshold_w\":%u,\"auto_data_timeout_s\":%u,",autoCfg.bydThresholdW,autoCfg.dataTimeoutS);
    j.add("\"auto_share_pct\":%u,",autoCfg.sharePct);
    j.add("\"auto_sleep_at_cutoff\":%s,\"auto_low_voltage_lock\":%s,\"auto_sleeping\":%s,\"auto_byd_min_soc\":%u,\"auto_wake_min_w\":%u,\"auto_wake_delay_s\":%u,",autoCfg.sleepAtCutoff?"true":"false",autoLowLock?"true":"false",autoSleeping?"true":"false",autoCfg.bydMinSoc,autoCfg.wakeMinW,autoCfg.wakeDelayS);
    j.add("\"auto_enabled\":%s,\"auto_charge_limit_w\":%u,\"auto_discharge_limit_w\":%u,\"auto_cutoff_v\":%.2f,\"auto_resume_delta_v\":%.2f,",autoEnabled?"true":"false",autoCfg.chargeW,autoCfg.dischargeW,autoCfg.cutoffV,autoCfg.resumeDeltaV);
    j.add("\"energy_total_charge_kwh\":%.6f,\"energy_total_discharge_kwh\":%.6f,",energyBook.chargeWh/1000,energyBook.dischargeWh/1000);
    if(timeValid)j.add("\"energy_today_charge_kwh\":%.6f,\"energy_today_discharge_kwh\":%.6f,",today?today->chargeWh/1000:0,today?today->dischargeWh/1000:0);
    else j.add("\"energy_today_charge_kwh\":null,\"energy_today_discharge_kwh\":null,");
    if(yesterday)j.add("\"energy_yesterday_charge_kwh\":%.6f,\"energy_yesterday_discharge_kwh\":%.6f,",yesterday->chargeWh/1000,yesterday->dischargeWh/1000);
    else j.add("\"energy_yesterday_charge_kwh\":null,\"energy_yesterday_discharge_kwh\":null,");
    j.add("\"energy_time_valid\":%s,\"energy_unassigned_charge_kwh\":%.6f,\"energy_unassigned_discharge_kwh\":%.6f,\"discharge_blocked\":%s}",timeValid?"true":"false",energyBook.unassignedChargeWh/1000,energyBook.unassignedDischargeWh/1000,dischargeBlocked()?"true":"false");
}

static void publishEnergy();
static void publishState()
{
    Buf j(payloadBuf, sizeof(payloadBuf));
    buildStateJson(j);
    char t[96];
    snprintf(t, sizeof(t), "%s/state", cfg.prefix);
    mqtt.publish(t, payloadBuf);
    static uint32_t lastEnergy=0;
    if(!lastEnergy || millis()-lastEnergy>=60000){lastEnergy=millis();publishEnergy();}
}

// =======================================================================
// HA MQTT auto-discovery — one entity per loop() pass so a slow broker
// never blocks the web UI. Unique ids match earlier versions of this
// example so existing HA entities are kept.
// =======================================================================
struct Entity {
    const char *comp;     // HA component
    const char *uid;      // unique id (prefixed with device id if devUid)
    bool        devUid;
    const char *name;
    const char *key;      // key in the state JSON (nullptr = stateless)
    const char *cmd;      // command suffix (nullptr = read-only)
    const char *extra;    // extra discovery JSON fields
};

#define MEAS  ",\"stat_cla\":\"measurement\""
#define DIAG  "\"ent_cat\":\"diagnostic\""

static const Entity ENTITIES[] = {
    {"sensor", "vebus_energy_today_charge", true, "Batterie heute geladen", "energy_today_charge_kwh", nullptr, "\"unit_of_meas\":\"kWh\",\"dev_cla\":\"energy\",\"stat_cla\":\"total\",\"last_reset_value_template\":\"{{ value_json.energy_day_start }}\""},
    {"sensor", "vebus_energy_today_discharge", true, "Batterie heute entladen", "energy_today_discharge_kwh", nullptr, "\"unit_of_meas\":\"kWh\",\"dev_cla\":\"energy\",\"stat_cla\":\"total\",\"last_reset_value_template\":\"{{ value_json.energy_day_start }}\""},
    {"sensor", "vebus_energy_total_charge", true, "Batterie gesamt geladen", "energy_total_charge_kwh", nullptr, "\"unit_of_meas\":\"kWh\",\"dev_cla\":\"energy\",\"stat_cla\":\"total\""},
    {"sensor", "vebus_energy_total_discharge", true, "Batterie gesamt entladen", "energy_total_discharge_kwh", nullptr, "\"unit_of_meas\":\"kWh\",\"dev_cla\":\"energy\",\"stat_cla\":\"total\""},
    {"sensor", "vebus_energy_yesterday_charge", true, "Batterie gestern geladen", "energy_yesterday_charge_kwh", nullptr, "\"unit_of_meas\":\"kWh\",\"dev_cla\":\"energy\""},
    {"sensor", "vebus_energy_yesterday_discharge", true, "Batterie gestern entladen", "energy_yesterday_discharge_kwh", nullptr, "\"unit_of_meas\":\"kWh\",\"dev_cla\":\"energy\""},
    // Core values
    {"sensor", "vebus_bat_volt",   false, "Battery Voltage",     "bat_volt",       nullptr, "\"unit_of_meas\":\"V\",\"dev_cla\":\"voltage\"" MEAS},
    {"sensor", "vebus_ac_power",   false, "AC Power",            "ac_power",       nullptr, "\"unit_of_meas\":\"W\",\"dev_cla\":\"power\"" MEAS},
    {"sensor", "vebus_dc_current", false, "DC Current",          "dc_current",     nullptr, "\"unit_of_meas\":\"A\",\"dev_cla\":\"current\"" MEAS},
    {"sensor", "vebus_temp",       false, "WR-Temperatur",       "temp",           nullptr, "\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\"" MEAS},
    {"sensor", "vebus_charger",    false, "Charger Status",      "charger_status", nullptr, ""},
    {"sensor", "vebus_ess_state",  false, "ESS Power",           "ess_power",      nullptr, "\"unit_of_meas\":\"W\",\"dev_cla\":\"power\"" MEAS},
    {"sensor", "vebus_ess_eff",    false, "Effective ESS Power", "ess_power_eff",  nullptr, "\"unit_of_meas\":\"W\",\"dev_cla\":\"power\"" MEAS},
    // Extended RAM batch 1
    {"sensor", "vebus_mains_v",    false, "Mains Voltage",       "mains_voltage",  nullptr, "\"unit_of_meas\":\"V\",\"dev_cla\":\"voltage\"" MEAS},
    {"sensor", "vebus_mains_a",    false, "Mains Current",       "mains_current",  nullptr, "\"unit_of_meas\":\"A\",\"dev_cla\":\"current\"" MEAS},
    {"sensor", "vebus_inv_v",      false, "Inverter Voltage",    "inv_voltage",    nullptr, "\"unit_of_meas\":\"V\",\"dev_cla\":\"voltage\"" MEAS},
    {"sensor", "vebus_inv_a",      false, "Inverter Current",    "inv_current",    nullptr, "\"unit_of_meas\":\"A\",\"dev_cla\":\"current\"" MEAS},
    {"sensor", "vebus_output_w",   false, "Output Power",        "output_power",   nullptr, "\"unit_of_meas\":\"W\",\"dev_cla\":\"power\"" MEAS},
    {"sensor", "vebus_mains_w",    false, "Mains Power",         "mains_power",    nullptr, "\"unit_of_meas\":\"W\",\"dev_cla\":\"power\"" MEAS},
    // Extended RAM batch 2
    {"sensor", "vebus_bat_a",      false, "Battery Current",     "bat_current",    nullptr, "\"unit_of_meas\":\"A\",\"dev_cla\":\"current\"" MEAS},
    {"sensor", "vebus_soc",        false, "State of Charge",     "soc",            nullptr, "\"unit_of_meas\":\"%\",\"dev_cla\":\"battery\"" MEAS},
    {"sensor", "vebus_mains_hz",   false, "Mains Frequency",     "mains_freq",     nullptr, "\"unit_of_meas\":\"Hz\",\"dev_cla\":\"frequency\"" MEAS},
    {"sensor", "vebus_inv_hz",     false, "Inverter Frequency",  "inv_freq",       nullptr, "\"unit_of_meas\":\"Hz\",\"dev_cla\":\"frequency\"" MEAS},
    // LED, limits, status
    {"sensor", "vebus_led_on",       false, "LED On",            "led_on",           nullptr, ""},
    {"sensor", "vebus_led_blink",    false, "LED Blink",         "led_blink",        nullptr, ""},
    {"sensor", "vebus_ac_in_min",    false, "AC Input Min",      "ac_in_min",        nullptr, "\"unit_of_meas\":\"A\",\"dev_cla\":\"current\"" MEAS},
    {"sensor", "vebus_ac_in_max",    false, "AC Input Max",      "ac_in_max",        nullptr, "\"unit_of_meas\":\"A\",\"dev_cla\":\"current\"" MEAS},
    {"sensor", "vebus_ac_in_actual", false, "AC Input Actual",   "ac_in_actual",     nullptr, "\"unit_of_meas\":\"A\",\"dev_cla\":\"current\"" MEAS},
    {"sensor", "vebus_ac_in_cfg",    false, "AC Input Config",   "ac_in_config",     nullptr, ""},
    {"sensor", "vebus_dev_state",    false, "Device State",      "device_state",     nullptr, ""},
    {"sensor", "vebus_charge_sub",   false, "Charge Sub-State",  "charge_sub_state", nullptr, ""},
    {"sensor", "vebus_ups_status",   false, "UPS Status",        "ups_status",       nullptr, ""},
    // Diagnostics
    {"sensor", "vebus_chksum",     false, "Checksum Faults",   "checksum_faults",  nullptr, DIAG},
    {"sensor", "vebus_fw",         false, "Firmware Version",  "firmware_version", nullptr, DIAG},
    {"sensor", "vebus_rssi",       false, "WiFi Signal",       "rssi",             nullptr, DIAG ",\"unit_of_meas\":\"dBm\",\"dev_cla\":\"signal_strength\"" MEAS},
    {"sensor", "vebus_uptime",     false, "Uptime",            "uptime",           nullptr, DIAG ",\"unit_of_meas\":\"s\",\"dev_cla\":\"duration\""},
    {"sensor", "vebus_heap",       false, "Free Heap",         "free_heap",        nullptr, DIAG ",\"unit_of_meas\":\"B\"" MEAS},
    // Binary sensors
    {"binary_sensor", "vebus_sync",  false, "VE.Bus Sync",         "sync",          nullptr, "\"dev_cla\":\"connectivity\""},
    {"binary_sensor", "vebus_dc_ok", false, "DC Allows Inverting", "dc_allows_inv", nullptr, "\"dev_cla\":\"power\""},
    // Controls
    {"number", "ess",              true,  "ESS Power Setpoint", "ess_power",    "ess_power",
        "\"min\":-32767,\"max\":32767,\"step\":1,\"unit_of_meas\":\"W\",\"mode\":\"box\""},
    {"select", "switch",           true,  "Switch State",       "switch_state", "switch_state",
        "\"options\":[\"on\",\"off\",\"charger_only\",\"inverter_only\"]"},
    {"switch", "vebus_virtual_mode", false, "Battery-Neutral UPS Mode", "virtual_mode", "virtual_mode", ""},
    {"button", "vebus_wakeup",      false, "Wakeup Multiplus", nullptr, "wakeup",           ""},
    {"button", "vebus_sleep",       false, "Sleep Multiplus",  nullptr, "sleep",            ""},
    {"button", "vebus_force_abs",   false, "Force Absorption", nullptr, "force_absorption", ""},
    {"button", "vebus_force_float", false, "Force Float",      nullptr, "force_float",      ""},
    {"button", "vebus_force_eq",    false, "Force Equalise",   nullptr, "force_equalise",   ""},
};
static const uint8_t ENTITY_COUNT = sizeof(ENTITIES) / sizeof(ENTITIES[0]);

static bool publishEntity(const Entity &e)
{
    char uid[64], ct[128];
    if (e.devUid) snprintf(uid, sizeof(uid), "%s_%s", cfg.deviceId, e.uid);
    else          strlcpy(uid, e.uid, sizeof(uid));
    snprintf(ct, sizeof(ct), "homeassistant/%s/%s/config", e.comp, uid);

    Buf j(payloadBuf, sizeof(payloadBuf));
    j.add("{\"name\":\"%s\",\"uniq_id\":\"%s\",\"avty_t\":\"%s/status\"", e.name, uid, cfg.prefix);
    if (e.key)
        j.add(",\"stat_t\":\"%s/state\",\"val_tpl\":\"{{ value_json.%s }}\"", cfg.prefix, e.key);
    if (e.cmd)
        j.add(",\"cmd_t\":\"%s/%s/set\"", cfg.prefix, e.cmd);
    if (!strcmp(e.comp, "sensor") || !strcmp(e.comp, "binary_sensor"))
        j.add(",\"exp_aft\":60");
    if (e.extra[0])
        j.add(",%s", e.extra);
    j.add(",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Victron Multiplus\",\"mf\":\"Victron Energy\","
          "\"mdl\":\"Multiplus\",\"cu\":\"http://%s/\"}}",
          cfg.deviceId, WiFi.localIP().toString().c_str());
    return mqtt.publish(ct, payloadBuf, true);
}

// =======================================================================
// MQTT
// =======================================================================
void mqttCallback(char *topicStr, byte *payload, unsigned int length)
{
    char msg[32];
    unsigned int len = min((unsigned int)sizeof(msg) - 1, length);
    memcpy(msg, payload, len);
    msg[len] = '\0';

    // HA restarted → resend discovery
    if (!strcmp(topicStr, "homeassistant/status")) {
        if (!strcmp(msg, "online")) g_discIdx = 0;
        return;
    }

    // <prefix>/<cmd>/set
    size_t plen = strlen(cfg.prefix);
    if (strncmp(topicStr, cfg.prefix, plen) || topicStr[plen] != '/') return;
    const char *rest = topicStr + plen + 1;
    const char *slash = strchr(rest, '/');
    if (!slash || strcmp(slash, "/set")) return;
    char cmd[32];
    strlcpy(cmd, rest, min(sizeof(cmd), (size_t)(slash - rest + 1)));
    handleCommand(cmd, msg, "MQTT");
}

static void connectMqtt()
{
    static uint32_t lastAttempt = 0;
    if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
    if (lastAttempt && millis() - lastAttempt < MQTT_RECONNECT_MS) return;
    lastAttempt = millis();

    Serial.print("[MQTT] Connecting...");
    char statusTopic[96];
    snprintf(statusTopic, sizeof(statusTopic), "%s/status", cfg.prefix);

    bool ok = cfg.mqttUser[0]
        ? mqtt.connect(cfg.deviceId, cfg.mqttUser, cfg.mqttPass, statusTopic, 1, true, "offline")
        : mqtt.connect(cfg.deviceId, statusTopic, 1, true, "offline");

    if (ok) {
        Serial.println(" connected!");
        g_mqttReconnects++;
        mqtt.publish(statusTopic, "online", true);

        char sub[96];
        snprintf(sub, sizeof(sub), "%s/+/set", cfg.prefix);
        mqtt.subscribe(sub);
        mqtt.subscribe("homeassistant/status");

        g_discIdx = 0;          // discovery is sent incrementally from loop()
        g_publishNow = true;
        vebus.requestDeviceState();
    } else {
        Serial.printf(" failed (rc=%d)\n", mqtt.state());
    }
}

// =======================================================================
// History
// =======================================================================
static void histAccumulate()
{
    if (vebus.hasNoSync()) return;   // no data → leave a gap
    int32_t v[6] = { lroundf(live.outW), lroundf(live.mainsW), vebus.getEffectiveESSPower(),
                     lroundf(vebus.getBatVolt() * 100.0f), lroundf(live.soc * 10.0f),
                     lroundf(live.batA * 10.0f) };
    for (int i = 0; i < 6; i++) histAcc[i] += v[i];
    histAccN++;
}

static void histPush()
{
    HistSample s;
    int16_t *f = &s.out;
    for (int i = 0; i < 6; i++) {
        f[i] = histAccN ? (int16_t)constrain(histAcc[i] / histAccN, -32767, 32767) : HIST_GAP;
        histAcc[i] = 0;
    }
    histAccN = 0;
    hist[histHead] = s;
    histHead = (histHead + 1) % HIST_LEN;
    if (histCount < HIST_LEN) histCount++;
}

// =======================================================================
// Web server
// =======================================================================
// Opaque random session tokens: only SHA-256 digests are persisted, never the cookie.
static String authHashes[4];
static uint64_t authExpiry[4]={};
static uint8_t authNext=0;
static uint32_t authRetryAt=0;
static String authHash(const String &value) {
    unsigned char digest[32];char hex[65];
    mbedtls_sha256_ret((const unsigned char*)value.c_str(),value.length(),digest,0);
    for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);
    return String(hex);
}
static void authStore() {
    Preferences ap;if(!ap.begin("web_auth",false))return;
    ap.putString("password",authHash(cfg.adminPass));ap.putUChar("next",authNext);
    for(unsigned i=0;i<4;i++){char key[8];snprintf(key,sizeof(key),"s%u",i);ap.putString(key,authHashes[i]);snprintf(key,sizeof(key),"e%u",i);ap.putULong64(key,authExpiry[i]);}
    ap.end();
}
static void authInit() {
    Preferences ap;if(!ap.begin("web_auth",true))return;
    if(ap.getString("password")==authHash(cfg.adminPass)) {
        authNext=ap.getUChar("next",0)%4;
        for(unsigned i=0;i<4;i++){char key[8];snprintf(key,sizeof(key),"s%u",i);authHashes[i]=ap.getString(key);snprintf(key,sizeof(key),"e%u",i);authExpiry[i]=ap.getULong64(key,0);}
    }
    ap.end();
}
static void authClear() { for(unsigned i=0;i<4;i++){authHashes[i]="";authExpiry[i]=0;}authStore(); }
static String authCookie() {
    String cookies=server.header("Cookie");int start=0;
    while(start<(int)cookies.length()) {
        int end=cookies.indexOf(';',start);if(end<0)end=cookies.length();
        String part=cookies.substring(start,end);part.trim();
        if(part.startsWith("vebus_session="))return part.substring(14);
        start=end+1;
    }
    return "";
}
static int authSlot() {
    const String token=authCookie();if(token.length()!=64)return -1;
    const String hash=authHash(token);
    for(unsigned i=0;i<4;i++)if(authHashes[i].length()==64 && authHashes[i]==hash) {
        const time_t now=time(nullptr);
        if(now>1700000000) {
            if(authExpiry[i] && (uint64_t)now>=authExpiry[i])return -1;
            if(!authExpiry[i]){authExpiry[i]=(uint64_t)now+2592000;authStore();}
        }
        return i;
    }
    return -1;
}
static bool authOrigin() {
    const String origin=server.header("Origin");
    return !origin.length() || origin=="http://"+server.hostHeader() || origin=="https://"+server.hostHeader();
}
static bool checkAuth() {
    if(authSlot()>=0 && (server.method()==HTTP_GET || authOrigin()))return true;
    server.sendHeader("Cache-Control","no-store");
    if(server.uri()=="/admin/") {server.sendHeader("Location","/login");server.send(303);}
    else server.send(401,"application/json","{\"ok\":false,\"error\":\"Bitte erneut anmelden\"}");
    return false;
}
static void handleLogin() {
    server.sendHeader("Cache-Control","no-store");
    if(!authOrigin()){server.send(403,"application/json","{\"error\":\"Unzulässige Herkunft\"}");return;}
    if(authRetryAt && (int32_t)(millis()-authRetryAt)<0){server.send(429,"application/json","{\"error\":\"Bitte kurz warten\"}");return;}
    if(server.arg("username")!=ADMIN_USER || server.arg("password")!=cfg.adminPass) {
        authRetryAt=millis()+1500;server.send(401,"application/json","{\"error\":\"Benutzername oder Passwort falsch\"}");return;
    }
    authRetryAt=0;unsigned char random[32];char token[65];esp_fill_random(random,sizeof(random));
    for(unsigned i=0;i<32;i++)snprintf(token+2*i,3,"%02x",random[i]);
    authHashes[authNext]=authHash(token);authExpiry[authNext]=time(nullptr)>1700000000?(uint64_t)time(nullptr)+2592000:0;authNext=(authNext+1)%4;authStore();
    String cookie=String("vebus_session=")+token+"; Path=/; HttpOnly; SameSite=Strict";
    if(server.arg("remember")=="1")cookie+="; Max-Age=2592000";
    server.sendHeader("Set-Cookie",cookie);server.send(200,"application/json","{\"ok\":true}");
}
static void handleLogout() {
    if(!checkAuth())return;
    const int slot=authSlot();if(slot>=0){authHashes[slot]="";authExpiry[slot]=0;authStore();}
    server.sendHeader("Set-Cookie","vebus_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
    server.send(200,"application/json","{\"ok\":true}");
}

static void sendJson(const char *json, int code = 200)
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(code, "application/json", json);
}

static void handleState()
{
    Buf j(payloadBuf, sizeof(payloadBuf));
    buildStateJson(j);
    j.len--;                          // reopen the object
    j.add(",\"mqtt\":%s,\"mqtt_reconnects\":%lu,\"wifi_reconnects\":%lu,\"ip\":\"%s\","
          "\"host\":\"%s\",\"reset_reason\":\"%s\",\"crash_stage\":\"%s\",\"default_pw\":%s,\"ess_timeout\":%u,"
          "\"device_id\":",
          mqtt.connected() ? "true" : "false", (unsigned long)g_mqttReconnects,
          (unsigned long)g_wifiReconnects, WiFi.localIP().toString().c_str(), hostName,
          resetReasonName(), STAGE_NAMES[g_crashStage], isDefaultPassword() ? "true" : "false",
          cfg.essTimeoutS);
    j.str(cfg.deviceId);
    j.add("}");
    sendJson(payloadBuf);
}

// Public read-only system information; no credentials or control endpoints.
static void handleSystem() {
    const uint32_t heapFree=ESP.getFreeHeap();
    DynamicJsonDocument doc(4096);
    doc["hostname"]=hostName;doc["uptime_s"]=millis()/1000;
    doc["reset_reason"]=resetReasonName();doc["reset_code"]=(int)esp_reset_reason();doc["crash_stage"]=STAGE_NAMES[g_crashStage];
    if(!vebus.hasNoSync() && g_fwVersion[0])doc["vebus_firmware"]=g_fwVersion;else doc["vebus_firmware"]=nullptr;
    JsonObject hw=doc.createNestedObject("hardware");
    hw["chip_model"]=ESP.getChipModel();hw["revision"]=ESP.getChipRevision();hw["cores"]=ESP.getChipCores();hw["cpu_mhz"]=ESP.getCpuFreqMHz();
    JsonObject mem=doc.createNestedObject("memory");
    mem["heap_free"]=heapFree;mem["heap_total"]=ESP.getHeapSize();mem["heap_min_free"]=ESP.getMinFreeHeap();mem["heap_max_block"]=ESP.getMaxAllocHeap();
    mem["flash_total"]=ESP.getFlashChipSize();mem["sketch_used"]=ESP.getSketchSize();mem["psram_total"]=ESP.getPsramSize();mem["psram_free"]=ESP.getFreePsram();
    const esp_partition_t *partition=esp_ota_get_running_partition();
    if(partition)mem["sketch_capacity"]=partition->size;else mem["sketch_capacity"]=nullptr;
    JsonObject fw=doc.createNestedObject("firmware");
    fw["application"]="VE.Bus MQTT / LILYGO";fw["build"]=__DATE__ " " __TIME__;fw["sdk"]=ESP.getSdkVersion();
    if(partition)fw["partition"]=partition->label;else fw["partition"]=nullptr;
#ifdef VEBUS_READ_ONLY
    fw["read_only"]=true;
#else
    fw["read_only"]=false;
#endif
    JsonObject network=doc.createNestedObject("network");
    const bool connected=WiFi.status()==WL_CONNECTED;network["connected"]=connected;
    if(connected){network["ip"]=WiFi.localIP().toString();network["gateway"]=WiFi.gatewayIP().toString();network["subnet"]=WiFi.subnetMask().toString();network["rssi"]=WiFi.RSSI();}
    JsonObject con=doc.createNestedObject("connections");
    con["mqtt"]=mqtt.connected();con["vebus"]=!vebus.hasNoSync();con["wifi_reconnects"]=g_wifiReconnects;con["mqtt_reconnects"]=g_mqttReconnects;
    String out;serializeJson(doc,out);sendJson(out.c_str());
}

// Streams ~40 kB without building it in RAM.
static void handleHistory()
{
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");

    char buf[1200];
    Buf j(buf, sizeof(buf));
    j.add("{\"interval\":%lu,\"age\":%lu,\"cols\":[\"out\",\"mains\",\"ess\",\"batv\",\"soc\",\"bata\"],\"data\":[",
          HIST_INTERVAL_MS / 1000, (unsigned long)((millis() - histLastMs) / 1000));
    uint16_t start = (histHead + HIST_LEN - histCount) % HIST_LEN;
    for (uint16_t i = 0; i < histCount; i++) {
        const HistSample &s = hist[(start + i) % HIST_LEN];
        if (i) j.add(",");
        if (s.out == HIST_GAP) j.add("null");
        else j.add("[%d,%d,%d,%d,%d,%d]", s.out, s.mains, s.ess, s.batv, s.soc, s.bata);
        if (j.len > sizeof(buf) - 64) {
            server.sendContent(buf, j.len);
            j = Buf(buf, sizeof(buf));
        }
    }
    j.add("]}");
    server.sendContent(buf, j.len);
    server.sendContent("");
}

static void handleAcState() {
    const uint32_t now = millis();
    char buf[1024]; Buf j(buf, sizeof(buf));
    const uint32_t elapsed = acStarted ? (acRunning ? now : acStopMs) - acStartMs : 0;
    j.add("{\"running\":%s,\"started\":%s,\"fresh\":%s,\"inverted\":%s,",
          acRunning ? "true" : "false", acStarted ? "true" : "false",
          acFresh(now) ? "true" : "false", acInvert ? "true" : "false");
    j.add("\"elapsed_s\":%lu,\"measured_s\":%lu,\"import_wh\":%.3f,\"export_wh\":%.3f,",
          (unsigned long)(elapsed / 1000), (unsigned long)(acEnergy.measuredMs / 1000),
          acEnergy.importWh, acEnergy.exportWh);
    j.add("\"rows\":%u,\"rows_total\":%lu,\"power_w\":", acCount, (unsigned long)acRowsTotal);
    if (acFresh(now)) j.add("%.1f", live.mainsW * (acInvert ? -1 : 1)); else j.add("null");
    j.add(",\"setpoint_w\":%d,\"effective_w\":%d,\"virtual_mode\":%s,",
          (int)g_essPower, (int)vebus.getEffectiveESSPower(),
          vebus.isVirtualSetpointMode() ? "true" : "false");
    j.add("\"battery_fresh\":%s,\"battery_measured_s\":%lu,\"battery_charge_kwh\":%.6f,\"battery_discharge_kwh\":%.6f,\"battery_power_w\":",
          batFresh(now) ? "true" : "false", (unsigned long)(batEnergy.measuredMs / 1000),
          batEnergy.importWh / 1000, batEnergy.exportWh / 1000);
    if (batFresh(now)) j.add("%.1f", batSampleV * batSampleA); else j.add("null");
    j.add(",\"read_only\":%s}",
#ifdef VEBUS_READ_ONLY
          "true"
#else
          "false"
#endif
    );
    sendJson(buf);
}
static void handleAcControl() {
    if (!checkAuth()) return;
    const String action = server.arg("action");
    const uint32_t now = millis();
    if (action == "start") {
        if (autoEnabled) { sendJson("{\"ok\":false,\"error\":\"Automatik zuerst ausschalten\"}", 409); return; }
        if (!acRows) { sendJson("{\"ok\":false,\"error\":\"Nicht genug Speicher fuer den Messlauf\"}", 503); return; }
        if (acRunning) { sendJson("{\"ok\":false,\"error\":\"Messlauf läuft bereits\"}", 409); return; }
        if (!acFresh(now) || !batFresh(now)) { sendJson("{\"ok\":false,\"error\":\"Frische AC-IN- und Batteriewerte erforderlich\"}", 409); return; }
        acEnergy = AcEnergy(); acHead = acCount = 0; acRowsTotal = 0;
        batEnergy = BatteryEnergy();
        acInvert = server.arg("invert") == "1";
        acStartMs = acRowMs = now; acStarted = acRunning = true;
        // First energy interval starts with the next actual RAM response.
        acRecord(now);
    } else if (action == "power") {
#ifdef VEBUS_READ_ONLY
        sendJson("{\"ok\":false,\"error\":\"Read-only diagnosis: controls blocked\"}", 403); return;
#endif
        if (!acRunning || !acFresh(now)) {
            sendJson("{\"ok\":false,\"error\":\"Leistungsvorgabe benötigt einen laufenden Messlauf und frische AC-IN-Werte\"}", 409); return;
        }
        const String power = server.arg("watts"), direction = server.arg("direction");
        int16_t signedW = 0;
        if ((direction != "charge" && direction != "discharge") ||
            !acPowerValue(power.c_str(), direction == "charge", signedW)) {
            sendJson("{\"ok\":false,\"error\":\"Ganzzahlige Leistung von 0 bis 32767 W und gültige Richtung erforderlich\"}", 400); return;
        }
        char value[8];
        snprintf(value, sizeof(value), "%d", (int)signedW);
        if (!handleCommand("ess_power", value, "ac-test")) {
            sendJson("{\"ok\":false,\"error\":\"Leistungsvorgabe gesperrt\"}", 409); return;
        }
    } else if (action == "stop") {
        if (acRunning) { acStopMs = now; acRecord(now); acRunning = false; acEnergy.gap(); batEnergy.gap(); }
    } else { sendJson("{\"ok\":false,\"error\":\"Unbekannte Messaktion\"}", 400); return; }
    sendJson("{\"ok\":true}");
}
static void handleAcCsv() {
    if (!checkAuth()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Content-Disposition", "attachment; filename=ac-in-messlauf.csv");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/csv; charset=utf-8", "");
    server.sendContent("elapsed_s;valid_measurement_s;ac_in_w;import_wh;export_wh;battery_valid_s;battery_voltage_v;battery_current_a;battery_power_w;battery_charge_kwh;battery_discharge_kwh\r\n");
    const uint16_t first = (acHead + AC_ROWS - acCount) % AC_ROWS;
    char buf[256];
    for (uint16_t i = 0; i < acCount; ++i) {
        const AcRow &r = acRows[(first + i) % AC_ROWS];
        if (isfinite(r.watts)) snprintf(buf, sizeof(buf), "%lu;%lu;%.1f;%.3f;%.3f;",
            (unsigned long)r.elapsedS, (unsigned long)r.measuredS, r.watts, r.importWh, r.exportWh);
        else snprintf(buf, sizeof(buf), "%lu;%lu;;%.3f;%.3f;",
            (unsigned long)r.elapsedS, (unsigned long)r.measuredS, r.importWh, r.exportWh);
        const size_t n = strlen(buf);
        if (isfinite(r.batW)) snprintf(buf+n, sizeof(buf)-n, "%lu;%.2f;%.2f;%.1f;%.6f;%.6f\r\n",
            (unsigned long)r.batMeasuredS, r.batV, r.batA, r.batW, r.batChargeWh/1000, r.batDischargeWh/1000);
        else snprintf(buf+n, sizeof(buf)-n, "%lu;;;;%.6f;%.6f\r\n",
            (unsigned long)r.batMeasuredS, r.batChargeWh/1000, r.batDischargeWh/1000);
        server.sendContent(buf);
    }
    server.sendContent("");
}

static void handleGetConfig()
{
    if (!checkAuth()) return;
    Buf j(payloadBuf, sizeof(payloadBuf));
    j.add("{\"host\":");   j.str(cfg.mqttHost);
    j.add(",\"port\":%u,\"user\":", cfg.mqttPort); j.str(cfg.mqttUser);
    j.add(",\"pass_set\":%s,\"device\":", cfg.mqttPass[0] ? "true" : "false"); j.str(cfg.deviceId);
    j.add(",\"prefix\":"); j.str(cfg.prefix);
    j.add(",\"ess_timeout\":%u,\"default_pw\":%s", cfg.essTimeoutS,
          isDefaultPassword() ? "true" : "false");
    j.add(",\"nut\":%s,\"nut_ups\":", cfg.nutEnabled ? "true" : "false"); j.str(cfg.nutUps);
    j.add(",\"nut_user\":"); j.str(cfg.nutUser);
    j.add(",\"nut_pass_set\":%s,\"low_soc\":%u,\"nominal_w\":%u,\"apc\":%s,\"battery_wh\":%u}",
          cfg.nutPass[0] ? "true" : "false", cfg.lowSoc, cfg.nominalW,
          cfg.apcEnabled ? "true" : "false", cfg.batteryWh);
    sendJson(payloadBuf);
}

static void handlePostConfig()
{
    if (!checkAuth()) return;
    if (server.hasArg("host") && server.arg("host").length())
        strlcpy(cfg.mqttHost, server.arg("host").c_str(), sizeof(cfg.mqttHost));
    if (server.hasArg("port")) {
        long p = server.arg("port").toInt();
        cfg.mqttPort = (p > 0 && p < 65536) ? (uint16_t)p : DEFAULT_MQTT_PORT;
    }
    if (server.hasArg("user"))
        strlcpy(cfg.mqttUser, server.arg("user").c_str(), sizeof(cfg.mqttUser));
    if (server.hasArg("pass") && server.arg("pass").length())          // empty = keep
        strlcpy(cfg.mqttPass, server.arg("pass").c_str(), sizeof(cfg.mqttPass));
    if (server.hasArg("clear_pass"))
        cfg.mqttPass[0] = '\0';
    if (server.hasArg("device") && server.arg("device").length())
        strlcpy(cfg.deviceId, server.arg("device").c_str(), sizeof(cfg.deviceId));
    if (server.hasArg("prefix") && server.arg("prefix").length())
        strlcpy(cfg.prefix, server.arg("prefix").c_str(), sizeof(cfg.prefix));
    if (server.hasArg("ess_timeout"))
        cfg.essTimeoutS = (uint16_t)constrain(server.arg("ess_timeout").toInt(), 0, 65535);
    if (server.hasArg("nut_ups")) {           // NUT fields are posted together
        cfg.nutEnabled = server.hasArg("nut");
        if (server.arg("nut_ups").length())
            strlcpy(cfg.nutUps, server.arg("nut_ups").c_str(), sizeof(cfg.nutUps));
        strlcpy(cfg.nutUser, server.arg("nut_user").c_str(), sizeof(cfg.nutUser));
        if (server.arg("nut_pass").length())
            strlcpy(cfg.nutPass, server.arg("nut_pass").c_str(), sizeof(cfg.nutPass));
        if (!cfg.nutUser[0]) cfg.nutPass[0] = '\0';
        cfg.lowSoc   = (uint8_t)constrain(server.arg("low_soc").toInt(), 0, 100);
        cfg.nominalW = (uint16_t)constrain(server.arg("nominal_w").toInt(), 0, 65535);
        cfg.apcEnabled = server.hasArg("apc");
        cfg.batteryWh  = (uint16_t)constrain(server.arg("battery_wh").toInt(), 0, 65535);
    }
    if (server.hasArg("admin_pass")) {
        String p = server.arg("admin_pass");
        if (p.length() >= 4 && p.length() < sizeof(cfg.adminPass))
        { if(p!=cfg.adminPass){strlcpy(cfg.adminPass, p.c_str(), sizeof(cfg.adminPass));authClear();} }
        else if (p.length()) {
            sendJson("{\"ok\":false,\"error\":\"Admin password must be 4-31 characters\"}", 400);
            return;
        }
    }
    saveConfig();
    makeHostName();
    applyMqttConfig();
    applyUpsConfig();
    Serial.println("[web] config saved — reconnecting MQTT");
    sendJson("{\"ok\":true}");
}

static void handleAutoState() {
    StaticJsonDocument<1536> doc;
    doc["enabled"]=autoEnabled;doc["fresh"]=autoHaveSample&&autoSample.valid&&millis()-autoSample.atMs<=15000;
    doc["sleep_at_cutoff"]=autoCfg.sleepAtCutoff;doc["sleeping"]=autoSleeping;doc["low_voltage_lock"]=autoLowLock;
    doc["byd_min_soc"]=autoCfg.bydMinSoc;doc["wake_min_w"]=autoCfg.wakeMinW;doc["wake_delay_s"]=autoCfg.wakeDelayS;
    if(autoHaveSample&&autoSample.bydSocValid)doc["byd_soc"]=autoSample.bydSoc;else doc["byd_soc"]=nullptr;
    doc["byd_threshold_w"]=autoCfg.bydThresholdW;doc["data_timeout_s"]=autoCfg.dataTimeoutS;
    doc["data_age_s"]=autoHaveSample?(millis()-autoSample.atMs)/1000:0;
    doc["status"]=autoReason;doc["share_pct"]=autoCfg.sharePct;doc["target_w"]=autoWanted;doc["setpoint_w"]=g_essPower;
    doc["solar_valid"]=autoSample.solarValid;doc["solar_residual_w"]=autoSample.solarResidualW;
    doc["grid_w"]=autoSample.gridW;doc["byd_w"]=autoSample.otherW;doc["victron_w"]=autoSample.victronW;doc["demand_w"]=autoSample.demandW;
    doc["charge_limit_w"]=autoCfg.chargeW;doc["discharge_limit_w"]=autoCfg.dischargeW;
    doc["cutoff_v"]=autoCfg.cutoffV;doc["resume_delta_v"]=autoCfg.resumeDeltaV;doc["discharge_blocked"]=dischargeBlocked();
    String out;serializeJson(doc,out);sendJson(out.c_str());
}
static bool autoNumber(const char *key,double &value) {
    if(!server.hasArg(key))return false;
    const String text=server.arg(key);if(!text.length())return false;
    char *end;value=strtod(text.c_str(),&end);return *end==0&&isfinite(value);
}
static void handleAutoConfig() {
    if(!checkAuth())return;
#ifdef VEBUS_READ_ONLY
    sendJson("{\"ok\":false,\"error\":\"Nur-Lesen-Firmware\"}",403);return;
#endif
    double soc=autoCfg.bydMinSoc,wakeW=autoCfg.wakeMinW,wakeS=autoCfg.wakeDelayS,sleep=autoCfg.sleepAtCutoff?1:0;
    double charge,discharge,cutoff,delta,share=autoCfg.sharePct,byd=autoCfg.bydThresholdW,wait=autoCfg.dataTimeoutS;
    if((server.hasArg("byd_min_soc")&&!autoNumber("byd_min_soc",soc))||soc<0||soc>100||floor(soc)!=soc||
       (server.hasArg("sleep_at_cutoff")&&!autoNumber("sleep_at_cutoff",sleep))||(sleep!=0&&sleep!=1)||
       (server.hasArg("wake_min_w")&&!autoNumber("wake_min_w",wakeW))||wakeW<100||wakeW>10000||floor(wakeW)!=wakeW||
       (server.hasArg("wake_delay_s")&&!autoNumber("wake_delay_s",wakeS))||wakeS<10||wakeS>600||floor(wakeS)!=wakeS||
       (server.hasArg("byd_threshold_w")&&!autoNumber("byd_threshold_w",byd))||byd<0||byd>1000||floor(byd)!=byd||
       (server.hasArg("data_timeout_s")&&!autoNumber("data_timeout_s",wait))||wait<5||wait>120||floor(wait)!=wait||
       !autoNumber("charge_limit_w",charge)||!autoNumber("discharge_limit_w",discharge)||
       !autoNumber("cutoff_v",cutoff)||!autoNumber("resume_delta_v",delta)||
       (server.hasArg("share_pct")&&!autoNumber("share_pct",share))||share<0||share>100||floor(share)!=share||
       charge<0||charge>32767||floor(charge)!=charge||discharge<0||discharge>32767||floor(discharge)!=discharge||
       (cutoff!=0&&(cutoff<38||cutoff>60))||(sleep==1&&(cutoff==0||(charge>0&&wakeW>charge)))||delta<.1||delta>5) {
        sendJson("{\"ok\":false,\"error\":\"BYD-Ziel: 0–100 %; Wiederanlauf: 100–10000 W, 10–600 s; Abschalten benötigt eine Spannungsgrenze und einen Wiederanlaufwert bis zur Ladegrenze; Leistungs-/Spannungsgrenzen prüfen\"}",400);return;
    }
    autoCfg.sleepAtCutoff=sleep==1;autoCfg.bydMinSoc=soc;autoCfg.wakeMinW=wakeW;autoCfg.wakeDelayS=wakeS;
    autoSurplusTimer.reset();
    autoCfg.bydThresholdW=byd;autoCfg.dataTimeoutS=wait;
    autoCfg.sharePct=share;autoCfg.chargeW=charge;autoCfg.dischargeW=discharge;autoCfg.cutoffV=cutoff;autoCfg.resumeDeltaV=delta;
    updateVoltageGuard();saveConfig();
    if(autoEnabled) {
        // Reductions take effect immediately; increases retain the existing ramp.
        if(g_essPower>0 && autoHaveSample && autoSample.valid && millis()-autoSample.atMs<=15000) {
            autoWanted=AutoPower::target(autoSample.demandW,autoSample.otherW,autoCfg.chargeW,autoCfg.dischargeW,autoCfg.sharePct,autoCfg.bydThresholdW);
            autoSetPower(min((int)g_essPower,max(0,autoWanted)));
        } else if(g_essPower<0 && !AutoPower::priorityReady(autoCfg.bydMinSoc,autoSample.bydSocValid,autoSample.bydSoc))autoSetPower(0);
        else autoSetPower(g_essPower);
    }
    sendJson("{\"ok\":true}");
}
static void energySummary(JsonDocument &doc) {
    const uint32_t date=localDate(time(nullptr));const EnergyDay *today=todayEnergy(),*yesterday=yesterdayEnergy();
    char dateBuffer[11];dateText(date,dateBuffer,sizeof(dateBuffer));doc["date"]=dateBuffer;
    char start[32];dayStartText(start,sizeof(start));if(date)doc["day_start"]=start;else doc["day_start"]=nullptr;
    doc["time_valid"]=date!=0;doc["timezone"]="Europe/Berlin";doc["fresh"]=batFresh(millis());
    if(date){doc["today_charge_kwh"]=today?today->chargeWh/1000:0;doc["today_discharge_kwh"]=today?today->dischargeWh/1000:0;}
    else {doc["today_charge_kwh"]=nullptr;doc["today_discharge_kwh"]=nullptr;}
    doc["today_measured_s"]=today?today->measuredMs/1000:0;
    if(yesterday){doc["yesterday_charge_kwh"]=yesterday->chargeWh/1000;doc["yesterday_discharge_kwh"]=yesterday->dischargeWh/1000;}
    else {doc["yesterday_charge_kwh"]=nullptr;doc["yesterday_discharge_kwh"]=nullptr;}
    doc["total_charge_kwh"]=energyBook.chargeWh/1000;doc["total_discharge_kwh"]=energyBook.dischargeWh/1000;
    doc["unassigned_charge_kwh"]=energyBook.unassignedChargeWh/1000;doc["unassigned_discharge_kwh"]=energyBook.unassignedDischargeWh/1000;
    doc["total_measured_s"]=energyBook.measuredMs/1000;doc["gap_s"]=energyBook.gapMs/1000;
    doc["storage_ok"]=energyStorageOk;doc["save_interval_s"]=600;
    char topic[96];snprintf(topic,sizeof(topic),"%s/energy",cfg.prefix);doc["mqtt_topic"]=topic;
}
static void publishEnergyDay(const EnergyDay *day) {
    if(!day)return;
    char date[11],topic[128],json[256];dateText(day->date,date,sizeof(date));
    snprintf(topic,sizeof(topic),"%s/energy/day/%s",cfg.prefix,date);
    snprintf(json,sizeof(json),"{\"date\":\"%s\",\"charged_kwh\":%.6f,\"discharged_kwh\":%.6f,\"measured_s\":%u}",date,day->chargeWh/1000,day->dischargeWh/1000,day->measuredMs/1000);
    mqtt.publish(topic,json,true);
}
static void publishEnergy() {
    StaticJsonDocument<1536> doc;energySummary(doc);
    char json[1536],topic[96];serializeJson(doc,json,sizeof(json));snprintf(topic,sizeof(topic),"%s/energy",cfg.prefix);
    mqtt.publish(topic,json,true);
    publishEnergyDay(todayEnergy());publishEnergyDay(yesterdayEnergy());
}
static void handleEnergy() {
    DynamicJsonDocument doc(8192);energySummary(doc);
    JsonArray days=doc.createNestedArray("days");
    const uint32_t first=energyBook.count==32?energyBook.next:0;
    for(uint32_t i=0;i<energyBook.count;++i) {
        const EnergyDay &d=energyBook.days[(first+i)%32];char date[11];dateText(d.date,date,sizeof(date));
        JsonObject row=days.createNestedObject();row["date"]=date;row["charged_kwh"]=d.chargeWh/1000;
        row["discharged_kwh"]=d.dischargeWh/1000;row["measured_s"]=d.measuredMs/1000;
    }
    if(doc.overflowed()){sendJson("{\"error\":\"Nicht genug Speicher\"}",503);return;}
    String out;serializeJson(doc,out);sendJson(out.c_str());
}
static void handleAutoControl() {
    if (!checkAuth()) return;
#ifdef VEBUS_READ_ONLY
    sendJson("{\"ok\":false,\"error\":\"Nur-Lesen-Firmware\"}", 403); return;
#endif
    const String action = server.arg("action");
    if (action == "off") { autoStop(); sendJson("{\"ok\":true}"); return; }
    if (action != "on") { sendJson("{\"ok\":false,\"error\":\"Ungueltige Aktion\"}",400); return; }
    if (!autoQueue || acRunning || (!autoSleeping && (!acFresh(millis()) || !batFresh(millis())))) {
        sendJson("{\"ok\":false,\"error\":\"Frische VE.Bus-Werte erforderlich; Batterietest muss beendet sein\"}",409); return;
    }
    // Never restore an automatic power command after a reboot.
    prefs.begin("vebus_mqtt", false); prefs.putBool("autosession", true); prefs.end();
    g_essDirty = false;
    vebus.enableVirtualSetpointMode(false);
    xQueueReset(autoQueue); autoHaveSample = false;
    autoEnabled = true; autoReason = "Warte auf neuen evcc-Messwert"; autoWanted = 0;
    autoSetPower(0);
    sendJson("{\"ok\":true}");
}

static void handleControl()
{
    if (!checkAuth()) return;
#ifdef VEBUS_READ_ONLY
    sendJson("{\"ok\":false,\"error\":\"Read-only diagnosis: controls blocked\"}", 403); return;
#endif
    String cmd = server.arg("cmd"), val = server.arg("value");
    if(cmd=="ess_power" && val.toInt()>0 && dischargeBlocked()) {
        sendJson("{\"ok\":false,\"error\":\"Entladesperre aktiv oder Batteriespannung nicht aktuell\"}",409);return;
    }
    if (autoEnabled && (cmd == "ess_power" || cmd == "virtual_mode")) {
        sendJson("{\"ok\":false,\"error\":\"Automatik zuerst ausschalten\"}",409); return;
    }
    if (handleCommand(cmd.c_str(), val.c_str(), "web"))
        sendJson("{\"ok\":true}");
    else
        sendJson("{\"ok\":false,\"error\":\"unknown command\"}", 400);
}

// Diagnostics: recent VE.Bus responses + scaler state
static void handleDebug()
{
    if (!checkAuth()) return;
    Buf j(payloadBuf, sizeof(payloadBuf));
    j.add("{\"responses\":[");
    for (uint8_t i = 0; i < VEBus::VEBUS_RESP_LOG; i++) {
        uint8_t b[12];
        uint8_t n = vebus.getResponseLog(i, b, sizeof(b));
        j.add(i ? ",\"" : "\"");
        for (uint8_t k = 0; k < n; k++) j.add("%02X", b[k]);
        j.add("\"");
    }
    j.add("],\"scale\":{");
    for (uint8_t id = 0; id < VEBusScaler::MAX_ID; id++) {
        const VEBusVarInfo &vi = scaler.info(id);
        j.add("%s\"%u\":[%g,%d,%d,%d]", id ? "," : "", id, vi.scale, vi.offset,
              vi.isSigned, scaler.fromDevice(id));
    }
    j.add("},\"scaler_done\":%s}", scaler.complete() ? "true" : "false");
    sendJson(payloadBuf);
}

static void handleReboot()
{
    if (!checkAuth()) return;
    saveEnergy(true);
    sendJson("{\"ok\":true}");
    g_restartAtMs = millis() + 1000;
}

static void handleWifiReset()
{
    if (!checkAuth()) return;
    saveEnergy(true);
    sendJson("{\"ok\":true}");
    server.client().flush();
    delay(200);
    wm.resetSettings();       // forget WiFi → captive portal on next boot
    ESP.restart();
}

static void handleUpdateDone()
{
    if (!checkAuth()) return;
    bool ok = g_otaAuthed && !Update.hasError();
    server.sendHeader("Connection", "close");
    server.send(ok ? 200 : 500, "text/plain", ok ? "OK — rebooting" : Update.errorString());
    if (ok) g_restartAtMs = millis() + 1000;
}

static void handleUpdateUpload()
{
    HTTPUpload &up = server.upload();
    esp_task_wdt_reset();                       // large uploads take a while
    STAGE(ST_OTA);
    if (up.status == UPLOAD_FILE_START) {
        g_otaAuthed = authSlot()>=0 && authOrigin();
        if (!g_otaAuthed) return;
        saveEnergy(true);
        busQuiet();                             // flash writes stall both cores
        ledUpdating = true;
        showStatusLed(LedStatus::Updating, millis());
        Serial.printf("[ota] start: %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (!g_otaAuthed) {
        return;
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("[ota] done: %u bytes\n", up.totalSize);
        else                { Update.printError(Serial); busResume(); ledUpdating = false; }
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        busResume();
        ledUpdating = false;
        Serial.println("[ota] aborted");
    }
}

static void sendPage(const char *html)
{
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html; charset=utf-8", html);
}

static void setupWebServer()
{
    authInit();
    const char *headers[]={"Cookie","Origin"};server.collectHeaders(headers,2);
    server.on("/login",HTTP_GET,[]{if(authSlot()>=0){server.sendHeader("Location","/admin/");server.send(303);}else sendPage(LOGIN_HTML);});
    server.on("/auth/login",HTTP_POST,handleLogin);
    server.on("/auth/logout",HTTP_POST,handleLogout);
    server.on("/",                 HTTP_GET,  [] { sendPage(INDEX_HTML); });
    server.on("/theme.js", HTTP_GET, [] {
        server.sendHeader("Cache-Control", "max-age=3600");
        server.send_P(200,"application/javascript; charset=utf-8",THEME_JS);
    });
    server.on("/style.css",        HTTP_GET,  [] {
        server.sendHeader("Cache-Control", "max-age=3600");
        server.send_P(200, "text/css", STYLE_CSS);
    });
    server.on("/info",HTTP_GET,[]{server.sendHeader("Location","/info/system");server.send(302);});
    server.on("/info/",HTTP_GET,[]{server.sendHeader("Location","/info/system");server.send(302);});
    server.on("/info/system",HTTP_GET,[]{sendPage(INFO_HTML);});
    server.on("/api/system",HTTP_GET,handleSystem);
    server.on("/api/state",        HTTP_GET,  handleState);
    server.on("/api/led", HTTP_GET, [] {
        char json[128];
        snprintf(json, sizeof(json), "{\"status\":\"%s\",\"gpio\":4,\"brightness\":16}", ledStatusName(ledStatus));
        sendJson(json);
    });
    server.on("/api/energy", HTTP_GET, handleEnergy);
    server.on("/admin/api/auto/config", HTTP_POST, handleAutoConfig);
    server.on("/api/auto", HTTP_GET, handleAutoState);
    server.on("/admin/api/auto", HTTP_POST, handleAutoControl);
    server.on("/api/ac-log", HTTP_GET, handleAcState);
    server.on("/admin/api/ac-log", HTTP_POST, handleAcControl);
    server.on("/admin/ac-log.csv", HTTP_GET, handleAcCsv);
    server.on("/api/history",      HTTP_GET,  handleHistory);
    server.on("/admin",            HTTP_GET,  [] { server.sendHeader("Location", "/admin/"); server.send(302); });
    server.on("/admin/",           HTTP_GET,  [] { if (checkAuth()) sendPage(ADMIN_HTML); });
    server.on("/admin/api/config", HTTP_GET,  handleGetConfig);
    server.on("/admin/api/config", HTTP_POST, handlePostConfig);
    server.on("/admin/api/control",HTTP_POST, handleControl);
    server.on("/admin/api/reboot", HTTP_POST, handleReboot);
    server.on("/admin/api/debug",  HTTP_GET,  handleDebug);
    server.on("/admin/api/wifireset", HTTP_POST, handleWifiReset);
    server.on("/admin/update",     HTTP_POST, handleUpdateDone, handleUpdateUpload);
    server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
    if(!wm.getConfigPortalActive())server.begin();
}

// =======================================================================
// Bus silence: stop transmitting and switch the transceiver off before a
// restart or while flash is written (OTA), so nothing can reach the VE.Bus
// out of its time slot.
// =======================================================================
static void busQuiet()
{
    vebus.setTxEnabled(false);
    delay(20);                                  // a frame on the wire is < 1 ms
    digitalWrite(VEBUS_PIN_SHDN, LOW);
}

static void busResume()
{
    digitalWrite(VEBUS_PIN_SHDN, HIGH);
    vebus.setTxEnabled(true);
}

// =======================================================================
// Watchdogs
// =======================================================================
static void setupTaskWatchdog()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t c = {};
    c.timeout_ms     = WDT_TIMEOUT_S * 1000;
    c.idle_core_mask = 0;
    c.trigger_panic  = true;
    esp_task_wdt_reconfigure(&c);
#else
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
    esp_task_wdt_add(NULL);   // loopTask: reboot if loop() stalls
}

// Recover the saved network and offer a nonblocking captive portal on loss.
// The dashboard and WiFiManager take turns owning TCP port 80.
static void superviseWiFi(uint32_t now) {
    static bool down=false;
    static uint32_t downSince=0,lastKick=0,lastPortalAttempt=0;
    if(wm.getConfigPortalActive())wm.process();
    if(WiFi.status()==WL_CONNECTED) {
        if(wm.getConfigPortalActive())wm.stopConfigPortal();
        if(down || wifiPortalOwnsHttp) {
            Serial.printf("[wifi] connected (%s), restoring dashboard\n",WiFi.localIP().toString().c_str());
            g_wifiReconnects++;
            wifiPortalOwnsHttp=false;
            WiFi.setSleep(false);
            server.stop();server.begin();
            nut.end();apc.end();
            MDNS.end();if(MDNS.begin(hostName))MDNS.addService("http","tcp",80);
        }
        down=false;
        return;
    }
    if(!down){down=true;downSince=lastKick=now;Serial.println("[wifi] connection lost");}
    if(wifiPortalOwnsHttp && !wm.getConfigPortalActive()) {
        wifiPortalOwnsHttp=false;server.begin();
    }
    if(WiFiRecovery::openPortal(false,wm.getConfigPortalActive(),now-downSince,now-lastPortalAttempt)) {
        lastPortalAttempt=now;
        server.stop();nut.end();apc.end();MDNS.end();
        wm.startConfigPortal(AP_SSID,AP_PASS);
        wifiPortalOwnsHttp=wm.getConfigPortalActive();
        if(!wifiPortalOwnsHttp)server.begin();
        Serial.printf("[wifi] setup AP %s: %s\n",AP_SSID,wifiPortalOwnsHttp?"active":"start failed, will retry");
    }
    if(WiFiRecovery::retry(false,wm.getConfigPortalActive(),WiFi.softAPgetStationNum(),now-lastKick)) {
        lastKick=now;
        WiFi.begin(); // enable STA alongside AP; do not interrupt an attached setup client
    }
}

// =======================================================================
// Serial console: w=wake, s=sleep, number=ESS watts (non-blocking)
// =======================================================================
static void pollSerial()
{
    static char line[24];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = Serial.read();
        if (c != '\n' && c != '\r') {
            if (n < sizeof(line) - 1) line[n++] = c;
            continue;
        }
        line[n] = '\0';
        n = 0;
        if (!line[0]) continue;
        if (!strcasecmp(line, "w"))      handleCommand("wakeup", "", "serial");
        else if (!strcasecmp(line, "s")) handleCommand("sleep", "", "serial");
        else                             handleCommand("ess_power", line, "serial");
    }
}

// =======================================================================
// setup()
// =======================================================================
void setup()
{
    // Read frequency scale/offset from the device; retain proven defaults
    // for all other measurements (battery current, SoC, voltage, power).
    scaler.queryPeriodsOnly = true;
    scaler.queryDevice = true;

    statusPixel.begin();
    statusPixel.setBrightness(16); // about 6%: visible without lighting the room
    showStatusLed(LedStatus::Startup, millis());
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== VEBus MQTT → Home Assistant ===");
    Serial.printf("Reset reason: %s\n", resetReasonName());
    esp_reset_reason_t rr = esp_reset_reason();
    if (g_stageMagic == 0x5EB0CAFE && g_stage < sizeof(STAGE_NAMES) / sizeof(STAGE_NAMES[0]) &&
        (rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT || rr == ESP_RST_WDT || rr == ESP_RST_PANIC))
    {
        g_crashStage = g_stage;
        Serial.printf("Last reset happened in: %s\n", STAGE_NAMES[g_crashStage]);
    }
    g_stageMagic = 0x5EB0CAFE;
    g_stage = ST_NONE;

    // RS485 transceiver stays off until the UART drives TX to its idle level
    pinMode(VEBUS_PIN_SHDN, OUTPUT);
    digitalWrite(VEBUS_PIN_SHDN, LOW);

    // Start VEBus (configures UART, RS485 task on core 1), then enable the bus
    pinMode(VEBUS_PIN_POWER, OUTPUT);
    digitalWrite(VEBUS_PIN_POWER, HIGH);
    vebus.begin(VEBUS_PIN_RX, VEBUS_PIN_TX, VEBUS_PIN_RE);
    digitalWrite(VEBUS_PIN_SHDN, HIGH);
    esp_register_shutdown_handler(busQuiet);   // every orderly restart goes quiet first
    Serial.println("VE.Bus started.");
    Serial.printf("[ac-log] integration self-check: %s\n", acEnergySelfCheck() ? "PASS" : "FAIL");
    Serial.printf("[battery-log] 2.4 kWh integration self-check: %s\n", batteryEnergySelfCheck() ? "PASS" : "FAIL");
    Serial.printf("[status-led] logic self-check: %s\n", statusLedSelfCheck() ? "PASS" : "FAIL");
    acRows = static_cast<AcRow *>(calloc(AC_ROWS, sizeof(AcRow)));
    if (!acRows) Serial.println("[battery-log] CSV allocation failed; measurement start blocked");

    loadConfig();
    voltageBlocked=autoCfg.cutoffV>0; // conservative restart: release at cutoff + hysteresis
    loadEnergy();

    // Automatic startup always begins at zero; never replay a saved manual
    // or automatic setpoint before fresh evcc and VE.Bus measurements arrive.
    g_essPower = 0;
    bool vmode = false;
    if(autoCfg.cutoffV>0 && g_essPower>0){g_essPower=0;vmode=false;}
    g_lastEssCmdMs = millis();                  // fail-safe timeout counts from boot
    vebus.setESSPower(g_essPower);
    if (vmode) vebus.enableVirtualSetpointMode(true);
    Serial.printf("[cfg] ESS startup setpoint %d W, battery-neutral %s\n",
                  g_essPower, vmode ? "ON" : "OFF");
    makeHostName();
    Serial.printf("[cfg] MQTT=%s:%u user='%s' device='%s' prefix='%s'\n",
                  cfg.mqttHost, cfg.mqttPort, cfg.mqttUser, cfg.deviceId, cfg.prefix);

    setupWiFiPortal();
    // Attempt saved WiFi for at most 15 seconds. The fallback AP returns
    // immediately; process it from loop(), without offline reboot loops.
    wm.autoConnect(AP_SSID,AP_PASS);
    wifiPortalOwnsHttp=wm.getConfigPortalActive();
    WiFi.setSleep(false);             // modem sleep makes the web UI sluggish/unreachable
    WiFi.setAutoReconnect(true);
    Serial.printf("[wifi] mode: %s\n",wifiPortalOwnsHttp?"setup AP":"station");

    if (WiFi.status()==WL_CONNECTED && MDNS.begin(hostName)) MDNS.addService("http", "tcp", 80);
    setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();   // timestamps for apcupsd
    setupWebServer();
    Serial.printf("[web] dashboard at http://%s/ (http://%s.local/)\n",
                  WiFi.localIP().toString().c_str(), hostName);
    if (isDefaultPassword())
        Serial.println("[web] WARNING: admin password is the default — change it at /admin/");

    // MQTT — short timeouts so a dead broker never stalls the loop for long
    mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
    mqtt.setBufferSize(sizeof(payloadBuf) + 128);
    mqtt.setSocketTimeout(5);
    mqtt.setKeepAlive(30);
    mqtt.setCallback(mqttCallback);

    // NUT server (started from loop() once WiFi is up)
    nut.upsDesc = "Victron MultiPlus (VE.Bus)";
    applyUpsConfig();
    if (cfg.nutEnabled)
        Serial.printf("[nut] UPS '%s' on port 3493%s\n", cfg.nutUps,
                      cfg.nutUser[0] ? " (auth required)" : "");

    autoQueue = xQueueCreate(1, sizeof(AutoSnapshot));
    if (autoQueue && xTaskCreatePinnedToCore(autoWorker, "evcc-read", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
        vQueueDelete(autoQueue); autoQueue = nullptr;
    }
#ifndef VEBUS_READ_ONLY
    if(autoQueue) {
        xQueueReset(autoQueue);
        autoHaveSample=false;
        autoWanted=0;
        autoEnabled=true;
        autoReason="Automatik gestartet · warte auf frische Messwerte";
    } else autoReason="Automatik nicht gestartet · evcc-Abruf nicht verfügbar";
#endif
    histLastMs = millis();
    setupTaskWatchdog();
}

// =======================================================================
// loop()
// =======================================================================
void loop()
{
    static uint32_t lastESSMs     = 0;
    static uint32_t lastPublishMs = 0;
    static bool     ramRequested     = false;
    static bool     extRam1Requested = false;
    static bool     extRam2Requested = false;
    static bool     stateRequested   = false;

    esp_task_wdt_reset();
    uint32_t now = millis();

    STAGE(ST_WIFI);
    superviseWiFi(now);
    STAGE(ST_WEB);
    if(!wm.getConfigPortalActive())server.handleClient();

    STAGE(ST_MQTT);
    connectMqtt();
    mqtt.loop();

    STAGE(ST_UPS);
    if (cfg.nutEnabled && WiFi.status() == WL_CONNECTED) nut.begin();   // no-op once running
    nut.loop();
    if (cfg.apcEnabled && WiFi.status() == WL_CONNECTED) apc.begin();
    apc.loop();
    ups.loop();                       // mains-failure statistics, clears FSD

    STAGE(ST_DISCOVERY);
    // Discovery, one entity per pass; a failed publish is retried (max 3x)
    if (g_discIdx < ENTITY_COUNT && mqtt.connected())
    {
        static uint8_t tries = 0;
        if (publishEntity(ENTITIES[g_discIdx]) || ++tries >= 3) {
            if (tries >= 3) Serial.printf("[MQTT] discovery failed: %s\n", ENTITIES[g_discIdx].uid);
            g_discIdx++;
            tries = 0;
        }
    }

    updateVoltageGuard();
    autoLoop(millis());

    // ESS fail-safe: no fresh setpoint for essTimeoutS → back to 0 W
    if (cfg.essTimeoutS && g_essPower != 0 &&
        now - g_lastEssCmdMs >= (uint32_t)cfg.essTimeoutS * 1000UL)
    {
        g_essPower = 0;
        vebus.setESSPower(0);
        g_publishNow = true;
        Serial.println("[app] ESS setpoint timed out — fallback to 0 W");
    }

    // Queue ESS power every ESS_INTERVAL_MS
    if (now - lastESSMs >= ESS_INTERVAL_MS)
    {
        lastESSMs = now;
        vebus.setESSPower(g_essPower);
        ramRequested     = false;
        extRam1Requested = false;
        extRam2Requested = false;
        stateRequested   = false;
    }

    // Queue legacy read RAM (bat voltage + AC power)
    if (!ramRequested && (now - lastESSMs >= (ESS_INTERVAL_MS - RAM_OFFSET_MS)))
    {
        vebus.requestReadRAM();
        ramRequested = true;
    }

    // Queue extended RAM batch 1: mains V/A, inverter V/A, output W, mains W
    if (!extRam1Requested && (now - lastESSMs >= EXT_RAM1_OFFSET_MS))
    {
        const uint8_t ids[] = {
            VEBUS_RAM_UMAINS_RMS, VEBUS_RAM_IMAINS_RMS,
            VEBUS_RAM_UINVERTER_RMS, VEBUS_RAM_IINVERTER_RMS,
            VEBUS_RAM_OUTPUT_POWER, VEBUS_RAM_MAINS_POWER
        };
        vebus.readRAMVars(ids, 6);
        extRam1Requested = true;
    }

    // Queue extended RAM batch 2: battery current, SoC, mains freq, inverter freq
    if (!extRam2Requested && (now - lastESSMs >= EXT_RAM2_OFFSET_MS))
    {
        const uint8_t ids[] = {
            VEBUS_RAM_UBAT, VEBUS_RAM_IBAT, VEBUS_RAM_CHARGE_STATE,
            VEBUS_RAM_MAINS_PERIOD, VEBUS_RAM_INVERTER_PERIOD
        };
        vebus.readRAMVars(ids, 5);
        extRam2Requested = true;
    }

    // Queue device state request
    if (!stateRequested && (now - lastESSMs >= STATE_OFFSET_MS))
    {
        vebus.requestDeviceState();
        stateRequested = true;
    }

    STAGE(ST_VEBUS);
    scaler.loop();          // queries scale/offset of each RAM variable once

    // Extended RAM responses — matched by RAM id, so a late or dropped
    // response can never land in the wrong variable; converted to real units.
    if (vebus.hasRAMVarResponse())
    {
        bool gotBatV = false, gotBatA = false;
        float sampleV = 0, sampleA = 0;
        for (uint8_t i = 0; i < vebus.getRAMVarCount(); i++)
        {
            uint8_t id  = vebus.getRAMVarId(i);
            int16_t raw = vebus.getRAMVarValue(i);
            bool period = id == VEBUS_RAM_MAINS_PERIOD || id == VEBUS_RAM_INVERTER_PERIOD;
            float v = period ? scaler.frequency(id, raw) : scaler.value(id, raw);
            ups.set(id, v);
            if (id == VEBUS_RAM_UBAT) { gotBatV = true; sampleV = v; }
            if (id == VEBUS_RAM_IBAT) { gotBatA = true; sampleA = v; }
            if (id == VEBUS_RAM_MAINS_POWER && !vebus.hasNoSync()) {
                acHaveMains = true; acMainsMs = millis();
                if (acRunning) acEnergy.sample(acMainsMs, v * (acInvert ? -1 : 1));
            }
            if (id == VEBUS_RAM_OUTPUT_POWER)
                vebus.setACOutLoad((int16_t)lroundf(v));   // virtual setpoint mode
        }
        if (gotBatV && gotBatA && !vebus.hasNoSync() && isfinite(sampleV) && sampleV > 0 && isfinite(sampleA)) {
            batSampleMs = millis(); batHaveSample = true;
            batSampleV = sampleV; batSampleA = sampleA;
            sampleEnergy(batSampleMs,sampleV,sampleA);
            if (acRunning) batEnergy.sampleDC(batSampleMs, sampleV, sampleA);
        }
        vebus.clearRAMVarResponse();
    }

    // Firmware version: asked once after VE.Bus is in sync (it never changes)
    static bool versionRequested = false;
    if (!versionRequested && !vebus.hasNoSync())
    {
        versionRequested = true;
        vebus.requestVersion();
    }

    // Persist ESS setpoint / battery-neutral mode (at most once a minute)
    if (g_essDirty && (!g_essSavedMs || now - g_essSavedMs >= 60000))
    {
        prefs.begin("vebus_mqtt", false);
        prefs.putBool("autosession", false);
        prefs.putShort("essw", g_essPower);
        prefs.putBool("vmode", vebus.isVirtualSetpointMode());
        prefs.end();
        g_essDirty   = false;
        g_essSavedMs = now;
    }

    if (vebus.hasDeviceStateResponse())
    {
        g_devState    = vebus.getDeviceState();
        g_devSubState = vebus.getDeviceSubState();
        ups.data.deviceState = g_devState;
        vebus.clearDeviceStateResponse();
    }

    if (vebus.hasVersionResponse())
    {
        snprintf(g_fwVersion, sizeof(g_fwVersion), "%u.%u",
                 vebus.getVersionHigh(), vebus.getVersionLow());
        strlcpy(ups.data.firmware, g_fwVersion, sizeof(ups.data.firmware));
        vebus.clearVersionResponse();
    }

    // Auto-wakeup on no-sync (not while the bus is deliberately muted)
#if !defined(VEBUS_CONTROL_TEST) && !defined(VEBUS_READ_ONLY)
    if (!autoLowLock && vebus.hasNoSync() && vebus.isTxEnabled())
    {
        static uint32_t lastWakeupMs = 0;
        if (now - lastWakeupMs >= WAKEUP_RETRY_MS)
        {
            lastWakeupMs = now;
            vebus.requestWakeup();
            Serial.println("[app] No sync — queued wakeup");
        }
    }
#endif

    if (acRunning) {
        const uint32_t acNow = millis();
        if (!acFresh(acNow)) acEnergy.gap();
        if (!batFresh(acNow)) batEnergy.gap();
        if (acNow - acRowMs >= 60000) acRecord(acNow);
    }

    if(!batFresh(millis()))energyHaveSample=false;
    saveEnergy();
    STAGE(ST_PUBLISH);
    // Publish state periodically (or right after a command)
    if (now - lastPublishMs >= PUBLISH_INTERVAL_MS || g_publishNow)
    {
        if (now - lastPublishMs >= PUBLISH_INTERVAL_MS) histAccumulate();
        lastPublishMs = now;
        g_publishNow  = false;
        if (vebus.hasNewData()) vebus.clearNewData();
        if (mqtt.connected()) publishState();
    }

    STAGE(ST_HISTORY);
    if (now - histLastMs >= HIST_INTERVAL_MS)
    {
        histLastMs += HIST_INTERVAL_MS;
        histPush();
    }

    STAGE(ST_SERIAL);
    pollSerial();
    showStatusLed(chooseLedStatus(ledUpdating, WiFi.status() == WL_CONNECTED,
                                 !vebus.hasNoSync(), WiFi.RSSI(), mqtt.connected(), acRunning), millis());
    STAGE(ST_NONE);

    if (g_restartAtMs && (int32_t)(now - g_restartAtMs) >= 0)
    {
        Serial.println("[app] restarting");
        saveEnergy(true);
        mqtt.disconnect();
        delay(100);
        ESP.restart();
    }
}
