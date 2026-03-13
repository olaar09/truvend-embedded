#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#include <Arduino.h>

class CryptoManager {
public:
    static char* decrypt(const char* encoded, const char* key);
};

#endif