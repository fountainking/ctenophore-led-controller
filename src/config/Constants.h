#ifndef CONSTANTS_H
#define CONSTANTS_H

// Hardware Pin Configuration
namespace HardwareConfig {
  constexpr int LED_PIN = 10;           // D10 on Xiao ESP32-C3
  constexpr int BATTERY_PIN = A0;        // A0 for battery voltage
  constexpr int NUM_LEDS = 7;            // Total LED count
}

// MPU-6050 Sensor Configuration
namespace MPUConfig {
  constexpr byte MPU_ADDRESS = 0x68;     // I2C address
  constexpr int INIT_DELAY_MS = 100;     // Initialization delay
  constexpr int READ_INTERVAL_MS = 10;   // How often to read sensor
}

// Motion Detection Thresholds
namespace MotionConfig {
  constexpr float MOTION_THRESHOLD = 0.05f;      // General motion sensitivity (ALSO used for tap detection in original!)
  constexpr float SHAKE_THRESHOLD = 0.05f;       // Shake detection sensitivity
  constexpr float TAP_THRESHOLD = 0.05f;          // Tap detection - LOWERED to match motion threshold (original had dual detection)
  constexpr unsigned long MOTION_TIMEOUT_MS = 1500;   // Motion decay time
  constexpr unsigned long SHAKE_DEBOUNCE_MS = 200;    // Shake debounce
  constexpr unsigned long TAP_DEBOUNCE_MS = 200;      // Tap debounce
  constexpr int TAP_HISTORY_SIZE = 5;            // Smoothing window for tap detection
}

// Rotation/Gesture Detection
namespace RotationConfig {
  constexpr float TRIGGER_DEGREES = 360.0f;      // Degrees needed to trigger action (full rotation + buffer)
  constexpr float RESET_DEGREES = 90.0f;         // Degrees below which rotation resets
  constexpr unsigned long ROTATION_TIMEOUT_MS = 1000;   // Time before checking reset
  constexpr unsigned long FULL_RESET_MS = 5000;         // Time before full rotation reset
  constexpr float GYRO_THRESHOLD = 50.0f;        // Minimum gyro value to count as rotating (was 1.0 - WAY too sensitive!)
  constexpr float GYRO_SCALE_FACTOR = 0.01f;     // Gyro to degrees conversion
}

// Tempo Detection & BPM
namespace TempoConfig {
  constexpr int MIN_BPM = 30;                    // Minimum allowed BPM
  constexpr int MAX_BPM = 300;                   // Maximum allowed BPM
  constexpr int MIN_TAPS_FOR_PREDICTION = 3;     // Start predicting on 3rd tap
  constexpr int PRESS_HISTORY_SIZE = 4;          // Sliding window size
  constexpr unsigned long TEMPO_MODE_TIMEOUT_MS = 60000;  // Auto-return to liquid after 60s
  constexpr unsigned long DRIFT_CORRECTION_INTERVAL_MS = 10000; // Drift correction frequency
}

// Visual Effects & Animations
namespace EffectsConfig {
  constexpr float MAX_BRIGHTNESS = 0.6f;         // Maximum LED brightness
  constexpr float DIM_BRIGHTNESS = 0.02f;        // Minimum/dim brightness
  constexpr float WAVE_SPEED = 0.4f;             // Speed of wave effects
  constexpr float TRAIL_LENGTH = 3.0f;           // Length of trailing effects
  constexpr unsigned long STROBE_INTERVAL_MS = 20;       // Strobe flash interval
  constexpr unsigned long ANIMATION_INTERVAL_MS = 50;    // Animation update rate (20 FPS)
  constexpr unsigned long IDLE_SPARKLE_INTERVAL_MS = 3000; // Idle mode sparkle frequency
}

// Battery Monitoring
namespace BatteryConfig {
  constexpr float MIN_VOLTAGE = 3.3f;            // Minimum battery voltage
  constexpr float MAX_VOLTAGE = 4.2f;            // Maximum battery voltage (fully charged LiPo)
  constexpr float LOW_BATTERY_THRESHOLD = 3.5f;  // Low battery warning threshold
  constexpr unsigned long CHECK_INTERVAL_MS = 10000;  // Check every 10 seconds
  constexpr float VOLTAGE_SMOOTHING = 0.9f;      // Exponential smoothing factor (0.9 = smooth, 0.1 = responsive)
  constexpr float ADC_TO_VOLTAGE_FACTOR = 2.0f;  // Voltage divider scaling
}

// WiFi & Web Server
namespace WiFiConfig {
  constexpr char SSID[] = "Ctenophore-Control";
  constexpr char PASSWORD[] = "tempo123";
  constexpr int SERVER_PORT = 80;
  constexpr unsigned long STATUS_UPDATE_INTERVAL_MS = 100; // Web dashboard polling rate
}

// System Timing & Behavior
namespace SystemConfig {
  constexpr unsigned long IDLE_TIMEOUT_MS = 300000;  // Auto-return to liquid after 5 min inactivity
  constexpr unsigned long SERIAL_BAUD_RATE = 115200;
  constexpr unsigned long DEBUG_PRINT_INTERVAL_MS = 5000; // Debug output frequency
}

// Color Palette Configuration
namespace PaletteConfig {
  constexpr int PREDEFINED_PALETTE_COUNT = 8;
  constexpr int MAX_CUSTOM_PALETTES = 10;
  constexpr int COLORS_PER_PALETTE = 4;
  constexpr float TILT_TRANSITION_SMOOTHING = 0.05f;
  constexpr int TILT_ZONE_COUNT = 3;
}

// Tilt-based Palette Zones (angles for switching palettes)
namespace TiltConfig {
  constexpr float ZONE_DOWN_MIN = -1.0f;
  constexpr float ZONE_DOWN_MAX = -0.5f;
  constexpr int ZONE_DOWN_PALETTE = 1;       // Ocean palette

  constexpr float ZONE_CENTER_MIN = -0.5f;
  constexpr float ZONE_CENTER_MAX = 0.5f;
  constexpr int ZONE_CENTER_PALETTE = 0;     // Rainbow palette

  constexpr float ZONE_UP_MIN = 0.5f;
  constexpr float ZONE_UP_MAX = 1.0f;
  constexpr int ZONE_UP_PALETTE = 2;         // Fire palette
}

#endif // CONSTANTS_H
