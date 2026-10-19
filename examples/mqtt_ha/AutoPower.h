#pragma once
#include <stdint.h>

// AC-IN watts, positive = Victron discharges. Other battery positive = discharge.
namespace AutoPower {
constexpr int LIMIT = 3000;
constexpr int DEADBAND = 100;
constexpr int STEP = 500; // maximum change per fresh evcc snapshot (5 s polling)
constexpr int bounded(int w, int chargeLimit = LIMIT, int dischargeLimit = LIMIT) { return w < -chargeLimit ? -chargeLimit : w > dischargeLimit ? dischargeLimit : w; }
constexpr bool inputsFresh(bool valid, uint32_t ageMs, bool wifi, bool ac, bool battery, bool tx) {
    return valid && ageMs <= 15000 && wifi && ac && battery && tx;
}
constexpr int target(int residualW, int otherBatteryW, int chargeLimit = LIMIT, int dischargeLimit = LIMIT, unsigned sharePct = 90) {
    return residualW > DEADBAND
        ? (otherBatteryW < -150 ? 0 : bounded(residualW * (int)sharePct / 100, chargeLimit, dischargeLimit))
        : residualW < -DEADBAND
          ? (otherBatteryW > 150 ? 0 : bounded(residualW, chargeLimit, dischargeLimit)) : 0;
}
constexpr int approach(int current, int wanted) {
    return (current < 0 && wanted >= 0) || (current > 0 && wanted <= 0) ? 0
        : wanted > current + STEP ? current + STEP
        : wanted < current - STEP ? current - STEP : wanted;
}
static_assert(!inputsFresh(true, 15001, true, true, true, true), "evcc stale timeout");
static_assert(!inputsFresh(false, 0, true, true, true, true), "invalid evcc stops control");
static_assert(!inputsFresh(true, 0, true, false, true, true), "stale VE.Bus stops control");
static_assert(!inputsFresh(true, 0, true, true, true, false), "muted bus stops control");
static_assert(inputsFresh(true, 5000, true, true, true, true), "fresh input accepted");
// Compile-time cases exercise the actual control functions, without a device.
constexpr bool voltageLatch(bool latched, float volts, float cutoff, float delta) {
    return cutoff <= 0 ? false : volts <= cutoff ? true : latched && volts < cutoff + delta;
}
static_assert(voltageLatch(false,46,47,1), "cutoff latches");
static_assert(voltageLatch(true,47.5f,47,1), "hysteresis holds");
static_assert(!voltageLatch(true,48,47,1), "recovery releases");
static_assert(!voltageLatch(true,46,0,1), "disabled guard releases");
static_assert(target(6000,0,2000,4500)==4500, "configurable discharge limit");
static_assert(target(-6000,0,2000,4500)==-2000, "configurable charge limit");
static_assert(target(2000, 200) == 1800, "90 percent household share");
static_assert(target(5000, 200) == 3000, "discharge capped at 3 kW");
static_assert(target(-5000, -200) == -3000, "charge capped at 3 kW");
static_assert(target(-1000, 500) == 0, "do not charge from BYD discharge");
static_assert(target(1000, -500) == 0, "do not discharge into BYD charging");
static_assert(target(90, 0) == 0 && target(-90, 0) == 0, "deadband");
static_assert(approach(0, 3000) == 500, "ramp");
static_assert(approach(-2000, 1000) == 0, "zero before reversal");
static_assert(approach(2000, 0) == 0, "stop immediately");
}

static_assert(AutoPower::target(2000,200,3000,3000,0)==0,"zero share disables discharge");
static_assert(AutoPower::target(2000,200,3000,3000,10)==200,"10 percent share");
static_assert(AutoPower::target(2000,200,3000,3000,50)==1000,"50 percent share");
static_assert(AutoPower::target(2000,200,3000,3000,80)==1600,"80 percent share");
static_assert(AutoPower::target(2000,200,3000,3000,100)==2000,"100 percent share");
static_assert(AutoPower::target(6000,200,3000,3000,100)==3000,"share respects watt limit");
static_assert(AutoPower::target(-2000,-200,3000,3000,0)==-2000,"share does not restrict PV charging");
