#pragma once
#include <stdint.h>
#include <math.h>

namespace EnergyMath {
// Exact linear integration, including a change of current direction.
constexpr double positiveWh(double a, double b, double hours) {
    return a >= 0 && b >= 0 ? (a + b) * .5 * hours
        : a <= 0 && b <= 0 ? 0
        : a > 0 ? a * a / (2 * (a - b)) * hours
        : b * b / (2 * (b - a)) * hours;
}
static_assert(positiveWh(3600,3600,1.0/3600) == 1, "1 Wh charge");
static_assert(positiveWh(-3600,-3600,1.0/3600) == 0, "no charge during discharge");
static_assert(positiveWh(3600,-3600,1.0/3600) == .25, "direction crossing charge");
static_assert(positiveWh(-3600,3600,1.0/3600) == .25, "direction crossing discharge");
static_assert(positiveWh(2400,2400,.5)+positiveWh(2400,2400,.5)==2400, "midnight split conserves energy");
}
#if __cplusplus >= 201402L
#define ENERGY_CONSTEXPR14 constexpr
#else
#define ENERGY_CONSTEXPR14
#endif
struct EnergyDay {
    uint32_t date; double chargeWh, dischargeWh; uint32_t measuredMs;
    constexpr EnergyDay():date(0),chargeWh(0),dischargeWh(0),measuredMs(0){}
};
struct EnergyBook {
    static constexpr uint32_t MAGIC = 0x454E0001;
    uint32_t magic = MAGIC;
    uint32_t next = 0, count = 0;
    double chargeWh = 0, dischargeWh = 0, unassignedChargeWh = 0, unassignedDischargeWh = 0;
    uint64_t measuredMs = 0, gapMs = 0;
    EnergyDay days[32] = {};
    bool valid() const {
        if(magic!=MAGIC || next>=32 || count>32 || !isfinite(chargeWh) || !isfinite(dischargeWh) ||
           !isfinite(unassignedChargeWh) || !isfinite(unassignedDischargeWh) || chargeWh<0 || dischargeWh<0 ||
           unassignedChargeWh<0 || unassignedDischargeWh<0) return false;
        for(const auto &d:days) if((d.date && (d.date<20200101 || d.date>20991231)) ||
            !isfinite(d.chargeWh) || !isfinite(d.dischargeWh) || d.chargeWh<0 || d.dischargeWh<0) return false;
        return true;
    }
    ENERGY_CONSTEXPR14 const EnergyDay *find(uint32_t date) const { for(unsigned i=0;i<32;++i)if(date && days[i].date==date)return &days[i];return nullptr; }
    ENERGY_CONSTEXPR14 uint32_t dayIndex(uint32_t date) {
        for(unsigned i=0;i<32;++i)if(days[i].date==date)return i;
        const uint32_t i=next;days[i].date=date;days[i].chargeWh=0;days[i].dischargeWh=0;days[i].measuredMs=0;
        next=(next+1)%32;if(count<32)++count;return i;
    }
    EnergyDay &day(uint32_t date) {return days[dayIndex(date)];}
    ENERGY_CONSTEXPR14 void addDay(uint32_t date,double a,double b,double ms) {
        const uint32_t i=dayIndex(date);const double h=ms/3600000.0;
        days[i].chargeWh+=EnergyMath::positiveWh(a,b,h);days[i].dischargeWh+=EnergyMath::positiveWh(-a,-b,h);
        days[i].measuredMs+=(uint32_t)(ms+.5);
    }
    ENERGY_CONSTEXPR14 void add(uint32_t dateA,uint32_t dateB,double fraction,uint32_t ms,double a,double b) {
        const double h=ms/3600000.0, charged=EnergyMath::positiveWh(a,b,h), discharged=EnergyMath::positiveWh(-a,-b,h);
        chargeWh+=charged;dischargeWh+=discharged;measuredMs+=ms;
        if(dateA && dateA==dateB) addDay(dateB,a,b,ms);
        else if(dateA && dateB && fraction>=0 && fraction<=1) {
            const double middle=a+(b-a)*fraction;
            addDay(dateA,a,middle,ms*fraction);addDay(dateB,middle,b,ms*(1-fraction));
        } else {unassignedChargeWh+=charged;unassignedDischargeWh+=discharged;}
    }
};

#undef ENERGY_CONSTEXPR14
