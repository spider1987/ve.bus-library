#include "../examples/mqtt_ha/AutoPower.h"
#include <cassert>
#include <iostream>
using namespace AutoPower;
int main(){
 assert(target(500,0,3000,3000,100,100,50)==550);
 assert(target(500,0,3000,3000,90,100,50)==500);
 assert(target(363,-36,4000,4000,100,100,50)==413);
 assert(target(500,0,3000,3000,0,100,500)==0);
 assert(target(500,-151,3000,3000,100,100,500)==0);
 assert(target(0,0,3000,3000,100,100,50)==0);
 assert(target(100,0,3000,3000,100,100,50)==0);
 assert(target(500,0,3000,0,100,100,50)==0);
 assert(target(4000,0,3000,3000,100,100,500)==3000);
 assert(target(-300,0,3000,3000,100,100,500)==-250);
 assert(target(-300,101,3000,3000,100,100,500)==0);
 for(int w=-6000;w<=6000;++w)assert(target(w,0,3000,3000,90,100,0)==target(w,0,3000,3000,90,100));
 assert(approach(500,550)==550);assert(approach(500,0)==0);
 std::cout<<"Discharge offset: share then offset, cap, zero share/limit, BYD guard, PV charging, deadband and legacy default OK\n";
}
