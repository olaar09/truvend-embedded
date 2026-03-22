#include "setupFiles.h"
#include "PowerMeter.h"
#include "RelayController.h"
#include "DisplayManager.h"
#include "StorageManager.h"
#include "BLEManager.h"
#include "MeterLogic.h"
#include <CloudClient.h>
#include <addFile.h>

// const char* ssid = "aDevXSY8TZkZcdk";
// const char* password = "u3tgYkyn2JX8gUx";





// =================Setup files ============
const char* ssid = "aDevXSY8TZkZcdk";
const char* password = "u3tgYkyn2JX8gUx";
String meterNo = "87800000078";
String jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2VfaW52ZW50b3J5X3JlZiI6Ijg3ODAwMDAwMDc4Iiwic2NvcGUiOiJpb3RfZGV2aWNlIiwiaWF0IjoxNzc0MjIwNTkxLCJleHAiOjIwODk3OTY1OTF9.NA_AjhovKPquG8i015CDYc1w7ujpVN5vvJfpl8zi9Go";






//String url = "http://iot.truvend.online/iot/set_status/87800000004?action=set_status&balance=49&relay=on&power=118&energy=23&seconds=609997&meter_number=87800000004";
//String getDataUrl = "http://iot.truvend.online/iot/get_command/87800000004";

CloudClient cloud(ssid, password, jwtToken);
//const unsigned long RESTART_INTERVAL = 6UL * 60UL * 60UL * 1000UL; // 6 hours in ms
const unsigned long RESTART_INTERVAL = 21600000; // 6 hours in ms
//const unsigned long RESTART_INTERVAL = 60000; // 6 hours in ms
// ================= Objects =================
PowerMeter powerMeter;
RelayController relay;
DisplayManager display;
BLEManager ble;
MeterLogic meterLogic;
float newBalanceTop = 0;
float availableUnits = 0;
uint32_t timeSeconds = 0;
bool resetMeterL = false;
bool wifiCon = false;
float energy = 0;

bool serverRUnning = false;

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
    availableUnits = StorageManager::loadUnits();
    serverRUnning = false;
}

// =====================================================
void sendUpdate()
{
    String url = "http://iot.truvend.online/iot/set_status/";
    url += meterNo;
    url += "?action=set_status";

    url += "&balance=" + String(availableUnits-energy);
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

void checkServerData()
{
    if (millis() - lastServerCheck >= serverInterval)
    {
        lastServerCheck = millis();

        if (!serverRUnning)
        {
            if (millis() >= RESTART_INTERVAL) {
                Serial.println("Restarting Meter...");
                ESP.restart(); //Restart
            }
            String getDataUrl = "http://iot.truvend.online/iot/get_command/" + String(meterNo);
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
    if (resetMeterL)
        {
            bool stateAA = powerMeter.resetEnergy();

            if (stateAA)
            {
                Serial.println("Reset OK");
                resetMeterL = false;
            }
            else
            {
                Serial.println("Reset failed, retrying...");
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
}

// =====================================================
void updateRelayState()
{
    float remaining = availableUnits - energy;

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
    if ((availableUnits - energy <= 0.01 || timeSeconds <= 0) && power > 10) {
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

    displayState = (displayState + 1) % 6;   // was %4

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
            int units = (int)max(0.0f, availableUnits - energy);
            display.showUnits(units);
            break;
        }

        case 3:
            display.showState(relay.isOn());
            break;

        case 4:
        {
            int countdown = timeSeconds/3600; 
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
        

        if (timeSeconds > 0){
            timeSeconds--;
        }
            
            
    }
//if (now - lastFsWrite >= 300000)
    if (now - lastFsWrite >= 60000)
    {
        lastFsWrite = now;
        Serial.println("Writing now.....");
        StorageManager::saveTime(timeSeconds);

    }

   
}

////////////////////////////////////////////////////////
///////////////////// TASKS ////////////////////////////
////////////////////////////////////////////////////////

void printTimerTaskOnly()
{
    TaskStatus_t taskStatus[20];
    UBaseType_t taskCount;

    // get all tasks
    taskCount = uxTaskGetSystemState(
        taskStatus,
        20,
        NULL
    );

    // loop through tasks and find "Timer Task"
    for (int i = 0; i < taskCount; i++)
    {
        if (strcmp(taskStatus[i].pcTaskName, "BLE Task") == 0)
        {
            Serial.printf(
                "%s  stack left: %u words\n",
                taskStatus[i].pcTaskName,
                taskStatus[i].usStackHighWaterMark
            );
            break; // found it, no need to continue
        }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // optional delay
}

void bleTask(void *pvParameters)
{
    for(;;)
    {
        //Serial.printf("Stack left ble: %u\n", uxTaskGetStackHighWaterMark(NULL));
        //printTimerTaskOnly();
        handleBleCommands();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void meterTask(void *pvParameters)
{
    for(;;)
    {
        //Serial.printf("Stack left meter task: %u\n", uxTaskGetStackHighWaterMark(NULL));
        updatePowerReadings();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void relayTask(void *pvParameters)
{
    for(;;)
    {
        //Serial.printf("Stack left relay: %u\n", uxTaskGetStackHighWaterMark(NULL));
        updateRelayState();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void displayTask(void *pvParameters)
{
    for(;;)
    {
        //Serial.printf("Stack left display: %u\n", uxTaskGetStackHighWaterMark(NULL));
        updateDisplay();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void timerTask(void *pvParameters)
{
    for(;;)
    {
        //Serial.printf("Stack left timer: %u\n", uxTaskGetStackHighWaterMark(NULL));
        countdownTimer();
        vTaskDelay(1100 / portTICK_PERIOD_MS);
    }
}

void cloudTask(void *pvParameters)
{
    cloud.begin();
    for(;;)
    {
        //Serial.printf("Stack left cloud: %u\n", uxTaskGetStackHighWaterMark(NULL));
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

    Serial.print("Units: ");
    Serial.println(availableUnits);

    Serial.print("Time: ");
    Serial.println(timeSeconds);

    powerMeter.begin(Serial2, PZEM_RX, PZEM_TX);
    relay.begin(RELAY1, RELAY2);
    display.begin(DIN, CLK, CS);
    ble.begin(meterNo.c_str());

    xTaskCreate(bleTask,"BLE Task",8096,NULL,1,NULL);
    xTaskCreate(meterTask,"Meter Task",6096,NULL,0,NULL);
    xTaskCreate(relayTask,"Relay Task",2048,NULL,0,NULL);
    xTaskCreate(displayTask,"Display Task",4096,NULL,1,NULL);
    xTaskCreate(timerTask,"Timer Taskk",5048,NULL,0,NULL);
    xTaskCreate(cloudTask,"Cloud Task",10192,NULL,1,NULL);

    Serial.println("System ready");
}

// =====================================================
void loop()
{
}

//1540 left with 2048