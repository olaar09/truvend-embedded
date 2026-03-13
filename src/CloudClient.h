#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

class CloudClient {

public:
    CloudClient(const char* ssid, const char* password, const String& token);

    void begin();
    void sendRequest(const String& url);
    bool serverRUnning = false;
    int id;

private:
    const char* _ssid;
    const char* _password;
    String _jwtToken;
};

#endif