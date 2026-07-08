#ifndef POWER_METER_H
#define POWER_METER_H

#include <Arduino.h>
#include <PZEM004Tv30.h>

// NOTE: merge with your existing PowerMeter.h if it has extra members.
// v1.1 changes:
//  - every reading is range-validated (a corrupted-but-not-NaN frame
//    like 776000 kWh is rejected before it can touch the balance)
//  - freshEnergy() hands out each validated energy reading exactly once,
//    so the accounting loop only ever deducts real, fresh deltas
//  - resetEnergy() kept for bench/maintenance use only; the normal
//    billing flow never calls it anymore.

class PowerMeter {
public:
    void begin(HardwareSerial& serial, int rx, int tx);
    void update();

    float voltage();
    float power();
    float energy();

    // Returns true once per NEW validated energy reading.
    bool freshEnergy(float &outKwh);

    bool resetEnergy();   // maintenance only - unused in normal flow

private:
    PZEM004Tv30* pzem = nullptr;
    unsigned long lastRead = 0;
    float v = 0, p = 0, e = 0;
    bool eFresh = false;
};

#endif