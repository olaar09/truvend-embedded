#ifndef SETUP_FILES_H
#define SETUP_FILES_H

#include <Arduino.h>


// ================= Configuration =================
#define DIN 18
#define CS 19
#define CLK 21
#define PZEM_RX 4
#define PZEM_TX 16
#define RELAY1 5
#define RELAY2 17



// ================= Runtime Variables =================


float voltage = 0;
float power = 0;
//uint32_t timeSeconds = 0;
String relayState = "off";


// display rotation
unsigned long lastDisplayUpdate = 0;
uint8_t displayState = 0;
const uint32_t DISPLAY_INTERVAL = 2000;

// countdown timer
unsigned long lastSecondTick = 0;
unsigned long lastFsWrite = 0;

#endif