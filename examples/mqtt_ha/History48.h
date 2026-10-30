#pragma once
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <math.h>
namespace History48 {
static const uint32_t CAPACITY=1440, MAGIC=0x56424831, VERSION=1;
static const int16_t GAP=INT16_MIN;
static const uint32_t MIN_MINUTE=1704067200UL/60, MAX_MINUTE=4102444799UL/60;
struct Sample {int16_t value[6];};
struct Point {uint32_t minute;Sample sample;};
struct DiskRecord {Point point;uint32_t checksum;};
struct Header {uint32_t magic,version,slots,recordBytes,checksum;};
static_assert(sizeof(Sample)==12&&sizeof(Point)==16&&sizeof(DiskRecord)==20&&sizeof(Header)==20,"stable history format");
inline uint32_t crc(const void *data,size_t length){
    uint32_t c=0xffffffffUL;const auto *p=(const uint8_t*)data;
    while(length--){c^=*p++;for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?0xedb88320UL:0);}
    return ~c;
}
inline Header header(){Header h={MAGIC,VERSION,CAPACITY,sizeof(DiskRecord),0};h.checksum=crc(&h,16);return h;}
inline bool valid(const Header &h){return h.magic==MAGIC&&h.version==VERSION&&h.slots==CAPACITY&&h.recordBytes==sizeof(DiskRecord)&&h.checksum==crc(&h,16);}
inline bool validField(unsigned i,float v){return isfinite(v)&&v>=-32767&&v<=32767&&(i!=3||(v>0&&v<=8000))&&(i!=4||(v>=0&&v<=1000));}
inline bool valid(const Sample &s){for(unsigned i=0;i<6;++i)if(s.value[i]!=GAP&&!validField(i,s.value[i]))return false;return true;}
inline bool empty(const Sample &s){for(auto v:s.value)if(v!=GAP)return false;return true;}
inline bool inWindow(uint32_t minute,uint32_t now){return minute>=MIN_MINUTE&&minute<=MAX_MINUTE&&minute<=now&&now-minute<CAPACITY;}
inline DiskRecord record(const Point &p){DiskRecord r={p,0};r.checksum=crc(&r.point,sizeof(Point));return r;}
inline bool valid(const DiskRecord &r){return r.point.minute>=MIN_MINUTE&&r.point.minute<=MAX_MINUTE&&valid(r.point.sample)&&r.checksum==crc(&r.point,sizeof(Point));}
inline uint32_t offset(uint32_t minute){return sizeof(Header)+(minute%CAPACITY)*sizeof(DiskRecord);}
inline uint32_t fileBytes(){return sizeof(Header)+CAPACITY*sizeof(DiskRecord);}
// Starting length is explicit: filesystem stat may lag buffered writes.
template<class File> bool padFile(File &f,uint32_t length){
    if(length<sizeof(Header)||length>fileBytes()||!f.seek(length))return false;
    uint8_t zeros[256]={};
    while(length<fileBytes()){
        const uint32_t n=fileBytes()-length<sizeof(zeros)?fileBytes()-length:sizeof(zeros);
        if(f.write(zeros,n)!=n)return false;length+=n;
    }
    return true;
}
struct Accumulator {
    int64_t sum[6]={};uint16_t count[6]={};
    void add(unsigned i,float value){if(i<6&&validField(i,value)){sum[i]+=(int32_t)lroundf(value);++count[i];}}
    Sample finish(){Sample s;for(unsigned i=0;i<6;++i){s.value[i]=count[i]?(int16_t)(sum[i]/count[i]):GAP;sum[i]=0;count[i]=0;}return s;}
};
struct Window {
    Point points[CAPACITY]={};uint32_t latest=0;
    void clear(){for(auto &p:points)p.minute=0;latest=0;}
    bool put(const Point &p){
        if(!p.minute||!valid(p.sample)||(latest&&p.minute+CAPACITY<=latest))return false;
        auto &slot=points[p.minute%CAPACITY];if(slot.minute>p.minute)return false;
        slot=p;if(p.minute>latest)latest=p.minute;return true;
    }
    const Point *get(uint32_t minute) const {if(!minute)return nullptr;const auto &p=points[minute%CAPACITY];return p.minute==minute?&p:nullptr;}
    uint32_t first() const {uint32_t first=latest;for(const auto &p:points)if(p.minute&&p.minute<=latest&&latest-p.minute<CAPACITY&&p.minute<first)first=p.minute;return first;}
    uint32_t count() const {uint32_t n=0;for(const auto &p:points)if(p.minute&&p.minute<=latest&&latest-p.minute<CAPACITY)++n;return n;}
};
}
