#include "CommandParser.h"
#include <ArduinoJson.h>

bool CommandParser::parse(const String& json)
{
    StaticJsonDocument<512> doc;

    DeserializationError error = deserializeJson(doc, json);

    if (error)
    {
        //Serial.println("JSON parse failed");
        return false;
    }

    JsonArray commands = doc["commands"];

    if (commands.size() == 0)
        return false;

    JsonObject first = commands[0];

    commandId = first["id"];
    payload = String((const char*)first["p"]);

    return true;
}

int CommandParser::getId()
{
    return commandId;
}

String CommandParser::getPayload()
{
    return payload;
}