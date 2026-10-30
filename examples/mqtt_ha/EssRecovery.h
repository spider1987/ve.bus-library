#pragma once
#include <stdint.h>

// Request ON only for a permitted discharge episode, never from device OFF.
struct EssRecovery {
    uint32_t since=0, lastObservation=0, lastRetry=0;
    unsigned retries=0;
    bool tracking=false, exhausted=false;
    bool dischargeActive=false;
    bool beginDischarge(uint32_t now,bool permitted,bool deviceActive){
        if(!permitted||!deviceActive){dischargeActive=false;return false;}
        // A data gap does not itself authorize another immediate ON command.
        (void)now;
        if(dischargeActive)return false;
        dischargeActive=true;return true;
    }
    void reset(){*this=EssRecovery{};}
    bool observe(uint32_t now,bool eligible,bool delivering){
        if(!eligible){tracking=false;lastObservation=now;return false;}
        if(delivering){since=0;lastRetry=0;retries=0;tracking=false;exhausted=false;lastObservation=now;return false;}
        if(!tracking || now-lastObservation>7500){since=now;tracking=true;}
        lastObservation=now;
        if(now-since<30000 || (retries && now-lastRetry<60000))return false;
        if(retries>=3){exhausted=true;return false;}
        ++retries;lastRetry=now;tracking=false;
        return true;
    }
};
