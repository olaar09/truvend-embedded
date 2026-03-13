#include "DecryptHandler.h"
#include <string.h>

// ======================== Constructor ========================
DecryptHandler::DecryptHandler() {}

// ======================== Decrypt Function ========================
char* DecryptHandler::decrypt(const char* encoded, const char* key) {
    static char out[80];
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char temp[80];
    int len = strlen(encoded), outLen = 0;

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
        out[i] = temp[i] ^ key[i % strlen(key)];
    }
    out[outLen] = 0;

    return out;
}

// ======================== Handle Data ========================
String DecryptHandler::handleData(String request) {

    String encrypted = request;

    if (encrypted.length() == 0) {
        Serial.println(" Failed to extract encrypted data.");
        return String("fail");;
    }

    //Serial.print("The encrypted data: ");
    //Serial.println(encrypted);

    const char* encryptedChar = encrypted.c_str();
    char* result = decrypt(encryptedChar, key);

    //Serial.print("result: ");
    //Serial.println(result);

    //Serial.println("Completed");

    return String(result);
}