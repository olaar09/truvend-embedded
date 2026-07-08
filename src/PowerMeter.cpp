#include "PowerMeter.h"

// Physical plausibility limits. Anything outside these is a corrupted
// frame or sensor fault and must never reach the billing math.
static const float V_MIN = 0.0f,  V_MAX = 300.0f;     // mains voltage
static const float P_MIN = 0.0f,  P_MAX = 25000.0f;   // PZEM-100A max ~23 kW
static const float E_MIN = 0.0f,  E_MAX = 10000.0f;   // PZEM caps ~9999.99 kWh

void PowerMeter::begin(HardwareSerial& serial, int rx, int tx)
{
    pzem = new PZEM004Tv30(serial, rx, tx);
}

void PowerMeter::update()
{
    if (millis() - lastRead < 4000) return;
    lastRead = millis();

    float rv = pzem->voltage();
    float rp = pzem->power();
    float re = pzem->energy();

    if (!isnan(rv) && rv >= V_MIN && rv <= V_MAX) v = rv;
    if (!isnan(rp) && rp >= P_MIN && rp <= P_MAX) p = rp;

    // Energy is the money value: NaN check AND hard range check.
    // A corrupted frame decoding to e.g. 776000 kWh dies right here.
    if (!isnan(re) && re >= E_MIN && re < E_MAX) {
        e = re;
        eFresh = true;
    }
}

float PowerMeter::voltage() { return v; }
float PowerMeter::power()   { return p; }
float PowerMeter::energy()  { return e; }

bool PowerMeter::freshEnergy(float &outKwh)
{
    if (!eFresh) return false;
    eFresh = false;
    outKwh = e;
    return true;
}

bool PowerMeter::resetEnergy()
{
    // Maintenance only. Normal billing flow uses delta accounting and
    // never resets the PZEM. If you ever do call this, the accounting
    // loop treats the backwards jump as a resync (no deduction).
    return pzem->resetEnergy();
}