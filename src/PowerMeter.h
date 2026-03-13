#ifndef POWER_METER_H
#define POWER_METER_H

#include <Arduino.h>
#include <PZEM004Tv30.h>

class PowerMeter {

public:

    void begin(HardwareSerial& serial, int rx, int tx);

    void update();

    float voltage();
    float power();
    float energy();

    bool resetEnergy();

private:

    PZEM004Tv30* pzem;

    float v;
    float p;
    float e;

    unsigned long lastRead = 0;

};

#endif