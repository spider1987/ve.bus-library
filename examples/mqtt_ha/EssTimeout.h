#pragma once
#include <stdint.h>
namespace EssTimeout {
inline bool expired(uint32_t now,uint32_t last,uint32_t timeoutS){
    const uint32_t age=now-last;
    return timeoutS&&age<=0x7fffffffUL&&age>=timeoutS*1000UL;
}
}
