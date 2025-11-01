#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "../config/Constants.h"

// Wrapper class for NeoPixel LED strip control
class LEDController {
private:
  Adafruit_NeoPixel* strip;
  int numLeds;
  float globalBrightness;

public:
  LEDController(Adafruit_NeoPixel* stripPtr)
    : strip(stripPtr),
      numLeds(HardwareConfig::NUM_LEDS),
      globalBrightness(0.6f) {}

  // Initialize LED strip
  void begin() {
    strip->begin();
    strip->setBrightness(255);  // Full brightness (we control via color values)
    clear();
    show();
    Serial.println("💡 LED strip initialized");
  }

  // Update LED display
  void show() {
    strip->show();
  }

  // Set global brightness
  void setBrightness(float brightness) {
    globalBrightness = constrain(brightness, 0.0f, 1.0f);
  }

  float getBrightness() const {
    return globalBrightness;
  }

  // Clear all LEDs
  void clear() {
    strip->clear();
  }

  // Set LED color using RGB (0-255 range)
  void setColorRGB(int led, uint8_t r, uint8_t g, uint8_t b) {
    if (led >= 0 && led < numLeds) {
      strip->setPixelColor(led, strip->Color(r, g, b));
    }
  }

  // Set LED color using HSV
  // hue: 0.0-1.0 (full color wheel)
  // brightness: 0.0-1.0
  void setColorHSV(int led, float hue, float brightness) {
    if (led < 0 || led >= numLeds) return;

    // Clamp values
    hue = constrain(hue, 0.0, 1.0);
    brightness = constrain(brightness, 0.0, 1.0);

    // Convert HSV to RGB
    float h = hue * 6.0;
    float s = 1.0;  // Full saturation
    float v = brightness;

    float c = v * s;
    float x = c * (1.0 - abs(fmod(h, 2.0) - 1.0));
    float m = v - c;

    float r, g, b;
    if (h < 1.0)      { r = c; g = x; b = 0; }
    else if (h < 2.0) { r = x; g = c; b = 0; }
    else if (h < 3.0) { r = 0; g = c; b = x; }
    else if (h < 4.0) { r = 0; g = x; b = c; }
    else if (h < 5.0) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    setColorRGB(led,
                (uint8_t)((r + m) * 255),
                (uint8_t)((g + m) * 255),
                (uint8_t)((b + m) * 255));
  }

  // Set LED color using uint32_t packed color
  void setColor(int led, uint32_t color) {
    if (led >= 0 && led < numLeds) {
      strip->setPixelColor(led, color);
    }
  }

  // Get LED color
  uint32_t getColor(int led) const {
    if (led >= 0 && led < numLeds) {
      return strip->getPixelColor(led);
    }
    return 0;
  }

  // Set all LEDs to same color
  void fill(uint32_t color) {
    strip->fill(color);
  }

  void fillRGB(uint8_t r, uint8_t g, uint8_t b) {
    fill(strip->Color(r, g, b));
  }

  // Set brightness multiplier (0-255)
  void setBrightness(uint8_t brightness) {
    strip->setBrightness(brightness);
  }

  // Get number of LEDs
  int getNumLeds() const {
    return numLeds;
  }

  // Interpolate between two colors
  static uint32_t interpolateColor(uint32_t color1, uint32_t color2, float factor) {
    factor = constrain(factor, 0.0, 1.0);

    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;

    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;

    uint8_t r = r1 + (r2 - r1) * factor;
    uint8_t g = g1 + (g2 - g1) * factor;
    uint8_t b = b1 + (b2 - b1) * factor;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  // Adjust color temperature (-1.0 = cool, 0.0 = neutral, +1.0 = warm)
  static uint32_t adjustColorTemperature(uint32_t color, float temperature) {
    temperature = constrain(temperature, -1.0, 1.0);

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    if (temperature > 0) {
      // Warm: increase red, decrease blue
      r = min(255, (int)(r * (1.0 + temperature * 0.3)));
      b = max(0, (int)(b * (1.0 - temperature * 0.3)));
    } else {
      // Cool: decrease red, increase blue
      float coolFactor = -temperature;
      r = max(0, (int)(r * (1.0 - coolFactor * 0.3)));
      b = min(255, (int)(b * (1.0 + coolFactor * 0.3)));
    }

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  // Convert hex string to color (e.g., "#FF5500" or "FF5500")
  static uint32_t hexToColor(String hex) {
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

  // Create packed color from RGB
  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }
};

#endif // LED_CONTROLLER_H
