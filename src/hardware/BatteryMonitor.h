#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>
#include "../config/Constants.h"

// Battery voltage monitoring and percentage calculation
class BatteryMonitor {
private:
  int pin;
  float voltage;
  int percentage;
  bool lowBatteryWarning;
  float smoothedVoltage;
  unsigned long lastCheckTime;

public:
  BatteryMonitor()
    : pin(HardwareConfig::BATTERY_PIN),
      voltage(0),
      percentage(100),
      lowBatteryWarning(false),
      smoothedVoltage(BatteryConfig::MAX_VOLTAGE),
      lastCheckTime(0) {}

  // Initialize battery monitoring
  void begin() {
    pinMode(pin, INPUT);
    update();  // Initial read
    Serial.println("🔋 Battery monitor initialized");
  }

  // Update battery voltage reading (call periodically)
  void update() {
    unsigned long currentTime = millis();

    // Only update at specified interval
    if (currentTime - lastCheckTime < BatteryConfig::CHECK_INTERVAL_MS) {
      return;
    }

    lastCheckTime = currentTime;

    // Read ADC value
    int adcValue = analogRead(pin);

    // Convert ADC to voltage
    // ESP32-C3 ADC: 12-bit (0-4095), reference voltage typically 3.3V
    // With voltage divider: actual voltage = measured * 2.0
    float measuredVoltage = (adcValue / 4095.0) * 3.3 * BatteryConfig::ADC_TO_VOLTAGE_FACTOR;

    // Apply exponential smoothing to reduce noise
    smoothedVoltage = smoothedVoltage * BatteryConfig::VOLTAGE_SMOOTHING +
                      measuredVoltage * (1.0 - BatteryConfig::VOLTAGE_SMOOTHING);

    voltage = smoothedVoltage;

    // Calculate percentage
    percentage = calculatePercentage(voltage);

    // Check for low battery warning
    lowBatteryWarning = (voltage < BatteryConfig::LOW_BATTERY_THRESHOLD);

    if (lowBatteryWarning) {
      Serial.println("⚠️ LOW BATTERY WARNING!");
    }
  }

  // Get current voltage
  float getVoltage() const {
    return voltage;
  }

  // Get battery percentage (0-100)
  int getPercentage() const {
    return percentage;
  }

  // Check if low battery warning
  bool isLowBattery() const {
    return lowBatteryWarning;
  }

  // Calculate battery percentage from voltage
  static int calculatePercentage(float voltage) {
    // LiPo discharge curve approximation
    // 4.2V = 100%, 3.3V = 0%

    if (voltage >= BatteryConfig::MAX_VOLTAGE) {
      return 100;
    }
    if (voltage <= BatteryConfig::MIN_VOLTAGE) {
      return 0;
    }

    // Linear interpolation (could be improved with LiPo discharge curve)
    float range = BatteryConfig::MAX_VOLTAGE - BatteryConfig::MIN_VOLTAGE;
    float normalized = (voltage - BatteryConfig::MIN_VOLTAGE) / range;

    return (int)(normalized * 100);
  }

  // Print battery status
  void printStatus() const {
    Serial.print("🔋 Battery: ");
    Serial.print(voltage, 2);
    Serial.print("V (");
    Serial.print(percentage);
    Serial.print("%)");

    if (lowBatteryWarning) {
      Serial.print(" ⚠️ LOW");
    }
    Serial.println();
  }

  // Get battery level as bar string for display
  String getBatteryBar() const {
    int bars = percentage / 20;  // 5 levels: 0-20, 21-40, 41-60, 61-80, 81-100
    String bar = "[";
    for (int i = 0; i < 5; i++) {
      if (i < bars) {
        bar += "█";
      } else {
        bar += "░";
      }
    }
    bar += "]";
    return bar;
  }
};

#endif // BATTERY_MONITOR_H
