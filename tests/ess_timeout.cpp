#include "../examples/mqtt_ha/EssTimeout.h"
#include <cassert>
#include <iostream>
int main(){
 assert(!EssTimeout::expired(1000,1001,30)); // older loop clock after fresh command
 assert(!EssTimeout::expired(1001,1001,30));
 assert(!EssTimeout::expired(30999,1000,30));
 assert(EssTimeout::expired(31000,1000,30));
 assert(!EssTimeout::expired(999999,0,0));
 const uint32_t last=0xfffff000U;
 assert(!EssTimeout::expired(uint32_t(last+29999U),last,30));
 assert(EssTimeout::expired(uint32_t(last+30000U),last,30));
 std::cout<<"ESS timeout: refreshed command, stale clock, real expiry, disabled timeout and millis rollover OK\n";
}
