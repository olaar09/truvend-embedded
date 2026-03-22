#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>

class RelayController {

public:

    void begin(int r1, int r2);

    void turnOn();
    void turnOff();

    bool isOn();

private:

    int relay1;
    int relay2;

    bool state = false;

};

#endif