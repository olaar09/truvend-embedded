#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>

// NOTE: if your existing BLEManager.h has anything extra, keep it and
// just add the ensureAdvertising() line - that's the only v1.2 change.

class BLEManager {
public:
    void begin(const char* name);
    void stop();
    bool connected();
    bool actionPending();
    String getValue();
    void send(String msg);

    // v1.2: advertising watchdog - call periodically from bleTask.
    void ensureAdvertising();
};

#endif