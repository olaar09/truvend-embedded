#include "StorageManager.h"

// =====================================================================
//  v1.1-final STORAGE DESIGN  (wear-optimized for 10+ year life)
//
//  ONE record per save:  seq  balance  lastEnergyKwh  timeSeconds  chk
//  written to two alternating slots (/bal_a.txt, /bal_b.txt).
//
//  Wear strategy:
//   - time.txt is no longer written at all (time lives in the record)
//   - the record is saved every FS_SAVE_INTERVAL (5 min, set in main)
//     and only when something changed - an idle meter writes nothing
//   - A/B alternation halves per-slot wear; LittleFS wear-leveling
//     spreads erases across the whole 960KB partition on top of that
//
//  Crash safety (unchanged from v1.1):
//   - a power cut mid-write can only damage the slot being written;
//     the other slot still holds the previous good record
//   - every record carries a checksum; corrupt/partial lines are
//     rejected on load
//   - every save is read back and verified before being trusted
//
//  Backward compatibility:
//   - old 4-field records (v1.1 without time) still load; their time
//     comes from the legacy /time.txt
//   - after the first successful 5-field save, /time.txt is deleted
// =====================================================================

static char lastNonce[24];
static SemaphoreHandle_t fsMutex = nullptr;
static uint32_t curSeq = 0;
static bool legacyTimeFileRemoved = false;

static const char* SLOT_A = "/bal_a.txt";
static const char* SLOT_B = "/bal_b.txt";
static const char* LEGACY_UNITS = "/units.txt";
static const char* LEGACY_TIME  = "/time.txt";

static const uint32_t MAX_TIME_SECONDS = 315360000UL;   // 10 years

// ---------------------------------------------------------------------
static void lockFS()   { if (fsMutex) xSemaphoreTake(fsMutex, portMAX_DELAY); }
static void unlockFS() { if (fsMutex) xSemaphoreGive(fsMutex); }

// ---------------------------------------------------------------------
// Checksums. v5 covers (seq, balance, lastEnergy, time); v4 is the
// original v1.1 formula, kept only so pre-update records still load.
static uint32_t balChecksum5(uint32_t seq, float bal, float lastE, uint32_t t)
{
    uint32_t b = (uint32_t)lroundf(bal   * 100.0f);   // 0.01 unit resolution
    uint32_t e = (uint32_t)lroundf(lastE * 1000.0f);  // 1 Wh resolution
    uint32_t x = 0xA5C3F00Du;
    x ^= seq * 2654435761u;
    x ^= b   * 2246822519u;
    x ^= e   * 3266489917u;
    x ^= t   * 374761393u;
    return x;
}

static uint32_t balChecksum4(uint32_t seq, float bal, float lastE)
{
    uint32_t b = (uint32_t)lroundf(bal   * 100.0f);
    uint32_t e = (uint32_t)lroundf(lastE * 1000.0f);
    uint32_t x = 0xA5C3F00Du;
    x ^= seq * 2654435761u;
    x ^= b   * 2246822519u;
    x ^= e   * 3266489917u;
    return x;
}

// Parse one slot. Accepts the new 5-field format, or a legacy 4-field
// record (hasTime=false). Returns true only if the checksum matches
// AND every value is physically plausible.
static bool readSlot(const char* path, uint32_t &seq, float &bal,
                     float &lastE, uint32_t &timeS, bool &hasTime)
{
    if (!LittleFS.exists(path)) return false;

    File f = LittleFS.open(path, "r");
    if (!f) return false;
    String line = f.readStringUntil('\n');
    f.close();

    unsigned long s = 0, t = 0, c = 0;
    float b = 0, e = 0;

    int n = sscanf(line.c_str(), "%lu %f %f %lu %lu", &s, &b, &e, &t, &c);

    if (n == 5) {
        if ((uint32_t)c != balChecksum5((uint32_t)s, b, e, (uint32_t)t))
            return false;
        hasTime = true;
    }
    else {
        // Legacy 4-field record: seq bal lastE chk  (t slot holds chk)
        n = sscanf(line.c_str(), "%lu %f %f %lu", &s, &b, &e, &c);
        if (n != 4) return false;
        if ((uint32_t)c != balChecksum4((uint32_t)s, b, e)) return false;
        t = 0;
        hasTime = false;
    }

    if (isnan(b) || isnan(e))            return false;
    if (b < 0.0f || b > 1000000.0f)      return false;   // balance sanity
    if (e < 0.0f || e > 20000.0f)        return false;   // PZEM caps ~9999 kWh
    if (t > MAX_TIME_SECONDS)            return false;   // > 10 years: corrupt

    seq = (uint32_t)s; bal = b; lastE = e; timeS = (uint32_t)t;
    return true;
}

static bool writeSlot(const char* path, uint32_t seq, float bal,
                      float lastE, uint32_t timeS)
{
    File f = LittleFS.open(path, "w");
    if (!f) return false;

    char line[80];
    snprintf(line, sizeof(line), "%lu %.2f %.3f %lu %lu",
             (unsigned long)seq, bal, lastE, (unsigned long)timeS,
             (unsigned long)balChecksum5(seq, bal, lastE, timeS));
    f.println(line);
    f.close();

    // Verify by reading back before trusting the write.
    uint32_t vs, vt; float vb, ve; bool ht;
    return readSlot(path, vs, vb, ve, vt, ht) && vs == seq && ht;
}

// Atomic single-line write (nonce only now): tmp -> remove -> rename.
static bool atomicWriteLine(const char* path, const String& content)
{
    String tmp = String(path) + ".tmp";

    File f = LittleFS.open(tmp.c_str(), "w");
    if (!f) return false;
    f.println(content);
    f.close();

    LittleFS.remove(path);
    return LittleFS.rename(tmp.c_str(), path);
}

// ---------------------------------------------------------------------
void StorageManager::init()
{
    if (!fsMutex) fsMutex = xSemaphoreCreateMutex();

    if (!LittleFS.begin(true, "/littlefs")) {
        Serial.println("LittleFS mount failed");
        return;
    }

    // Clean up any stray temp files from an interrupted write.
    LittleFS.remove("/time.txt.tmp");
    LittleFS.remove("/nonce.txt.tmp");

    if (!LittleFS.exists("/nonce.txt")) {
        File f = LittleFS.open("/nonce.txt", "w");
        if (f) { f.println("0"); f.close(); }
    }

    File f = LittleFS.open("/nonce.txt", "r");
    if (f) {
        String n = f.readStringUntil('\n');
        n.trim();
        strncpy(lastNonce, n.c_str(), sizeof(lastNonce) - 1);
        lastNonce[sizeof(lastNonce) - 1] = '\0';
        f.close();
    } else {
        strcpy(lastNonce, "0");
    }
}

// ---------------------------------------------------------------------
bool StorageManager::loadBalance(float &balance, float &lastEnergyKwh,
                                 uint32_t &timeSeconds)
{
    lockFS();

    uint32_t seqA = 0, seqB = 0, tA = 0, tB = 0;
    float balA = 0, eA = 0, balB = 0, eB = 0;
    bool htA = false, htB = false;
    bool okA = readSlot(SLOT_A, seqA, balA, eA, tA, htA);
    bool okB = readSlot(SLOT_B, seqB, balB, eB, tB, htB);

    bool found = false, hasTime = false;
    if (okA && (!okB || seqA >= seqB)) {
        balance = balA; lastEnergyKwh = eA; timeSeconds = tA;
        hasTime = htA; curSeq = seqA; found = true;
    } else if (okB) {
        balance = balB; lastEnergyKwh = eB; timeSeconds = tB;
        hasTime = htB; curSeq = seqB; found = true;
    }

    unlockFS();

    // Legacy record without time: pull it from the old time.txt once.
    if (found && !hasTime) {
        timeSeconds = loadTime();
    }

    return found;
}

bool StorageManager::saveBalance(float balance, float lastEnergyKwh,
                                 uint32_t timeSeconds)
{
    if (isnan(balance) || isnan(lastEnergyKwh)) return false;
    if (balance < 0.0f) balance = 0.0f;           // never persist negative
    if (lastEnergyKwh < 0.0f) lastEnergyKwh = 0.0f;
    if (timeSeconds > MAX_TIME_SECONDS) timeSeconds = MAX_TIME_SECONDS;

    lockFS();

    uint32_t seq = curSeq + 1;
    const char* path = (seq & 1) ? SLOT_A : SLOT_B;   // alternate slots

    bool ok = writeSlot(path, seq, balance, lastEnergyKwh, timeSeconds);
    if (ok) {
        curSeq = seq;

        // Time now lives inside the record - retire the legacy file
        // (one-time; saves its 60s write cycle worth of wear forever).
        if (!legacyTimeFileRemoved) {
            if (LittleFS.exists(LEGACY_TIME)) LittleFS.remove(LEGACY_TIME);
            legacyTimeFileRemoved = true;
        }
    }

    unlockFS();
    return ok;
}

// ---------------------------------------------------------------------
bool StorageManager::legacyUnitsExists()
{
    lockFS();
    bool e = LittleFS.exists(LEGACY_UNITS);
    unlockFS();
    return e;
}

float StorageManager::loadLegacyUnits()
{
    lockFS();

    float v = 0;
    if (LittleFS.exists(LEGACY_UNITS)) {
        File f = LittleFS.open(LEGACY_UNITS, "r");
        if (f) { v = f.parseFloat(); f.close(); }
    }

    unlockFS();

    if (isnan(v) || v < 0.0f || v > 1000000.0f) v = 0;   // corrupt -> 0
    return v;
}

void StorageManager::removeLegacyUnits()
{
    lockFS();
    LittleFS.remove(LEGACY_UNITS);
    unlockFS();
}

// ---------------------------------------------------------------------
// Legacy time.txt - read-only now, used once when migrating.
uint32_t StorageManager::loadTime()
{
    lockFS();

    long v = 0;
    if (LittleFS.exists(LEGACY_TIME)) {
        File f = LittleFS.open(LEGACY_TIME, "r");
        if (f) { v = f.parseInt(); f.close(); }
    }

    unlockFS();

    if (v < 0) v = 0;
    if (v > (long)MAX_TIME_SECONDS) v = 0;    // > 10 years -> corrupt, reset
    return (uint32_t)v;
}

// ---------------------------------------------------------------------
const char* StorageManager::getLastNonce()
{
    return lastNonce;
}

void StorageManager::storeNonce(const char* nonce)
{
    lockFS();

    atomicWriteLine("/nonce.txt", String(nonce));
    strncpy(lastNonce, nonce, sizeof(lastNonce) - 1);
    lastNonce[sizeof(lastNonce) - 1] = '\0';

    unlockFS();
}

bool StorageManager::isNonceValid(const char* nonce)
{
    // `long` is 32-bit on ESP32 (max 2,147,483,647). Unix-second nonces
    // overflow it on 19 Jan 2038; millisecond nonces overflow it today.
    // Compare as 64-bit unsigned so replay protection outlives the meter.
    unsigned long long last    = strtoull(lastNonce, NULL, 10);
    unsigned long long current = strtoull(nonce,    NULL, 10);
    return current > last;
}