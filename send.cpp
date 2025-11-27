#include <stdio.h>
#include <string.h>

const char* key = "prepaid-secret-key-2024";
const char* encoded = "OkNFMF9cV15BVkNSWlcXQVdTQlVCG1lRTxk=";  // this is coming from the bluethooth


char* decrypt(const char* encoded, const char* key) {
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


void setup() {
  Serial.begin(115200);
  while (!Serial);

  char* result = decrypt(encoded, key);
  Serial.print("Decrypted: ");
  Serial.println(result);

  char* nonce = strtok(result, ":");
  char* amount = strtok(NULL, ":");
  char* meter = strtok(NULL, ":");

  if (amount && meter) {
    Serial.print("Amount: ");
    Serial.println(amount);
    Serial.print("Meter: ");
    Serial.println(meter);
  } else {
    Serial.println("Invalid format.");
  }
}