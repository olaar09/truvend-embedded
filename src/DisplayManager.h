#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "LedController.hpp"

class DisplayManager {

public:

    void begin(int din, int clk, int cs);

    void showVoltage(int value);
    void showPower(int value);
    void showUnits(int value);
    void showSeconds(long sec);

    void showState(bool on);

private:

    LedController<1,1> lc;

};

#endif