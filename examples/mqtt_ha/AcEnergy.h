#pragma once
#include <stdint.h>
#include <math.h>

// Integrate only adjacent, fresh AC-IN samples. Never bridge a missing interval.
struct AcEnergy {
    double importWh = 0, exportWh = 0;
    uint32_t measuredMs = 0, lastMs = 0;
    float lastW = 0;
    bool haveLast = false;
    void gap() { haveLast = false; }
    void sample(uint32_t now, float watts) {
        if (!isfinite(watts)) { gap(); return; }
        const uint32_t dt = now - lastMs;
        if (haveLast && dt && dt <= 10000) {
            const double a = lastW, b = watts, hours = dt / 3600000.0;
            if (a >= 0 && b >= 0) importWh += (a + b) * 0.5 * hours;
            else if (a <= 0 && b <= 0) exportWh -= (a + b) * 0.5 * hours;
            else {
                const double fraction = fabs(a) / (fabs(a) + fabs(b));
                const double first = fabs(a) * fraction * 0.5 * hours;
                const double second = fabs(b) * (1.0 - fraction) * 0.5 * hours;
                if (a > 0) { importWh += first; exportWh += second; }
                else { exportWh += first; importWh += second; }
            }
            measuredMs += dt;
        }
        lastMs = now; lastW = watts; haveLast = true;
    }
};

// Strict unsigned watt input, mapped to the direct ESS command sign.
inline bool acPowerValue(const char *text, bool charge, int16_t &signedW) {
    if (!text || !*text) return false;
    unsigned watts = 0;
    for (const char *c = text; *c; ++c) {
        if (*c < '0' || *c > '9') return false;
        watts = watts * 10 + (*c - '0');
        if (watts > 32767) return false;
    }
    signedW = charge ? -(int16_t)watts : (int16_t)watts;
    return true;
}

// Uses independent synthetic inputs, never touches the inverter.
inline bool acEnergySelfCheck() {
    AcEnergy e;
    e.sample(0, 3600); e.sample(1000, 3600);
    if (fabs(e.importWh - 1) > 1e-8 || e.exportWh != 0) return false;
    e = AcEnergy(); e.sample(0, -3600); e.sample(1000, -3600);
    if (fabs(e.exportWh - 1) > 1e-8 || e.importWh != 0) return false;
    e = AcEnergy(); e.sample(0, 3600); e.sample(1000, -3600);
    if (fabs(e.importWh - .25) > 1e-8 || fabs(e.exportWh - .25) > 1e-8) return false;
    e = AcEnergy(); e.sample(0, 3600); e.sample(11000, 3600);
    if (e.measuredMs || e.importWh) return false;
    e.gap(); e.sample(12000, 3600);
    if (e.measuredMs || e.importWh) return false;
    e = AcEnergy(); e.sample(UINT32_MAX - 499, 3600); e.sample(500, 3600);
    if (e.measuredMs != 1000 || fabs(e.importWh - 1) > 1e-8) return false;
    e.sample(1000, NAN); e.sample(1500, 3600);
    if (e.measuredMs != 1000) return false;
    int16_t w = 0;
    if (!acPowerValue("300", true, w) || w != -300) return false;
    if (!acPowerValue("300", false, w) || w != 300) return false;
    if (!acPowerValue("0", true, w) || w != 0) return false;
    if (!acPowerValue("4000", true, w) || w != -4000) return false;
    if (!acPowerValue("32767", true, w) || w != -32767) return false;
    if (!acPowerValue("32767", false, w) || w != 32767) return false;
    const char *invalid[] = {"", "-1", "1.5", "32768", "999999999999", "abc"};
    for (const char *bad : invalid)
        if (acPowerValue(bad, true, w)) return false;
    return true;
}
