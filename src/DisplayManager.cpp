#include "DisplayManager.h"

void DisplayManager::begin(int din,int clk,int cs)
{
    lc = LedController<1,1>(din,clk,cs);

    lc.setIntensity(2);
    lc.clearMatrix();
}

void DisplayManager::showVoltage(int value)
{
    lc.clearMatrix();

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

void DisplayManager::showPower(int value)
{
    lc.clearMatrix();

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
    lc.setChar(0,0, ones,      false); 
}

void DisplayManager::showUnits(int value)
{
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

void DisplayManager::showState(bool on)
{
    lc.clearMatrix();

    if(on){
        lc.setRow(0,7, B1011011);  // s
        lc.setRow(0,6, B0001111);  // t
        lc.setRow(0,5, B1110111);  // a
        lc.setRow(0,4, B0001111);  // t
        lc.setRow(0,3, B0001001);  // =
        lc.setRow(0,2, B0011101);  // o
        lc.setRow(0,1, B0010101);  // n
    }
    else{
        lc.setRow(0,7, B1011011);  // s
        lc.setRow(0,6, B0001111);  // t
        lc.setRow(0,5, B1110111);  // a
        lc.setRow(0,4, B0001111);  // t
        lc.setRow(0,3, B0001001);  // -
        lc.setRow(0,2, B0011101);  // o
        lc.setRow(0,1, B1000111);  // f
        lc.setRow(0,0, B1000111);  // f
    }
}


void DisplayManager::showCountdown(int countValue)
{
    lc.clearMatrix();

    lc.setRow(0,7, B0110111);  // h
    lc.setRow(0,6, B0000101);  // r
    lc.setRow(0,5, B1011011);  // s
    lc.setRow(0,4, B0001001);  // =

    long value = countValue;

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





void DisplayManager::wifi(bool on)
{
    lc.clearMatrix();

    if(on){
        lc.setRow(0,7, B1110110);  // n
        lc.setRow(0,6, B1101111);  // e
        lc.setRow(0,5, B0001111);  // T
        lc.setRow(0,4, B0001001);  // =
        lc.setRow(0,3, B0011101);  // O
        lc.setRow(0,2, B0010101);  // N
    }
    else{
        lc.setRow(0,7, B1110110);  // n
        lc.setRow(0,6, B1101111);  // e
        lc.setRow(0,5, B0001111);  // T
        lc.setRow(0,4, B0001001);  // =
        lc.setRow(0,3, B0011101);  // O
        lc.setRow(0,2, B1000111);  // F
        lc.setRow(0,1, B1000111);  // F
    }
}
