#include "PowerMeter.h"

void PowerMeter::begin(HardwareSerial& serial,int rx,int tx)
{
    pzem = new PZEM004Tv30(serial,rx,tx);
}

void PowerMeter::update()
{
    if (millis() - lastRead < 4000) return;

    lastRead = millis();

    float rv = pzem->voltage();
    float rp = pzem->power();
    float re = pzem->energy();

    if (!isnan(rv)) v = rv;
    if (!isnan(rp)) p = rp;
    if (!isnan(re)) e = re;
}

float PowerMeter::voltage()
{
    return v;
}

float PowerMeter::power()
{
    return p;
}

float PowerMeter::energy()
{
    return e;
}

bool PowerMeter::resetEnergy()
{
    return pzem->resetEnergy();
}