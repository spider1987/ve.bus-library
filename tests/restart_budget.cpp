#include "../examples/mqtt_ha/RestartBudget.h"
#include <cassert>
#include <iostream>
int main(){
 const uint32_t t=1793300000,day=20261029;
 RestartBudget b;assert(RestartBudget::valid(b.record));
 assert(!b.available(0,0));assert(!b.reserve(0,t));
 assert(b.reserve(day,t));assert(b.count(day)==1&&b.cooldown(t)==1800);
 // Restore the exact persisted record as on a controller reboot.
 RestartBudget reboot;reboot.record=b.record;
 assert(!reboot.reserve(day,t+1799));assert(reboot.cooldown(t+1799)==1);
 assert(reboot.reserve(day,t+1800));assert(reboot.count(day)==2);
 assert(!reboot.reserve(day,t+7200)); // third attempt refused
 assert(!reboot.reserve(day-1,t+9000)); // clock/date rollback
 assert(!reboot.reserve(day+1,t+1799)); // epoch rollback
 assert(reboot.count(day+1)==0);
 assert(!reboot.reserve(day+1,t+1801)); // midnight does not erase cooldown
 assert(reboot.reserve(day+1,t+3600));assert(reboot.count(day+1)==1);
 RestartBudget::Record corrupt=reboot.record;corrupt.count=3;
 assert(!RestartBudget::valid(corrupt));
 assert(!reboot.available(21010101,4102444799UL));
 std::cout<<"Restart budget: two/day, 30-minute spacing, reboot replay, midnight, rollback and invalid clock/storage OK\n";
}
