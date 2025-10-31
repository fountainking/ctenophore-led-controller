#ifndef DEVICE_MODE_H
#define DEVICE_MODE_H

#include <Arduino.h>

// Device operating modes
enum class DeviceMode {
  LIQUID_IDLE,        // Tilt-based liquid physics (default state)
  LIQUID_TILTING,     // Active tilting with liquid response
  TEMPO_DETECTING,    // Collecting taps to calculate tempo
  TEMPO_PLAYING,      // Playing beats at locked tempo
  BATTERY_DISPLAY,    // Showing battery level visualization
  ROTATION_EFFECT     // Showing rotation sparkle effect
};

// Mode controller handles state transitions and timeouts
class ModeController {
private:
  DeviceMode currentMode = DeviceMode::LIQUID_IDLE;
  DeviceMode previousMode = DeviceMode::LIQUID_IDLE;
  unsigned long modeStartTime = 0;
  unsigned long lastActivityTime = 0;

public:
  ModeController() : modeStartTime(millis()), lastActivityTime(millis()) {}

  // Transition to a new mode
  void transitionTo(DeviceMode newMode) {
    if (newMode != currentMode) {
      previousMode = currentMode;
      currentMode = newMode;
      modeStartTime = millis();
      lastActivityTime = millis();

      Serial.print("🔄 Mode transition: ");
      Serial.print(getModeString(previousMode));
      Serial.print(" -> ");
      Serial.println(getModeString(currentMode));
    }
  }

  // Get current mode
  DeviceMode getCurrentMode() const {
    return currentMode;
  }

  // Get previous mode (useful for returning to previous state)
  DeviceMode getPreviousMode() const {
    return previousMode;
  }

  // Check if in any liquid mode
  bool isInLiquidMode() const {
    return currentMode == DeviceMode::LIQUID_IDLE ||
           currentMode == DeviceMode::LIQUID_TILTING;
  }

  // Check if in any tempo mode
  bool isInTempoMode() const {
    return currentMode == DeviceMode::TEMPO_DETECTING ||
           currentMode == DeviceMode::TEMPO_PLAYING;
  }

  // Check if tempo is actively playing
  bool isTempoPlaying() const {
    return currentMode == DeviceMode::TEMPO_PLAYING;
  }

  // Check if detecting tempo (collecting taps)
  bool isTempoDetecting() const {
    return currentMode == DeviceMode::TEMPO_DETECTING;
  }

  // Update activity timestamp (call when user interaction happens)
  void recordActivity() {
    lastActivityTime = millis();
  }

  // Get time in current mode (ms)
  unsigned long getTimeInMode() const {
    return millis() - modeStartTime;
  }

  // Get time since last activity (ms)
  unsigned long getTimeSinceActivity() const {
    return millis() - lastActivityTime;
  }

  // Check if should timeout and return to liquid mode
  bool shouldTimeoutToLiquid(unsigned long timeoutMs) const {
    return isInTempoMode() && (getTimeInMode() >= timeoutMs);
  }

  // Check if idle timeout reached
  bool shouldIdleTimeout(unsigned long idleTimeoutMs) const {
    return getTimeSinceActivity() >= idleTimeoutMs;
  }

  // Return to liquid mode
  void returnToLiquid() {
    transitionTo(DeviceMode::LIQUID_IDLE);
  }

  // Convert mode to human-readable string
  static const char* getModeString(DeviceMode mode) {
    switch(mode) {
      case DeviceMode::LIQUID_IDLE:      return "LIQUID_IDLE";
      case DeviceMode::LIQUID_TILTING:   return "LIQUID_TILTING";
      case DeviceMode::TEMPO_DETECTING:  return "TEMPO_DETECTING";
      case DeviceMode::TEMPO_PLAYING:    return "TEMPO_PLAYING";
      case DeviceMode::BATTERY_DISPLAY:  return "BATTERY_DISPLAY";
      case DeviceMode::ROTATION_EFFECT:  return "ROTATION_EFFECT";
      default:                           return "UNKNOWN";
    }
  }

  // Print current mode status
  void printStatus() const {
    Serial.print("🎮 Mode: ");
    Serial.print(getModeString(currentMode));
    Serial.print(" | Time in mode: ");
    Serial.print(getTimeInMode() / 1000.0, 1);
    Serial.print("s | Since activity: ");
    Serial.print(getTimeSinceActivity() / 1000.0, 1);
    Serial.println("s");
  }
};

#endif // DEVICE_MODE_H
