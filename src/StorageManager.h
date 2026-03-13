#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

class StorageManager {

public:

    static void init();

    static void storeNonce(const char* nonce);
    static bool isNonceValid(const char* nonce);
    static const char* getLastNonce();

    static float loadUnits();
    static void saveUnits(float units);

    static uint32_t loadTime();
    static void saveTime(uint32_t value);

};

#endif