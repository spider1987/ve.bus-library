#pragma once
#include <stdint.h>

// One NVS blob per reserved attempt. Calendar date is supplied in Europe/Berlin.
struct RestartBudget {
    struct Record {uint32_t day=0,count=0,lastEpoch=0;} record;
    static bool valid(const Record &r){
        return (!r.day&&!r.count&&!r.lastEpoch)||
            (r.day>=20240101&&r.day<=21001231&&r.count>=1&&r.count<=2&&r.lastEpoch>=1704067200UL);
    }
    uint32_t count(uint32_t day)const{return day&&day>record.day?0:record.count;}
    uint32_t cooldown(uint32_t epoch)const{
        if(!record.lastEpoch)return 0;
        if(epoch<record.lastEpoch)return 1800;
        const uint32_t elapsed=epoch-record.lastEpoch;return elapsed>=1800?0:1800-elapsed;
    }
    bool available(uint32_t day,uint32_t epoch)const{
        return day>=20240101&&day<=21001231&&epoch>=1704067200UL&&epoch<=4102444799UL&&day>=record.day&&
            count(day)<2&&!cooldown(epoch);
    }
    bool reserve(uint32_t day,uint32_t epoch){
        if(!available(day,epoch))return false;
        record.count=count(day)+1;record.day=day;record.lastEpoch=epoch;return true;
    }
};
