#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>

class CommandParser
{
public:
    bool parse(const String& json);

    int getId();
    String getPayload();

private:
    int commandId;
    String payload;
};

#endif