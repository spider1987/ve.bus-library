#pragma once
#include "AcEnergy.h"

// DC measurement: positive current charges, negative current discharges.
// Voltage and current must come from the same fresh RAM response batch.
struct BatteryEnergy : AcEnergy {
    void sampleDC(uint32_t now, float volts, float amps) {
        if (!isfinite(volts) || volts <= 0 || !isfinite(amps)) { gap(); return; }
        sample(now, volts * amps);
    }
};

inline bool batteryEnergySelfCheck() {
    BatteryEnergy e;
    // 48 V * 50 A for one hour = 2.4 kWh, independent of AC losses.
    for (uint32_t t = 0; t <= 3600000; t += 5000) e.sampleDC(t, 48, -50);
    if (fabs(e.exportWh - 2400) > 1e-6 || e.importWh != 0 || e.measuredMs != 3600000) return false;
    e = BatteryEnergy();
    for (uint32_t t = 0; t <= 3600000; t += 5000) e.sampleDC(t, 48, 50);
    if (fabs(e.importWh - 2400) > 1e-6 || e.exportWh != 0) return false;
    e = BatteryEnergy();
    e.sampleDC(0, 48, -50); e.sampleDC(5000, 0, -50); e.sampleDC(10000, 48, -50);
    if (e.measuredMs || e.exportWh) return false;
    e.sampleDC(15000, NAN, -50); e.sampleDC(20000, 48, -50);
    if (e.measuredMs || e.exportWh) return false;
    e = BatteryEnergy();
    e.sampleDC(0, 48, -50); e.sampleDC(11000, 48, -50);
    return !e.measuredMs && e.exportWh == 0;
}
