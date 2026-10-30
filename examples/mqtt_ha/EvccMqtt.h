#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// No broker/Arduino dependency: production cache is also used by native tests.
struct EvccMqtt {
    static const unsigned MAX_LP=8;
    struct Number {
        float value=0;uint32_t at=0;unsigned count=0;bool valid=false;
        void put(const char *s,uint32_t now,float minV,float maxV) {
            char *end=nullptr;float v=strtof(s,&end);
            valid=*s&&end&&!*end&&isfinite(v)&&v>=minV&&v<=maxV;
            if(!valid){count=0;return;}value=v;at=now;if(count<2)++count;
        }
    };
    struct Lp {Number power,limit;bool boost=false,boostKnown=false,connected=false,connectedKnown=false,pv=false;uint32_t boostAt=0;};
    Number grid,home,pv,byd,vic,soc,lpCount;
    Lp lp[MAX_LP];bool online=false,dischargeAllowed=false;
    void reset(){*this=EvccMqtt();}
    bool ingest(const char *topic,const char *value,uint32_t now) {
        if(!strcmp(topic,"evcc/status")) {
            if(strcmp(value,"online")){reset();return true;}online=true;return true;
        }
        if(!strcmp(topic,"evcc/site/batteryMode")){dischargeAllowed=!strcmp(value,"normal")||!strcmp(value,"discharge");return true;}
        const struct {const char *topic;Number *number;float minimum,maximum;} fields[]={
            {"evcc/site/grid/power",&grid,-99999,99999},{"evcc/site/homePower",&home,0,99999},
            {"evcc/site/pvPower",&pv,0,99999},{"evcc/site/battery/devices/1/power",&byd,-99999,99999},
            {"evcc/site/battery/devices/2/power",&vic,-99999,99999},{"evcc/site/battery/devices/1/soc",&soc,0,100},
            {"evcc/loadpoints",&lpCount,0,MAX_LP}};
        for(const auto &f:fields)if(!strcmp(topic,f.topic)){f.number->put(value,now,f.minimum,f.maximum);return true;}
        const char *prefix="evcc/loadpoints/";
        if(strncmp(topic,prefix,strlen(prefix)))return false;
        char *end=nullptr;long id=strtol(topic+strlen(prefix),&end,10);
        if(id<1||id>MAX_LP||!end||*end!='/')return true;
        auto &p=lp[id-1];const char *key=end+1;
        if(!strcmp(key,"chargePower"))p.power.put(value,now,0,99999);
        else if(!strcmp(key,"batteryBoostLimit"))p.limit.put(value,now,0,100);
        else if(!strcmp(key,"batteryBoost")){
            p.boostKnown=!strcmp(value,"true")||!strcmp(value,"false");p.boost=!strcmp(value,"true");p.boostAt=now;
        }else if(!strcmp(key,"connected")){
            p.connectedKnown=!strcmp(value,"true")||!strcmp(value,"false");p.connected=!strcmp(value,"true");
        }else if(!strcmp(key,"mode"))p.pv=!strcmp(value,"pv")||!strcmp(value,"smart");
        return true;
    }
    template<class Snapshot> void snapshot(Snapshot &s,uint32_t now,unsigned timeoutS) const {
        s.fetchError="MQTT: warte auf vollständige Live-Messwerte";
        if(!online){s.fetchError="MQTT: evcc offline";return;}
        if(!lpCount.valid||floorf(lpCount.value)!=lpCount.value)return;
        uint32_t oldest=0,newest=0xffffffffUL;
        auto accept=[&](const Number &n)->bool {
            if(!n.valid||n.count<2)return false;
            uint32_t age=now-n.at;if(age>timeoutS*1000UL)return false;
            if(age>oldest)oldest=age;if(age<newest)newest=age;return true;
        };
        if(!accept(grid)||!accept(home)||!accept(pv)||!accept(byd)||!accept(vic))return;
        float ev=0;bool boost=false;float limit=0;int boostW=0;
        for(unsigned i=0;i<(unsigned)lpCount.value;++i){
            const auto &p=lp[i];if(!accept(p.power))return;ev+=p.power.value;
            if(dischargeAllowed&&p.boostKnown&&p.boost&&p.connectedKnown&&p.connected&&p.pv&&now-p.boostAt<=65000&&
               p.power.value>100){
                boost=true;if(p.limit.value>limit)limit=p.limit.value;boostW+=(int)lroundf(p.power.value);
            }
        }
        if(oldest-newest>5000||ev>=100000){s.fetchError="MQTT: Messwerte aus unterschiedlichen Zyklen";return;}
        s.gridW=lroundf(grid.value);s.otherW=lroundf(byd.value);s.victronW=lroundf(vic.value);
        s.demandW=s.gridW+s.otherW+s.victronW;s.solarResidualW=lroundf(home.value+ev-pv.value);
        s.bydSocValid=soc.valid&&now-soc.at<=timeoutS*1000UL;s.bydSoc=soc.value;
        s.boostActive=boost;s.boostLimitSoc=limit;s.boostChargeW=boostW;
        s.valid=true;s.solarValid=true;s.atMs=now-oldest;s.fetchError="OK";
    }
};
