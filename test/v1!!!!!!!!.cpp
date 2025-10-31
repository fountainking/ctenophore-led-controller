#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <math.h>

#define LED_PIN 10 // D10 on Xiao ESP32-C3
#define BATTERY_PIN A0 // A0 for battery voltage reading (top left pin)
#define NUM_LEDS 7 // Updated for 7 LEDs

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Battery monitoring variables
float batteryVoltage = 0;
int batteryPercentage = 0;
bool lowBatteryWarning = false;
unsigned long lastBatteryCheck = 0;
unsigned long batteryCheckInterval = 10000; // Check every 10 seconds
bool showingBatteryLevel = false;
// Rail mode always enabled - no toggle needed

// MPU-6050 variables
byte mpuAddress = 0x68;
bool mpuAvailable = false;
float accelX = 0, accelY = 0, accelZ = 0;
float tiltAngle = 0; // -1.0 (down) to 1.0 (up)
float smoothTiltAngle = 0; // Smoothed version for liquid physics

// Motion detection - ADJUSTABLE via serial
float motionThreshold = 0.05; // More sensitive - lowered from 0.08
float shakeThreshold = 0.05; // More sensitive
bool isMoving = false;
bool isShaking = false;
float lastAccelMagnitude = 0;
unsigned long lastMotionTime = 0;
unsigned long lastShakeTime = 0;
unsigned long motionTimeout = 1500;
unsigned long shakeDebounce = 200; // Reduced from 300

// Immediate tempo switching (no more 3-trigger requirement)
bool tempoModeActive = false;
unsigned long tempoModeStartTime = 0;
unsigned long tempoModeTimeout = 60000; // 60 seconds
unsigned long lastTriggerTime = 0;

// Tap detection - MORE SENSITIVE
float tapThreshold = 0.8; // Much more sensitive - lowered from 1.2
float baselineAccel = 1.0;
float lastTotalAccel = 1.0;
unsigned long lastTapTime = 0;
unsigned long tapDebounce = 250;
float tapHistory[5] = {1.0, 1.0, 1.0, 1.0, 1.0};

// Liquid physics variables - Updated for 7 LEDs with smoothing
float liquidLevels[NUM_LEDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
float targetLevels[NUM_LEDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
bool liquidMode = true;

// Tempo detection variables - FIXED FOR 3 TAPS
unsigned long pressHistory[3] = {0, 0, 0}; // Only need 3 for tempo calculation
int pressCount = 0;
int bpm = 0;
unsigned long tempoInterval = 0;
unsigned long lastTempoTime = 0;
bool autoStrobing = false;

// Visual effect variables
float wavePosition = 0;
unsigned long strobeInterval = 20;
unsigned long lastStrobeTime = 0;
bool strobing = false;
float maxBrightness = 0.4; // DIMMED: Reduced from 0.6 to 0.4
float dimBrightness = 0.02;
float waveSpeed = 0.4;
float trailLength = 3.0; // Increased for 7 LEDs

// Breathing effect
float breathPhase = 0;
float globalHueShift = 0;

// Color mode variables - ENHANCED: Auto-switching between rainbow and ctenophore
float colorModeBlend = 0.0; // 0.0 = pure rainbow, 1.0 = pure ctenophore
float colorModeTarget = 0.0;
bool rainbowMode = true;
unsigned long lastColorSwitch = 0;
unsigned long colorSwitchInterval = 3000; // Switch every 3 seconds for smooth flow

// Auto-return to liquid mode
unsigned long lastActivity = 0;
unsigned long idleTimeout = 300000; // 5 minutes

// Function declarations
void initMPU();
void readMPU();
void checkDeviceTap();
void checkAnyMovement();
void handleMovementTrigger();
void processSerialCommands();
void startStrobe();
void stopSequence();
void calculateAndUpdateTempo(unsigned long currentTime);
void updateLiquidPhysics();
void checkLiquidBatteryTrigger();
void doRippleEffect();
void setLEDColorHSV(int led, float hue, float brightness);
void updateLEDs();
void checkIdleTimeout();
void checkBatteryLevel();
void showBatteryLevel();
int calculateBatteryPercentage(float voltage);
void updateColorMode();
void debugBatteryRaw();

void initMPU() {
  Wire.begin();
  delay(100);
  Serial.println("🔍 Connecting to MPU-6050...");
  
  Wire.beginTransmission(0x68);
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.println("✅ MPU-6050 found and ready!");
    mpuAvailable = true;
  } else {
    Serial.println("❌ MPU-6050 not responding - using static liquid physics");
    return;
  }
  
  // Wake up and configure
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x6B); // Power management
  Wire.write(0x00); // Wake up
  Wire.endTransmission();
  
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x1C); // Accelerometer config
  Wire.write(0x00); // ±2g range
  Wire.endTransmission();
  
  Serial.println("🚀 Ready for smooth liquid physics with 7 LEDs!");
}

void readMPU() {
  if (!mpuAvailable) return;
  
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B); // Accelerometer data start
  Wire.endTransmission(false);
  Wire.requestFrom(mpuAddress, (byte)6);
  
  if (Wire.available() >= 6) {
    int16_t rawX = Wire.read() << 8 | Wire.read();
    int16_t rawY = Wire.read() << 8 | Wire.read();
    int16_t rawZ = Wire.read() << 8 | Wire.read();
    
    // Convert to g-force
    accelX = rawX / 16384.0;
    accelY = rawY / 16384.0;
    accelZ = rawZ / 16384.0;
    
    // Use X-axis for up/down tilt
    tiltAngle = constrain(accelX, -1.0, 1.0);
    
    // SMOOTHING FOR LIQUID PHYSICS - exponential moving average
    float smoothingFactor = 0.1; // Lower = smoother, higher = more responsive
    smoothTiltAngle = smoothTiltAngle * (1.0 - smoothingFactor) + tiltAngle * smoothingFactor;
    
    // Detect motion
    float currentMagnitude = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
    float motionDelta = abs(currentMagnitude - lastAccelMagnitude);
    
    // Regular motion detection
    if (motionDelta > motionThreshold) {
      isMoving = true;
      lastMotionTime = millis();
    } else if (millis() - lastMotionTime > motionTimeout) {
      isMoving = false;
    }
    
    lastAccelMagnitude = currentMagnitude;
    
    // Check for any movement (tap trigger system)
    checkAnyMovement();
    
    // Keep original tap detection for fine control
    checkDeviceTap();
  }
}

void checkAnyMovement() {
  if (!mpuAvailable) return;
  
  float currentMagnitude = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
  float motionDelta = abs(currentMagnitude - lastAccelMagnitude);
  
  // Check for any movement above threshold
  if (motionDelta > motionThreshold && (millis() - lastTriggerTime) > shakeDebounce) {
    lastTriggerTime = millis();
    handleMovementTrigger();
  }
}

void handleMovementTrigger() {
  lastActivity = millis();
  
  // Immediate switch to tempo mode on any tap/movement
  if (liquidMode) {
    Serial.println("🌊➡️🎵 TAP! Switching to tempo mode!");
    liquidMode = false;
    tempoModeActive = true;
    tempoModeStartTime = millis();
    pressCount = 0; // Reset tempo counter
    
    // Clear press history
    for(int i = 0; i < 3; i++) {
      pressHistory[i] = 0;
    }
  }
  
  // If already in tempo mode, treat as tempo trigger
  if (!liquidMode) {
    pressCount++;
    Serial.print("🎵 Tempo trigger "); Serial.print(pressCount);
    
    // Start strobe immediately
    startStrobe();
    
    // FIXED: Calculate tempo from 3rd press onward
    if (pressCount >= 3) {
      calculateAndUpdateTempo(millis());
    } else {
      // Store press time for tempo calculation
      pressHistory[pressCount - 1] = millis();
      Serial.println(" - Building tempo...");
    }
  }
}

void processSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command.startsWith("threshold=")) {
      float newThreshold = command.substring(10).toFloat();
      if (newThreshold > 0 && newThreshold < 1.0) {
        motionThreshold = newThreshold;
        tapThreshold = newThreshold * 16; // Scale tap threshold accordingly
        Serial.print("🎛️ Motion threshold set to: ");
        Serial.print(motionThreshold, 3);
        Serial.print(" | Tap threshold: ");
        Serial.println(tapThreshold, 3);
      } else {
        Serial.println("❌ Invalid threshold. Use 0.01-0.99");
      }
    } else if (command.startsWith("brightness=")) {
      float newBrightness = command.substring(11).toFloat();
      if (newBrightness > 0 && newBrightness <= 1.0) {
        maxBrightness = newBrightness;
        Serial.print("💡 Brightness set to: ");
        Serial.print(maxBrightness, 2);
        Serial.println(" (0.1=dim, 1.0=bright)");
      } else {
        Serial.println("❌ Invalid brightness. Use 0.1-1.0");
      }
    } else if (command.startsWith("colorspeed=")) {
      unsigned long newInterval = command.substring(11).toInt();
      if (newInterval >= 1000 && newInterval <= 30000) {
        colorSwitchInterval = newInterval;
        Serial.print("🌈 Color switch interval set to: ");
        Serial.print(colorSwitchInterval / 1000);
        Serial.println(" seconds");
      } else {
        Serial.println("❌ Invalid interval. Use 1000-30000 (1-30 seconds)");
      }
    } else if (command == "reset") {
      stopSequence();
      Serial.println("🔄 Manual reset to liquid mode");
    } else if (command == "battery") {
      checkBatteryLevel();
      showBatteryLevel();
    } else if (command == "rawbatt") {
      debugBatteryRaw();
    } else if (command == "rainbow") {
      rainbowMode = true;
      colorModeTarget = 0.0;
      Serial.println("🌈 Switching to rainbow mode");
    } else if (command == "ctenophore") {
      rainbowMode = false;
      colorModeTarget = 1.0;
      Serial.println("🌊 Switching to ctenophore mode");
    } else if (command == "ctenophore") {
      rainbowMode = false;
      colorModeTarget = 1.0;
      Serial.println("🌊 Switching to ctenophore mode");
    } else if (command == "help") {
      Serial.println("📋 Commands:");
      Serial.println("  threshold=0.08  - Set motion sensitivity");
      Serial.println("  brightness=0.4  - Set LED brightness (0.1-1.0)");
      Serial.println("  colorspeed=3000 - Color switch speed in ms");
      Serial.println("  reset          - Return to liquid mode");
      Serial.println("  battery        - Show battery level");
      Serial.println("  rawbatt        - Debug raw battery reading");
      Serial.println("  rainbow        - Switch to rainbow colors");
      Serial.println("  ctenophore     - Switch to ctenophore colors");
      Serial.println("  ctenophore     - Switch to ctenophore colors");
      Serial.println("  help           - Show this menu");
    }
  }
}

void debugBatteryRaw() {
  int rawReading = analogRead(BATTERY_PIN);
  float voltage = (rawReading / 4095.0) * 3.3;
  float scaledVoltage = voltage * 2.0; // With voltage divider
  
  Serial.println("🔋 BATTERY DEBUG (Rail Mode Always Enabled):");
  Serial.print("  Raw ADC: "); Serial.println(rawReading);
  Serial.print("  Raw Voltage: "); Serial.print(voltage, 3); Serial.println("V");
  Serial.print("  Scaled Voltage: "); Serial.print(scaledVoltage, 3); Serial.println("V");
  Serial.print("  Percentage: "); Serial.print(calculateBatteryPercentage(scaledVoltage)); Serial.println("%");
  Serial.println("  Expected: 1.5V-2.0V range for 3V rail monitoring");
  
  // Diagnostic analysis
  Serial.println("🔍 DIAGNOSIS:");
  if (scaledVoltage < 1.4) {
    Serial.println("  ❌ CRITICAL: No battery detected or severe issue");
  } else if (scaledVoltage < 1.6) {
    Serial.println("  ⚠️  WARNING: Battery very low - charge soon");
  } else if (scaledVoltage < 1.8) {
    Serial.println("  📉 Battery moderate - consider charging");
  } else if (scaledVoltage > 2.1) {
    Serial.println("  ⚠️  Reading unexpectedly high for rail mode");
  } else {
    Serial.println("  ✅ Battery level in normal range for rail monitoring");
  }
}

void checkDeviceTap() {
  if (!mpuAvailable) return;
  
  unsigned long currentTime = millis();
  
  // Calculate total acceleration magnitude
  float totalAccel = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
  
  // Update rolling average
  tapHistory[0] = tapHistory[1];
  tapHistory[1] = tapHistory[2];
  tapHistory[2] = tapHistory[3];
  tapHistory[3] = tapHistory[4];
  tapHistory[4] = totalAccel;
  
  // Calculate average
  float avgAccel = (tapHistory[0] + tapHistory[1] + tapHistory[2] + tapHistory[3] + tapHistory[4]) / 5.0;
  
  // Look for spike above baseline
  float spikeAboveBaseline = totalAccel - avgAccel;
  
  if (spikeAboveBaseline > tapThreshold && 
      (currentTime - lastTapTime) > tapDebounce) {
    
    Serial.print("👆 Tap detected! Spike: ");
    Serial.print(spikeAboveBaseline);
    Serial.print(" | Total: ");
    Serial.println(totalAccel);
    
    lastTapTime = currentTime;
    handleMovementTrigger(); // Use the same trigger system
  }
  
  lastTotalAccel = totalAccel;
}

void calculateAndUpdateTempo(unsigned long currentTime) {
  // FIXED: Proper 3-tap tempo calculation
  unsigned long avgInterval = 0;
  
  if (pressCount == 3) {
    // First tempo calculation: use last 2 intervals
    unsigned long interval1 = pressHistory[1] - pressHistory[0];
    unsigned long interval2 = currentTime - pressHistory[1];
    avgInterval = (interval1 + interval2) / 2;
    Serial.println("🎯 First tempo calculated!");
  } else {
    // Subsequent taps: shift history and recalculate
    pressHistory[0] = pressHistory[1];
    pressHistory[1] = pressHistory[2];
    pressHistory[2] = currentTime;
    
    // Use weighted average of last 2 intervals
    unsigned long interval1 = pressHistory[1] - pressHistory[0];
    unsigned long interval2 = pressHistory[2] - pressHistory[1];
    avgInterval = (interval1 + interval2 * 2) / 3; // Weight recent interval more
    Serial.println("🔄 Tempo refined!");
  }
  
  // Update tempo
  tempoInterval = avgInterval;
  bpm = 60000 / avgInterval;
  
  // Clamp BPM to reasonable range
  if(bpm < 30) {
    bpm = 30;
    tempoInterval = 60000 / bpm;
  }
  if(bpm > 300) {
    bpm = 300;
    tempoInterval = 60000 / bpm;
  }
  
  Serial.print("BPM: "); Serial.print(bpm);
  Serial.print(" ("); Serial.print(tempoInterval); Serial.println("ms)");
  
  autoStrobing = true;
  lastTempoTime = millis();
}

void stopSequence() {
  strobing = false;
  autoStrobing = false;
  pressCount = 0;
  bpm = 0;
  liquidMode = true;
  tempoModeActive = false; // Reset tempo mode flag
  
  // Clear press history
  for(int i = 0; i < 3; i++) {
    pressHistory[i] = 0;
  }
  
  Serial.println("🛑 RESET! → 🌊 Back to liquid mode!");
}

void startStrobe() {
  strobing = true;
  wavePosition = 0;
  lastStrobeTime = millis();
  Serial.println("🌈 Ripple effect started!");
}

void updateLiquidPhysics() {
  if (!liquidMode) return;
  
  // Calculate target levels for 7 LEDs using SMOOTHED tilt angle
  if (!mpuAvailable) {
    // Fallback - center LED only
    for(int i = 0; i < NUM_LEDS; i++) {
      targetLevels[i] = dimBrightness;
    }
    targetLevels[3] = 1.0; // CENTER LED (LED 3 of 0-6)
  } else {
    // Start with all LEDs dim
    for(int i = 0; i < NUM_LEDS; i++) {
      targetLevels[i] = dimBrightness;
    }
    
    if (abs(smoothTiltAngle) < 0.1) {
      // Nearly flat - center light with subtle breathing
      targetLevels[3] = 0.8 + 0.2 * sin(breathPhase * 0.5);
    } else {
      // Tilted - map to LED position (0-6) using smoothed angle
      float ledPosition = 3.0 + smoothTiltAngle * 2.5; // Reduced sensitivity for smoother movement
      ledPosition = constrain(ledPosition, 0, 6);
      
      int mainLED = (int)ledPosition;
      float fraction = ledPosition - mainLED;
      
      // Smooth blending between adjacent LEDs
      if (mainLED >= 0 && mainLED < NUM_LEDS) {
        targetLevels[mainLED] = 1.0 - fraction * 0.3;
      }
      if (mainLED + 1 < NUM_LEDS && fraction > 0.1) {
        targetLevels[mainLED + 1] = fraction * 0.8;
      }
      if (mainLED - 1 >= 0 && fraction < 0.9) {
        targetLevels[mainLED - 1] = (1.0 - fraction) * 0.3;
      }
    }
  }
  
  // Smooth interpolation with better physics
  float smoothingFactor = 0.08; // Slower, more liquid-like movement
  for(int i = 0; i < NUM_LEDS; i++) {
    float diff = targetLevels[i] - liquidLevels[i];
    liquidLevels[i] += diff * smoothingFactor;
    
    // Ensure minimum brightness
    if (liquidLevels[i] < dimBrightness) {
      liquidLevels[i] = dimBrightness;
    }
  }
  
  // Check if liquid has reached the end (battery display trigger)
  checkLiquidBatteryTrigger();
}

void checkLiquidBatteryTrigger() {
  static unsigned long endPositionStartTime = 0;
  static bool wasAtEnd = false;
  
  if (!liquidMode || showingBatteryLevel) return;
  
  // Check if liquid is at the end
  bool atEnd = (liquidLevels[6] > 0.3);
  
  if (atEnd && !wasAtEnd) {
    // Just reached end - start timer
    endPositionStartTime = millis();
    wasAtEnd = true;
    Serial.println("🌊 Liquid at end - hold position for 2 seconds...");
  } else if (!atEnd && wasAtEnd) {
    // Left end position - reset
    wasAtEnd = false;
    Serial.println("🌊 Left end position");
  } else if (atEnd && wasAtEnd) {
    // Held at end - check if enough time has passed
    if ((millis() - endPositionStartTime) > 2000 && (millis() - lastBatteryCheck) > 5000) {
      Serial.println("🔋 Held at end for 2 seconds! Showing battery level...");
      checkBatteryLevel();
      showBatteryLevel();
      lastBatteryCheck = millis();
      wasAtEnd = false; // Reset to prevent repeated triggers
    }
  }
}

void doRippleEffect() {
  // Move wave forward
  wavePosition += waveSpeed;
  
  // Update each LED
  for(int i = 0; i < NUM_LEDS; i++) {
    float distance = abs(i - wavePosition);
    
    // Create ripple
    float rippleBrightness = 0;
    if (distance <= trailLength) {
      rippleBrightness = cos(distance * PI / (trailLength * 2)) * maxBrightness;
      if (rippleBrightness < 0) rippleBrightness = 0;
      liquidLevels[i] = max(liquidLevels[i], rippleBrightness);
    }
    
    // Apply fading
    if (distance > trailLength) {
      liquidLevels[i] *= 0.85;
      if (liquidLevels[i] < dimBrightness) liquidLevels[i] = dimBrightness;
    }
  }
  
  // Color shifting
  globalHueShift += 1.5;
  if (globalHueShift >= 360) globalHueShift -= 360;
}

void updateColorMode() {
  // Auto-switch between rainbow and ctenophore every 3 seconds
  if (millis() - lastColorSwitch > colorSwitchInterval) {
    rainbowMode = !rainbowMode;
    colorModeTarget = rainbowMode ? 0.0 : 1.0;
    lastColorSwitch = millis();
    Serial.print("🌈 Auto-switching to "); 
    Serial.println(rainbowMode ? "RAINBOW" : "CTENOPHORE");
  }
  
  // Smooth transition between color modes
  float colorTransitionSpeed = 0.02;
  float diff = colorModeTarget - colorModeBlend;
  colorModeBlend += diff * colorTransitionSpeed;
  
  // Clamp to prevent overshoot
  if (abs(diff) < 0.01) {
    colorModeBlend = colorModeTarget;
  }
}

void setLEDColorHSV(int led, float hue, float brightness) {
  brightness = constrain(brightness * maxBrightness, 0.0, 1.0);
  hue = fmod(hue, 360.0);
  
  // HSV to RGB conversion
  float c = brightness;
  float x = c * (1 - abs(fmod(hue / 60.0, 2) - 1));
  float r, g, b;
  
  if (hue < 60) { r = c; g = x; b = 0; }
  else if (hue < 120) { r = x; g = c; b = 0; }
  else if (hue < 180) { r = 0; g = c; b = x; }
  else if (hue < 240) { r = 0; g = x; b = c; }
  else if (hue < 300) { r = x; g = 0; b = c; }
  else { r = c; g = 0; b = x; }
  
  strip.setPixelColor(led,
    (int)(r * 255),
    (int)(g * 255),
    (int)(b * 255)
  );
}

void updateLEDs() {
  // Skip if showing battery level
  if (showingBatteryLevel) return;
  
  // Update color mode transition
  updateColorMode();
  
  // Breathing effect when in liquid mode and not strobing
  if (liquidMode && !strobing && !isMoving) {
    breathPhase += 0.015; // Slower breathing
    float pulse = 0.5 + 0.3 * sin(breathPhase);
    
    for(int i = 0; i < NUM_LEDS; i++) {
      float brightness = liquidLevels[i] * pulse;
      
      // ENHANCED: More varied ctenophore colors
      float rainbowHue = i * 51.4 + globalHueShift; // 360/7 = 51.4 degrees per LED
      
      // Varied ctenophore palette: blues, teals, purples, aquas
      float baseHue = 180 + i * 25; // Spread across blue-green-purple spectrum
      float timeOffset = millis() * 0.0008 + i * 0.8;
      float ctenophoreHue = baseHue + sin(timeOffset) * 40 + cos(timeOffset * 1.3) * 20;
      
      float finalHue = rainbowHue * (1.0 - colorModeBlend) + ctenophoreHue * colorModeBlend;
      setLEDColorHSV(i, finalHue, brightness);
    }
  } else {
    // Normal operation
    for(int i = 0; i < NUM_LEDS; i++) {
      // ENHANCED: More varied ctenophore colors
      float rainbowHue = i * 51.4 + globalHueShift;
      
      // Varied ctenophore palette with multiple oscillations
      float baseHue = 180 + i * 25; // Base blue-green-purple spread
      float timeOffset = millis() * 0.0008 + i * 0.8;
      float ctenophoreHue = baseHue + sin(timeOffset) * 40 + cos(timeOffset * 1.3) * 20;
      
      float finalHue = rainbowHue * (1.0 - colorModeBlend) + ctenophoreHue * colorModeBlend;
      setLEDColorHSV(i, finalHue, liquidLevels[i]);
    }
  }
  
  // Slow color shift
  globalHueShift += 0.3;
  if (globalHueShift >= 360) globalHueShift -= 360;
  
  strip.show();
}

void checkIdleTimeout() {
  // Auto-return to liquid mode after 60 seconds of tempo mode
  if (tempoModeActive && (millis() - tempoModeStartTime) > tempoModeTimeout) {
    Serial.println("⏰ 60 seconds of tempo mode - returning to liquid mode");
    stopSequence();
    return;
  }
  
  // Original 5-minute idle timeout (as backup)
  if (!liquidMode && millis() - lastActivity > idleTimeout) {
    Serial.println("⏰ 5 minutes idle - returning to liquid mode");
    stopSequence();
  }
}

int calculateBatteryPercentage(float voltage) {
  // Always use 3V rail monitoring mode (1.5V-2.0V range after voltage divider)
  if (voltage >= 2.0) return 100;  // Rail at full 3.0V
  if (voltage <= 1.5) return 0;    // Rail dropping (battery very low)
  return map(voltage * 100, 150, 200, 0, 100);
}

void checkBatteryLevel() {
  // Read battery voltage through voltage divider
  int rawReading = analogRead(BATTERY_PIN);
  
  // Convert to voltage (ESP32-C3 ADC: 0-4095 = 0-3.3V)
  // With 10k/10k voltage divider, multiply by 2
  batteryVoltage = (rawReading / 4095.0) * 3.3 * 2.0;
  
  // Calculate battery percentage
  batteryPercentage = calculateBatteryPercentage(batteryVoltage);
  
  // Check for low battery warning
  if (batteryPercentage <= 20 && !lowBatteryWarning) {
    lowBatteryWarning = true;
    Serial.println("⚠️ LOW BATTERY WARNING! ⚠️");
  } else if (batteryPercentage > 25) {
    lowBatteryWarning = false;
  }
}

void showBatteryLevel() {
  showingBatteryLevel = true;
  strip.clear();
  
  // Enhanced battery LED display - FILLING FROM RIGHT TO LEFT
  if (batteryPercentage >= 80) {
    // 80-100%: All 7 LEDs green (from right to left)
    for (int i = NUM_LEDS - 1; i >= 0; i--) {
      strip.setPixelColor(i, 0, 255, 0); // Green
    }
  } else if (batteryPercentage >= 20) {
    // 20-79%: Show percentage as yellow LEDs (from right to left)
    int ledsToLight = map(batteryPercentage, 20, 79, 1, NUM_LEDS);
    for (int i = NUM_LEDS - 1; i >= NUM_LEDS - ledsToLight; i--) {
      strip.setPixelColor(i, 255, 255, 0); // Yellow
    }
  } else {
    // 1-19%: Show percentage as red LEDs (from right to left)
    int ledsToLight = map(batteryPercentage, 1, 19, 1, NUM_LEDS);
    for (int i = NUM_LEDS - 1; i >= NUM_LEDS - ledsToLight; i--) {
      strip.setPixelColor(i, 255, 0, 0); // Red
    }
  }
  
  strip.show();
  delay(8000); // Show for 8 seconds
  
  Serial.print("🔋 Battery: ");
  Serial.print(batteryPercentage);
  Serial.print("% (");
  Serial.print(batteryVoltage, 2);
  Serial.println("V)");
  
  showingBatteryLevel = false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Initialize battery monitoring
  pinMode(BATTERY_PIN, INPUT);
  
  strip.begin();
  strip.setBrightness(255);
  strip.clear();
  strip.show();
  
  Serial.println("🌊✨ CTENOPHORE SYSTEM v1.0 - FINAL ✨🌊");
  Serial.println("🎉 RELEASE FEATURES:");
  Serial.println(" ✅ Auto-switching rainbow↔ctenophore every 3 seconds");
  Serial.println(" ✅ Dimmed lights (0.4 brightness) with adjustable setting");
  Serial.println(" ✅ Battery display fills from right to left");
  Serial.println(" ✅ Rich varied ctenophore color palette");
  Serial.println(" ✅ Hold-to-show battery (2 seconds at end)");
  Serial.println(" ✅ Perfect 3V rail battery monitoring");
  Serial.println(" ✅ Smooth 3-tap tempo detection");
  Serial.println("");
  Serial.println("🎮 CORE FEATURES:");
  Serial.println(" 🌊 Real liquid tilt physics via MPU6050");
  Serial.println(" 👆 Device tap detection");
  Serial.println(" 🎵 3-tap tempo detection & auto-strobing");
  Serial.println(" 🌈 Auto-cycling rainbow/ctenophore effects");
  Serial.println(" 💡 Hold liquid at end for 2s → battery display");
  Serial.println(" 🔋 Automatic 3V rail battery monitoring");
  Serial.println(" 🎛️ Adjustable motion sensitivity");
  Serial.println("");
  Serial.println("📋 Serial Commands:");
  Serial.println("  threshold=0.08  - Set motion sensitivity");
  Serial.println("  reset          - Return to liquid mode");
  Serial.println("  battery        - Show battery level");
  Serial.println("  rawbatt        - Debug raw battery reading");
  Serial.println("  rainbow        - Force rainbow colors");
  Serial.println("  ctenophore     - Force ctenophore colors");
  Serial.println("  help           - Show command menu");
  Serial.println("");
  
  initMPU();
  lastActivity = millis();
  
  // Show initial battery level
  checkBatteryLevel();
  showBatteryLevel();
  
  Serial.println("🪄 Ready! Tilt for liquid, 3 taps for tempo!");
  Serial.println("💡 Colors auto-cycle every 3s, hold at end for battery!");
  Serial.println("🔧 Try: brightness=0.2 (dim) or colorspeed=5000 (slower)");
}

void loop() {
  static unsigned long lastMPURead = 0;
  static unsigned long lastDebug = 0;
  unsigned long currentTime = millis();
  
  // Process serial commands
  processSerialCommands();
  
  // Read MPU every 10ms
  if (currentTime - lastMPURead > 10) {
    readMPU();
    lastMPURead = currentTime;
  }
  
  // Check battery level every 10 seconds
  if (currentTime - lastBatteryCheck > batteryCheckInterval) {
    checkBatteryLevel();
    lastBatteryCheck = currentTime;
  }
  
  // Update liquid physics
  if (liquidMode) {
    updateLiquidPhysics();
  }
  
  // Handle strobing
  if (strobing && currentTime - lastStrobeTime >= strobeInterval) {
    doRippleEffect();
    lastStrobeTime = currentTime;
  }
  
  // Auto-restart at tempo
  if (autoStrobing && tempoInterval > 0) {
    if (currentTime - lastTempoTime >= tempoInterval) {
      Serial.print("🎵 Auto-beat "); Serial.print(pressCount);
      Serial.print(" ("); Serial.print(bpm); Serial.println(" BPM)");
      startStrobe();
      lastTempoTime = currentTime;
      lastActivity = currentTime;
    }
  }
  
  // Check idle timeout
  checkIdleTimeout();
  
  // Update LEDs
  updateLEDs();
  
  // Debug output every 5 seconds
  if (currentTime - lastDebug > 5000) {
    Serial.print("🌊 Mode: "); Serial.print(liquidMode ? "LIQUID" : "TEMPO");
    if (liquidMode && mpuAvailable) {
      Serial.print(" | Tilt: "); Serial.print(smoothTiltAngle, 2);
      Serial.print(" | Colors: "); Serial.print(colorModeBlend < 0.5 ? "RAINBOW" : "CTENOPHORE");
    } else if (!liquidMode) {
      Serial.print(" | BPM: "); Serial.print(bpm);
    }
    Serial.print(" | 🔋 "); Serial.print(batteryPercentage); Serial.print("%");
    Serial.print(" | Threshold: "); Serial.print(motionThreshold, 3);
    Serial.println();
    lastDebug = currentTime;
  }
  
  delay(5);
}