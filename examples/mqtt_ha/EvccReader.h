#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ArduinoJson custom reader: buffer TCP reads without storing the full forecast.
// The same reader is exercised by the host regression test.
template<class Source> class EvccReader {
public:
    static const uint32_t MAX_BYTES=524288;
    EvccReader(Source &source,uint32_t (*clock)()=nullptr):source_(source),clock_(clock),started_(clock?clock():0){}
    int read() {
        if(clock_ && (uint32_t)(clock_()-started_)>=15000){timedOut=true;return -1;}
        if(pos_==size_ && !refill())return -1;
        return (unsigned char)buffer_[pos_++];
    }
    size_t readBytes(char *out,size_t n) {
        size_t count=0;for(;count<n;++count){int c=read();if(c<0)break;out[count]=(char)c;}return count;
    }
    uint32_t bytes=0;
    bool tooLarge=false,timedOut=false;
private:
    bool refill() {
        if(bytes>=MAX_BYTES){tooLarge=true;return false;}
        int available=source_.available();
        size_t wanted=available>0?(size_t)available:1;
        if(wanted>sizeof(buffer_))wanted=sizeof(buffer_);
        if(wanted>MAX_BYTES-bytes)wanted=MAX_BYTES-bytes;
        size_=source_.readBytes(buffer_,wanted);pos_=0;bytes+=size_;
        return size_>0;
    }
    Source &source_;uint32_t (*clock_)();uint32_t started_;
    char buffer_[1024];size_t pos_=0,size_=0;
};
