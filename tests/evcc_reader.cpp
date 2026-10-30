#include "../examples/mqtt_ha/EvccReader.h"
#include "../examples/mqtt_ha/EvccState.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <algorithm>
struct Source {
    std::string input;size_t pos=0,chunk=1024,calls=0;
    Source(std::string text,size_t part=1024):input(text),chunk(part){}
    int available(){return (int)std::min(chunk,input.size()-pos);}
    size_t readBytes(char*out,size_t n){++calls;n=std::min(n,input.size()-pos);memcpy(out,input.data()+pos,n);pos+=n;return n;}
};
struct Snapshot {bool boostActive=false;float boostLimitSoc=0;int boostChargeW=0;const char*fetchError=nullptr;bool valid=false,solarValid=false,bydSocValid=false;float bydSoc=0;int gridW=0,otherW=0,victronW=0,demandW=0,solarResidualW=0;};
std::string base=R"({"grid":{"power":0},"homePower":500,"pvPower":800,"battery":{"devices":[{"power":0,"soc":80},{"power":-300,"soc":10}]},"loadpoints":[{"chargePower":0}]})";
Snapshot parse(const std::string &text,size_t chunk=1024){
    Source source(text,chunk);EvccReader<Source> reader(source);
    StaticJsonDocument<1024> filter;configureEvccFilter(filter);assert(!filter.overflowed());
    DynamicJsonDocument doc(4096);auto err=deserializeJson(doc,reader,DeserializationOption::Filter(filter));
    Snapshot snap;if(!err&&!reader.tooLarge&&!reader.timedOut)decodeEvcc(doc.as<JsonVariantConst>(),snap);
    if(text.size()>65536&&text.size()<EvccReader<Source>::MAX_BYTES){assert(source.calls<text.size()/10);assert(doc.memoryUsage()<2048);}
    return snap;
}
uint32_t fakeTime=0;uint32_t clockNow(){return fakeTime;}
int main(int argc,char**argv){
    for(size_t part:{size_t(17),size_t(1024)}){
        auto s=parse(base,part);assert(s.valid&&s.solarValid&&s.bydSocValid&&s.bydSoc==80);assert(s.demandW==-300&&s.solarResidualW==-300);
    }
    std::string large=base;large.insert(1,"\"forecast\":\""+std::string(300000,'x')+"\",");
    assert(parse(large).solarValid);
    std::string excessive=base;excessive.insert(1,"\"forecast\":\""+std::string(600000,'x')+"\",");
    assert(!parse(excessive).valid);assert(!parse(base.substr(0,base.size()-2)).valid);
    DynamicJsonDocument bad(2048);deserializeJson(bad,base);bad.remove("pvPower");std::string text;serializeJson(bad,text);assert(!parse(text).solarValid);
    deserializeJson(bad,base);bad["battery"]["devices"].remove(0);text.clear();serializeJson(bad,text);assert(!parse(text).valid);
    deserializeJson(bad,base);bad["loadpoints"][0].remove("chargePower");text.clear();serializeJson(bad,text);assert(!parse(text).solarValid);
    deserializeJson(bad,base);bad["battery"]["devices"][0].remove("soc");text.clear();serializeJson(bad,text);auto noSoc=parse(text);assert(noSoc.solarValid&&!noSoc.bydSocValid);
    deserializeJson(bad,base);bad["batteryMode"]="normal";bad["loadpoints"][0]["chargePower"]=3000;bad["loadpoints"][0]["batteryBoost"]=true;bad["loadpoints"][0]["batteryBoostLimit"]=60;bad["loadpoints"][0]["connected"]=true;bad["loadpoints"][0]["mode"]="pv";
    text.clear();serializeJson(bad,text);auto boost=parse(text);assert(boost.boostActive&&boost.boostChargeW==3000&&boost.boostLimitSoc==60);
    bad["loadpoints"][0]["mode"]="now";text.clear();serializeJson(bad,text);assert(!parse(text).boostActive);
    bad["loadpoints"][0]["mode"]="pv";bad["batteryMode"]="hold";text.clear();serializeJson(bad,text);assert(!parse(text).boostActive);
    Source input(base);EvccReader<Source> timed(input,clockNow);fakeTime=15000;assert(timed.read()==-1&&timed.timedOut);
    for(int i=1;i<argc;++i){std::ifstream file(argv[i]);assert(file);std::string live((std::istreambuf_iterator<char>(file)),{});auto s=parse(live);assert(s.valid&&s.solarValid&&s.bydSocValid);std::cout<<"Captured evcc response "<<live.size()<<" bytes: decoded OK, BYD "<<s.bydSoc<<"%, residual "<<s.demandW<<" W\n";}
    std::cout<<"evcc reader: fragmented TCP, compact/large input, size/time limits and missing-data gates OK\n";
}
