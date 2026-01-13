#include <Arduino.h>
#include <PZEM004Tv30.h>
#include "LedController.hpp"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>














// ======================== Configuration ========================
String meterNo = "M87800000001";
//String bluetoothName = "M87800000003";
#define DIN 18
#define CS 19
#define CLK 21
LedController<1,1> lc;
#define PZEM_RX_PIN 4
#define PZEM_TX_PIN 16
#define relay1 17 //17 //Rerlay for off
#define relay2 5  //5 //Relay for on
//#define unit_addr 0
//#define nonceAddr 20
long timeSeconds = 0;
int macAddress = 0;
int timeAddr = 40;   // just ensure it does not overlap others
bool relayState = false;
const char* key = "truvendprepaid-secret-key-2024";
#define PZEM_SERIAL Serial2
PZEM004Tv30 pzems1(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN);
float voltage;
float current;
float power;
float energy;
float availableUnit;
char lastNonce[20];  // Stores last used nonce
File nonceFile;
File unitFile;
File countdownFile;
const char* TIME_FILE = "/time.txt";
static unsigned long lastDisplayMillis = 0;
static uint8_t displayState = 0;          // 0 = VOL, 1 = POWER, 2 = UNIT
const unsigned long DISPLAY_INTERVAL = 2000UL; // 2000 ms = 2 seconds
unsigned long lastNotify = 0;
String BleValue;


//bluetooth configs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
static NimBLEServer* pServer = nullptr;
static NimBLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
const char *hardcodedMsg = "Hello from ESP32";


// //========= Setup Keypad ===================
// const byte ROWS = 4; /* four rows */
// const byte COLS = 3; /* four columns */
// /* define the symbols on the buttons of the keypads */
// char hexaKeys[ROWS][COLS] = {
//   {'1','2','3'},
//   {'4','5','6'},
//   {'7','8','9'},
//   {'*','0','#'}
// };
// //32|| 33, 25, 26, 27, 14, 22, 23
// byte rowPins[ROWS] = {25, 23, 22, 27}; /* connect to the row pinouts of the keypad */
// byte colPins[COLS] = {26, 33, 14}; /* connect to the column pinouts of the keypad */
// Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 



//===========Configuration ends here =============













// ======================== Decrypt Function ========================
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

// ======================== Nonce Handling (LittleFS) ========================


// Store new nonce
void storeNonce(const char* nonce) {
  nonceFile = LittleFS.open("/nonceFile.txt", "w"); // overwrite previous
  if (!nonceFile) {
    Serial.println("Failed to open nonce file for writing");
    return;
  }

  nonceFile.println(nonce);
  nonceFile.close();

  // Update lastNonce in RAM
  strncpy(lastNonce, nonce, sizeof(lastNonce));
  Serial.print("Stored new nonce: ");
  Serial.println(lastNonce);
}

bool isNonceValid(const char* newNonce) {
  // Convert lastNonce and newNonce to numbers (or compare lexicographically if they are numeric strings)
  long last = atol(lastNonce);
  long current = atol(newNonce);

  if (current > last) {
    return true;
  } else {
    Serial.println("Nonce is not greater than last stored nonce. Rejecting top-up.");
    return false;
  }
}


void initializeMemory1() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }
  Serial.println("LittleFS mounted");

  // ---------- Nonce File ----------
  if (!LittleFS.exists("/nonceFile.txt")) {
    Serial.println("Nonce file missing. Creating default.");
    nonceFile = LittleFS.open("/nonceFile.txt", "w");
    if (nonceFile) {
      nonceFile.println("0");  // start with 0 for monotonic comparison
      nonceFile.close();
    }
  }

  // Read last nonce
  nonceFile = LittleFS.open("/nonceFile.txt", "r");
  if (nonceFile) {
    String storedNonce = nonceFile.readStringUntil('\n');
    nonceFile.close();
    storedNonce.trim();

    if (storedNonce.length() == 0) {
      // File was empty, initialize to "0"
      strcpy(lastNonce, "0");
    } else {
      strncpy(lastNonce, storedNonce.c_str(), sizeof(lastNonce));
      lastNonce[sizeof(lastNonce)-1] = 0; // ensure null-termination
    }

    Serial.print("Last Nonce: ");
    Serial.println(lastNonce);
  } else {
    Serial.println("Failed to read nonce file. Resetting.");
    strcpy(lastNonce, "0");
  }

  // ---------- Unit File ----------
  if (!LittleFS.exists("/unitFile.txt")) {
    Serial.println("Unit file missing. Creating default 0.0");
    unitFile = LittleFS.open("/unitFile.txt", "w");
    if (unitFile) {
      unitFile.println("0.0");
      unitFile.close();
    }
  }

  // Read availableUnit
  unitFile = LittleFS.open("/unitFile.txt", "r");
  if (unitFile) {
    String unitStr = unitFile.readStringUntil('\n');
    unitFile.close();
    unitStr.trim();
    availableUnit = unitStr.toFloat();
    if (isnan(availableUnit)) {
      Serial.println("Unit value corrupt. Resetting to 0.0");
      availableUnit = 0.0;
    }
    Serial.print("Available Unit: ");
    Serial.println(availableUnit);
  }
}





bool resetEnergyBlocking() {
  unsigned long start = millis();

  while (true) {
    if (pzems1.resetEnergy()) {
      Serial.println("PZEM energy reset successful");
      return true;
    }

    // Log once every second
    if (millis() - start >= 1000) {
      Serial.println("Waiting for PZEM energy reset...");
      start = millis();
    }

    delay(50);        // allow FreeRTOS + BLE to run
    yield();           // critical on ESP32
  }
}



// ======================== Core Logic ========================
String extractEncryptedData(String request) {
  String prefix = "/device-top-up/";
  int start = request.indexOf(prefix);
  if (start == -1) return "";

  start += prefix.length();
  int end = request.indexOf(' ', start);
  if (end == -1) return "";

  return request.substring(start, end);
}

float handleData(String request) {
  // String encrypted = extractEncryptedData(request);
  String encrypted = request;
  if (encrypted.length() == 0) {
    Serial.println(" Failed to extract encrypted data.");
    return -99;
  }
  Serial.print("The encrypted data: ");
  Serial.println(encrypted);

  const char* encryptedChar = encrypted.c_str();
  char* result = decrypt(encryptedChar, key);
  Serial.print("result: ");
  Serial.println(result);

  char* nonce = strtok(result, ":");
  char* amount = strtok(NULL, ":");
  char* meter = strtok(NULL, ":");
  char* secondsStr = strtok(NULL, ":");
  long remainingSeconds = atol(secondsStr);

  if (!(nonce && amount && meter)) {
    Serial.println(" Invalid top-up format.");
    return -99;
  }

  Serial.print("Nonce: ");  Serial.println(nonce);
  Serial.print("Amount: "); Serial.println(amount);
  Serial.print("Meter: "); Serial.println(meter);
  Serial.print("Seconds: "); Serial.println(secondsStr);

  if (strcmp(meter, meterNo.c_str()) != 0) {
    Serial.println(" Meter mismatch. Rejecting top-up.");
    return -99;
  }

  // ---------- Check nonce ----------
if (!isNonceValid(nonce)) {
    Serial.println("Top-up rejected due to old or repeated nonce.");
    return -99;  // reject top-up
}

// ---------- Store new nonce ----------
storeNonce(nonce);


  // ---------- Update timeSeconds ----------
  timeSeconds = remainingSeconds;
  countdownFile = LittleFS.open("/time.txt", "w");
  if (countdownFile) {
    countdownFile.println(timeSeconds);
    countdownFile.close();
  }

  // ---------- Update availableUnit ----------
  float floatAmount = atof(amount);
  float newBalance = floatAmount + (availableUnit - energy);
  availableUnit = newBalance;

  unitFile = LittleFS.open("/unitFile.txt", "w");
  if (unitFile) {
    unitFile.println(availableUnit);
    unitFile.close();
  }

  // ---------- Reset energy ----------
  resetEnergyBlocking();



  Serial.println("Top-up completed and new nonce stored.");

  return newBalance;
}








void writeStateOff() {
  lc.clearMatrix();
  // Display V O L T -
  lc.setRow(0,7, B1011011);  // s
  lc.setRow(0,6, B0001111);  // t
  lc.setRow(0,5, B1110111);  // a
  lc.setRow(0,4, B0001111);  // t
  lc.setRow(0,3, B0001001);  // -
  lc.setRow(0,2, B0011101);  // o
  lc.setRow(0,1, B1000111);  // f
  lc.setRow(0,0, B1000111);  // f

}



void writeStateOn() {
  lc.clearMatrix();
  // Display V O L T -
  lc.setRow(0,7, B1011011);  // s
  lc.setRow(0,6, B0001111);  // t
  lc.setRow(0,5, B1110111);  // a
  lc.setRow(0,4, B0001111);  // t
  lc.setRow(0,3, B0001001);  // -
  lc.setRow(0,2, B0011101);  // o
  lc.setRow(0,1, B0010101);  // n

}



void writeVol(int value) {
  lc.clearMatrix();
  // Display V O L T -
  lc.setRow(0,7, B0111110);  // V
  lc.setRow(0,6, B0011101);  // O
  lc.setRow(0,5, B0001110);  // L
  lc.setRow(0,4, B0001111);  // T
  lc.setRow(0,3, B0001001);  // -


  // Extract digits (hundreds, tens, ones)
  int hundreds = (value / 100) % 10;
  int tens     = (value / 10) % 10;
  int ones     = value % 10;

  // Display digits
  lc.setChar(0,2, hundreds, false);
  lc.setChar(0,1, tens, false);
  lc.setChar(0,0, ones, false);
}



// // writePower: writes "POWER" then a 3-digit value (hundreds,tens,ones)
// // Rows: 7=P, 6=O, 5=W, 4=E, 3=R, 2=hundreds, 1=tens, 0=ones
void writePower(int value) {
  lc.clearMatrix();
  // Letters at rows 7..3
  lc.setRow(0,7, B1001110);  // C
  lc.setRow(0,6, B1111110);  // O
  lc.setRow(0,5, B1110110);  // n
  //lc.setRow(0,4, B0111101);  // d
  lc.setRow(0,4, B0001001);  // =

  // Limit 0–9999
  if (value < 0) value = 0;
  if (value > 9999) value = 9999;

  // Extract 4 digits
  int thousands = (value / 1000) % 10;
  int hundreds  = (value / 100) % 10;
  int tens      = (value / 10) % 10;
  int ones      = value % 10;

  // Display 4 digits on rows 2,1,0, and use row? (depending on your layout)
  lc.setChar(0,3, thousands, false);
  lc.setChar(0,2, hundreds,  false);
  lc.setChar(0,1, tens,      false);
  lc.setChar(0,0, ones,      false); // if you prefer all 4 together, tell me rows available
}


void writeUnit(int value) {
    // Letters using mapping bit6..bit0 = A..G
    lc.clearMatrix();
    lc.setRow(0,7, B0111110);  // U
    lc.setRow(0,6, B0010101);  // n
    lc.setRow(0,5, B0001111);  // t
    lc.setRow(0,4, B0001001);  // =

    // Limit value to 0..9999
    if (value < 0) value = 0;
    if (value > 9999) value = 9999;

    // Extract digits (thousands, hundreds, tens, ones)
    int thousands = (value / 1000) % 10;
    int hundreds  = (value / 100) % 10;
    int tens      = (value / 10) % 10;
    int ones      = value % 10;

    // Display digits using setChar (CodeB digits)
    lc.setChar(0,3, thousands, false);
    lc.setChar(0,2, hundreds, false);
    lc.setChar(0,1, tens, false);
    lc.setChar(0,0, ones, false);
}


void writeSecondsRemaining() {
  lc.clearMatrix();
    // Letters: "sec="
    lc.setRow(0,7, B0110111);  // h
    lc.setRow(0,6, B0000101);  // r
    lc.setRow(0,5, B1011011);  // s
    lc.setRow(0,4, B0001001);  // =

    long value = timeSeconds/3600;

    // Use only the last 4 digits
    value = value % 10000;

    if (value < 0) value = 0;

    // Extract digits
    int thousands = (value / 1000) % 10;
    int hundreds  = (value / 100) % 10;
    int tens      = (value / 10) % 10;
    int ones      = value % 10;

    // Display
    lc.setChar(0,3, thousands, false);
    lc.setChar(0,2, hundreds, false);
    lc.setChar(0,1, tens, false);
    lc.setChar(0,0, ones, false);
}



void writeConnected() {
  //lc.clearMatrix();
    // Letters: "sec="
    lc.setRow(0,7, B1001110);  // c
    lc.setRow(0,6, B1111110);  // o
    lc.setRow(0,5, B1110110);  // n
    lc.setRow(0,4, B1110110);  // n
    lc.setRow(0,3, B1001111);  // e
    lc.setRow(0,2, B1001110);  // c
    lc.setRow(0,1, B0001111);  // t
    lc.setRow(0,0, B0111101);  // d


}




void processDataAndNotify() {
  if (!deviceConnected) return;
  //Serial.println("ble is connected, data is processing...");

  StaticJsonDocument<300> doc;
  doc["remainingUnit"] = max(0.0f, availableUnit - energy);
  doc["voltage"] = voltage;
  doc["relayState"] = relayState;
  doc["power"] = power;
  doc["timeSeconds"] = timeSeconds;
  doc["meterNo"] = meterNo;

  char buffer[200];
  size_t n = serializeJson(doc, buffer);

  pCharacteristic->setValue((uint8_t*)buffer, n);  // cast char* to uint8_t*
  pCharacteristic->notify();

  //Serial.print("Sent BLE JSON: ");
  //Serial.println(buffer);
}




void readPzem() {
    static unsigned long lastPzemRead = 0; // remembers between calls
    const unsigned long PZEM_INTERVAL = 1000;

    unsigned long now = millis();
    if (now - lastPzemRead >= PZEM_INTERVAL) {
        lastPzemRead = now;
        voltage = pzems1.voltage();
        energy  = pzems1.energy();
        power   = pzems1.power();
    }
}




void debugReadings() {
  //#define DEBUGPZEM
  
   readPzem();
     if (voltage <= 0 || isnan(voltage)) return;
    if (isnan(energy)) return;
     if (isnan(power)) return;
    unsigned long now = millis();

//   // Call every 5 seconds
//   if (now - lastNotify >= 1000) {
//     lastNotify = now;
//     //processDataAndNotify();
//   }


     if ((now - lastDisplayMillis) >= DISPLAY_INTERVAL) {
    // advance state in order 0 -> 1 -> 2 -> 0 ...
    displayState = (displayState + 1) % 5;
    lastDisplayMillis = now;
     #ifdef DEBUGPZEM 

    Serial.println("=== PZEM Reading ===");
    Serial.print("Voltage: "); Serial.println(voltage);
    //Serial.print("Current: "); Serial.println(current);
    Serial.print("Power: "); Serial.println(power);
    Serial.print("Energy: "); Serial.println(energy);
    //Serial.print("Power Factor: "); Serial.println(powerFactor);
    Serial.println();
    #endif
      int displayValue;
    switch (displayState) {
      case 0:
        writeVol((int)voltage);
        break;

      case 1:
        // use measured power (or cast/scale as needed)
        writePower((int)power);    // replace with whatever numeric you want
        break;

      case 2:
        // use measured energy or some other unit value
        displayValue = (int)max(0.0f, availableUnit - energy);
        writeUnit(displayValue);
        break;
      case 3:
        if (relayState == true){
          writeStateOn();
        }
        if (relayState == false){
         writeStateOff();
        }
      
        break;
      case 4:
        writeSecondsRemaining();
        break;
        default:
            break;

    }

}
}
  




void calConsumption() {
  if (((availableUnit - energy) >= 0.01) && (relayState == false) && (timeSeconds > 0)) {
    digitalWrite(relay1, 0);  // Power ON
    digitalWrite(relay2, 1);  // Power ON
    delay(30);
    digitalWrite(relay1, 0);  // Power ON
    digitalWrite(relay2, 0);  // Power ON
    relayState = true;
    Serial.println("Relay on");
  }



   if ((((availableUnit - energy) < 0.01) && (relayState == true)) || ((timeSeconds<=0) && (relayState == true))) {
    digitalWrite(relay1, 1);  // Power ON
    digitalWrite(relay2, 0);  // Power OFF
    delay(30);
    digitalWrite(relay1, 0);  // 
    digitalWrite(relay2, 0);  // 
    relayState = false;
    Serial.println("relay off");
  }
}




void saveTimeToFS(uint32_t value) {
  File f = LittleFS.open(TIME_FILE, "w");
  if (!f) return;
  f.print(value);
  f.close();
}

uint32_t loadTimeFromFS() {
  if (!LittleFS.exists(TIME_FILE)) return 0;

  File f = LittleFS.open(TIME_FILE, "r");
  if (!f) return 0;

  uint32_t v = f.parseInt();
  f.close();
  return v;
}




void countDown() {
  static unsigned long lastSecondTick = 0;
  static unsigned long lastFsWrite   = 0;

  unsigned long now = millis();

  // deduct every 1 second
  if (now - lastSecondTick >= 1000) {
    lastSecondTick = now;

    if (timeSeconds > 0) {
      timeSeconds--;
    }
  }

  // write to LittleFS every 5 minutes (300,000 ms)
  if (now - lastFsWrite >= 300000) {
    lastFsWrite = now;
    saveTimeToFS(timeSeconds);
  }
}


void bleCheck(){
   if (deviceConnected) {
    pCharacteristic->setValue("Device Connected");
    pCharacteristic->notify();
  }
}


















//-----------BLE code begins -----------------//

class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        deviceConnected = true;
        Serial.println("NimBLE client connected");
        //delay(1000);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        deviceConnected = false;
        Serial.println("NimBLE client disconnected");
        NimBLEDevice::startAdvertising();
    }
};



void bluetoothPendingFunction(){
        // ---- Parse JSON ----
    StaticJsonDocument<400> doc;
    DeserializationError err = deserializeJson(doc, BleValue);

    if (err) {
      Serial.println("JSON parse failed");
      pCharacteristic->setValue("ERR:INVALID_JSON");
      pCharacteristic->notify();
      return;
    }

    // ---- Extract action ----
    const char* action = doc["action"];
    if (!action) {
      pCharacteristic->setValue("ERR:NO_ACTION");
      pCharacteristic->notify();
      return;
    }

    String response;

    // ---- Action routing ----
    if (strcmp(action, "load_credit") == 0) {

      const char* token = doc["value"];
      if (!token) {
        pCharacteristic->setValue("ERR:NO_VALUE");
        pCharacteristic->notify();
        return;
      }

      float newBalance = handleData(String(token));
       pCharacteristic->setValue(String(newBalance));
        pCharacteristic->notify();


    }
    else if (strcmp(action, "get_status") == 0) {
      Serial.println("sending data to ble");
      processDataAndNotify();


    }
    else {
      response = "ERR:UNKNOWN_ACTION";
    }

    
}

volatile bool bleActionPending = false;

class MyCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();
        if (value.length() == 0) return;
        

        BleValue = String(value.c_str());
        //Serial.print("Received via NimBLE: ");
        //Serial.println(BleValue);
        bleActionPending = true;

    

    }
};




void setupBle(){
NimBLEDevice::init("M87800000001");
 NimBLEDevice::setMTU(23);

pServer = NimBLEDevice::createServer();
pServer->setCallbacks(new MyServerCallbacks());

NimBLEService* pService = pServer->createService(SERVICE_UUID);

pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::NOTIFY
);

pCharacteristic->setCallbacks(new MyCallbacks());
pCharacteristic->setValue(hardcodedMsg);

// Add 2902 descriptor for notifications
pCharacteristic->createDescriptor(NimBLEUUID((uint16_t)0x2902));

pService->start();

NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
pAdvertising->setName("M87800000001");
pAdvertising->addServiceUUID(SERVICE_UUID);
pAdvertising->enableScanResponse(true);
pAdvertising->start();

}













// ======================== Setup & Loop ========================
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting now...");

  
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  initializeMemory1();
  //resetNonceFile();
  timeSeconds = loadTimeFromFS();
  Serial.print("timeSeconds: ");
  Serial.println(timeSeconds);
  setupBle();
  lc=LedController<1,1>(DIN,CLK,CS);
  lc.setIntensity(2); /* Set the brightness to a medium values */
  lc.clearMatrix(); /* and clear the display */
  delay(100);
  Serial.println(esp_reset_reason());
  delay(100);
  debugReadings();
  delay(100);

  //2619

}




void loop() {
  debugReadings();
  calConsumption();
  countDown();
if (bleActionPending) {
    bleActionPending = false;
    bluetoothPendingFunction();
}


  
}

//12


