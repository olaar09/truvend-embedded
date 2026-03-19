#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>

class BLEManager {

public:

    void begin(const char* deviceName);

    bool connected();

    bool actionPending();

    String getValue();
    void stop();

    void send(String msg);

private:

};

#endif