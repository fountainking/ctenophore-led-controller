#ifndef ANIMATION_ENGINE_H
#define ANIMATION_ENGINE_H

#include <Arduino.h>
#include "../config/Constants.h"
#include "../hardware/LEDController.h"
#include "PaletteManager.h"

// Animation pattern types
enum AnimationPattern {
  PATTERN_RAINBOW_CYCLE,
  PATTERN_BREATHING,
  PATTERN_CHASE,
  PATTERN_SPARKLE,
  PATTERN_STROBE,
  PATTERN_FADE,
  PATTERN_CUSTOM
};

// Animation engine handles all visual effects
class AnimationEngine {
private:
  // Current pattern
  AnimationPattern currentPattern = PATTERN_RAINBOW_CYCLE;

  // LED brightness levels (0.0 - 1.0)
  float liquidLevels[HardwareConfig::NUM_LEDS];
  float targetLevels[HardwareConfig::NUM_LEDS];

  // Animation state
  float breathPhase = 0;
  float globalHueShift = 0;
  int chasePosition = 0;
  bool chaseDirection = true;

  // Sparkle state
  bool sparkleStates[HardwareConfig::NUM_LEDS];
  unsigned long sparkleTimers[HardwareConfig::NUM_LEDS];
  unsigned long lastIdleSparkle = 0;

  // Timing
  unsigned long lastAnimationUpdate = 0;

  // References
  LEDController* leds;
  PaletteManager* palettes;

  // Tempo-reactive coloring
  bool tempoColorReactive = false;
  float temperatureShift = 0;  // -1.0 (cool) to +1.0 (warm)

public:
  AnimationEngine(LEDController* ledController, PaletteManager* paletteManager)
    : leds(ledController), palettes(paletteManager) {
    // Initialize levels
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      liquidLevels[i] = 1.0;
      targetLevels[i] = 1.0;
      sparkleStates[i] = false;
      sparkleTimers[i] = 0;
    }
  }

  // Update animations (call every frame)
  void update() {
    unsigned long currentTime = millis();

    // Rate limit animation updates
    if (currentTime - lastAnimationUpdate < EffectsConfig::ANIMATION_INTERVAL_MS) {
      return;
    }

    lastAnimationUpdate = currentTime;

    // Update pattern-specific animation
    switch (currentPattern) {
      case PATTERN_BREATHING:
        updateBreathingEffect();
        break;
      case PATTERN_CHASE:
        updateChaseEffect();
        break;
      case PATTERN_SPARKLE:
        updateSparkleEffect();
        break;
      case PATTERN_FADE:
        updateFadeEffect();
        break;
      case PATTERN_STROBE:
        // Strobe handled in tempo system
        break;
      case PATTERN_RAINBOW_CYCLE:
      default:
        // Default rainbow behavior
        globalHueShift += 0.5;
        if (globalHueShift >= 360) globalHueShift -= 360;
        break;
    }
  }

  // Render LEDs with current palette and levels
  void render(float tiltAngle = 0) {
    // Get palette (possibly tilt-based)
    int paletteIndex = palettes->getPaletteIndexForTilt(tiltAngle);
    ColorPalette* palette = palettes->getPalette(paletteIndex);

    if (!palette) return;

    // Apply colors to LEDs
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      uint32_t color;

      // Check for custom LED override
      if (palettes->hasCustomColors()) {
        uint32_t customColor = palettes->getCustomLEDColor(i);
        if (customColor != 0) {
          color = customColor;
        } else {
          color = getColorFromPalette(palette, i);
        }
      } else {
        color = getColorFromPalette(palette, i);
      }

      // Apply tempo-reactive color temperature
      if (tempoColorReactive) {
        color = LEDController::adjustColorTemperature(color, temperatureShift);
      }

      // Apply brightness level
      uint8_t r = (color >> 16) & 0xFF;
      uint8_t g = (color >> 8) & 0xFF;
      uint8_t b = color & 0xFF;

      float level = liquidLevels[i];
      r = r * level;
      g = g * level;
      b = b * level;

      leds->setColorRGB(i, r, g, b);
    }

    leds->show();
  }

  // Pattern-specific update functions
  void updateBreathingEffect() {
    breathPhase += 0.05;
    float pulse = 0.3 + 0.7 * (sin(breathPhase) + 1) / 2;

    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      liquidLevels[i] = liquidLevels[i] * pulse;
    }
  }

  void updateChaseEffect() {
    // Clear all LEDs
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      liquidLevels[i] = EffectsConfig::DIM_BRIGHTNESS;
    }

    // Set chase LED
    liquidLevels[chasePosition] = EffectsConfig::MAX_BRIGHTNESS;

    // Move chase position
    if (chaseDirection) {
      chasePosition++;
      if (chasePosition >= HardwareConfig::NUM_LEDS) {
        chasePosition = HardwareConfig::NUM_LEDS - 1;
        chaseDirection = false;
      }
    } else {
      chasePosition--;
      if (chasePosition < 0) {
        chasePosition = 0;
        chaseDirection = true;
      }
    }
  }

  void updateSparkleEffect() {
    // Randomly trigger sparkles
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      if (!sparkleStates[i] && random(100) < 5) { // 5% chance per frame
        sparkleStates[i] = true;
        sparkleTimers[i] = millis();
        liquidLevels[i] = EffectsConfig::MAX_BRIGHTNESS;
      }

      // Fade out sparkles
      if (sparkleStates[i] && millis() - sparkleTimers[i] > 500) {
        sparkleStates[i] = false;
        liquidLevels[i] = EffectsConfig::DIM_BRIGHTNESS;
      }
    }
  }

  void updateFadeEffect() {
    static float fadePhase = 0;
    fadePhase += 0.02;

    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      float phase = fadePhase + (i * 0.3);
      liquidLevels[i] = 0.2 + 0.8 * (sin(phase) + 1) / 2;
    }
  }

  // Liquid physics simulation
  void updateLiquidPhysics(float tiltAngle, bool isActive) {
    if (!isActive) return;

    // Clear all LEDs
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      targetLevels[i] = EffectsConfig::DIM_BRIGHTNESS;
    }

    // Tilt-based liquid simulation
    if (abs(tiltAngle) < 0.15) {
      // Centered - all LEDs mid-level
      targetLevels[3] = EffectsConfig::MAX_BRIGHTNESS;
    } else {
      // Tilted - liquid flows to one side
      float normalizedTilt = constrain(tiltAngle, -1.0, 1.0);
      float ledPosition = (normalizedTilt + 1.0) / 2.0 * (HardwareConfig::NUM_LEDS - 1);

      for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        float distance = abs(i - ledPosition);
        if (distance < 1.5) {
          targetLevels[i] = EffectsConfig::MAX_BRIGHTNESS * (1.5 - distance) / 1.5;
        }
      }
    }

    // Smooth transition to target levels
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      liquidLevels[i] += (targetLevels[i] - liquidLevels[i]) * 0.15;
    }
  }

  // Ripple effect (for tap feedback)
  void doRippleEffect(float& wavePosition) {
    wavePosition += EffectsConfig::WAVE_SPEED;

    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      float distance = abs(i - wavePosition);

      if (distance <= EffectsConfig::TRAIL_LENGTH) {
        float rippleBrightness = cos(distance * PI / (EffectsConfig::TRAIL_LENGTH * 2)) * EffectsConfig::MAX_BRIGHTNESS;
        if (rippleBrightness < 0) rippleBrightness = 0;
        liquidLevels[i] = max(liquidLevels[i], rippleBrightness);
      }

      if (distance > EffectsConfig::TRAIL_LENGTH) {
        liquidLevels[i] *= 0.85;
        if (liquidLevels[i] < EffectsConfig::DIM_BRIGHTNESS) {
          liquidLevels[i] = EffectsConfig::DIM_BRIGHTNESS;
        }
      }
    }

    globalHueShift += 1.5;
    if (globalHueShift >= 360) globalHueShift -= 360;
  }

  // Rotation sparkle effect
  void triggerRotationSparkle() {
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      sparkleStates[i] = true;
      sparkleTimers[i] = millis();
      liquidLevels[i] = EffectsConfig::MAX_BRIGHTNESS;
    }
    Serial.println("✨ Rotation sparkle!");
  }

  // Pattern control
  void setPattern(AnimationPattern pattern) {
    currentPattern = pattern;
    Serial.print("🎨 Animation: ");
    Serial.println((int)pattern);
  }

  void cyclePattern() {
    currentPattern = (AnimationPattern)(((int)currentPattern + 1) % 7);
    Serial.print("🎨 Animation: ");
    Serial.println((int)currentPattern);
  }

  AnimationPattern getPattern() const { return currentPattern; }

  // Level access (for external manipulation)
  void setLevel(int led, float level) {
    if (led >= 0 && led < HardwareConfig::NUM_LEDS) {
      liquidLevels[led] = constrain(level, 0.0, 1.0);
    }
  }

  float getLevel(int led) const {
    if (led >= 0 && led < HardwareConfig::NUM_LEDS) {
      return liquidLevels[led];
    }
    return 0;
  }

  void setAllLevels(float level) {
    level = constrain(level, 0.0, 1.0);
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      liquidLevels[i] = level;
    }
  }

  // Tempo-reactive coloring
  void setTempoColorReactive(bool enable) { tempoColorReactive = enable; }
  void setTemperatureShift(float temp) { temperatureShift = constrain(temp, -1.0, 1.0); }
  bool isTempoColorReactive() const { return tempoColorReactive; }
  float getTemperatureShift() const { return temperatureShift; }

  // Global hue (for rainbow effects)
  float getGlobalHue() const { return globalHueShift; }
  void setGlobalHue(float hue) { globalHueShift = fmod(hue, 360.0); }

private:
  // Get color from palette for specific LED
  uint32_t getColorFromPalette(ColorPalette* palette, int ledIndex) {
    if (!palette || palette->colorCount == 0) {
      return 0xFFFFFF; // White default
    }

    // Rainbow pattern uses hue shift
    if (currentPattern == PATTERN_RAINBOW_CYCLE) {
      float hue = (globalHueShift + (ledIndex * 360.0 / HardwareConfig::NUM_LEDS)) / 360.0;
      hue = fmod(hue, 1.0);
      return hsvToRgb(hue, 1.0, 1.0);
    }

    // Map LED to palette color
    int colorIndex = (ledIndex * palette->colorCount) / HardwareConfig::NUM_LEDS;
    return palette->colors[colorIndex];
  }

  // HSV to RGB conversion
  uint32_t hsvToRgb(float h, float s, float v) {
    h = constrain(h, 0.0, 1.0);
    s = constrain(s, 0.0, 1.0);
    v = constrain(v, 0.0, 1.0);

    float hue = h * 6.0;
    float c = v * s;
    float x = c * (1.0 - abs(fmod(hue, 2.0) - 1.0));
    float m = v - c;

    float r, g, b;
    if (hue < 1.0)      { r = c; g = x; b = 0; }
    else if (hue < 2.0) { r = x; g = c; b = 0; }
    else if (hue < 3.0) { r = 0; g = c; b = x; }
    else if (hue < 4.0) { r = 0; g = x; b = c; }
    else if (hue < 5.0) { r = x; g = 0; b = c; }
    else                { r = c; g = 0; b = x; }

    uint8_t rByte = (r + m) * 255;
    uint8_t gByte = (g + m) * 255;
    uint8_t bByte = (b + m) * 255;

    return ((uint32_t)rByte << 16) | ((uint32_t)gByte << 8) | bByte;
  }
};

#endif // ANIMATION_ENGINE_H
