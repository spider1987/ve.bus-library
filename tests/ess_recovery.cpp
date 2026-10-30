#include "../examples/mqtt_ha/EssRecovery.h"
#include <cassert>
#include <iostream>
int main(){
 EssRecovery r;
 assert(!r.beginDischarge(0,false,true));
 assert(!r.beginDischarge(0,true,false)); // OFF must remain OFF
 assert(r.beginDischarge(0,true,true)); // charger-only or ON
 assert(!r.beginDischarge(5000,true,true));
 r.observe(5000,true,true); // delivering must not create another ON episode
 assert(!r.beginDischarge(10000,true,true));
 assert(!r.beginDischarge(15000,false,true));
 assert(r.beginDischarge(20000,true,true));
 r.reset();
 for(unsigned t=0;t<30000;t+=5000)assert(!r.observe(t,true,false));
 assert(r.observe(30000,true,false));
 for(unsigned t=35000;t<90000;t+=5000)assert(!r.observe(t,true,false));
 assert(r.observe(90000,true,false));
 for(unsigned t=95000;t<150000;t+=5000)assert(!r.observe(t,true,false));
 assert(r.observe(150000,true,false));
 for(unsigned t=155000;t<=220000;t+=5000)assert(!r.observe(t,true,false));
 assert(r.exhausted&&r.retries==3);
 assert(!r.observe(225000,true,true));assert(r.retries==0);
 assert(!r.observe(230000,true,false));
 assert(!r.observe(235000,false,false));
 assert(!r.observe(300000,true,false));
 assert(!r.observe(340000,true,false)); // missing observation restarts delay
 assert(r.retries==0);
 std::cout<<"ESS recovery timing, permissions, gaps, cooldown and retry cap OK\n";
}
