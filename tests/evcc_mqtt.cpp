#include "../examples/mqtt_ha/EvccMqtt.h"
#include "../examples/mqtt_ha/AutoPower.h"
#include <cassert>
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
struct Snapshot {const char*fetchError="";bool valid=false,solarValid=false,bydSocValid=false,boostActive=false;float bydSoc=0,boostLimitSoc=0;uint32_t atMs=0;int gridW=0,otherW=0,victronW=0,demandW=0,solarResidualW=0,boostChargeW=0;};
void cycle(EvccMqtt &m,uint32_t now){
 const char *t[]={"evcc/site/grid/power","evcc/site/homePower","evcc/site/pvPower","evcc/site/battery/devices/1/power","evcc/site/battery/devices/2/power","evcc/site/battery/devices/1/soc","evcc/loadpoints/1/chargePower"};
 const char *v[]={"0","500","1000","2000","500","80","3000"};
 for(unsigned i=0;i<7;++i)m.ingest(t[i],v[i],now);
}
int main(int argc,char **argv){EvccMqtt m;m.ingest("evcc/status","online",1000);m.ingest("evcc/site/batteryMode","normal",1000);m.ingest("evcc/loadpoints","1",1000);
 m.ingest("evcc/loadpoints/1/batteryBoost","true",1000);m.ingest("evcc/loadpoints/1/connected","true",1000);m.ingest("evcc/loadpoints/1/mode","pv",1000);
 cycle(m,1000);Snapshot initial;m.snapshot(initial,1000,35);assert(!initial.valid); // retained-only cannot control
 cycle(m,31000);Snapshot live;m.snapshot(live,32000,35);assert(live.valid&&live.solarValid&&live.boostActive&&live.boostChargeW==3000);assert(live.demandW==2500&&live.solarResidualW==2500);assert(live.atMs==31000);
 Snapshot cached;m.snapshot(cached,61000,35);assert(cached.valid&&cached.atMs==31000); // no invented timestamp
 Snapshot stale;m.snapshot(stale,66001,35);assert(!stale.valid);
 m.ingest("evcc/loadpoints/1/batteryBoost","false",32001);Snapshot off;m.snapshot(off,32001,35);assert(off.valid&&!off.boostActive);
 m.ingest("evcc/site/homePower","600",40000);Snapshot skew;m.snapshot(skew,40000,35);assert(!skew.valid);
 m.ingest("evcc/site/homePower","null",40001);Snapshot missing;m.snapshot(missing,40001,35);assert(!missing.valid);
 m.ingest("evcc/status","offline",41000);assert(!m.online);cycle(m,42000);Snapshot offline;m.snapshot(offline,42000,35);assert(!offline.valid);
 m.reset();m.ingest("evcc/status","online",43000);m.ingest("evcc/loadpoints","9",43000);cycle(m,43000);cycle(m,44000);Snapshot tooMany;m.snapshot(tooMany,44000,35);assert(!tooMany.valid);
 assert(AutoPower::boostDemand(3500,3000,true,true,false)==500);assert(AutoPower::boostReady(true,true,true,11,10));assert(!AutoPower::boostReady(true,true,true,10,10));
 if(argc>1){
  EvccMqtt replay;std::ifstream file(argv[1]);assert(file);DynamicJsonDocument doc(32768);assert(!deserializeJson(doc,file));uint32_t last=0;
  for(JsonObject r:doc.as<JsonArray>()){last=(uint32_t)(r["t"].as<double>()*1000);replay.ingest(r["topic"],r["payload"],last);}
  Snapshot captured;replay.snapshot(captured,last,60);assert(captured.valid&&captured.solarValid&&captured.bydSocValid);
  std::cout<<"Captured MQTT publications: coherent live snapshot OK, residual "<<captured.demandW<<" W\n";
 }
 std::cout<<"MQTT cache/boost: retained startup, live cycles, timestamps, stale/skew, offline, missing fields and own SoC floor OK\n";
}
