#ifndef DECRYPTHANDLER_H
#define DECRYPTHANDLER_H

#include <Arduino.h>

class DecryptHandler {
public:
    DecryptHandler();

    String handleData(String request);

private:
    const char* key = "truvendprepaid-secret-key-2024";

    char* decrypt(const char* encoded, const char* key);
};

#endif