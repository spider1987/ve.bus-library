#pragma once
#include <ArduinoJson.h>
#include <math.h>

template<class Filter> void configureEvccFilter(Filter &filter) {
                    filter["grid"]["power"] = true;
                    filter["homePower"] = true; filter["pvPower"] = true;
                    filter["batteryMode"]=true;
                    filter["loadpoints"][0]["chargePower"] = true;
                    filter["loadpoints"][0]["batteryBoost"] = true;
                    filter["loadpoints"][0]["batteryBoostLimit"] = true;
                    filter["loadpoints"][0]["connected"] = true;
                    filter["loadpoints"][0]["mode"] = true;
                    filter["battery"]["devices"][0]["power"] = true;
                    filter["battery"]["devices"][0]["soc"] = true;
}

template<class Snapshot> void decodeEvcc(JsonVariantConst data,Snapshot &snap) {
    snap.fetchError="Leistungsfelder fehlen / Batteriezuordnung prüfen";
                    JsonArrayConst devices = data["battery"]["devices"].as<JsonArrayConst>();
                    // Verified installation: device 1 = GoodWe/BYD, device 2 = LilyGo.
                    if (devices.size() == 2 && data["grid"]["power"].is<float>() &&
                        devices[0]["power"].is<float>() && devices[1]["power"].is<float>()) {
                        const float grid = data["grid"]["power"], other = devices[0]["power"], vic = devices[1]["power"];
                        if (isfinite(grid) && isfinite(other) && isfinite(vic) &&
                            fabsf(grid) < 100000 && fabsf(other) < 100000 && fabsf(vic) < 100000) {
                            snap.gridW = lroundf(grid); snap.otherW = lroundf(other); snap.victronW = lroundf(vic);
                            // Grid + all batteries = consumption minus PV (includes EV loads).
                            snap.demandW = snap.gridW + snap.otherW + snap.victronW;
                            snap.valid = true;snap.fetchError="PV-/Verbrauchsfelder fehlen oder ungültig";
                            if(devices[0]["soc"].is<float>()) {
                                const float soc=devices[0]["soc"];
                                snap.bydSocValid=isfinite(soc)&&soc>=0&&soc<=100;
                                if(snap.bydSocValid)snap.bydSoc=soc;
                            }
                            // Explicit solar gate: include EV consumption, never substitute missing values.
                            if(data["homePower"].is<float>() && data["pvPower"].is<float>() && data["loadpoints"].is<JsonArrayConst>()) {
                                float home=data["homePower"], pv=data["pvPower"], ev=0;
                                bool ok=isfinite(home) && isfinite(pv) && home>=0 && pv>=0 && home<100000 && pv<100000;
                                const char *batteryMode=data["batteryMode"] | "";
                                const bool boostAllowed=!strcmp(batteryMode,"normal")||!strcmp(batteryMode,"discharge");
                                for(JsonObjectConst lp: data["loadpoints"].as<JsonArrayConst>()) {
                                    if(!lp["chargePower"].is<float>()){ok=false;break;}
                                    float w=lp["chargePower"]; if(!isfinite(w) || w<0 || w>=100000){ok=false;break;} ev+=w;
                                    const char *mode=lp["mode"] | "";
                                    if(boostAllowed&&lp["batteryBoost"].is<bool>()&&lp["batteryBoost"].as<bool>()&&
                                       lp["connected"].is<bool>()&&lp["connected"].as<bool>()&&w>100&&
                                       (!strcmp(mode,"pv")||!strcmp(mode,"smart"))) {
                                        snap.boostActive=true;snap.boostChargeW+=lroundf(w);
                                        if(lp["batteryBoostLimit"].is<float>()){
                                            float limit=lp["batteryBoostLimit"];if(isfinite(limit)&&limit>=0&&limit<=100&&limit>snap.boostLimitSoc)snap.boostLimitSoc=limit;
                                        }
                                    }
                                }
                                snap.solarValid=ok && ev<100000;
                                if(snap.solarValid){snap.solarResidualW=lroundf(home+ev-pv);snap.fetchError="OK";}
                            }
                        }
                    }
}
