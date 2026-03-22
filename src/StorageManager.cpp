#include "StorageManager.h"

static char lastNonce[20];

void StorageManager::init()
{
    // Mount LittleFS on the correct partition
    if (!LittleFS.begin(true, "/littlefs")) {
        Serial.println("LittleFS mount failed");
        return;
    }

    if (!LittleFS.exists("/nonce.txt")) {
        File f = LittleFS.open("/nonce.txt","w");
        f.println("0");
        f.close();
    }

    File f = LittleFS.open("/nonce.txt","r");
    String n = f.readStringUntil('\n');
    n.trim();
    strncpy(lastNonce, n.c_str(), sizeof(lastNonce) - 1);
    lastNonce[sizeof(lastNonce) - 1] = '\0';
    f.close();
}

const char* StorageManager::getLastNonce()
{
    return lastNonce;
}

void StorageManager::storeNonce(const char* nonce)
{
    File f = LittleFS.open("/nonce.txt","w");
    f.println(nonce);
    f.close();

    strncpy(lastNonce, nonce, sizeof(lastNonce) - 1);
    lastNonce[sizeof(lastNonce) - 1] = '\0';
}

bool StorageManager::isNonceValid(const char* nonce)
{
    long last = atol(lastNonce);
    long current = atol(nonce);

    return current > last;
}

float StorageManager::loadUnits()
{
    if (!LittleFS.exists("/units.txt")) return 0;

    File f = LittleFS.open("/units.txt","r");
    float v = f.parseFloat();
    f.close();

    return v;
}

void StorageManager::saveUnits(float units)
{
    File f = LittleFS.open("/units.txt","w");
    f.println(units);
    f.close();
}

uint32_t StorageManager::loadTime()
{
    if (!LittleFS.exists("/time.txt")) return 0;

    File f = LittleFS.open("/time.txt","r");
    uint32_t v = f.parseInt();
    f.close();

    return v;
}

void StorageManager::saveTime(uint32_t value)
{
    File f = LittleFS.open("/time.txt","w");
    f.println(value);
    f.close();
}

