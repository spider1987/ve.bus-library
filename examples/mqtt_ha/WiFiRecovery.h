#pragma once
#include <stdint.h>

namespace WiFiRecovery {
constexpr uint32_t PORTAL_DELAY_MS = 20000;
constexpr uint32_t RETRY_MS = 30000;
constexpr bool openPortal(bool connected, bool active, uint32_t offlineMs, uint32_t sinceAttemptMs) {
    return !connected && !active && offlineMs >= PORTAL_DELAY_MS && sinceAttemptMs >= PORTAL_DELAY_MS;
}
constexpr bool retry(bool connected, bool portal, unsigned clients, uint32_t sinceRetryMs) {
    return !connected && sinceRetryMs >= RETRY_MS && (!portal || clients == 0);
}
static_assert(!openPortal(false,false,19999,20000), "short outage does not open portal");
static_assert(openPortal(false,false,20000,20000), "sustained outage opens portal");
static_assert(!openPortal(true,false,60000,60000), "connected network needs no portal");
static_assert(!openPortal(false,true,60000,60000), "never start duplicate portal");
static_assert(!openPortal(false,false,60000,1000), "failed portal startup has cooldown");
static_assert(retry(false,true,0,30000), "retry saved network when portal has no clients");
static_assert(!retry(false,true,1,30000), "do not interrupt an attached setup client");
static_assert(!retry(true,false,0,30000), "no retry on healthy network");
}
