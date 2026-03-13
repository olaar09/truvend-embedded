#include "BLEManager.h"
#include <NimBLEDevice.h>

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

void BLEManager::begin(const char* name)
{
    NimBLEDevice::init(name);
    NimBLEDevice::setMTU(23);

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