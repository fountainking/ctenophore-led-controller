#ifndef MPU_SENSOR_H
#define MPU_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "../config/Constants.h"

// MPU-6050 Accelerometer and Gyroscope sensor wrapper
class MPUSensor {
private:
  byte address;
  bool available;

public:
  // Sensor data (public for easy access)
  float accelX = 0;
  float accelY = 0;
  float accelZ = 0;
  float gyroX = 0;
  float gyroZ = 0;
  float tiltAngle = 0;  // Normalized -1.0 (down) to 1.0 (up)

  MPUSensor() : address(MPUConfig::MPU_ADDRESS), available(false) {}

  // Initialize MPU-6050 sensor
  bool begin() {
    Wire.begin();
    delay(MPUConfig::INIT_DELAY_MS);
    Serial.println("🔍 Connecting to MPU-6050...");

    // Test I2C connection
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.println("✅ MPU-6050 found and ready!");
      available = true;

      // Wake up MPU-6050 (register 0x6B, value 0x00)
      Wire.beginTransmission(address);
      Wire.write(0x6B);
      Wire.write(0x00);
      Wire.endTransmission();

      // Set accelerometer range to ±2g (register 0x1C, value 0x00)
      Wire.beginTransmission(address);
      Wire.write(0x1C);
      Wire.write(0x00);
      Wire.endTransmission();

      return true;
    } else {
      Serial.println("❌ MPU-6050 not responding - will use static mode");
      available = false;
      return false;
    }
  }

  // Check if sensor is available
  bool isAvailable() const {
    return available;
  }

  // Read all sensor data
  void read() {
    if (!available) return;

    Wire.beginTransmission(address);
    Wire.write(0x3B);  // Starting register for accel data
    Wire.endTransmission(false);
    Wire.requestFrom(address, (byte)14);  // Read accel + temp + gyro

    if (Wire.available() >= 14) {
      // Read accelerometer (6 bytes)
      int16_t rawX = Wire.read() << 8 | Wire.read();
      int16_t rawY = Wire.read() << 8 | Wire.read();
      int16_t rawZ = Wire.read() << 8 | Wire.read();

      // Skip temperature (2 bytes)
      Wire.read();
      Wire.read();

      // Read gyroscope (6 bytes)
      int16_t rawGX = Wire.read() << 8 | Wire.read();
      Wire.read(); Wire.read();  // Skip gyroY
      int16_t rawGZ = Wire.read() << 8 | Wire.read();

      // Convert to g's (±2g range = 16384 LSB/g)
      accelX = rawX / 16384.0;
      accelY = rawY / 16384.0;
      accelZ = rawZ / 16384.0;

      // Convert to degrees/second (±250°/s range = 131 LSB/°/s)
      gyroX = rawGX / 131.0;
      gyroZ = rawGZ / 131.0;

      // Calculate tilt angle (normalized to -1.0 to 1.0)
      // Using X-axis acceleration (assumes upright orientation)
      tiltAngle = constrain(accelX, -1.0, 1.0);
    }
  }

  // Get total acceleration magnitude
  float getAccelMagnitude() const {
    return sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
  }

  // Get motion delta (for movement detection)
  float getMotionDelta(float lastMagnitude) const {
    return abs(getAccelMagnitude() - lastMagnitude);
  }

  // Check if rotating on X-axis (barrel roll detection)
  bool isRotatingX() const {
    return abs(gyroX) > RotationConfig::GYRO_THRESHOLD;
  }

  // Check if rotating on Z-axis (spin detection)
  bool isRotatingZ() const {
    return abs(gyroZ) > RotationConfig::GYRO_THRESHOLD;
  }

  // Print sensor data for debugging
  void printData() const {
    Serial.print("Accel: X=");
    Serial.print(accelX, 2);
    Serial.print(" Y=");
    Serial.print(accelY, 2);
    Serial.print(" Z=");
    Serial.print(accelZ, 2);
    Serial.print(" | Gyro: X=");
    Serial.print(gyroX, 1);
    Serial.print(" Z=");
    Serial.print(gyroZ, 1);
    Serial.print(" | Tilt=");
    Serial.println(tiltAngle, 2);
  }
};

#endif // MPU_SENSOR_H
