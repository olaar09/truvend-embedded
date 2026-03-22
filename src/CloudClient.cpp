#include "CloudClient.h"
#include "CommandParser.h"
#include <DecryptHandler.h>
#include <MeterLogic.h>
#include <addFile.h>

MeterLogic loadmeter;
CommandParser parser;
DecryptHandler decryptHandler;

CloudClient::CloudClient(const char* ssid, const char* password, const String& token)
{
    _ssid = ssid;
    _password = password;
    _jwtToken = token;
}

void CloudClient::begin()
{
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    WiFi.begin(_ssid, _password);

    //Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        wifiCon = false;
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    wifiCon = true;

    //Serial.println();
    //Serial.println("WiFi connected");
}
void CloudClient::sendRequest(const String& url)
{
     if (WiFi.status() != WL_CONNECTED)
        {
            wifiCon = false;
            WiFi.disconnect();
            WiFi.begin(_ssid, _password);
            return;
        }
        wifiCon = true;


    serverRUnning = true;
    id = -1;

    HTTPClient http;

    //=====================
    // FETCH COMMAND
    //=====================

    http.begin(url);
    http.addHeader("Authorization", "Bearer " + _jwtToken);
    http.setTimeout(5000);

    int httpCode = http.GET();

    if (httpCode > 0)
    {
        String payload = http.getString();

        Serial.print(".");

        if (parser.parse(payload))
        {
            id = parser.getId();
            String payloadData = parser.getPayload();

            // Serial.print("Command ID: ");
            // Serial.println(id);
            newBalanceTop = loadmeter.handleTopup(payloadData);
            // Serial.print("The new balance is: ");
            // Serial.println(newBalanceTop);
        }
    }
    else
    {
        // Serial.print("HTTP request failed: ");
        // Serial.println(httpCode);
    }

    http.end();

    //=====================
    // ACK COMMAND
    //=====================

    if (id != -1)
    {
        String ackUrl = "http://iot.truvend.online/iot/ack_commands/" + String(meterNo) + "?success_command_ids=" + String(id);
        

        http.begin(ackUrl);
        http.addHeader("Authorization", "Bearer " + _jwtToken);
        http.setTimeout(5000);

        httpCode = http.GET();

        if (httpCode > 0)
        {
            Serial.print("HTTP Code: ");
            Serial.println(httpCode);

            String payload = http.getString();

            Serial.println("Response:");
            Serial.println(payload);
        }
        else
        {
            Serial.print("HTTP request failed: ");
            Serial.println(httpCode);
        }

        http.end();
    }

    serverRUnning = false;
    id = -1;
}