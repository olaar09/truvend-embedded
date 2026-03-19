#include "MeterLogic.h"
#include "CryptoManager.h"
#include "StorageManager.h"
#include <addFile.h>
#include <PowerMeter.h>

PowerMeter pzemCon;
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

    // if (atof(amount) == -10){
    //     Serial.println("turning off supply");
    //     return -10;
    // }

    // if (atof(amount) == -11){
    //     Serial.println("turning on supply");
    //     return -11;
    // }
    
    

    StorageManager::storeNonce(nonce); //store nonce

    //work with time and store it
    uint32_t loadedTime = strtoul(seconds, NULL, 10);
    StorageManager::saveTime(loadedTime);
    timeSeconds = loadedTime;
    //done


    //work with unit and store it
    float units = StorageManager::loadUnits();
    float receivedAmount = atof(amount);
//Serial.println("1");
    if (receivedAmount == -5){
        Serial.println("clearing credit");
        units = 0;
        receivedAmount = 0;
    }
    //Serial.println("2");

    float add = receivedAmount;
    units += add;

    StorageManager::saveUnits(units);
    availableUnits = units;
    bool resetStatus = false;
    //Serial.println("3");
    resetMeterL = true;
    //Serial.println("4");
    
    

    return units;
}