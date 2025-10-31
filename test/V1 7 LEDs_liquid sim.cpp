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

// MPU-6050 variables
byte mpuAddress = 0x68;
bool mpuAvailable = false;
float accelX = 0, accelY = 0, accelZ = 0;
float tiltAngle = 0; // -1.0 (down) to 1.0 (up)

// Motion detection - ADJUSTABLE via serial
float motionThreshold = 0.08; // Adjustable via serial commands
float shakeThreshold = 0.08; // Lowered for better sensitivity
bool isMoving = false;
bool isShaking = false;
float lastAccelMagnitude = 0;
unsigned long lastMotionTime = 0;
unsigned long lastShakeTime = 0;
unsigned long motionTimeout = 1500;
unsigned long shakeDebounce = 300; // Reduced debounce

// Three-trigger system for mode switching
int triggerCount = 0;
unsigned long lastTriggerTime = 0;
unsigned long triggerResetTime = 3000; // Reset trigger count after 3 seconds

// Tap detection for 7 LEDs (keeping original sensitivity)
float tapThreshold = 1.2; // Lowered from 1.8
float baselineAccel = 1.0;
float lastTotalAccel = 1.0;
unsigned long lastTapTime = 0;
unsigned long tapDebounce = 250;
float tapHistory[5] = {1.0, 1.0, 1.0, 1.0, 1.0};

// Liquid physics variables - Updated for 7 LEDs
float liquidLevels[NUM_LEDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
float targetLevels[NUM_LEDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
bool liquidMode = true;

// Tempo detection variables
unsigned long pressHistory[4] = {0, 0, 0, 0};
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
float maxBrightness = 0.6;
float dimBrightness = 0.02;
float waveSpeed = 0.4;
float trailLength = 3.0; // Increased for 7 LEDs

// Breathing effect
float breathPhase = 0;
float globalHueShift = 0;

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
  
  Serial.println("🚀 Ready for liquid physics with 7 LEDs!");
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
    
    // Check for any movement (new 3-trigger system)
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
  
  // Reset trigger count if too much time has passed
  if (millis() - lastTriggerTime > triggerResetTime) {
    triggerCount = 0;
  }
}

void handleMovementTrigger() {
  triggerCount++;
  lastActivity = millis();
  
  Serial.print("🌊 Movement trigger "); Serial.print(triggerCount); Serial.println("/3");
  
  // Check for mode switch (3 triggers)
  if (triggerCount >= 3 && liquidMode) {
    Serial.println("🌊➡️🎵 THREE TRIGGERS! Switching to tempo mode!");
    liquidMode = false;
    triggerCount = 0; // Reset for tempo mode
    pressCount = 0;   // Reset tempo counter
    return;
  }
  
  // If we're in tempo mode, treat as tempo trigger
  if (!liquidMode) {
    pressCount++;
    Serial.print("🎵 Tempo trigger "); Serial.print(pressCount);
    
    // Start strobe immediately
    startStrobe();
    
    // Calculate tempo from 3rd press onward
    if (pressCount >= 3) {
      calculateAndUpdateTempo(millis());
    } else {
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
        Serial.print("🎛️ Motion threshold set to: ");
        Serial.println(motionThreshold, 3);
      } else {
        Serial.println("❌ Invalid threshold. Use 0.01-0.99");
      }
    } else if (command == "reset") {
      stopSequence();
      Serial.println("🔄 Manual reset to liquid mode");
    } else if (command == "battery") {
      checkBatteryLevel();
      showBatteryLevel();
    } else if (command == "help") {
      Serial.println("📋 Commands:");
      Serial.println("  threshold=0.08  - Set motion sensitivity");
      Serial.println("  reset          - Return to liquid mode");
      Serial.println("  battery        - Show battery level");
      Serial.println("  help           - Show this menu");
    }
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
  // Shift press history
  for(int i = 0; i < 3; i++) {
    pressHistory[i] = pressHistory[i + 1];
  }
  pressHistory[3] = currentTime;
  
  unsigned long avgInterval = 0;
  
  if (pressCount == 3) {
    avgInterval = pressHistory[3] - pressHistory[2];
    Serial.println("🎯 First tempo guess!");
  } else if (pressCount == 4) {
    unsigned long interval1 = pressHistory[2] - pressHistory[1];
    unsigned long interval2 = pressHistory[3] - pressHistory[2];
    avgInterval = (interval1 + interval2) / 2;
    Serial.println("🎯 Improved tempo!");
  } else {
    unsigned long interval1 = pressHistory[1] - pressHistory[0];
    unsigned long interval2 = pressHistory[2] - pressHistory[1]; 
    unsigned long interval3 = pressHistory[3] - pressHistory[2];
    
    avgInterval = (interval1 + interval2 * 2 + interval3 * 3) / 6;
    Serial.println("🔄 Tempo adjusted!");
  }
  
  tempoInterval = avgInterval;
  bpm = 60000 / avgInterval;
  
  // Clamp BPM
  if(bpm < 30) bpm = 30;
  if(bpm > 300) bpm = 300;
  tempoInterval = 60000 / bpm;
  
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
  triggerCount = 0; // Reset trigger count
  
  // Clear press history
  for(int i = 0; i < 4; i++) {
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
  
  // Calculate target levels for 7 LEDs
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
    
    if (abs(tiltAngle) < 0.15) {
      // Nearly flat - center light
      targetLevels[3] = 1.0; // CENTER LED
    } else {
      // Tilted - map to LED position (0-6)
      float ledPosition = 3.0 + tiltAngle * 3.0; // Map tilt to LED position
      ledPosition = constrain(ledPosition, 0, 6);
      
      int mainLED = round(ledPosition);
      mainLED = constrain(mainLED, 0, NUM_LEDS-1);
      
      // Set main LED to full brightness
      targetLevels[mainLED] = 1.0;
      
      // Optional spillover for smoother movement
      float spillover = abs(ledPosition - mainLED);
      if (spillover > 0.3) {
        if (mainLED > 0 && ledPosition < mainLED) {
          targetLevels[mainLED-1] = 0.3;
        }
        if (mainLED < NUM_LEDS-1 && ledPosition > mainLED) {
          targetLevels[mainLED+1] = 0.3;
        }
      }
    }
  }
  
  // Smooth interpolation
  float smoothingFactor = 0.15;
  for(int i = 0; i < NUM_LEDS; i++) {
    liquidLevels[i] += (targetLevels[i] - liquidLevels[i]) * smoothingFactor;
  }
  
  // Check if liquid has reached the end (battery display trigger)
  checkLiquidBatteryTrigger();
}

void checkLiquidBatteryTrigger() {
  if (!liquidMode || showingBatteryLevel) return;
  
  // Check if the last LED (index 6) is significantly bright
  if (liquidLevels[6] > 0.8) {
    Serial.println("🔋 Liquid reached end! Showing battery level...");
    checkBatteryLevel();
    showBatteryLevel();
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
  
  // Breathing effect when in liquid mode and not strobing
  if (liquidMode && !strobing && !isMoving) {
    breathPhase += 0.02;
    float pulse = 0.4 + 0.2 * sin(breathPhase);
    
    for(int i = 0; i < NUM_LEDS; i++) {
      float brightness = liquidLevels[i] * pulse;
      float hue = i * 51.4 + globalHueShift; // 360/7 = 51.4 degrees per LED
      setLEDColorHSV(i, hue, brightness);
    }
  } else {
    // Normal operation
    for(int i = 0; i < NUM_LEDS; i++) {
      float hue = i * 51.4 + globalHueShift;
      setLEDColorHSV(i, hue, liquidLevels[i]);
    }
  }
  
  // Slow color shift
  globalHueShift += 0.5;
  if (globalHueShift >= 360) globalHueShift -= 360;
  
  strip.show();
}

void checkIdleTimeout() {
  if (!liquidMode && millis() - lastActivity > idleTimeout) {
    Serial.println("⏰ 5 minutes idle - returning to liquid mode");
    stopSequence();
  }
}

int calculateBatteryPercentage(float voltage) {
  // LiPo voltage curve: 4.2V (100%) to 3.3V (0%)
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.3) return 0;
  
  return (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);
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
  
  // Enhanced battery LED display per your specifications
  if (batteryPercentage >= 80) {
    // 80-100%: All 7 LEDs green
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0, 255, 0); // Green
    }
  } else if (batteryPercentage >= 20) {
    // 20-79%: Show percentage as yellow LEDs
    int ledsToLight = map(batteryPercentage, 20, 79, 1, NUM_LEDS);
    for (int i = 0; i < ledsToLight; i++) {
      strip.setPixelColor(i, 255, 255, 0); // Yellow
    }
  } else {
    // 1-19%: Show percentage as red LEDs
    int ledsToLight = map(batteryPercentage, 1, 19, 1, NUM_LEDS);
    for (int i = 0; i < ledsToLight; i++) {
      strip.setPixelColor(i, 255, 0, 0); // Red
    }
  }
  
  strip.show();
  delay(2000); // Show for 2 seconds
  
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
  
  Serial.println("🌊✨ ENHANCED CTENOPHORE SYSTEM ✨🌊");
  Serial.println("Features:");
  Serial.println(" 🌊 Real liquid tilt physics via MPU6050");
  Serial.println(" 👆 Device tap detection");
  Serial.println(" 🏃‍♂️ 3-trigger mode switching");
  Serial.println(" 🎵 Automatic tempo detection");
  Serial.println(" 🌈 Rainbow ripple effects");
  Serial.println(" 💡 Battery display when liquid reaches end");
  Serial.println(" 🔋 Battery level monitoring");
  Serial.println(" 🎛️ Adjustable motion sensitivity");
  Serial.println("");
  Serial.println("📋 Serial Commands:");
  Serial.println("  threshold=0.08  - Set motion sensitivity");
  Serial.println("  reset          - Return to liquid mode");
  Serial.println("  battery        - Show battery level");
  Serial.println("  help           - Show command menu");
  Serial.println("");
  
  initMPU();
  lastActivity = millis();
  
  // Show initial battery level
  checkBatteryLevel();
  showBatteryLevel();
  
  Serial.println("🪄 Ready! Tilt for liquid, 3 movements for tempo!");
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
      Serial.print(" | Tilt: "); Serial.print(tiltAngle, 2);
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