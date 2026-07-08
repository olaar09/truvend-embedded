#include "setupFiles.h"
#include "PowerMeter.h"
#include "RelayController.h"
#include "DisplayManager.h"
#include "StorageManager.h"
#include "BLEManager.h"
#include "MeterLogic.h"
#include <CloudClient.h>
#include <addFile.h>

// =====================================================================
//  v1.1-final ACCOUNTING MODEL
//
//  availableUnits = LIVE remaining balance ("what you have left").
//    - recharge:  availableUnits += amount   (never resets, never
//                 recomputed from the PZEM - a pure running tally)
//    - every 4s:  availableUnits -= validated delta since last poll
//
//  FLASH WEAR (10-year design):
//    - (balance, lastEnergyKwh, timeSeconds) = ONE record, saved every
//      FS_SAVE_INTERVAL (5 min) and only if something changed. The old
//      separate 60s time.txt write is GONE - time lives in the record.
//      An idle meter (no load, countdown expired) writes nothing.
//    - Longer save gaps cost almost no billing accuracy: the PZEM's own
//      counter survives power cuts, so the boot delta re-bills whatever
//      was consumed after the last save. Only countdown minutes can be
//      lost, always in the customer's favor.
// =====================================================================

// ================= Setup files =================
const char* ssid = "aDevXSY8TZkZcdk";
const char* password = "u3tgYkyn2JX8gUx";
String meterNo = "87800000443";
String jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2VfaW52ZW50b3J5X3JlZiI6Ijg3ODAwMDAwNDQzIiwic2NvcGUiOiJpb3RfZGV2aWNlIiwiaWF0IjoxNzgzNTA4MDI0LCJleHAiOjIwOTkwODQwMjR9.Na2OlRegZO5sH6u1VM6xfx33b51uaEmSGUn9XhNSyik";

CloudClient cloud(ssid, password, jwtToken);
const unsigned long RESTART_INTERVAL = 21600000UL;   // 6 hours

// Flash save cycle. 5 min keeps worst-case sector wear comfortably
// inside a 10-year life (see wear math in StorageManager.cpp header).
const unsigned long FS_SAVE_INTERVAL = 300000UL;     // 5 minutes

// ================= Objects =================
PowerMeter powerMeter;
RelayController relay;
DisplayManager display;
BLEManager ble;
MeterLogic meterLogic;

// ================= Shared globals (extern'd in addFile.h) =================
float newBalanceTop = 0;
float availableUnits = 0;        // LIVE remaining balance
float lastEnergyKwh = 0;         // PZEM cumulative at last accounting step
bool  energySynced = false;      // first valid PZEM read handled?
SemaphoreHandle_t balanceMutex = nullptr;
uint32_t timeSeconds = 0;
bool resetMeterL = false;        // kept only so old externs still link
bool wifiCon = false;
float energy = 0;                // latest PZEM cumulative (server reporting)
bool serverRUnning = false;

// ================= Accounting config =================
// Max believable consumption between two 4s polls:
// 25 kW * 4 s = 0.028 kWh. 0.05 gives comfortable headroom.
static const float POLL_MAX_DELTA_KWH = 0.05f;
// Max believable consumption while the ESP was rebooting/offline
// (latching relay can keep the load running with the ESP dark).
static const float BOOT_MAX_DELTA_KWH = 500.0f;

static bool haveStoredPair = false;   // valid record loaded from flash
static bool needMigration  = false;   // old v1.0 units.txt found
static uint8_t badDeltaStrikes = 0;
static float lastSavedBal = -1.0f, lastSavedE = -1.0f;
static uint32_t lastSavedTime = 0xFFFFFFFF;

// =====================================================
void handleBleCommands()
{
    if (!ble.actionPending()) return;
    serverRUnning = true;

    String value = ble.getValue();
    Serial.print("BLE value: ");
    Serial.println(value);

    float newBalance = meterLogic.handleTopup(value);
    ble.send(String(newBalance));
    // availableUnits is updated inside handleTopup - no reload needed.

    serverRUnning = false;
}

// =====================================================
void sendUpdate()
{
    String url = "http://iot.truvend.online/iot/set_status/";
    url += meterNo;
    url += "?action=set_status";

    url += "&balance=" + String(availableUnits);
    url += "&relay=" + relayState;
    url += "&power=" + String(power);
    url += "&energy=" + String(energy);
    url += "&seconds=" + String(timeSeconds);
    url += "&meter_number=" + meterNo;
    url += "&voltage=" + String(voltage, 2);

    cloud.sendRequest(url);
}

// ====================================================
unsigned long lastServerCheck = 0;
const unsigned long serverInterval = 10000;

// One unified snapshot: balance + energy anchor + countdown time,
// written as a single crash-safe record. Skips the write entirely if
// nothing changed - idle meters cost zero flash wear.
void saveBalanceSnapshot(bool force)
{
    float b, le;
    uint32_t t;
    bool synced;

    xSemaphoreTake(balanceMutex, portMAX_DELAY);
    b = availableUnits; le = lastEnergyKwh; synced = energySynced;
    t = timeSeconds;
    xSemaphoreGive(balanceMutex);

    if (!synced) return;   // no valid energy anchor yet - don't persist one

    bool changed = fabsf(b - lastSavedBal) > 0.001f ||
                   fabsf(le - lastSavedE) > 0.0005f ||
                   t != lastSavedTime;
    if (!force && !changed) return;   // don't wear flash for nothing

    if (StorageManager::saveBalance(b, le, t)) {
        lastSavedBal = b;
        lastSavedE = le;
        lastSavedTime = t;
    }
}

void checkServerData()
{
    if (millis() - lastServerCheck >= serverInterval)
    {
        lastServerCheck = millis();

        if (!serverRUnning)
        {
            if (millis() >= RESTART_INTERVAL) {
                Serial.println("Restarting Meter...");
                saveBalanceSnapshot(true);   // persist before restart
                ESP.restart();
            }
            String getDataUrl = "http://iot.truvend.online/iot/get_command/" + String(meterNo);
            cloud.sendRequest(getDataUrl);
            sendUpdate();
        }
    }
}

// =====================================================
//  THE ACCOUNTING LOOP
// =====================================================
void updatePowerReadings()
{
    powerMeter.update();
    voltage = powerMeter.voltage();
    power = powerMeter.power();

    float e;
    if (!powerMeter.freshEnergy(e)) return;   // only fresh, VALIDATED reads
    energy = e;                               // for server reporting

    if (xSemaphoreTake(balanceMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;

    if (!energySynced)
    {
        // -------- first valid PZEM reading since boot --------
        if (needMigration)
        {
            // One-time migration from v1.0: old semantics were
            // available = storedUnits - cumulativeEnergy, so the live
            // balance right now is exactly that. Negative (the corrupted
            // meters) clamps to 0 - reissue those users' credit manually.
            float legacy = StorageManager::loadLegacyUnits();
            float bal = legacy - e;
            if (isnan(bal) || bal < 0.0f) bal = 0.0f;

            availableUnits = bal;
            lastEnergyKwh = e;

            if (StorageManager::saveBalance(availableUnits, lastEnergyKwh,
                                            timeSeconds)) {
                StorageManager::removeLegacyUnits();
                needMigration = false;
                Serial.print("Migrated v1.0 balance: ");
                Serial.println(availableUnits);
            }
            // if save failed, we retry on the next fresh reading
        }
        else if (haveStoredPair)
        {
            // Normal boot: charge for whatever ran while the ESP was dark
            // (PZEM kept counting in its own memory). This is also why a
            // 5-min save interval loses no billing: this delta recovers
            // everything consumed after the last snapshot.
            float delta = e - lastEnergyKwh;
            if (delta > 0.0f && delta <= BOOT_MAX_DELTA_KWH) {
                availableUnits -= delta;
            }
            // delta < 0 -> PZEM was replaced/reset/rolled over: resync only
            // delta > BOOT_MAX -> impossible: resync, no deduction
            if (availableUnits < 0.0f) availableUnits = 0.0f;
            lastEnergyKwh = e;
        }
        else
        {
            // Brand new device, nothing stored: sync to the meter's
            // current counter, deduct nothing.
            lastEnergyKwh = e;
        }
        energySynced = true;
    }
    else
    {
        // -------- steady state: deduct only validated deltas --------
        float delta = e - lastEnergyKwh;

        if (delta < 0.0f) {
            // Energy went backwards: PZEM reset/rollover. Resync only -
            // the balance is untouched; billing resumes next poll.
            lastEnergyKwh = e;
            badDeltaStrikes = 0;
        }
        else if (delta <= POLL_MAX_DELTA_KWH) {
            availableUnits -= delta;
            if (availableUnits < 0.0f) availableUnits = 0.0f;
            lastEnergyKwh = e;
            badDeltaStrikes = 0;
        }
        else {
            // Impossible jump (>45 kW average over 4s). One glitched frame
            // gets ignored; if the SAME high value persists for 5 straight
            // reads the register really moved, so resync WITHOUT deducting
            // (never bill a user for a glitch).
            if (++badDeltaStrikes >= 5) {
                Serial.print("Energy jump resync, delta=");
                Serial.println(delta);
                lastEnergyKwh = e;
                badDeltaStrikes = 0;
            }
        }
    }

    xSemaphoreGive(balanceMutex);
}

// =====================================================
void updateRelayState()
{
    float remaining = availableUnits;   // live balance

    if (remaining >= 0.01 && !relay.isOn() && timeSeconds > 0)
    {
        relay.turnOn();
        Serial.println("Relay ON");
        relayState = "on";
    }

    if ((remaining < 0.01 || timeSeconds <= 0) && relay.isOn())
    {
        relay.turnOff();
        Serial.println("Relay OFF");
        relayState = "off";
    }

    // If relay should be OFF but there is power flowing
    if ((remaining <= 0.01 || timeSeconds <= 0) && power > 10) {
        relay.turnOff();  // send OFF pulse
        relayState = "off";
        Serial.println("Relay OFF correction pulse due to load > 10W");
    }
}

// =====================================================
void updateDisplay()
{
    if (millis() - lastDisplayUpdate < DISPLAY_INTERVAL)
        return;

    lastDisplayUpdate = millis();

    displayState = (displayState + 1) % 6;

    switch (displayState)
    {
        case 0:
            display.showVoltage((int)voltage);
            break;

        case 1:
            display.showPower((int)power);
            break;

        case 2:
        {
            int units = (int)max(0.0f, availableUnits);
            display.showUnits(units);
            break;
        }

        case 3:
            display.showState(relay.isOn());
            break;

        case 4:
        {
            int countdown = timeSeconds / 3600;
            display.showCountdown(countdown);
            break;
        }
        case 5:
        {
            display.wifi(wifiCon);
            break;
        }
    }
}

// =====================================================
void countdownTimer()
{
    unsigned long now = millis();

    if (now - lastSecondTick >= 1000)
    {
        lastSecondTick = now;

        if (timeSeconds > 0) {
            timeSeconds--;
        }
    }

    // ONE flash write per cycle covering balance + anchor + time,
    // and only if something changed. (Was: time.txt every 60s PLUS
    // the balance record - the main wear source, now eliminated.)
    if (now - lastFsWrite >= FS_SAVE_INTERVAL)
    {
        lastFsWrite = now;
        saveBalanceSnapshot(false);
    }
}

////////////////////////////////////////////////////////
///////////////////// TASKS ////////////////////////////
////////////////////////////////////////////////////////

void bleTask(void *pvParameters)
{
    for(;;)
    {
        handleBleCommands();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void meterTask(void *pvParameters)
{
    for(;;)
    {
        updatePowerReadings();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void relayTask(void *pvParameters)
{
    for(;;)
    {
        updateRelayState();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void displayTask(void *pvParameters)
{
    for(;;)
    {
        updateDisplay();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void timerTask(void *pvParameters)
{
    for(;;)
    {
        countdownTimer();
        vTaskDelay(1100 / portTICK_PERIOD_MS);
    }
}

void cloudTask(void *pvParameters)
{
    cloud.begin();
    for(;;)
    {
        checkServerData();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// =====================================================
void setup()
{
    Serial.begin(115200);
    Serial.println("Booting system v1.1-final...");

    // Mutex MUST exist before any task can touch the balance.
    balanceMutex = xSemaphoreCreateMutex();

    Serial.println("Initializing storage");
    StorageManager::init();
    Serial.println("Done with storage");

    // ---- load state (balance + energy anchor + time, one record) ----
    float b = 0, le = 0;
    uint32_t t = 0;
    if (StorageManager::loadBalance(b, le, t)) {
        availableUnits = b;
        lastEnergyKwh = le;
        timeSeconds = t;
        haveStoredPair = true;
        Serial.print("Loaded balance: "); Serial.println(availableUnits);
        Serial.print("Loaded lastEnergy: "); Serial.println(lastEnergyKwh, 3);
        Serial.print("Loaded time: "); Serial.println(timeSeconds);
    }
    else if (StorageManager::legacyUnitsExists()) {
        needMigration = true;   // completed after first valid PZEM read
        timeSeconds = StorageManager::loadTime();
        Serial.println("v1.0 units.txt found - will migrate");
    }
    else {
        timeSeconds = StorageManager::loadTime();   // legacy fallback
        Serial.println("Fresh device - no stored balance");
    }

    Serial.print("Time: ");
    Serial.println(timeSeconds);

    powerMeter.begin(Serial2, PZEM_RX, PZEM_TX);
    relay.begin(RELAY1, RELAY2);
    display.begin(DIN, CLK, CS);
    ble.begin(meterNo.c_str());

    xTaskCreate(bleTask,     "BLE Task",     8096,  NULL, 1, NULL);
    xTaskCreate(meterTask,   "Meter Task",   6096,  NULL, 0, NULL);
    xTaskCreate(relayTask,   "Relay Task",   2048,  NULL, 0, NULL);
    xTaskCreate(displayTask, "Display Task", 4096,  NULL, 1, NULL);
    xTaskCreate(timerTask,   "Timer Taskk",  5048,  NULL, 0, NULL);
    xTaskCreate(cloudTask,   "Cloud Task",   10192, NULL, 1, NULL);

    Serial.println("System ready");
}

// =====================================================
void loop()
{
}