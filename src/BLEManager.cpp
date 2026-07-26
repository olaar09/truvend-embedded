#include "BLEManager.h"
#include <NimBLEDevice.h>
#include "esp_bt.h"

static NimBLECharacteristic* pCharacteristic;
static bool deviceConnected=false;
static bool pending=false;
static String value;

class Callbacks: public NimBLECharacteristicCallbacks {

    void onWrite(NimBLECharacteristic* c,NimBLEConnInfo&) {

        std::string v=c->getValue();

        if(v.length()>0)
        {
            value = String(v.c_str());
            pending=true;
        }
    }
};

class ServerCallbacks: public NimBLEServerCallbacks {

    void onConnect(NimBLEServer*,NimBLEConnInfo&) {

        deviceConnected=true;
    }

    void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int) {

        deviceConnected=false;
        NimBLEDevice::startAdvertising();
    }
};


void BLEManager::stop()
{
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) adv->stop();

    NimBLEDevice::deinit(true);   // destroy NimBLE stack

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
    }

    deviceConnected = false;
    pending = false;
    pCharacteristic = nullptr;

    Serial.println("BLE stopped");
}

void BLEManager::begin(const char* name)
{
    NimBLEDevice::init(name);

    // v1.2: was setMTU(23). The new rich status reply
    // (OK:balance:relay:power:energy:seconds:voltage:reason) is
    // ~40-55 bytes; at MTU 23 a notification carries only 20 bytes and
    // the reply would be TRUNCATED. 185 fits comfortably and is what
    // iOS negotiates natively; Android apps must call requestMtu()
    // (see flutter side note).
    NimBLEDevice::setMTU(185);

    NimBLEServer* server = NimBLEDevice::createServer();

    server->setCallbacks(new ServerCallbacks());

    NimBLEService* service =
        server->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");

    pCharacteristic =
        service->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26a8",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
        );

    pCharacteristic->setCallbacks(new Callbacks());
    pCharacteristic->setValue("Truvend says hi");
    pCharacteristic->createDescriptor(NimBLEUUID((uint16_t)0x2902));

    service->start();

    NimBLEAdvertising* adv=NimBLEDevice::getAdvertising();
    adv->setName(name);
    adv->addServiceUUID(service->getUUID());
    adv->enableScanResponse(true);
    adv->start();
}

bool BLEManager::connected()
{
    return deviceConnected;
}

bool BLEManager::actionPending()
{
    return pending;
}

String BLEManager::getValue()
{
    pending=false;
    return value;
}

void BLEManager::send(String msg)
{
    if(!deviceConnected) return;

    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();
}

// v1.2: advertising watchdog. NimBLE restarts advertising on a clean
// disconnect (our callback + the library's own advertiseOnDisconnect),
// but a half-formed connection that dies, a silently-failed start, or
// radio contention with WiFi can leave advertising dead with no
// callback fired - meter invisible to phones until reboot. This
// re-asserts it. Safe to call every few seconds: does nothing while a
// phone is connected or advertising is already running.
void BLEManager::ensureAdvertising()
{
    if (deviceConnected) return;

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv && !adv->isAdvertising()) {
        adv->start();
        Serial.println("BLE watchdog: advertising restarted");
    }
}