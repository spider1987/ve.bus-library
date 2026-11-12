#pragma once
#include <stdint.h>

// RAM only: two attempts in a rolling hour, with 30-minute spacing.
// Advance ages on each controller loop/API call, including millis() rollover.
struct RestartBudget {
    uint32_t ages[2]={0,0},lastMs=0;
    uint8_t used=0;
    void update(uint32_t now){
        const uint32_t elapsed=now-lastMs;
        if(used&&elapsed>0x7fffffffUL)return; // Ignore an older loop timestamp after an API read.
        lastMs=now;
        uint8_t keep=0;
        for(uint8_t i=0;i<used;++i){
            if(elapsed>=3600000UL-ages[i])continue;
            ages[keep++]=ages[i]+elapsed;
        }
        used=keep;
    }
    uint32_t count(uint32_t now){update(now);return used;}
    uint32_t cooldown(uint32_t now){
        update(now);uint32_t wait=0;
        if(used&&ages[used-1]<1800000UL)wait=1800000UL-ages[used-1];
        if(used>=2&&3600000UL-ages[0]>wait)wait=3600000UL-ages[0];
        return (wait+999)/1000;
    }
    bool available(uint32_t now){return cooldown(now)==0;}
    bool reserve(uint32_t now){
        if(!available(now))return false;
        ages[used++]=0;return true;
    }
};
