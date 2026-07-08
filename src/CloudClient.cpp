#include "CloudClient.h"
#include "CommandParser.h"
#include <MeterLogic.h>
#include <addFile.h>

MeterLogic loadmeter;
CommandParser parser;

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

    while (WiFi.status() != WL_CONNECTED)
    {
        wifiCon = false;
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    wifiCon = true;
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

        // parse() now validates the whole structure; a malformed or
        // empty payload can no longer reach handleTopup.
        if (payload.length() > 0 && parser.parse(payload))
        {
            id = parser.getId();
            String payloadData = parser.getPayload();

            newBalanceTop = loadmeter.handleTopup(payloadData);

            // Don't ACK a topup that failed on storage (-98): leaving it
            // un-ACKed lets the server redeliver and the meter retry,
            // since the nonce was NOT burned.
            if (newBalanceTop == -98) {
                id = -1;
            }
        }
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