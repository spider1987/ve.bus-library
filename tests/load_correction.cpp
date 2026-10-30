#include "../examples/mqtt_ha/AutoPower.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
using namespace AutoPower;
for(float amps:{0.f,-10.f,-50.f,-100.f,10.f,100.f}){
 float expected=amps<0?48*-amps*.0002f:0;
 assert(std::fabs(voltageCorrection(48,amps,.0002f,2)-expected)<.0001f);
}
assert(!voltageStop(47.5,-100,48,.0002f,2,46));
assert(voltageStop(46,-100,48,.005f,5,46));
assert(voltageStop(47.5,100,48,.001f,2,46));
bool latch=false;
latch=compensatedLatch(latch,47.5,-100,48,.5,.0002,2,46);assert(!latch);
latch=compensatedLatch(latch,46,-100,48,.5,.0002,2,46);assert(latch);
latch=compensatedLatch(latch,48,-100,48,.5,.001,2,46);assert(latch);
latch=compensatedLatch(latch,48.5,0,48,.5,.001,2,46);assert(!latch);
assert(!compensatedLatch(true,40,-100,0,.5,0,2,0));
std::cout<<"Load correction: 100 A, sign, hard floor, cap, legacy zero-factor and raw recovery OK\n";
}
