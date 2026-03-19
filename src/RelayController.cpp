#include "RelayController.h"

void RelayController::begin(int r1,int r2)
{
    relay1 = r1;
    relay2 = r2;

    pinMode(relay1,OUTPUT);
    pinMode(relay2,OUTPUT);
}

void RelayController::turnOn()
{
    digitalWrite(relay1,0);
    digitalWrite(relay2,1);

    delay(50);

    digitalWrite(relay1,0);
    digitalWrite(relay2,0);

    state = true;
}

void RelayController::turnOff()
{
    digitalWrite(relay1,1);
    digitalWrite(relay2,0);

    delay(50);

    digitalWrite(relay1,0);
    digitalWrite(relay2,0);

    state = false;
}

bool RelayController::isOn()
{
    return state;
}