#include "CommandParser.h"
#include <ArduinoJson.h>

bool CommandParser::parse(const String& json)
{
    StaticJsonDocument<512> doc;

    DeserializationError error = deserializeJson(doc, json);

    if (error)
        return false;

    // v1.1: validate structure before touching fields.
    // v1.0 did String((const char*)first["p"]) - a missing "p" made that
    // String(nullptr): undefined behavior / crash.
    if (!doc["commands"].is<JsonArray>())
        return false;

    JsonArray commands = doc["commands"];

    if (commands.size() == 0)
        return false;

    JsonObject first = commands[0];

    if (!first["id"].is<int>() || !first["p"].is<const char*>())
        return false;

    commandId = first["id"];
    payload = String(first["p"].as<const char*>());

    if (payload.length() == 0)
        return false;

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