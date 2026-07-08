#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

// =====================================================================
//  v1.1-final STORAGE API
//
//  (balance, lastEnergyKwh, timeSeconds) are now ONE checksummed record
//  in two alternating A/B slots. This removes the separate 60-second
//  time.txt write - previously the biggest flash-wear source - and
//  makes the countdown time crash-safe too.
//
//  Old records (without time) and the old time.txt are read once and
//  migrated automatically on first save.
// =====================================================================

class StorageManager {
public:
    static void init();

    // ---- balance + lastEnergy + time (crash-safe A/B slots) ----
    // loadBalance: returns true if a valid stored record was found.
    //   If the record predates the time field, timeSeconds is filled
    //   from the legacy /time.txt automatically.
    static bool loadBalance(float &balance, float &lastEnergyKwh,
                            uint32_t &timeSeconds);
    static bool saveBalance(float balance, float lastEnergyKwh,
                            uint32_t timeSeconds);

    // ---- legacy v1.0 units.txt (one-time migration only) ----
    static bool  legacyUnitsExists();
    static float loadLegacyUnits();
    static void  removeLegacyUnits();

    // ---- legacy time.txt (read-only fallback; no longer written) ----
    static uint32_t loadTime();

    // ---- nonce ----
    static const char* getLastNonce();
    static void storeNonce(const char* nonce);
    static bool isNonceValid(const char* nonce);
};

#endif