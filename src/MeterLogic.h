#ifndef METER_LOGIC_H
#define METER_LOGIC_H

#include <Arduino.h>

// v1.1: handleTopup no longer touches the PZEM or deducts energy.
// It only validates the token and ADDS credit to the live balance.
// Return values:
//   >= 0  new balance
//   -99   invalid / rejected token (bad decrypt, missing fields,
//         reused nonce, wrong meter, implausible amount)
//   -98   storage busy or flash save failed (token NOT burned,
//         user can retry the same token)

class MeterLogic {
public:
    float handleTopup(String token);
};

#endif