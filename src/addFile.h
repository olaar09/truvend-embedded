#ifndef ADD_FILE_H
#define ADD_FILE_H

// =====================================================================
//  MERGE NOTE: this is a reference version of addFile.h.
//  If your existing addFile.h has other externs (e.g. `int id;` used by
//  CloudClient, or anything else), KEEP those lines and just make sure
//  every extern below is present. All of these are DEFINED in main.cpp.
// =====================================================================

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern float newBalanceTop;
extern float availableUnits;          // v1.1: LIVE remaining balance
extern float lastEnergyKwh;           // NEW: PZEM cumulative at last accounting step
extern bool  energySynced;            // NEW: first valid PZEM read handled
extern SemaphoreHandle_t balanceMutex;// NEW: guards balance/lastEnergy/timeSeconds
extern uint32_t timeSeconds;
extern bool resetMeterL;              // legacy, unused in v1.1 (kept for linking)
extern bool wifiCon;
extern float energy;                  // latest PZEM cumulative (reporting only)
extern bool serverRUnning;
extern String meterNo;

#endif