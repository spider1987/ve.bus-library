#include <type_traits>
#include "../examples/mqtt_ha/EnergyBook.h"
constexpr bool testMidnight() {
 EnergyBook b{};b.add(20261018,20261019,.5,1000,3600,3600);
 return b.chargeWh==1 && b.dischargeWh==0 && b.find(20261018)->chargeWh==.5 && b.find(20261019)->chargeWh==.5 && b.find(20261019)->measuredMs==500;
}
constexpr bool testDirection() {
 EnergyBook b{};b.add(20261018,20261018,-1,1000,3600,-3600);
 return b.chargeWh==.25 && b.dischargeWh==.25 && b.find(20261018)->chargeWh==.25 && b.find(20261018)->dischargeWh==.25;
}
constexpr bool testNoClock() {
 EnergyBook b{};b.add(0,0,-1,1000,3600,3600);
 return b.chargeWh==1 && b.unassignedChargeWh==1 && b.count==0;
}
constexpr bool testArchiveAndRestore() {
 EnergyBook b{};
 for(unsigned i=0;i<33;++i){unsigned date=i<31?20261001+i:20261101+i-31;b.add(date,date,-1,1000,3600,3600);}
 EnergyBook restored=b;
 return restored.count==32 && restored.find(20261001)==nullptr && restored.find(20261002)->chargeWh==1 && restored.find(20261102)->chargeWh==1 && restored.chargeWh==33;
}
static_assert(testMidnight(),"midnight split failed");
static_assert(testDirection(),"charge/discharge integration failed");
static_assert(testNoClock(),"unsynchronised clock accounting failed");
static_assert(testArchiveAndRestore(),"32 day archive/restore failed");

static_assert(std::is_trivially_copyable<EnergyBook>::value,"energy snapshot must be byte-copyable");

constexpr bool testMidnightDirectionCross() {
 EnergyBook b{};b.add(20261018,20261019,.5,1000,3600,-3600);
 return b.chargeWh==.25 && b.dischargeWh==.25 && b.find(20261018)->chargeWh==.25 && b.find(20261018)->dischargeWh==0 && b.find(20261019)->chargeWh==0 && b.find(20261019)->dischargeWh==.25;
}
constexpr bool testOneHour() {
 EnergyBook b{};for(unsigned i=0;i<720;++i)b.add(20261018,20261018,-1,5000,2400,2400);
 return b.chargeWh>2399.9999 && b.chargeWh<2400.0001 && b.measuredMs==3600000 && b.dischargeWh==0;
}
static_assert(testMidnightDirectionCross(),"direction crossing at midnight failed");
static_assert(testOneHour(),"one hour 2.4 kWh accumulation failed");
