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
//  v1.2 ACCOUNTING MODEL (unchanged from v1.1-final)
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
//
//  NEW IN v1.2:
//    - Zero-voltage protection: relay forced OFF if sensed voltage
//      stays below 5V for 30s while the relay is on (offReason = 3).
//      A successful recharge clears the fault (remote un-stick lever).
//    - Off-reason codes: 0=ON, 1=no balance, 2=time expired,
//      3=voltage fault.  Priority: 3 > 1 > 2.
//    - BLE rich status response (colon-delimited, see buildStatusString)
//      replacing the old bare-balance reply. App update required.
//    - BLE advertising watchdog (re-asserts advertising every ~5s if
//      it silently died).
// =====================================================================
//started from 457
// ================= Setup files =================
const char* ssid = "aDevXSY8TZkZcdk";
const char* password = "u3tgYkyn2JX8gUx";
String meterNo = "87800000517";
String jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2VfaW52ZW50b3J5X3JlZiI6Ijg3ODAwMDAwNTE3Iiwic2NvcGUiOiJpb3RfZGV2aWNlIiwiaWF0IjoxNzg0ODExNzQyLCJleHAiOjIxMDAzODc3NDJ9.O4sVXRsMr8C3L65-1VqDTdMjhjyFCHOH80fDT1BnjBw";

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

// ================= Display self-healing =================
// The MAX7219 has NO reset pin: its registers power up random and rely
// entirely on init writes. From a cold (fully drained) start the display
// rail can still be rising when setup() runs, so those writes land
// garbled - symptoms seen in the field: screen stays dark (stuck in
// shutdown) or only one digit lights (scan-limit corrupted). Re-sending
// the full init cures every variant, so we do it once shortly after
// boot and then periodically as a watchdog.
static uint8_t reinitStage = 0;                 // 0: pending 5s, 1: pending 30s, 2: periodic only
static unsigned long lastDisplayReinit = 0;
const unsigned long DISPLAY_REINIT_INTERVAL = 300000UL;   // 5 min watchdog

// ================= Zero-voltage protection (NEW v1.2) =================
// If the PZEM reports ~0V continuously for 30s WHILE THE RELAY IS ON,
// something is wrong (sensing fault / supply fault) and we fail safe:
// relay OFF, offReason = 3.
//
// !!! WIRING ASSUMPTION - VERIFY ON YOUR BOARD !!!
// This assumes the PZEM voltage tap is UPSTREAM of the relay (senses
// mains even when the relay is off). If your meters show VOLT-000 on
// the display whenever the relay is off, the tap is DOWNSTREAM and
// this feature needs different logic - tell me before flashing.
// Recovery: voltage returning above 100V clears the fault, and a
// successful recharge also clears it (remote un-stick lever).
static const float V_FAULT_THRESHOLD   = 5.0f;    // "000" territory
static const float V_RECOVER_THRESHOLD = 100.0f;  // clearly-live mains
static const unsigned long V_FAULT_TIME_MS = 30000UL;
static bool voltageFault = false;
static unsigned long vZeroSince = 0;

// Why the supply is off right now (also sent to app + server):
// 0 = supply ON / all good
// 1 = OFF: balance exhausted
// 2 = OFF: countdown time expired
// 3 = OFF: voltage fault (no voltage sensed for 30s)
static uint8_t offReason = 0;

// =====================================================
//  BLE STATUS PROTOCOL (NEW v1.2)
//  Success reply:  OK:<balance>:<relay>:<power>:<energy>:<seconds>:<voltage>:<reason>
//     example:     OK:150.25:on:1200:34.567:360000:229.8:0
//  Error reply:    ERR:<code>     (-99 invalid token, -98 busy/retry)
//  The app can also write the literal text  STATUS  to receive the
//  same string without performing a recharge.
//  NOTE: requires the app to negotiate MTU >= 64 (see BLEManager).
// =====================================================
String buildStatusString(float bal)
{
    String s = "OK:";
    s += String(bal, 2);          s += ":";
    s += relayState;              s += ":";
    s += String((int)power);      s += ":";
    s += String(energy, 3);       s += ":";
    s += String(timeSeconds);     s += ":";
    s += String(voltage, 1);      s += ":";
    s += String(offReason);
    return s;
}

// =====================================================
void handleBleCommands()
{
    if (!ble.actionPending()) return;
    serverRUnning = true;

    String value = ble.getValue();
    Serial.print("BLE value: ");
    Serial.println(value);

    // Plain status query - no recharge performed.
    if (value == "STATUS") {
        ble.send(buildStatusString(availableUnits));
        serverRUnning = false;
        return;
    }

    float newBalance = meterLogic.handleTopup(value);

    if (newBalance >= 0.0f) {
        // Successful recharge clears a latched voltage fault so support
        // can un-stick a meter remotely with a normal token.
        voltageFault = false;
        vZeroSince = 0;

        // Let relayTask run one cycle so relayState/offReason in the
        // reply reflect the topup we just applied.
        vTaskDelay(pdMS_TO_TICKS(250));

        ble.send(buildStatusString(newBalance));
    } else {
        ble.send("ERR:" + String((int)newBalance));
    }

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
    url += "&reason=" + String(offReason);   // NEW v1.2 - delete this one
                                             // line if the server rejects
                                             // unknown parameters

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

            // Sentinel so we can detect a successful CLOUD topup below
            // (handleTopup returns >= 0 only on success).
            newBalanceTop = -1000.0f;

            String getDataUrl = "http://iot.truvend.online/iot/get_command/" + String(meterNo);
            cloud.sendRequest(getDataUrl);

            if (newBalanceTop >= 0.0f) {
                // Cloud recharge succeeded - same remote un-stick lever
                // as the BLE path.
                voltageFault = false;
                vZeroSince = 0;
            }

            sendUpdate();
        }
    }
}

// =====================================================
//  THE ACCOUNTING LOOP (unchanged from v1.1-final)
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
//  Zero-voltage fault tracking (NEW v1.2)
// =====================================================
void updateVoltageFault()
{
    // Clearly-live mains: reset counter, clear any latched fault.
    if (voltage >= V_RECOVER_THRESHOLD) {
        vZeroSince = 0;
        if (voltageFault) {
            voltageFault = false;
            Serial.println("Voltage fault CLEARED (mains back)");
        }
        return;
    }

    // Near-zero voltage while the relay is supposed to be delivering
    // power: start/continue the 30s countdown to fail-safe cutoff.
    // (Only counted while relay is ON - if it's already off, there is
    // nothing to protect and, with upstream sensing, mains loss just
    // shows as NET/VOLT readings without tripping anything.)
    if (voltage < V_FAULT_THRESHOLD) {
        if (relay.isOn()) {
            if (vZeroSince == 0) {
                vZeroSince = millis();
            }
            else if (!voltageFault &&
                     millis() - vZeroSince >= V_FAULT_TIME_MS) {
                voltageFault = true;
                Serial.println("VOLTAGE FAULT: 0V for 30s - relay OFF");
            }
        }
    }
    else {
        // Between 5V and 100V: indeterminate (sag/brownout) - don't
        // accumulate toward a fault, don't clear one either.
        vZeroSince = 0;
    }
}

// =====================================================
void updateRelayState()
{
    updateVoltageFault();

    float remaining = availableUnits;   // live balance

    if (remaining >= 0.01 && !relay.isOn() && timeSeconds > 0 &&
        !voltageFault)
    {
        relay.turnOn();
        Serial.println("Relay ON");
        relayState = "on";
    }

    if ((remaining < 0.01 || timeSeconds <= 0 || voltageFault) && relay.isOn())
    {
        relay.turnOff();
        Serial.println("Relay OFF");
        relayState = "off";
    }

    // If relay should be OFF but there is power flowing
    if ((remaining <= 0.01 || timeSeconds <= 0 || voltageFault) && power > 10) {
        relay.turnOff();  // send OFF pulse
        relayState = "off";
        Serial.println("Relay OFF correction pulse due to load > 10W");
    }

    // Publish WHY the supply is off (0 = it's on / all good).
    // Priority: voltage fault > balance > time.
    if (voltageFault)              offReason = 3;
    else if (remaining < 0.01)     offReason = 1;
    else if (timeSeconds == 0)     offReason = 2;
    else                           offReason = 0;
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
    uint32_t bleWd = 0;
    for(;;)
    {
        handleBleCommands();

        // BLE advertising watchdog (NEW v1.2): every ~5s (250 x 20ms),
        // re-assert advertising if it silently died. NimBLE restarts
        // advertising on clean disconnects, but failed half-connections
        // or radio contention with WiFi can kill it without any
        // callback firing. Costs one flag-read when healthy.
        if (++bleWd >= 250) {
            bleWd = 0;
            ble.ensureAdvertising();
        }

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
        // Self-healing re-init: at 5s and 30s after boot (two chances
        // to catch a garbled cold-start init), then every 5 min as a
        // watchdog. The MAX7219 is write-only - its state can't be read
        // back and verified - so periodic blind refresh is the only
        // robust strategy. Control registers are rewritten here; digit
        // data is rewritten by the normal 2s display rotation. Between
        // them, no register can hold garbage for more than one cycle.
        // A healthy display just blinks for a frame. Touches nothing
        // but the display.
        unsigned long up = millis();
        if ((reinitStage == 0 && up > 5000) ||
            (reinitStage == 1 && up > 30000) ||
            (up - lastDisplayReinit >= DISPLAY_REINIT_INTERVAL)) {
            display.begin(DIN, CLK, CS);
            if (reinitStage < 2) reinitStage++;
            lastDisplayReinit = up;
            lastDisplayUpdate = 0;          // force immediate redraw
        }

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
    Serial.println("Booting system v1.2...");

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
    delay(250);   // let the display rail settle from a cold start
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