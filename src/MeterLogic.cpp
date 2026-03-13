#include "MeterLogic.h"
#include "CryptoManager.h"
#include "StorageManager.h"

static const char* key = "truvendprepaid-secret-key-2024";

float MeterLogic::handleTopup(String token)
{
    char* result = CryptoManager::decrypt(token.c_str(),key);
    //char* result = token.c_str();
    char* nonce = strtok(result,":");
    char* amount = strtok(NULL,":");
    char* meter = strtok(NULL,":");
    char* seconds = strtok(NULL,":");

    Serial.print("Nonce: ");
    Serial.println(nonce);

    Serial.print("Amount: ");
    Serial.println(amount);

    Serial.print("Meter: ");
    Serial.println(meter);

    Serial.print("Seconds: ");
    Serial.println(seconds);

    if(!(nonce && amount && meter)){
        return -99;
    }


    if(!StorageManager::isNonceValid(nonce)){
        return -99;
    }
    

    StorageManager::storeNonce(nonce);

    float units = StorageManager::loadUnits();

    float add = atof(amount);

    units += add;

    StorageManager::saveUnits(units);
    //availableU
    
    
    

    return units;
}