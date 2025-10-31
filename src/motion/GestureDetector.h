#ifndef GESTURE_DETECTOR_H
#define GESTURE_DETECTOR_H

#include <Arduino.h>
#include <functional>
#include "../hardware/MPUSensor.h"
#include "../config/Constants.h"

// Rotation detector for barrel rolls and spins
class RotationDetector {
private:
  float cumulativeRotation;
  unsigned long lastRotationTime;
  bool hasTriggered;
  std::function<void()> onTrigger;
  const char* name;

public:
  RotationDetector(const char* detectorName, std::function<void()> callback)
    : cumulativeRotation(0),
      lastRotationTime(0),
      hasTriggered(false),
      onTrigger(callback),
      name(detectorName) {}

  // Update with gyro value
  void update(float gyroValue, unsigned long currentTime) {
    // Accumulate rotation if spinning fast enough
    if (abs(gyroValue) > RotationConfig::GYRO_THRESHOLD) {
      cumulativeRotation += gyroValue * RotationConfig::GYRO_SCALE_FACTOR;
      lastRotationTime = currentTime;

      // Check if rotation threshold reached
      if (abs(cumulativeRotation) >= RotationConfig::TRIGGER_DEGREES) {
        if (!hasTriggered) {
          hasTriggered = true;

          Serial.print("🌀 ");
          Serial.print(name);
          Serial.println(" DETECTED!");

          if (onTrigger) {
            onTrigger();
          }
        }
      }
    }

    // Reset trigger if rotation drops below reset threshold
    if (abs(cumulativeRotation) < RotationConfig::RESET_DEGREES ||
        (currentTime - lastRotationTime > RotationConfig::ROTATION_TIMEOUT_MS)) {
      hasTriggered = false;
    }

    // Full reset if no rotation for extended period
    if (currentTime - lastRotationTime > RotationConfig::FULL_RESET_MS) {
      cumulativeRotation = 0;
    }
  }

  // Manual reset
  void reset() {
    cumulativeRotation = 0;
    hasTriggered = false;
    lastRotationTime = millis();
  }

  // Get current rotation
  float getRotation() const {
    return cumulativeRotation;
  }
};

// Gesture detector for taps, motion, and rotations
class GestureDetector {
private:
  // Tap detection state
  float tapHistory[MotionConfig::TAP_HISTORY_SIZE];
  unsigned long lastTapTime;
  std::function<void()> onTap;

  // Motion detection state
  float lastAccelMagnitude;
  unsigned long lastMotionTime;
  unsigned long lastShakeTime;
  bool isMoving;
  bool isShaking;

  // Rotation detectors
  RotationDetector* xRotationDetector;
  RotationDetector* zRotationDetector;

public:
  GestureDetector()
    : lastTapTime(0),
      lastAccelMagnitude(1.0),
      lastMotionTime(0),
      lastShakeTime(0),
      isMoving(false),
      isShaking(false),
      xRotationDetector(nullptr),
      zRotationDetector(nullptr) {
    // Initialize tap history
    for (int i = 0; i < MotionConfig::TAP_HISTORY_SIZE; i++) {
      tapHistory[i] = 1.0;
    }
  }

  // Set tap callback
  void setTapCallback(std::function<void()> callback) {
    onTap = callback;
  }

  // Set rotation detectors
  void setXRotationDetector(RotationDetector* detector) {
    xRotationDetector = detector;
  }

  void setZRotationDetector(RotationDetector* detector) {
    zRotationDetector = detector;
  }

  // Update gesture detection with latest sensor data
  void update(MPUSensor& mpu) {
    unsigned long currentTime = millis();

    // Update tap detection
    checkTap(mpu, currentTime);

    // Update motion detection
    checkMotion(mpu, currentTime);

    // Update rotation detection
    if (xRotationDetector) {
      xRotationDetector->update(mpu.gyroX, currentTime);
    }
    if (zRotationDetector) {
      zRotationDetector->update(mpu.gyroZ, currentTime);
    }
  }

  // Check for tap gesture
  void checkTap(MPUSensor& mpu, unsigned long currentTime) {
    float totalAccel = mpu.getAccelMagnitude();

    // Shift tap history
    for (int i = 0; i < MotionConfig::TAP_HISTORY_SIZE - 1; i++) {
      tapHistory[i] = tapHistory[i + 1];
    }
    tapHistory[MotionConfig::TAP_HISTORY_SIZE - 1] = totalAccel;

    // Calculate average baseline
    float avgAccel = 0;
    for (int i = 0; i < MotionConfig::TAP_HISTORY_SIZE; i++) {
      avgAccel += tapHistory[i];
    }
    avgAccel /= MotionConfig::TAP_HISTORY_SIZE;

    // Detect spike above baseline
    float spikeAboveBaseline = totalAccel - avgAccel;

    if (spikeAboveBaseline > MotionConfig::TAP_THRESHOLD &&
        (currentTime - lastTapTime) > MotionConfig::TAP_DEBOUNCE_MS) {

      Serial.print("👆 TAP! Spike: ");
      Serial.print(spikeAboveBaseline, 2);
      Serial.print(" | Total: ");
      Serial.println(totalAccel, 2);

      lastTapTime = currentTime;

      if (onTap) {
        onTap();
      }
    }
  }

  // Check for general movement
  void checkMotion(MPUSensor& mpu, unsigned long currentTime) {
    float currentMagnitude = mpu.getAccelMagnitude();
    float motionDelta = abs(currentMagnitude - lastAccelMagnitude);

    // Motion detection
    if (motionDelta > MotionConfig::MOTION_THRESHOLD) {
      isMoving = true;
      lastMotionTime = currentTime;
    } else if (currentTime - lastMotionTime > MotionConfig::MOTION_TIMEOUT_MS) {
      isMoving = false;
    }

    // Shake detection (rapid motion changes)
    if (motionDelta > MotionConfig::SHAKE_THRESHOLD &&
        (currentTime - lastShakeTime) > MotionConfig::SHAKE_DEBOUNCE_MS) {
      isShaking = true;
      lastShakeTime = currentTime;
    } else if (currentTime - lastShakeTime > MotionConfig::MOTION_TIMEOUT_MS) {
      isShaking = false;
    }

    lastAccelMagnitude = currentMagnitude;
  }

  // Query motion state
  bool getIsMoving() const { return isMoving; }
  bool getIsShaking() const { return isShaking; }
  unsigned long getLastTapTime() const { return lastTapTime; }
  unsigned long getLastMotionTime() const { return lastMotionTime; }
};

#endif // GESTURE_DETECTOR_H
