#include "../examples/mqtt_ha/EssRestart.h"
#include <cassert>
#include <iostream>
using R=EssRestart;
void trigger(R &r){for(unsigned t=0;t<120000;t+=5000)assert(!r.observe(t,true,false));assert(r.observe(120000,true,false));}
int main(){
 R r;trigger(r);assert(r.used&&r.phase==R::WaitOff);
 assert(r.advance(125000,true,true,false,false)==R::None);assert(r.phase==R::OffDelay);
 assert(r.advance(134999,true,true,false,false)==R::None);
 assert(r.advance(135000,true,true,false,false)==R::SendOn);
 assert(r.advance(136000,true,false,true,false)==R::None);assert(r.active());
 r.advance(140000,true,false,true,true);assert(r.phase==R::Done);
 assert(!r.observe(500000,true,false));r.cancel();assert(r.used);
 R aborted;trigger(aborted);assert(aborted.advance(125000,false,false,false,false)==R::Stop);assert(aborted.phase==R::Aborted&&aborted.used);
 R timeout;trigger(timeout);assert(timeout.advance(140000,true,false,false,false)==R::Stop);
 R onTimeout;trigger(onTimeout);onTimeout.advance(125000,true,true,false,false);assert(onTimeout.advance(135000,true,true,false,false)==R::SendOn);assert(onTimeout.advance(155000,true,false,false,false)==R::SendOff);
 R manual;trigger(manual);manual.cancel();assert(!manual.active()&&manual.used);assert(manual.advance(500000,true,true,true,true)==R::None);
 R gap;assert(!gap.observe(0,true,false));assert(!gap.observe(120000,true,false));assert(!gap.used);
 R delivered;for(unsigned t=0;t<150000;t+=5000)assert(!delivered.observe(t,true,true));assert(!delivered.used);
 R blocked;for(unsigned t=0;t<150000;t+=5000)assert(!blocked.observe(t,false,false));
 std::cout<<"ESS menu restart: 120s delay, OFF ack, 10s wait, ON ack/new samples, aborts/timeouts, manual cancel and one-shot latch OK\n";
}
