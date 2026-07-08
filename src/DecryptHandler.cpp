#include "DecryptHandler.h"
#include <string.h>

// NOTE: this class duplicates CryptoManager::decrypt. Nothing in the v1.1
// flow calls it anymore (handleTopup uses CryptoManager), but it is kept
// bounds-checked so it can never overflow if reintroduced. Recommended:
// delete this file + its include in CloudClient.cpp in a later cleanup.

DecryptHandler::DecryptHandler() {}

char* DecryptHandler::decrypt(const char* encoded, const char* key) {
    static char out[128];
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char temp[128];

    if (!encoded || !key) return nullptr;

    int len = strlen(encoded);
    size_t keyLen = strlen(key);

    if (len == 0 || (len % 4) != 0 || keyLen == 0) return nullptr;
    if (((len / 4) * 3) >= (int)sizeof(temp))      return nullptr;

    int outLen = 0;

    for (int i = 0; i < len; i += 4) {
        int b[4];
        for (int j = 0; j < 4; j++) {
            const char* p = strchr(b64, encoded[i + j]);
            b[j] = p ? (p - b64) : 0;
        }
        temp[outLen++] = (b[0] << 2) | (b[1] >> 4);
        if (encoded[i + 2] != '=') temp[outLen++] = ((b[1] & 0xF) << 4) | (b[2] >> 2);
        if (encoded[i + 3] != '=') temp[outLen++] = ((b[2] & 0x3) << 6) | b[3];
    }

    for (int i = 0; i < outLen; i++) {
        out[i] = temp[i] ^ key[i % keyLen];
    }
    out[outLen] = 0;

    return out;
}

String DecryptHandler::handleData(String request) {

    String encrypted = request;

    if (encrypted.length() == 0) {
        Serial.println(" Failed to extract encrypted data.");
        return String("fail");
    }

    const char* encryptedChar = encrypted.c_str();
    char* result = decrypt(encryptedChar, key);

    if (!result) {
        Serial.println(" Decrypt failed (invalid or oversized token).");
        return String("fail");
    }

    return String(result);
}