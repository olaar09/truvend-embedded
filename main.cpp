#include <stdio.h>
#include <string.h>

char* decrypt(const char* encoded, const char* key) {
    static char out[80];
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char temp[80];
    int len = strlen(encoded), outLen = 0;

    for (int i = 0; i < len; i += 4) {
        int b[4];
        for (int j = 0; j < 4; j++) {
            const char* p = strchr(b64, encoded[i + j]);
            b[j] = p ? (p - b64) : -1;
        }
        temp[outLen++] = (b[0] << 2) | (b[1] >> 4);
        if (b[2] >= 0) temp[outLen++] = ((b[1] & 0xF) << 4) | (b[2] >> 2);
        if (b[3] >= 0) temp[outLen++] = ((b[2] & 0x3) << 6) | b[3];
    }

    for (int i = 0; i < outLen; i++) {
        out[i] = temp[i] ^ key[i % strlen(key)];
    }
    out[outLen] = 0;
    return out;
}


int main() {
    const char* input = "JQUUI19XSkhLX0JRWV0cSlxQS1FOH15cSx0CAA==";
    const char* key = "truvendprepaid-secret-key-2024";
    char* result = decrypt(input, key);

    printf("Decrypted: %s\n", result);
    char* nonce = strtok(result, ":");
    char* amount = strtok(NULL, ":");
    char* meter = strtok(NULL, ":");
    char* expiry = strtok(NULL, ":");

    if (amount && meter) {
        printf("Nonce: %s\n", nonce);
        printf("Amount: %s\n", amount);
        printf("Meter: %s\n", meter);
        printf("Expiry: %s\n", expiry);
        // Optional: validate nonce freshness or uniqueness here
    } else {
        printf("Invalid format.\n");
    }

    return 0;
}
