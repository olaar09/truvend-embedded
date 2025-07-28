#include <Arduino.h>
#include <PZEM004Tv30.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>


const char ssid[] = "MeterV2.0";
const char pass[] = "";
WiFiServer server(80);

#define PZEM_RX_PIN 5
#define PZEM_TX_PIN 4
#define relay 13
#define reedSwitch 14


SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzems1(pzemSWSerial);

//PZEM004Tv30 pzems1(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0xF8);
#define led1 15

  

float voltage;
float current;
float energy;
float power;
float powerFactor;

uint8_t addr1 = 0;

unsigned long count = 0;
#define unit_addr 0
float availableUnit;
const char* key = "truvendprepaid-secret-key-2024";
unsigned long lastTime = 0;
float energyUsed = 0.0; // in KWh
int state = 1;








//--------------------- DECRYPTION FUNCTION ---------------------
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
//--------------------------------------------------------------



void setupWifi() {
  bool result = WiFi.softAP(ssid, pass);
 
  if (result) {
    Serial.println("Access Point started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Failed to start Access Point");
  }

  server.begin();
}


void resetMemory(){
  EEPROM.put(unit_addr, 0);
  EEPROM.commit();

}

void initializeMemory() {
  EEPROM.begin(64);
  //resetMemory();
  EEPROM.get(unit_addr, availableUnit);
  Serial.print("Available Unit: ");
  Serial.println(availableUnit);
  //state = 1;
  //EEPROM.put(1, state);
  //EEPROM.commit();
  EEPROM.get(1, state);
  Serial.print("State: ");
  Serial.println(state);
}

String extractEncryptedData(String request) {
  String prefix = "/device-top-up/";
  int start = request.indexOf(prefix);
  if (start == -1) return "";

  start += prefix.length();
  int end = request.indexOf(' ', start);
  if (end == -1) return "";

  return request.substring(start, end);
}



float handleData(String request){
  

  String encrypted = extractEncryptedData(request);
  if (encrypted.length() > 0) {
    Serial.print("Extracted encrypted data: ");
    Serial.println(encrypted);
  } else {
    Serial.println("Failed to extract encrypted data.");
    return -99;
  }
  const char* encryptedChar = encrypted.c_str();
  char* result = decrypt(encryptedChar, key);


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
    return -99;
  }


  float floatAmount = atof(amount);
  float newBalance = floatAmount + (availableUnit - energy);
  Serial.print("New Balance: ");
  Serial.println(newBalance);
  EEPROM.put(unit_addr, newBalance);
  EEPROM.commit();
  bool energyReset = pzems1.resetEnergy();
  availableUnit = newBalance;
  Serial.println("updated new balance");


  return newBalance;



  
}


void setup()
{
  delay(100);
  Serial.begin(115200); /*serial init */
  delay(100);
  Serial.println("Starting now");
  pinMode(led1, OUTPUT);
  pinMode(reedSwitch, INPUT_PULLUP);
  digitalWrite(led1, 0);
  pinMode(relay, OUTPUT);
  
  //delay(2000);
  setupWifi();
  initializeMemory();
  
  
}


void runServer() {
  WiFiClient client = server.available();
  if (!client) return;

  String request = client.readStringUntil('\r');
  Serial.println(request);
  if (request.indexOf("serg12$@a") != -1){
    state = 1;
    EEPROM.put(1, state);
    Serial.println("active");
    EEPROM.commit();
    //setup();
    delay(1000);
    request = ""                                                                                                                 ;
    return;
    delay(1000);
    //loop();
  }
  Serial.println("in here");
  float newBalance = handleData(request);
  if (newBalance == -99){
    //return;
  }

  EEPROM.get(unit_addr, availableUnit);
  
  
  client.println("HTTP/1.1 200 OK\r\n");
  client.println(availableUnit - pzems1.energy());
  client.flush();
  Serial.print("sent: ");
  Serial.println(availableUnit - pzems1.energy());

 
}



void calConsumption(){
  if (((availableUnit - pzems1.energy()) >= 0.01) && (state == 1)){
    //Turn off supply
    digitalWrite(relay, 1);
    digitalWrite(led1, 1);
  }
  else {
    digitalWrite(relay, 0);
    digitalWrite(led1, 0);
  }
  
}
  






void debugReadings(){
  //#define DEBUGPZEM
  #ifdef DEBUGPZEM
  // Read values
     // === PZEM 1 ===
    voltage       = pzems1.voltage();
    current       = pzems1.current();
    energy        = pzems1.energy();
    power         = pzems1.power();
    powerFactor   = pzems1.pf();

    
  
  
      Serial.println("=== PZEM 1 ===");
      Serial.print("Voltage: "); Serial.print(voltage); Serial.println(" V");
      Serial.print("Current: "); Serial.print(current); Serial.println(" A");
      Serial.print("Power: "); Serial.print(power); Serial.println(" W");
      Serial.print("Energy: "); Serial.print(energy); Serial.println(" kWh");
      Serial.print("Power Factor: "); Serial.println(powerFactor);

      Serial.println("\n");
  #endif

    
}









void checkTamper(){
  if (digitalRead(reedSwitch) == 0){
    state = 0;
    EEPROM.put(1, 0);
    EEPROM.commit();

  }
 
  //Serial.println(state);
}


void loop() {
 
    runServer();
    debugReadings();
    calConsumption();
    checkTamper();
    //Serial.print(".");
   
}






