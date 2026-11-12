#include "../examples/mqtt_ha/RestartBudget.h"
#include <cassert>
#include <iostream>
int main(){
 RestartBudget b;
 assert(b.reserve(0));assert(b.count(0)==1&&b.cooldown(0)==1800);
 assert(b.count(100)==1);assert(b.count(99)==1);assert(b.cooldown(99)==1800);
 assert(!b.reserve(1799999));assert(b.cooldown(1799999)==1);
 assert(b.reserve(1800000));assert(b.count(1800000)==2);
 assert(!b.reserve(3599999));assert(b.cooldown(3599999)==1);
 assert(b.reserve(3600000));assert(b.count(3600000)==2);
 assert(b.count(7200000)==0);assert(b.available(7200000));
 RestartBudget reboot;assert(reboot.count(0)==0&&reboot.available(0));
 RestartBudget wrap;const uint32_t start=0xffff0000UL;
 assert(wrap.reserve(start));assert(!wrap.reserve(uint32_t(start+1799999UL)));
 assert(wrap.reserve(uint32_t(start+1800000UL)));assert(!wrap.reserve(uint32_t(start+3599999UL)));
 assert(wrap.reserve(uint32_t(start+3600000UL)));
 std::cout<<"Restart budget: two/rolling hour, 30-minute spacing, failed attempt counted, reboot reset and millis rollover OK\n";
}
