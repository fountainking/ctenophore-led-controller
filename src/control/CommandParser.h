#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>
#include <functional>
#include <ArduinoJson.h>

// Command structure
struct Command {
  const char* name;
  std::function<void(String)> handler;
};

// Unified command parser for serial and web inputs
class CommandParser {
private:
  Command* commands;
  int commandCount;

public:
  CommandParser() : commands(nullptr), commandCount(0) {}

  // Register commands
  void registerCommands(Command* cmdArray, int count) {
    commands = cmdArray;
    commandCount = count;
  }

  // Parse and execute command
  bool parse(String input) {
    input.trim();

    // Handle key=value format
    int equalsIndex = input.indexOf('=');
    String cmdName, cmdValue;

    if (equalsIndex > 0) {
      cmdName = input.substring(0, equalsIndex);
      cmdValue = input.substring(equalsIndex + 1);
    } else {
      cmdName = input;
      cmdValue = "";
    }

    // Find and execute command
    for (int i = 0; i < commandCount; i++) {
      if (cmdName == commands[i].name || input.startsWith(commands[i].name)) {
        if (commands[i].handler) {
          commands[i].handler(cmdValue);
          return true;
        }
      }
    }

    Serial.println("❌ Unknown command: " + cmdName);
    return false;
  }

  // Process serial input
  void processSerial() {
    if (Serial.available()) {
      String command = Serial.readString();
      parse(command);
    }
  }

  // Helper: Parse float with validation
  static bool parseFloat(String value, float& result, float min, float max) {
    result = value.toFloat();
    if (result >= min && result <= max) {
      return true;
    }
    Serial.println("❌ Invalid value. Range: " + String(min) + " - " + String(max));
    return false;
  }

  // Helper: Parse int with validation
  static bool parseInt(String value, int& result, int min, int max) {
    result = value.toInt();
    if (result >= min && result <= max) {
      return true;
    }
    Serial.println("❌ Invalid value. Range: " + String(min) + " - " + String(max));
    return false;
  }

  // Helper: Parse boolean
  static bool parseBool(String value) {
    value.toLowerCase();
    return (value == "true" || value == "1" || value == "on");
  }

  // Helper: Parse hex color
  static uint32_t parseHexColor(String hex) {
    hex.trim();
    if (hex.startsWith("#")) {
      hex = hex.substring(1);
    }

    long number = strtol(hex.c_str(), NULL, 16);
    uint8_t r = (number >> 16) & 0xFF;
    uint8_t g = (number >> 8) & 0xFF;
    uint8_t b = number & 0xFF;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }
};

#endif // COMMAND_PARSER_H
