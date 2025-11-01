#ifndef PALETTE_MANAGER_H
#define PALETTE_MANAGER_H

#include <Arduino.h>
#include "../config/Constants.h"

// Color palette structure
struct ColorPalette {
  String name;
  uint32_t colors[7];
  int colorCount;
};

// Tilt zone for palette switching
struct TiltZone {
  float minAngle;
  float maxAngle;
  int paletteIndex;
};

// Manages color palettes and tilt-based switching
class PaletteManager {
private:
  // Predefined palettes
  ColorPalette palettes[PaletteConfig::PREDEFINED_PALETTE_COUNT] = {
    {"Rainbow", {0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3}, 7},
    {"Ocean", {0x001F3F, 0x0074D9, 0x7FDBFF, 0x39CCCC, 0x2ECC40, 0x01FF70, 0xFFFFFF}, 7},
    {"Fire", {0x000000, 0x8B0000, 0xFF0000, 0xFF4500, 0xFF8C00, 0xFFD700, 0xFFFFFF}, 7},
    {"Sunset", {0x2C0735, 0x6A0572, 0xAB2567, 0xDE6E4B, 0xF4A261, 0xF7DC6F, 0xFFFFFF}, 7},
    {"Forest", {0x0B3D0B, 0x0F5132, 0x228B22, 0x32CD32, 0x90EE90, 0xADFF2F, 0xFFFFE0}, 7},
    {"Neon", {0xFF00FF, 0xFF1493, 0x00FFFF, 0x00FF00, 0xFFFF00, 0xFF6600, 0xFFFFFF}, 7},
    {"Ice", {0x000033, 0x003366, 0x336699, 0x6699CC, 0x99CCFF, 0xCCE5FF, 0xFFFFFF}, 7},
    {"Lava", {0x330000, 0x660000, 0x990000, 0xCC3300, 0xFF6600, 0xFF9933, 0xFFCC00}, 7}
  };

  // Custom user palettes
  ColorPalette customPalettes[PaletteConfig::MAX_CUSTOM_PALETTES];
  int customPaletteCount = 0;

  // Tilt zones for automatic palette switching
  TiltZone tiltZones[PaletteConfig::TILT_ZONE_COUNT] = {
    {TiltConfig::ZONE_DOWN_MIN, TiltConfig::ZONE_DOWN_MAX, TiltConfig::ZONE_DOWN_PALETTE},
    {TiltConfig::ZONE_CENTER_MIN, TiltConfig::ZONE_CENTER_MAX, TiltConfig::ZONE_CENTER_PALETTE},
    {TiltConfig::ZONE_UP_MIN, TiltConfig::ZONE_UP_MAX, TiltConfig::ZONE_UP_PALETTE}
  };

  // State
  int currentPaletteIndex = 0;
  bool useTiltPalettes = false;
  bool randomPaletteMode = false;
  unsigned long lastRandomChange = 0;

  // Custom LED color overrides
  uint32_t customLEDColors[HardwareConfig::NUM_LEDS] = {0};
  bool useCustomColors = false;

public:
  PaletteManager() {}

  // Get current palette
  ColorPalette* getCurrentPalette() {
    int totalCount = PaletteConfig::PREDEFINED_PALETTE_COUNT + customPaletteCount;

    if (currentPaletteIndex < PaletteConfig::PREDEFINED_PALETTE_COUNT) {
      return &palettes[currentPaletteIndex];
    } else if (currentPaletteIndex < totalCount) {
      return &customPalettes[currentPaletteIndex - PaletteConfig::PREDEFINED_PALETTE_COUNT];
    }

    // Default to rainbow
    return &palettes[0];
  }

  // Get palette by index (handles both predefined and custom)
  ColorPalette* getPalette(int index) {
    int totalCount = PaletteConfig::PREDEFINED_PALETTE_COUNT + customPaletteCount;

    if (index < 0 || index >= totalCount) {
      return &palettes[0]; // Default
    }

    if (index < PaletteConfig::PREDEFINED_PALETTE_COUNT) {
      return &palettes[index];
    } else {
      return &customPalettes[index - PaletteConfig::PREDEFINED_PALETTE_COUNT];
    }
  }

  // Cycle to next palette
  void cycleNext() {
    int totalCount = PaletteConfig::PREDEFINED_PALETTE_COUNT + customPaletteCount;
    currentPaletteIndex = (currentPaletteIndex + 1) % totalCount;

    Serial.print("🎨 Palette: ");
    Serial.println(getCurrentPalette()->name);
  }

  // Cycle to previous palette
  void cyclePrevious() {
    int totalCount = PaletteConfig::PREDEFINED_PALETTE_COUNT + customPaletteCount;
    currentPaletteIndex--;
    if (currentPaletteIndex < 0) {
      currentPaletteIndex = totalCount - 1;
    }

    Serial.print("🎨 Palette: ");
    Serial.println(getCurrentPalette()->name);
  }

  // Set palette by index
  void setPalette(int index) {
    int totalCount = PaletteConfig::PREDEFINED_PALETTE_COUNT + customPaletteCount;
    if (index >= 0 && index < totalCount) {
      currentPaletteIndex = index;
      Serial.print("🎨 Palette set to: ");
      Serial.println(getCurrentPalette()->name);
    }
  }

  // Get palette index for tilt angle
  int getPaletteIndexForTilt(float tiltAngle) {
    if (!useTiltPalettes) {
      return currentPaletteIndex;
    }

    for (int i = 0; i < PaletteConfig::TILT_ZONE_COUNT; i++) {
      if (tiltAngle >= tiltZones[i].minAngle && tiltAngle <= tiltZones[i].maxAngle) {
        return tiltZones[i].paletteIndex;
      }
    }

    return currentPaletteIndex;
  }

  // Add custom palette
  bool addCustomPalette(String name, uint32_t colors[], int count) {
    if (customPaletteCount >= PaletteConfig::MAX_CUSTOM_PALETTES) {
      Serial.println("❌ Max custom palettes reached");
      return false;
    }

    customPalettes[customPaletteCount].name = name;
    customPalettes[customPaletteCount].colorCount = min(count, 7);

    for (int i = 0; i < customPalettes[customPaletteCount].colorCount; i++) {
      customPalettes[customPaletteCount].colors[i] = colors[i];
    }

    customPaletteCount++;
    Serial.print("✅ Added custom palette: ");
    Serial.println(name);
    return true;
  }

  // Custom LED color overrides
  void setCustomLEDColor(int led, uint32_t color) {
    if (led >= 0 && led < HardwareConfig::NUM_LEDS) {
      customLEDColors[led] = color;
      useCustomColors = true;
    }
  }

  void clearCustomColors() {
    useCustomColors = false;
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      customLEDColors[i] = 0;
    }
  }

  bool hasCustomColors() const { return useCustomColors; }
  uint32_t getCustomLEDColor(int led) const {
    if (led >= 0 && led < HardwareConfig::NUM_LEDS) {
      return customLEDColors[led];
    }
    return 0;
  }

  // Tilt palette control
  void setUseTiltPalettes(bool enable) { useTiltPalettes = enable; }
  bool getUseTiltPalettes() const { return useTiltPalettes; }

  // Random palette mode
  void setRandomPaletteMode(bool enable) { randomPaletteMode = enable; }
  bool getRandomPaletteMode() const { return randomPaletteMode; }

  void updateRandomPalette(unsigned long currentTime) {
    if (randomPaletteMode && (currentTime - lastRandomChange > 30000)) {
      int totalCount = PaletteConfig::PREDEFINED_PALETTE_COUNT + customPaletteCount;
      currentPaletteIndex = random(totalCount);
      lastRandomChange = currentTime;

      Serial.print("🎲 Random palette: ");
      Serial.println(getCurrentPalette()->name);
    }
  }

  // Getters
  int getCurrentPaletteIndex() const { return currentPaletteIndex; }
  int getTotalPaletteCount() const {
    return PaletteConfig::PREDEFINED_PALETTE_COUNT + customPaletteCount;
  }
  int getCustomPaletteCount() const { return customPaletteCount; }
};

#endif // PALETTE_MANAGER_H
