#include "CryptoManager.h"

// =====================================================================
//  v1.1 HOTFIX: this file was missed in the first merge - the old
//  unbounded version was still compiled into firmware.
//
//  This is the decrypt that handleTopup() actually calls, so it is the
//  live money path. Bounds checks below guarantee:
//   - a token longer than the buffers can NEVER overflow (returns nullptr)
//   - malformed base64 (not multiple of 4, empty) is rejected
//   - MeterLogic's `if (!result) return -99;` guard now actually works
// =====================================================================

char* CryptoManager::decrypt(const char* encoded, const char* key) {

  static char out[128];
  const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned char temp[128];

  if (!encoded || !key) return nullptr;

  int len = strlen(encoded);
  size_t keyLen = strlen(key);

  // base64 must be non-empty, a multiple of 4, and decode within buffers
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

    if (encoded[i + 2] != '=')
      temp[outLen++] = ((b[1] & 0xF) << 4) | (b[2] >> 2);

    if (encoded[i + 3] != '=')
      temp[outLen++] = ((b[2] & 0x3) << 6) | b[3];
  }

  for (int i = 0; i < outLen; i++) {
    out[i] = temp[i] ^ key[i % keyLen];
  }

  out[outLen] = 0;

  return out;
}