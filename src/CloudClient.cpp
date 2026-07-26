#include "CloudClient.h"
#include "CommandParser.h"
#include <MeterLogic.h>
#include <addFile.h>

MeterLogic loadmeter;
CommandParser parser;

// v1.2: WiFi reconnect backoff. The old code called
// WiFi.disconnect() + WiFi.begin() on EVERY sendRequest while
// disconnected - i.e. every ~5s. Each begin() is a full channel scan on
// the ESP32's single shared radio, which starves BLE advertising slots
// (meters in weak-WiFi spots became invisible to phones) and churns
// heap. Now: one manual reconnect attempt per 60s maximum, and
// setAutoReconnect handles gentle rejoin attempts in between.
static unsigned long lastWifiRetry = 0;
static const unsigned long WIFI_RETRY_INTERVAL = 60000UL;   // 60s

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

    // v1.2: bounded wait (was: infinite loop). If the AP is absent, give
    // up after 60s and enter normal operation - sendRequest's backoff
    // keeps retrying, and crucially the 6-hour maintenance restart
    // becomes reachable even for meters with no working WiFi (it used
    // to be blocked here forever, so offline meters never self-healed).
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        wifiCon = false;
        if (millis() - start > 60000UL) {
            Serial.println("WiFi not found in 60s - continuing offline");
            lastWifiRetry = millis();   // start the backoff clock
            return;
        }
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

        // v1.2: at most one manual reconnect kick per 60s (see note at
        // top of file). Between kicks, the stack's auto-reconnect keeps
        // trying quietly without hogging the radio.
        if (millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL)
        {
            lastWifiRetry = millis();
            WiFi.disconnect();
            WiFi.begin(_ssid, _password);
        }
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

        // parse() validates the whole structure; a malformed or
        // empty payload can never reach handleTopup.
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