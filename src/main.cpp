// #include "setupFiles.h"
// #include "PowerMeter.h"
// #include "RelayController.h"
// #include "DisplayManager.h"
// #include "StorageManager.h"
// #include "BLEManager.h"
// #include "MeterLogic.h"
// #include <CloudClient.h>

// const char* ssid = "Jossy_5g";
// const char* password = "olamide12121";
// String jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2VfaW52ZW50b3J5X3JlZiI6Ijg3ODAwMDAwMDA0Iiwic2NvcGUiOiJpb3RfZGV2aWNlIiwiaWF0IjoxNzcyOTc3OTA4LCJleHAiOjE4MDg5Nzc5MDh9.Cu9aiXUzuUT62-9wlixw8veKhoXCcDDB7ppDQgEIoLY";


// String url = "http://iot.truvend.online/iot/set_status/87800000004?action=set_status&balance=49&relay=on&power=118&energy=23&seconds=609997&meter_number=87800000004";
// String getDataUrl = "http://iot.truvend.online/iot/get_command/87800000004";
// CloudClient cloud(ssid, password, jwtToken);


// // ================= Objects =================
// PowerMeter powerMeter;
// RelayController relay;
// DisplayManager display;
// BLEManager ble;
// MeterLogic meterLogic;
// bool serverRUnning = false;


// // =====================================================
// void handleBleCommands()
// {
//     if (!ble.actionPending()) return;

//     String value = ble.getValue();
//     float newBalance = meterLogic.handleTopup(value);
//     ble.send(String(newBalance));
//     availableUnits = StorageManager::loadUnits();
// }

// // =====================================================



// void sendUpdate(){
//     String url = "http://iot.truvend.online/iot/set_status/";
//     url += meterNo;
//     url += "?action=set_status";

//     url += "&balance=" + String(availableUnits);
//     url += "&relay=" + relayState;
//     url += "&power=" + String(power);
//     url += "&energy=" + String(energy);
//     url += "&seconds=" + String(timeSeconds);
//     url += "&meter_number=" + meterNo;
//     url += "&voltage=" + String(voltage, 2);
//     cloud.sendRequest(url);
// }




// // ====================================================
// unsigned long lastServerCheck = 0;
// const unsigned long serverInterval = 30000; // 30 seconds
// void checkServerData()
// {
//     if (millis() - lastServerCheck >= serverInterval)
//     {
//         lastServerCheck = millis();

//         if (!serverRUnning)
//         {
//             cloud.sendRequest(getDataUrl);
//             sendUpdate();
//         }
//     }
// }

// //====================================================


// void updatePowerReadings()
// {
//     powerMeter.update();
//     voltage = powerMeter.voltage();
//     power = powerMeter.power();
//     energy = powerMeter.energy();
// }

// // =====================================================
// void updateRelayState()
// {
//     float remaining = availableUnits - energy;
//     if (remaining >= 0.01 && !relay.isOn() && timeSeconds > 0)
//     {
//         relay.turnOn();
//         Serial.println("Relay ON");
//     }

//     if ((remaining < 0.01 || timeSeconds <= 0) && relay.isOn())
//     {
//         relay.turnOff();
//         Serial.println("Relay OFF");
//     }
// }

// // =====================================================
// void updateDisplay()
// {
//     if (millis() - lastDisplayUpdate < DISPLAY_INTERVAL)
//         return;
//     lastDisplayUpdate = millis();
//     displayState = (displayState + 1) % 4;
//     switch (displayState)
//     {
//         case 0:
//             display.showVoltage((int)voltage);
//             break;
//         case 1:
//             display.showPower((int)power);
//             break;
//         case 2:
//         {
//             int units = (int)max(0.0f, availableUnits - energy);
//             display.showUnits(units);
//             break;
//         }
//         case 3:
//             display.showState(relay.isOn());
//             break;
//     }
// }

// // =====================================================
// void countdownTimer()
// {
//     unsigned long now = millis();

//     if (now - lastSecondTick >= 1000)
//     {
//         lastSecondTick = now;

//         if (timeSeconds > 0)
//             timeSeconds--;
//     }

//     if (now - lastFsWrite >= 300000)
//     {
//         lastFsWrite = now;
//         StorageManager::saveTime(timeSeconds);
//     }
// }

// // =====================================================
// void setup()
// {
//     Serial.begin(115200);
//     Serial.println("Booting system...");
//     cloud.begin();
//     //cloud.sendRequest(url);

//     // storage
//     Serial.println("Initializing storage");
//     StorageManager::init();
//     Serial.println("Done with storage");

//     availableUnits = StorageManager::loadUnits();
//     timeSeconds = StorageManager::loadTime();

//     // hardware
//     powerMeter.begin(Serial2, PZEM_RX, PZEM_TX);
//     relay.begin(RELAY1, RELAY2);
//     display.begin(DIN, CLK, CS);
//     ble.begin(meterNo1.c_str());

//     Serial.println("System ready");
// }

// // =====================================================
// void loop()
// {
//     handleBleCommands();
//     updatePowerReadings();
//     updateRelayState();
//     updateDisplay();
//     countdownTimer();
//    checkServerData();
    
// }




#include "setupFiles.h"
#include "PowerMeter.h"
#include "RelayController.h"
#include "DisplayManager.h"
#include "StorageManager.h"
#include "BLEManager.h"
#include "MeterLogic.h"
#include <CloudClient.h>

const char* ssid = "Jossy_5g";
const char* password = "olamide12121";

String jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2VfaW52ZW50b3J5X3JlZiI6Ijg3ODAwMDAwMDA0Iiwic2NvcGUiOiJpb3RfZGV2aWNlIiwiaWF0IjoxNzcyOTc3OTA4LCJleHAiOjE4MDg5Nzc5MDh9.Cu9aiXUzuUT62-9wlixw8veKhoXCcDDB7ppDQgEIoLY";

String url = "http://iot.truvend.online/iot/set_status/87800000004?action=set_status&balance=49&relay=on&power=118&energy=23&seconds=609997&meter_number=87800000004";
String getDataUrl = "http://iot.truvend.online/iot/get_command/87800000004";

CloudClient cloud(ssid, password, jwtToken);

// ================= Objects =================
PowerMeter powerMeter;
RelayController relay;
DisplayManager display;
BLEManager ble;
MeterLogic meterLogic;

bool serverRUnning = false;

// =====================================================
void handleBleCommands()
{
    if (!ble.actionPending()) return;

    String value = ble.getValue();
    float newBalance = meterLogic.handleTopup(value);
    ble.send(String(newBalance));
    availableUnits = StorageManager::loadUnits();
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
const unsigned long serverInterval = 30000;

void checkServerData()
{
    if (millis() - lastServerCheck >= serverInterval)
    {
        lastServerCheck = millis();

        if (!serverRUnning)
        {
            cloud.sendRequest(getDataUrl);
            sendUpdate();
        }
    }
}

// =====================================================
void updatePowerReadings()
{
    powerMeter.update();
    voltage = powerMeter.voltage();
    power = powerMeter.power();
    energy = powerMeter.energy();
}

// =====================================================
void updateRelayState()
{
    float remaining = availableUnits - energy;

    if (remaining >= 0.01 && !relay.isOn() && timeSeconds > 0)
    {
        relay.turnOn();
        Serial.println("Relay ON");
    }

    if ((remaining < 0.01 || timeSeconds <= 0) && relay.isOn())
    {
        relay.turnOff();
        Serial.println("Relay OFF");
    }
}

// =====================================================
void updateDisplay()
{
    if (millis() - lastDisplayUpdate < DISPLAY_INTERVAL)
        return;

    lastDisplayUpdate = millis();

    displayState = (displayState + 1) % 4;

    switch (displayState)
    {
        case 0: display.showVoltage((int)voltage); break;
        case 1: display.showPower((int)power); break;
        case 2:
        {
            int units = (int)max(0.0f, availableUnits - energy);
            display.showUnits(units);
            break;
        }
        case 3: display.showState(relay.isOn()); break;
    }
}

// =====================================================
void countdownTimer()
{
    unsigned long now = millis();

    if (now - lastSecondTick >= 1000)
    {
        lastSecondTick = now;

        if (timeSeconds > 0)
            timeSeconds--;
    }

    if (now - lastFsWrite >= 300000)
    {
        lastFsWrite = now;
        StorageManager::saveTime(timeSeconds);
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
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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
    Serial.println("Booting system...");

    

    Serial.println("Initializing storage");
    StorageManager::init();
    Serial.println("Done with storage");

    availableUnits = StorageManager::loadUnits();
    timeSeconds = StorageManager::loadTime();

    powerMeter.begin(Serial2, PZEM_RX, PZEM_TX);
    relay.begin(RELAY1, RELAY2);
    display.begin(DIN, CLK, CS);
    ble.begin(meterNo1.c_str());

    xTaskCreate(bleTask,"BLE Task",4096,NULL,1,NULL);
    xTaskCreate(meterTask,"Meter Task",4096,NULL,1,NULL);
    xTaskCreate(relayTask,"Relay Task",2048,NULL,1,NULL);
    xTaskCreate(displayTask,"Display Task",4096,NULL,1,NULL);
    xTaskCreate(timerTask,"Timer Task",2048,NULL,1,NULL);
    xTaskCreate(cloudTask,"Cloud Task",8192,NULL,1,NULL);

    Serial.println("System ready");
}

// =====================================================
void loop()
{
}