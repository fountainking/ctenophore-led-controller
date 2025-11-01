/**
 * Ctenophore LED Controller - Refactored v2.0
 *
 * ESP32-C3 based NeoPixel controller with modular architecture
 *
 * Features:
 * - 7-LED liquid physics simulation with tilt sensing
 * - Tempo detection from taps (3 taps = prediction, 4+ = continuous adjustment)
 * - Rotation-based gesture controls (barrel rolls & spins)
 * - 8 color palettes + 10 custom palette slots
 * - 6 animation patterns
 * - WiFi hotspot web dashboard
 * - Battery monitoring
 * - Stride/cadence tracking ready
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <esp_pm.h>

// Modular components
#include "config/Constants.h"
#include "hardware/MPUSensor.h"
#include "hardware/LEDController.h"
#include "hardware/BatteryMonitor.h"
#include "motion/GestureDetector.h"
#include "effects/PaletteManager.h"
#include "effects/AnimationEngine.h"
#include "tempo/TempoDetector.h"
#include "tempo/BeatSynchronizer.h"
#include "control/DeviceMode.h"
#include "control/CommandParser.h"
#include "control/WiFiServer.h"
#include "control/DashboardHTML.h"

// ===== GLOBAL HARDWARE =====
Adafruit_NeoPixel strip(HardwareConfig::NUM_LEDS, HardwareConfig::LED_PIN, NEO_GRB + NEO_KHZ800);

// ===== MODULE INSTANCES =====
MPUSensor mpu;
LEDController leds(&strip);
BatteryMonitor battery;
GestureDetector gestures;
PaletteManager palettes;
AnimationEngine animations(&leds, &palettes);
TempoDetector tempo;
BeatSynchronizer beatSync;
ModeController mode;
CommandParser cmdParser;
CtenophoreWiFiServer wifiServer(
  WiFiConfig::SSID,
  WiFiConfig::PASSWORD,
  dashboard_html
);

// ===== FUNCTION DECLARATIONS =====
void setupCommands();
void setupGestures();
void handleTap();
void stopTempo();

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(2000);

  // Power management - disable auto-sleep
  esp_pm_config_t pm_config;
  pm_config.max_freq_mhz = 160;
  pm_config.min_freq_mhz = 10;
  pm_config.light_sleep_enable = false;
  esp_pm_configure(&pm_config);
  WiFi.setSleep(false);

  Serial.println("🌊✨ CTENOPHORE v2.0 - REFACTORED ✨🌊");
  Serial.println("Modular architecture with clean separation of concerns");
  Serial.println("");

  // Initialize hardware
  pinMode(HardwareConfig::BATTERY_PIN, INPUT);

  strip.begin();
  strip.setBrightness(255);
  strip.clear();
  strip.show();

  // Initialize MPU-6050
  if (mpu.begin()) {
    Serial.println("✅ MPU-6050 initialized");
  } else {
    Serial.println("⚠️ MPU-6050 not found - continuing without motion");
  }

  // Initialize battery monitor
  // (battery monitor auto-initializes with global pin, no begin() needed)

  // Setup WiFi and web server
  if (wifiServer.begin()) {
    Serial.println("✅ WiFi hotspot ready");
    Serial.print("   Connect to: ");
    Serial.println(WiFiConfig::SSID);
    Serial.print("   Password: ");
    Serial.println(WiFiConfig::PASSWORD);
    Serial.print("   Dashboard: http://");
    Serial.println(wifiServer.getIP());
  }

  // Register command handlers
  setupCommands();

  // Setup gesture callbacks
  setupGestures();

  // Setup beat callback
  beatSync.setOnBeat([]() {
    Serial.print("🎵 Beat ");
    Serial.print(tempo.getTapCount());
    Serial.print(" @ ");
    Serial.print(tempo.getBPM());
    Serial.println(" BPM");

    // Trigger strobe on beat
    animations.triggerStrobe();
    mode.recordActivity();
  });

  // Start in liquid mode
  mode.transitionTo(DeviceMode::LIQUID_IDLE);

  Serial.println("");
  Serial.println("🎨 Features:");
  Serial.println("  💧 Liquid tilt physics");
  Serial.println("  👟 Tap-to-tempo (stride tracking ready!)");
  Serial.println("  🔄 Rotation gestures (flip/spin)");
  Serial.println("  🌈 8 palettes + 10 custom slots");
  Serial.println("  ✨ 6 animation patterns");
  Serial.println("  📱 WiFi web dashboard");
  Serial.println("  🔋 Battery monitoring");
  Serial.println("");
  Serial.println("🪄 Ready! Tilt for liquid, tap for tempo!");
}

// ===== MAIN LOOP =====
void loop() {
  unsigned long currentTime = millis();
  static unsigned long lastMPURead = 0;

  // Read sensors (100Hz)
  if (currentTime - lastMPURead >= 10) {
    mpu.read();
    lastMPURead = currentTime;
  }

  // Update gesture detection
  gestures.update(mpu, currentTime);

  // Update battery monitor
  battery.update();

  // Process serial commands
  cmdParser.processSerial();

  // Update beat synchronization
  beatSync.update(currentTime);

  // Update mode timeout
  mode.update(currentTime);

  // Mode-specific updates
  switch (mode.getMode()) {
    case DeviceMode::LIQUID_IDLE:
    case DeviceMode::LIQUID_TILTING:
      // Liquid physics based on tilt
      animations.updateLiquidPhysics(mpu.getTiltAngle(), gestures.getIsMoving());
      animations.render(mpu.getTiltAngle());

      // Check for mode transition
      if (gestures.getIsMoving()) {
        mode.transitionTo(DeviceMode::LIQUID_TILTING);
        mode.recordActivity();
      } else if (mode.getMode() == DeviceMode::LIQUID_TILTING) {
        mode.transitionTo(DeviceMode::LIQUID_IDLE);
      }
      break;

    case DeviceMode::TEMPO_DETECTING:
    case DeviceMode::TEMPO_PLAYING:
      // Tempo mode - update animations
      animations.update();
      animations.render(mpu.getTiltAngle());

      // Auto-return to liquid after timeout
      if (mode.getTimeInMode() > TempoConfig::TEMPO_MODE_TIMEOUT_MS) {
        Serial.println("⏰ Tempo mode timeout - returning to liquid");
        stopTempo();
      }
      break;

    case DeviceMode::BATTERY_DISPLAY:
      // Show battery level
      battery.displayOnLEDs(leds);

      // Return to liquid after 5 seconds
      if (mode.getTimeInMode() > 5000) {
        mode.transitionTo(DeviceMode::LIQUID_IDLE);
      }
      break;

    case DeviceMode::ROTATION_EFFECT:
      // Rotation sparkle effect
      animations.render(mpu.getTiltAngle());

      // Return to previous mode after effect
      if (mode.getTimeInMode() > 1000) {
        mode.transitionToPrevious();
      }
      break;
  }

  // Apply final LED output
  strip.show();
}

// ===== COMMAND SETUP =====
void setupCommands() {
  static Command commands[] = {
    {"tap", [](String) {
      handleTap();
    }},
    {"reset", [](String) {
      stopTempo();
      Serial.println("🔄 Reset to liquid mode");
    }},
    {"battery", [](String) {
      mode.transitionTo(DeviceMode::BATTERY_DISPLAY);
    }},
    {"threshold", [](String value) {
      float threshold;
      if (CommandParser::parseFloat(value, threshold, 0.01f, 1.0f)) {
        gestures.setTapThreshold(threshold);
        Serial.print("🎛️ Tap threshold: ");
        Serial.println(threshold);
      }
    }},
    {"brightness", [](String value) {
      float brightness;
      if (CommandParser::parseFloat(value, brightness, 0.1f, 1.0f)) {
        leds.setBrightness(brightness);
        Serial.print("☀️ Brightness: ");
        Serial.println(brightness);
      }
    }},
    {"palette", [](String value) {
      int index;
      if (CommandParser::parseInt(value, index, 0, 17)) {
        palettes.setCurrentPalette(index);
        Serial.print("🎨 Palette: ");
        Serial.println(index);
      }
    }},
    {"pattern", [](String value) {
      // Map pattern name to index
      AnimationPattern pattern = AnimationPattern::PATTERN_RAINBOW_CYCLE;
      if (value == "breathing") pattern = AnimationPattern::PATTERN_BREATHING;
      else if (value == "chase") pattern = AnimationPattern::PATTERN_CHASE;
      else if (value == "sparkle") pattern = AnimationPattern::PATTERN_SPARKLE;
      else if (value == "strobe") pattern = AnimationPattern::PATTERN_STROBE;
      else if (value == "fade") pattern = AnimationPattern::PATTERN_FADE;

      animations.setPattern(pattern);
      Serial.print("✨ Pattern: ");
      Serial.println(value);
    }},
    {"bpm", [](String value) {
      int bpm;
      if (CommandParser::parseInt(value, bpm, TempoConfig::MIN_BPM, TempoConfig::MAX_BPM)) {
        unsigned long interval = 60000 / bpm;
        beatSync.start(interval, millis());
        mode.transitionTo(DeviceMode::TEMPO_PLAYING);
        Serial.print("🎵 Manual BPM: ");
        Serial.println(bpm);
      }
    }},
    {"help", [](String) {
      Serial.println("📋 Commands:");
      Serial.println("  tap              - Simulate tap");
      Serial.println("  reset            - Return to liquid mode");
      Serial.println("  battery          - Show battery level");
      Serial.println("  threshold=0.4    - Set tap sensitivity");
      Serial.println("  brightness=0.6   - Set LED brightness");
      Serial.println("  palette=0        - Change color palette (0-17)");
      Serial.println("  pattern=rainbow  - Change animation pattern");
      Serial.println("  bpm=120          - Set manual tempo");
      Serial.println("  help             - Show this menu");
    }}
  };

  cmdParser.registerCommands(commands, sizeof(commands) / sizeof(Command));

  // Setup WiFi callbacks
  wifiServer.setCommandCallback([](String cmd) {
    cmdParser.parse(cmd);
  });

  wifiServer.setStatusCallback([]() -> String {
    StaticJsonDocument<512> doc;

    doc["mode"] = mode.getMode() == DeviceMode::LIQUID_IDLE || mode.getMode() == DeviceMode::LIQUID_TILTING ? "liquid" : "tempo";
    doc["bpm"] = tempo.getBPM();
    doc["batteryPercent"] = battery.getPercentage();
    doc["currentPalette"] = palettes.getCurrentIndex();
    doc["currentPattern"] = (int)animations.getPattern();
    doc["tiltAngle"] = mpu.getTiltAngle();
    doc["accelY"] = mpu.getAccelY();
    doc["accelZ"] = mpu.getAccelZ();
    doc["beat"] = beatSync.getIsActive();

    // LED brightness array
    JsonArray leds_array = doc.createNestedArray("leds");
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      leds_array.add(animations.getLEDBrightness(i));
    }

    String output;
    serializeJson(doc, output);
    return output;
  });
}

// ===== GESTURE SETUP =====
void setupGestures() {
  // Tap callback - EVERY tap triggers visual feedback (wearable-ready!)
  gestures.setOnTap([]() {
    handleTap();
  });

  // Barrel roll callback (X-axis) - cycle animations
  gestures.setOnXRotation([](bool clockwise) {
    Serial.println("🔄 Barrel roll detected!");
    animations.cyclePattern(clockwise);
    mode.transitionTo(DeviceMode::ROTATION_EFFECT);
    mode.recordActivity();
  });

  // Spin callback (Z-axis) - cycle palettes
  gestures.setOnZRotation([](bool clockwise) {
    Serial.println("🌀 Spin detected!");
    palettes.cycleNext(clockwise);
    mode.transitionTo(DeviceMode::ROTATION_EFFECT);
    mode.recordActivity();
  });

  // Motion callback
  gestures.setOnMotionChange([](bool moving) {
    if (moving) {
      mode.recordActivity();
    }
  });
}

// ===== TAP HANDLER =====
void handleTap() {
  unsigned long currentTime = millis();

  // Add tap to tempo detector
  tempo.addTap(currentTime);

  // ALWAYS trigger visual feedback (stride tracking!)
  animations.triggerStrobe();
  mode.recordActivity();

  Serial.print("👟 Tap ");
  Serial.print(tempo.getTapCount());
  Serial.print(" | ");

  // Tap 3+: Start/adjust tempo
  if (tempo.hasEnoughTaps()) {
    // Start tempo mode if not already active
    if (!mode.isInTempoMode()) {
      mode.transitionTo(DeviceMode::TEMPO_DETECTING);
      Serial.println("🎵 Tempo mode activated!");
    }

    // Get updated tempo
    unsigned long interval = tempo.getInterval();
    int bpm = tempo.getBPM();

    // Start or resync beat synchronization
    if (!beatSync.getIsActive()) {
      // First time - start beats
      beatSync.start(interval, currentTime);
      mode.transitionTo(DeviceMode::TEMPO_PLAYING);
      Serial.print("🎯 Tempo locked: ");
      Serial.print(bpm);
      Serial.println(" BPM");
    } else {
      // Already playing - resync to this tap
      beatSync.resyncToTap(currentTime, interval);
      Serial.print("⚡ Resynced: ");
      Serial.print(bpm);
      Serial.println(" BPM");
    }
  } else {
    Serial.println("Waiting for 3rd tap...");
  }
}

// ===== TEMPO STOP =====
void stopTempo() {
  tempo.reset();
  beatSync.stop();
  animations.stopStrobe();
  mode.transitionTo(DeviceMode::LIQUID_IDLE);
  Serial.println("⏹️ Tempo stopped");
}
