#include "PowerMeter.h"

void PowerMeter::begin(HardwareSerial& serial,int rx,int tx)
{
    pzem = new PZEM004Tv30(serial,rx,tx);
}

void PowerMeter::update()
{
    if(millis() - lastRead < 4000) return;

    lastRead = millis();

    v = pzem->voltage();
    p = pzem->power();
    e = pzem->energy();
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