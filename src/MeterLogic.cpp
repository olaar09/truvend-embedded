#include "MeterLogic.h"
#include "CryptoManager.h"
#include "StorageManager.h"
#include <addFile.h>

static const char* key = "truvendprepaid-secret-key-2024";

// Reject absurd recharge amounts even if the token decrypts cleanly.
static const float MAX_TOPUP_UNITS = 100000.0f;

float MeterLogic::handleTopup(String token)
{
    // ---------- decode ----------
    char* result = CryptoManager::decrypt(token.c_str(), key);
    if (!result) {
        Serial.println("Topup rejected: decrypt failed / token too long");
        return -99;
    }

    char* nonce   = strtok(result, ":");
    char* amount  = strtok(NULL, ":");
    char* meter   = strtok(NULL, ":");
    char* seconds = strtok(NULL, ":");

    // v1.0 bug: 'seconds' was never null-checked -> strtoul(NULL) crash.
    if (!(nonce && amount && meter && seconds)) {
        Serial.println("Topup rejected: missing fields");
        return -99;
    }

    Serial.print("Nonce: ");   Serial.println(nonce);
    Serial.print("Amount: ");  Serial.println(amount);
    Serial.print("Meter: ");   Serial.println(meter);
    Serial.print("Seconds: "); Serial.println(seconds);

    // Token must be for THIS meter. (Comment out this block if your
    // token generator encodes the meter field differently.)
    if (meterNo != String(meter)) {
        Serial.println("Topup rejected: wrong meter number");
        return -99;
    }

    if (!StorageManager::isNonceValid(nonce)) {
        Serial.println("Topup rejected: nonce reused/old");
        return -99;
    }

    // ---------- validate amount ----------
    float receivedAmount = strtof(amount, NULL);
    bool clearCredit = (receivedAmount == -5.0f);   // admin: zero the balance

    if (!clearCredit) {
        if (isnan(receivedAmount) || receivedAmount <= 0.0f ||
            receivedAmount > MAX_TOPUP_UNITS) {
            Serial.println("Topup rejected: implausible amount");
            return -99;
        }
    }

    uint32_t loadedTime = strtoul(seconds, NULL, 10);

    // ---------- apply under mutex ----------
    // BLE task and cloud task can both land here; the mutex makes the
    // read-modify-write-save sequence atomic.
    if (xSemaphoreTake(balanceMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("Topup deferred: balance busy");
        return -98;   // nonce NOT burned - token can be retried
    }

    float newBal;
    if (clearCredit) {
        Serial.println("clearing credit");
        newBal = 0.0f;
    } else {
        newBal = availableUnits + receivedAmount;   // delta model: just ADD
    }
    if (newBal < 0.0f || isnan(newBal)) newBal = 0.0f;

    // Persist FIRST - balance, energy anchor and countdown time are now
    // ONE atomic checksummed record (one flash write instead of two).
    // Only if flash confirms do we update RAM and burn the nonce. A
    // failed save leaves everything untouched and the token stays valid.
    bool saved = StorageManager::saveBalance(newBal, lastEnergyKwh, loadedTime);
    if (!saved) {
        xSemaphoreGive(balanceMutex);
        Serial.println("Topup failed: flash save error");
        return -98;
    }

    availableUnits = newBal;
    timeSeconds = loadedTime;

    xSemaphoreGive(balanceMutex);

    StorageManager::storeNonce(nonce);   // burn nonce only after success

    Serial.print("Topup OK. New balance: ");
    Serial.println(newBal);
    return newBal;
}