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
    float units = availableUnits - energy; //So units is what my last recharge - what I have left
    float receivedAmount = atof(amount);  //receivedAmount is what is coming in
    if (receivedAmount == -5){ //If received amount is -5, clear both unit and received amount so all is zero
        Serial.println("clearing credit");
        units = 0;
        receivedAmount = 0;
    }

    float add = receivedAmount;
    units += add;

    StorageManager::saveUnits(units);
    availableUnits = units;
    bool resetStatus = false;
    resetMeterL = true;
    
    

    return units;
}