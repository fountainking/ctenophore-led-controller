#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <esp_pm.h>
#include <esp_task_wdt.h>

#define LED_PIN 10 // D10 on Xiao ESP32-C3
#define BATTERY_PIN A0 // A0 for battery voltage reading (top left pin) #NO BATTERY monitoring Ver.#
#define NUM_LEDS 7 // Updated for 7 LEDs

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// WiFi Configuration - HOTSPOT MODE
const char* ssid = "Ctenophore-Control";
const char* password = "tempo123";
AsyncWebServer server(80);

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

// Rotation Variables - Separate X and Z axis tracking
float cumulativeXRotation = 0;  // For barrel rolls (animation changes)
float cumulativeZRotation = 0;  // For spins (palette changes)
unsigned long lastXRotationTime = 0;
unsigned long lastZRotationTime = 0;
bool hasTriggeredXRoll = false;
bool hasTriggeredZRoll = false;

// Motion detection - ADJUSTABLE via serial/web
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
float tapThreshold = 0.4; // ULTRA sensitive - lowered from 0.6
float baselineAccel = 1.0;
float lastTotalAccel = 1.0;
unsigned long lastTapTime = 0;
unsigned long tapDebounce = 200; // Reduced debounce for faster tap detection
float tapHistory[5] = {1.0, 1.0, 1.0, 1.0, 1.0};

// Liquid physics variables - Updated for 7 LEDs
float liquidLevels[NUM_LEDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
float targetLevels[NUM_LEDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
bool liquidMode = true;

// Tempo detection variables
unsigned long pressHistory[4] = {0, 0, 0, 0};
int pressCount = 0;
int bpm = 0;
unsigned long nextBeatTime = 0;        // Absolute time for next beat (prevents drift)
bool tempoLocked = false;              // Track if we have a stable tempo
unsigned long lastDriftCorrection = 0; // For periodic sync correction
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

// ===== NEW LIGHT CUSTOMIZATION SYSTEM =====

// Color palette system
struct ColorPalette {
  String name;
  uint32_t colors[7];
  int colorCount;
};

// Predefined palettes
ColorPalette palettes[] = {
  {"Rainbow", {0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3}, 7},
  {"Ocean", {0x001F3F, 0x0074D9, 0x7FDBFF, 0x39CCCC, 0x2ECC40, 0x01FF70, 0xFFFFFF}, 7},
  {"Fire", {0x000000, 0x8B0000, 0xFF0000, 0xFF4500, 0xFF8C00, 0xFFD700, 0xFFFFFF}, 7},
  {"Ctenophore", {0x000033, 0x000066, 0x003366, 0x0066CC, 0x00CCFF, 0x66FFFF, 0xFFFFFF}, 7},
  {"Sunset", {0x2D1B69, 0x11235A, 0x1E3A8A, 0x3B82F6, 0xF59E0B, 0xF97316, 0xDC2626}, 7},
  {"Cyberpunk", {0xFF00FF, 0xFF0080, 0xFF0040, 0x00FFFF, 0x0080FF, 0x0040FF, 0x8000FF}, 7},
  {"Peppermint", {0xFF0000, 0xFFFFFF, 0xFF0000, 0xFFFFFF, 0xFF0000, 0xFFFFFF, 0xFF0000}, 7},
  {"Aesthetic", {0x000080, 0xB0C4DE, 0xFF0000, 0xFFA500, 0xFFFF00, 0xFFFFFF, 0xFFFFFF}, 7},
};

int currentPaletteIndex = 0; // Default to Rainbow
int paletteCount = 8;

// Add this after your existing palettes array
ColorPalette customPalettes[10]; // Storage for 10 custom palettes
int customPaletteCount = 0;
int totalPaletteCount = 8; // Will increase as you add custom ones

// Animation patterns
enum AnimationPattern {
  PATTERN_RAINBOW_CYCLE,
  PATTERN_BREATHING,
  PATTERN_CHASE,
  PATTERN_SPARKLE,
  PATTERN_STROBE,
  PATTERN_FADE,
  PATTERN_CUSTOM
};

AnimationPattern currentPattern = PATTERN_RAINBOW_CYCLE;

// Individual LED color overrides
uint32_t customLEDColors[NUM_LEDS] = {0, 0, 0, 0, 0, 0, 0}; // 0 = use palette
bool useCustomColors = false;

// Tilt-based palette zones with smooth transitions
struct TiltZone {
  float tiltMin;
  float tiltMax;
  int paletteIndex;
};

TiltZone tiltZones[] = {
  {-1.0, -0.5, 1}, // Ocean for down tilt
  {-0.5, 0.5, 0},  // Rainbow for center
  {0.5, 1.0, 2}    // Fire for up tilt
};

bool useTiltPalettes = false;
float tiltTransitionSmoothing = 0.05; // Smooth color transitions

// Tempo-reactive coloring
bool tempoColorReactive = false;
float temperatureShift = 0; // -1.0 (cool) to 1.0 (warm)

// Animation timing
unsigned long lastAnimationUpdate = 0;
unsigned long animationInterval = 50; // 20 FPS for smooth animations

// Sparkle effect variables
bool sparkleStates[NUM_LEDS] = {false};
unsigned long sparkleTimers[NUM_LEDS] = {0};
unsigned long lastIdleSparkle = 0;
unsigned long idleSparkleInterval = 3000; // Sparkle every 3 seconds when idle

// Random palette mode
bool randomPaletteMode = false;
unsigned long lastRandomChange = 0;

// Chase effect variables
int chasePosition = 0;
bool chaseDirection = true;

// Web server variables
unsigned long lastStatusUpdate = 0;
String lastStatusJson = "";

// HTML Dashboard with Light Customization - Embedded in ESP32 Flash
const char dashboard_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ctempo v2.0</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&display=swap');
        
        * { 
            margin: 0; 
            padding: 0; 
            box-sizing: border-box; 
            -webkit-tap-highlight-color: transparent;
        }
        
        body { 
            font-family: 'Space Mono', monospace;
            background: #f5f5f5;
            color: #333;
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        
        .device-frame {
            width: 100%;
            max-width: 400px;
            background: white;
            border-radius: 25px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.1);
            padding: 25px;
            position: relative;
            overflow: hidden;
        }
        
        /* Header */
        .header {
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
            margin-bottom: 25px;
        }
        
        .battery-meter {
            font-size: 12px;
            color: #666;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        
        .battery-icon {
            width: 25px;
            height: 12px;
            border: 1.5px solid #666;
            border-radius: 2px;
            position: relative;
            background: white;
        }
        
        .battery-icon::after {
            content: '';
            position: absolute;
            right: -3px;
            top: 3px;
            width: 2px;
            height: 6px;
            background: #666;
            border-radius: 0 1px 1px 0;
        }
        
        .battery-fill {
            height: 100%;
            background: linear-gradient(90deg, #ff4757, #ffa502, #2ed573);
            border-radius: 1px;
            transition: width 0.3s ease;
            width: 75%;
        }
        
        .version {
            font-size: 11px;
            color: #999;
        }
        
        /* Main Brand */
        .brand {
            text-align: center;
            margin-bottom: 30px;
        }
        
        .brand h1 {
            font-size: 48px;
            font-weight: 700;
            letter-spacing: -2px;
            margin-bottom: 5px;
        }
        
        /* Tap Button */
        .tap-section {
            margin-bottom: 30px;
        }
        
        .tap-button {
            width: 100%;
            height: 120px;
            background: #000;
            border-radius: 15px;
            border: none;
            color: white;
            font-family: inherit;
            font-size: 16px;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.1s ease;
            position: relative;
            overflow: hidden;
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 25px;
        }
        
        .tap-button:active {
            transform: scale(0.98);
        }
        
        .tap-circle {
            width: 70px;
            height: 70px;
            background: white;
            border-radius: 50%;
            transition: all 0.2s ease;
        }
        
        .tap-bpm {
            text-align: right;
        }
        
        .bpm-number {
            font-size: 36px;
            font-weight: 700;
            line-height: 1;
        }
        
        .bpm-label {
            font-size: 16px;
            opacity: 0.8;
        }
        
        /* Live LED Monitor */
        .led-section {
            margin-bottom: 25px;
        }
        
        .led-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 8px;
        }
        
        .led {
            width: 35px;
            height: 35px;
            border-radius: 50%;
            background: #e0e0e0;
            transition: all 0.1s ease;
            border: 2px solid transparent;
        }
        
        .led.active {
            box-shadow: 0 0 15px currentColor;
            border-color: rgba(255,255,255,0.3);
        }
        
        /* Mode Indicator with Liquid Button */
        .mode-section {
            margin-bottom: 30px;
        }
        
        .mode-indicator {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 15px 20px;
            background: #f8f9fa;
            border-radius: 12px;
            position: relative;
        }
        
        .mode-icon {
            font-size: 24px;
        }
        
        .mode-text {
            font-size: 18px;
            font-weight: 700;
            flex: 1;
        }
        
        .liquid-btn {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            border: 2px solid #333;
            background: white;
            font-size: 20px;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .liquid-btn:hover {
            background: #333;
            color: white;
            transform: scale(1.05);
        }
        
        .liquid-btn:active {
            transform: scale(0.95);
        }
        
        /* Palette Grid */
        .palette-section {
            margin-bottom: 25px;
        }
        
        .section-label {
            font-size: 12px;
            color: #666;
            margin-bottom: 12px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        .palette-grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 12px;
            margin-bottom: 15px;
        }
        
        .palette-card {
            aspect-ratio: 1;
            border-radius: 12px;
            border: 2px solid #e0e0e0;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 8px;
            position: relative;
            overflow: hidden;
        }
        
        .palette-card:hover {
            border-color: #666;
            transform: translateY(-1px);
        }
        
        .palette-card.active {
            border-color: #000;
            border-width: 3px;
        }
        
        .palette-preview {
            display: flex;
            gap: 2px;
            margin-bottom: 4px;
            flex-wrap: wrap;
            justify-content: center;
        }
        
        .palette-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
        }
        
        .palette-name {
            font-size: 10px;
            text-align: center;
            color: #666;
            font-weight: 700;
        }
        
        /* Custom Palette Section */
        .custom-palette {
            border: 2px dashed #ccc;
            background: #fafafa;
            position: relative;
        }
        
        .custom-colors {
            display: flex;
            gap: 2px;
            margin-bottom: 4px;
            justify-content: center;
        }
        
        .custom-color {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            cursor: pointer;
            border: 1px solid rgba(0,0,0,0.1);
        }
        
        .pattern-icon {
            font-size: 16px;
            display: flex;
            align-items: center;
            justify-content: center;
            height: 20px;
        }
        
        .add-icon {
            font-size: 20px;
            color: #666;
            margin-bottom: 4px;
        }
        
        /* Tilt Sliders */
        .tilt-section {
            margin-bottom: 25px;
        }
        
        .tilt-sliders {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 15px;
        }
        
        .tilt-slider {
            text-align: center;
        }
        
        .slider-container {
            width: 100%;
            height: 25px;
            background: #f0f0f0;
            border-radius: 12px;
            position: relative;
            margin-bottom: 8px;
            overflow: hidden;
        }
        
        .slider-track {
            position: absolute;
            top: 50%;
            left: 0;
            right: 0;
            height: 2px;
            background: #ccc;
            transform: translateY(-50%);
        }
        
        .slider-thumb {
            position: absolute;
            top: 2px;
            width: 21px;
            height: 21px;
            background: #333;
            border-radius: 50%;
            transition: left 0.3s ease;
            border: 2px solid white;
            box-shadow: 0 2px 6px rgba(0,0,0,0.2);
            cursor: pointer;
        }
        
        .slider-label {
            font-size: 9px;
            color: #666;
            font-weight: 700;
        }
        
        /* Control Dials - Make them functional rotary controls */
        .controls-section {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 20px;
            margin-bottom: 25px;
        }
        
        .control-dial {
            text-align: center;
        }
        
        .dial {
            width: 80px;
            height: 80px;
            border-radius: 50%;
            border: 3px solid #e0e0e0;
            margin: 0 auto 8px;
            position: relative;
            cursor: pointer;
            transition: all 0.2s ease;
            background: white;
            user-select: none;
        }
        
        .dial:hover {
            border-color: #666;
        }
        
        .dial-track {
            position: absolute;
            top: -3px;
            left: -3px;
            right: -3px;
            bottom: -3px;
            border-radius: 50%;
            background: conic-gradient(from -90deg, #333 0deg, #333 var(--angle, 120deg), transparent var(--angle, 120deg));
            z-index: 0;
        }
        
        .dial-background {
            position: absolute;
            top: 3px;
            left: 3px;
            right: 3px;
            bottom: 3px;
            border-radius: 50%;
            background: white;
            z-index: 1;
        }
        
        .dial-icon {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            font-size: 24px;
            color: #666;
            z-index: 2;
        }
        
        .dial-value {
            position: absolute;
            bottom: 8px;
            left: 50%;
            transform: translateX(-50%);
            font-size: 10px;
            color: #666;
            font-weight: 700;
            z-index: 2;
        }
        
        .dial-label {
            font-size: 11px;
            color: #666;
            font-weight: 700;
        }
        
        /* Brand Label */
        .brand-label {
            text-align: center;
            font-size: 11px;
            color: #999;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        /* Responsive */
        @media (max-width: 480px) {
            .device-frame {
                margin: 10px;
                padding: 20px;
            }
            
            .brand h1 {
                font-size: 40px;
            }
            
            .tap-button {
                height: 100px;
                padding: 20px;
            }
            
            .bpm-number {
                font-size: 28px;
            }
        }
        
        /* Custom palette modal */
        .modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0,0,0,0.5);
            z-index: 1000;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        
        .modal.show {
            display: flex;
        }
        
        .modal-content {
            background: white;
            border-radius: 20px;
            padding: 25px;
            width: 100%;
            max-width: 350px;
        }
        
        .modal-header {
            font-size: 18px;
            font-weight: 700;
            margin-bottom: 20px;
            text-align: center;
        }
        
        .color-picker-grid {
            display: grid;
            grid-template-columns: repeat(7, 1fr);
            gap: 8px;
            margin-bottom: 20px;
        }
        
        .color-picker {
            width: 35px;
            height: 35px;
            border-radius: 50%;
            border: none;
            cursor: pointer;
            transition: transform 0.1s ease;
        }
        
        .color-picker:hover {
            transform: scale(1.1);
        }
        
        .palette-name-input {
            width: 100%;
            padding: 12px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-family: inherit;
            font-size: 14px;
            margin-bottom: 20px;
        }
        
        .modal-buttons {
            display: flex;
            gap: 12px;
        }
        
        .modal-btn {
            flex: 1;
            padding: 12px;
            border: none;
            border-radius: 8px;
            font-family: inherit;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        
        .modal-btn.primary {
            background: #000;
            color: white;
        }
        
        .modal-btn.secondary {
            background: #f0f0f0;
            color: #666;
        }
        
        .modal-btn:hover {
            transform: translateY(-1px);
        }
    </style>
</head>
<body>
    <div class="device-frame">
        <!-- Header -->
        <div class="header">
            <div class="battery-meter">
                Battery Meter
                <div class="battery-icon">
                    <div class="battery-fill" id="batteryFill"></div>
                </div>
            </div>
            <div class="version">ver. 2.0</div>
        </div>
        
        <!-- Brand -->
        <div class="brand">
            <h1>Ctempo</h1>
        </div>
        
        <!-- Tap Button -->
        <div class="tap-section">
            <button class="tap-button" id="tapButton">
                <div class="tap-circle" id="tapCircle"></div>
                <div class="tap-bpm">
                    <div class="bpm-number" id="bpmDisplay">120</div>
                    <div class="bpm-label">bpm</div>
                </div>
            </button>
        </div>
        
        <!-- Live LED Monitor -->
        <div class="led-section">
            <div class="led-row">
                <div class="led" id="led0"></div>
                <div class="led" id="led1"></div>
                <div class="led" id="led2"></div>
                <div class="led" id="led3"></div>
                <div class="led" id="led4"></div>
                <div class="led" id="led5"></div>
                <div class="led" id="led6"></div>
            </div>
        </div>
        
        <!-- Mode Indicator with Liquid Button -->
        <div class="mode-section">
            <div class="mode-indicator" id="modeIndicator">
                <div class="mode-icon">💧</div>
                <div class="mode-text">Liquid</div>
                <button class="liquid-btn" id="liquidBtn">💧</button>
            </div>
         </div>
        
        <!-- Color Palettes -->
        <div class="palette-section">
            <div class="section-label">Pre-determined color palettes</div>
            <div class="palette-grid" id="paletteGrid">
                <!-- Palettes will be populated by JavaScript -->
            </div>
            
            <div class="section-label">Custom palettes</div>
            <div class="palette-grid">
                <div class="palette-card custom-palette" id="customPaletteBtn">
                    <div class="add-icon">+</div>
                    <div class="palette-name">Custom</div>
                </div>
            </div>
        </div>
        
        <!-- Animation Patterns -->
        <div class="palette-section">
            <div class="section-label">Animation patterns</div>
            <div class="palette-grid" id="patternGrid">
                <div class="palette-card active" data-pattern="rainbow">
                    <div class="palette-preview">
                        <div class="pattern-icon">🌈</div>
                    </div>
                    <div class="palette-name">Rainbow</div>
                </div>
                <div class="palette-card" data-pattern="breathing">
                    <div class="palette-preview">
                        <div class="pattern-icon">💨</div>
                    </div>
                    <div class="palette-name">Breathing</div>
                </div>
                <div class="palette-card" data-pattern="chase">
                    <div class="palette-preview">
                        <div class="pattern-icon">🏃</div>
                    </div>
                    <div class="palette-name">Chase</div>
                </div>
                <div class="palette-card" data-pattern="sparkle">
                    <div class="palette-preview">
                        <div class="pattern-icon">✨</div>
                    </div>
                    <div class="palette-name">Sparkle</div>
                </div>
                <div class="palette-card" data-pattern="strobe">
                    <div class="palette-preview">
                        <div class="pattern-icon">⚡</div>
                    </div>
                    <div class="palette-name">Strobe</div>
                </div>
                <div class="palette-card" data-pattern="fade">
                    <div class="palette-preview">
                        <div class="pattern-icon">🌅</div>
                    </div>
                    <div class="palette-name">Fade</div>
                </div>
            </div>
        </div>
        
        <!-- Tilt Sliders -->
        <div class="tilt-section">
            <div class="section-label">Tilt meter</div>
            <div class="tilt-sliders">
                <div class="tilt-slider">
                    <div class="slider-container">
                        <div class="slider-track"></div>
                        <div class="slider-thumb" id="tiltThumb1"></div>
                    </div>
                    <div class="slider-label">X-Axis</div>
                </div>
                <div class="tilt-slider">
                    <div class="slider-container">
                        <div class="slider-track"></div>
                        <div class="slider-thumb" id="tiltThumb2"></div>
                    </div>
                    <div class="slider-label">Y-Axis</div>
                </div>
                <div class="tilt-slider">
                    <div class="slider-container">
                        <div class="slider-track"></div>
                        <div class="slider-thumb" id="tiltThumb3"></div>
                    </div>
                    <div class="slider-label">Z-Axis</div>
                </div>
            </div>
        </div>
        
        <!-- Control Dials -->
        <div class="controls-section">
            <div class="control-dial">
                <div class="dial" id="tempoDial" data-min="30" data-max="300" data-value="30">
                    <div class="dial-track" style="--angle: 0deg;"></div>
                    <div class="dial-background"></div>
                    <div class="dial-icon">?</div>
                    <div class="dial-value">30</div>
                </div>
                <div class="dial-label">Manual Tempo</div>
            </div>
            
            <div class="control-dial">
                <div class="dial" id="brightnessDial" data-min="0.1" data-max="1.0" data-value="0.6">
                    <div class="dial-track" style="--angle: 135deg;"></div>
                    <div class="dial-background"></div>
                    <div class="dial-icon">☀</div>
                    <div class="dial-value">60%</div>
                </div>
                <div class="dial-label">Brightness</div>
            </div>
            
            <div class="control-dial">
                <div class="dial" id="sensitivityDial" data-min="0.01" data-max="0.20" data-value="0.05">
                    <div class="dial-track" style="--angle: 56deg;"></div>
                    <div class="dial-background"></div>
                    <div class="dial-icon">—</div>
                    <div class="dial-value">0.05</div>
                </div>
                <div class="dial-label">Sensitivity threshold</div>
            </div>
        </div>
        
        <!-- Brand Label -->
        <div class="brand-label">Aesthetic.</div>
    </div>
    
    <!-- Custom Palette Modal -->
    <div class="modal" id="customPaletteModal">
        <div class="modal-content">
            <div class="modal-header">Create Custom Palette</div>
            
            <div class="color-picker-grid">
                <input type="color" class="color-picker" id="color0" value="#FF0000">
                <input type="color" class="color-picker" id="color1" value="#FF7F00">
                <input type="color" class="color-picker" id="color2" value="#FFFF00">
                <input type="color" class="color-picker" id="color3" value="#00FF00">
                <input type="color" class="color-picker" id="color4" value="#0000FF">
                <input type="color" class="color-picker" id="color5" value="#4B0082">
                <input type="color" class="color-picker" id="color6" value="#9400D3">
            </div>
            
            <input type="text" class="palette-name-input" id="paletteNameInput" placeholder="Palette Name">
            
            <div class="modal-buttons">
                <button class="modal-btn secondary" id="cancelBtn">Cancel</button>
                <button class="modal-btn primary" id="createBtn">Create</button>
            </div>
        </div>
    </div>

    <script>
        let currentData = {};
        let isDragging = false;
        let currentDial = null;
        
        // Predefined palettes
        const palettes = [
            {name: "Rainbow", colors: ["#FF0000", "#FF7F00", "#FFFF00", "#00FF00", "#0000FF", "#4B0082", "#9400D3"]},
            {name: "Ocean", colors: ["#001F3F", "#0074D9", "#7FDBFF", "#39CCCC", "#2ECC40", "#01FF70", "#FFFFFF"]},
            {name: "Fire", colors: ["#000000", "#8B0000", "#FF0000", "#FF4500", "#FF8C00", "#FFD700", "#FFFFFF"]},
            {name: "Ctenophore", colors: ["#000033", "#000066", "#003366", "#0066CC", "#00CCFF", "#66FFFF", "#FFFFFF"]},
            {name: "Sunset", colors: ["#2D1B69", "#11235A", "#1E3A8A", "#3B82F6", "#F59E0B", "#F97316", "#DC2626"]},
            {name: "Cyberpunk", colors: ["#FF00FF", "#FF0080", "#FF0040", "#00FFFF", "#0080FF", "#0040FF", "#8000FF"]},
            {name: "Peppermint", colors: ["#FF0000", "#FFFFFF", "#FF0000", "#FFFFFF", "#FF0000", "#FFFFFF", "#FF0000"]},
            {name: "Aesthetic", colors: ["#000080", "#B0C4DE", "#FF0000", "#FFA500", "#FFFF00", "#FFFFFF", "#FFFFFF"]}
        ];
        
        // Initialize
        initializePalettes();
        setupEventListeners();
        startPolling();
        
        function initializePalettes() {
            const paletteGrid = document.getElementById('paletteGrid');
            
            palettes.forEach((palette, index) => {
                const paletteCard = document.createElement('div');
                paletteCard.className = 'palette-card' + (index === 0 ? ' active' : '');
                paletteCard.dataset.index = index;
                
                const preview = document.createElement('div');
                preview.className = 'palette-preview';
                palette.colors.forEach(color => {
                    const dot = document.createElement('div');
                    dot.className = 'palette-dot';
                    dot.style.backgroundColor = color;
                    preview.appendChild(dot);
                });
                
                const name = document.createElement('div');
                name.className = 'palette-name';
                name.textContent = palette.name;
                
                paletteCard.appendChild(preview);
                paletteCard.appendChild(name);
                paletteGrid.appendChild(paletteCard);
                
                paletteCard.addEventListener('click', () => selectPalette(index));
            });
        }
        
        function setupEventListeners() {
            // Tap button
            document.getElementById('tapButton').addEventListener('click', () => {
                sendCommand('tap');
                animateTap();
            });
            
            // Custom palette button
            document.getElementById('customPaletteBtn').addEventListener('click', () => {
                document.getElementById('customPaletteModal').classList.add('show');
            });
            
            // Liquid mode button
            document.getElementById('liquidBtn').addEventListener('click', () => {
                sendCommand('reset');
            });
            
            // Animation pattern selection
            document.querySelectorAll('[data-pattern]').forEach(card => {
                card.addEventListener('click', () => {
                    // Remove active class from all pattern cards
                    document.querySelectorAll('[data-pattern]').forEach(c => c.classList.remove('active'));
                    // Add active class to clicked card
                    card.classList.add('active');
                    // Send pattern command
                    sendCommand('pattern=' + card.dataset.pattern);
                });
            });
            
            // Modal buttons
            document.getElementById('cancelBtn').addEventListener('click', () => {
                document.getElementById('customPaletteModal').classList.remove('show');
            });
            
            document.getElementById('createBtn').addEventListener('click', createCustomPalette);
            
            // Dial interactions
            setupDialInteractions();
        }
        
        function setupDialInteractions() {
            const dials = document.querySelectorAll('.dial');
            
            dials.forEach(dial => {
                let startAngle = 0;
                let currentValue = parseFloat(dial.dataset.value) || 0;
                const minValue = parseFloat(dial.dataset.min) || 0;
                const maxValue = parseFloat(dial.dataset.max) || 100;
                
                function valueToAngle(value) {
                    const normalized = (value - minValue) / (maxValue - minValue);
                    return normalized * 270; // 270 degrees of rotation
                }
                
                function angleToValue(angle) {
                    const normalized = Math.max(0, Math.min(1, angle / 270));
                    return minValue + (normalized * (maxValue - minValue));
                }
                
                function updateDial(value) {
                    currentValue = Math.max(minValue, Math.min(maxValue, value));
                    const angle = valueToAngle(currentValue);
                    
                    const track = dial.querySelector('.dial-track');
                    const valueDisplay = dial.querySelector('.dial-value');
                    
                    track.style.setProperty('--angle', angle + 'deg');
                    
                    // Update value display based on dial type
                    if (dial.id === 'tempoDial') {
                        valueDisplay.textContent = Math.round(currentValue);
                    } else if (dial.id === 'brightnessDial') {
                        valueDisplay.textContent = Math.round(currentValue * 100) + '%';
                    } else if (dial.id === 'sensitivityDial') {
                        valueDisplay.textContent = currentValue.toFixed(2);
                    }
                    
                    // Send command
                    handleDialChange(dial.id, currentValue);
                }
                
                function handleStart(e) {
                    isDragging = true;
                    currentDial = dial.id;
                    const rect = dial.getBoundingClientRect();
                    const centerX = rect.left + rect.width / 2;
                    const centerY = rect.top + rect.height / 2;
                    const clientX = e.clientX || e.touches[0].clientX;
                    const clientY = e.clientY || e.touches[0].clientY;
                    startAngle = Math.atan2(clientY - centerY, clientX - centerX) * 180 / Math.PI;
                    e.preventDefault();
                }
                
                function handleMove(e) {
                    if (!isDragging || currentDial !== dial.id) return;
                    
                    const rect = dial.getBoundingClientRect();
                    const centerX = rect.left + rect.width / 2;
                    const centerY = rect.top + rect.height / 2;
                    const clientX = e.clientX || e.touches[0].clientX;
                    const clientY = e.clientY || e.touches[0].clientY;
                    const currentMouseAngle = Math.atan2(clientY - centerY, clientX - centerX) * 180 / Math.PI;
                    
                    let delta = currentMouseAngle - startAngle;
                    if (delta > 180) delta -= 360;
                    if (delta < -180) delta += 360;
                    
                    const currentAngle = valueToAngle(currentValue);
                    const newAngle = Math.max(0, Math.min(270, currentAngle + delta));
                    const newValue = angleToValue(newAngle);
                    
                    updateDial(newValue);
                    startAngle = currentMouseAngle;
                    e.preventDefault();
                }
                
                function handleEnd() {
                    isDragging = false;
                    currentDial = null;
                }
                
                // Initialize dial
                updateDial(currentValue);
                
                // Mouse events
                dial.addEventListener('mousedown', handleStart);
                document.addEventListener('mousemove', handleMove);
                document.addEventListener('mouseup', handleEnd);
                
                // Touch events
                dial.addEventListener('touchstart', handleStart);
                document.addEventListener('touchmove', handleMove);
                document.addEventListener('touchend', handleEnd);
            });
        }
        
        function handleDialChange(dialId, value) {
            switch(dialId) {
                case 'tempoDial':
                    sendCommand('bpm=' + Math.round(value));
                    break;
                case 'brightnessDial':
                    sendCommand('brightness=' + value.toFixed(1));
                    break;
                case 'sensitivityDial':
                    sendCommand('threshold=' + value.toFixed(2));
                    break;
            }
        }
        
        function animateTap() {
            const circle = document.getElementById('tapCircle');
            circle.style.transform = 'scale(0.9)';
            setTimeout(() => {
                circle.style.transform = 'scale(1)';
            }, 100);
        }
        
        function selectPalette(index) {
            document.querySelectorAll('.palette-card').forEach(card => card.classList.remove('active'));
            document.querySelector(`[data-index="${index}"]`).classList.add('active');
            sendCommand('palette=' + index);
        }
        
        function createCustomPalette() {
            const name = document.getElementById('paletteNameInput').value.trim();
            if (!name) {
                alert('Please enter a palette name!');
                return;
            }
            
            const colors = [];
            for (let i = 0; i < 7; i++) {
                colors.push(document.getElementById(`color${i}`).value);
            }
            
            // Add to local palettes array
            const newPalette = {name: name, colors: colors};
            palettes.push(newPalette);
            
            // Rebuild palette grid to show new palette
            initializePalettes();
            
            // Send to device
            const paletteData = {
                name: name,
                colors: colors.map(c => c.replace('#', ''))
            };
            
            sendCommand('customPalette=' + JSON.stringify(paletteData));
            
            // Close modal and clear form
            document.getElementById('customPaletteModal').classList.remove('show');
            document.getElementById('paletteNameInput').value = '';
            
            // Select the new palette (it's now the last one)
            const newPaletteIndex = palettes.length - 1;
            selectPalette(newPaletteIndex);
        }
        
        function sendCommand(command) {
            fetch('/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: command })
            }).catch(err => console.error('Command failed:', err));
        }
        
        function startPolling() {
            setInterval(() => {
                fetch('/status')
                    .then(response => response.json())
                    .then(data => {
                        currentData = data;
                        updateUI();
                    })
                    .catch(err => console.error('Status poll failed:', err));
            }, 100); // Faster polling for more responsive LED updates
        }
        
        function updateUI() {
            if (!currentData) return;
            
            // Update BPM
            document.getElementById('bpmDisplay').textContent = currentData.bpm || 0;
            
            // Update battery
            const battery = currentData.batteryPercent || 75;
            document.getElementById('batteryFill').style.width = battery + '%';
            
            // Update mode
            const isLiquid = currentData.mode === 'liquid';
            const modeIndicator = document.getElementById('modeIndicator');
            modeIndicator.innerHTML = isLiquid ? 
                '<div class="mode-icon">💧</div><div class="mode-text">Liquid</div>' :
                '<div class="mode-icon">🎵</div><div class="mode-text">Tempo</div>';
            
            // Update LEDs to match current palette
            const ledStates = currentData.leds || [];
            const currentPalette = palettes[currentData.currentPalette || 0];
            
            for (let i = 0; i < 7; i++) {
                const led = document.getElementById(`led${i}`);
                const brightness = ledStates[i] || 0;
                
                if (brightness > 0.1) {
                    let color;
                    if (currentPalette) {
                        // Use palette color for this LED
                        const colorIndex = Math.floor((i / 7) * currentPalette.colors.length);
                        color = currentPalette.colors[colorIndex];
                    } else {
                        // Fallback to hue-based coloring
                        const hue = i * 51.4;
                        color = `hsl(${hue}, 70%, 60%)`;
                    }
                    
                    led.style.backgroundColor = color;
                    led.style.boxShadow = `0 0 15px ${color}`;
                    led.classList.add('active');
                } else {
                    led.style.backgroundColor = '#e0e0e0';
                    led.style.boxShadow = 'none';
                    led.classList.remove('active');
                }
            }
            
            // Update tilt sliders - fixed calculation
            const tiltX = currentData.tiltAngle || 0; // Use main tiltAngle from device
            const accelY = currentData.accelY || 0;
            const accelZ = currentData.accelZ || 1; // Default to 1 for gravity
            
            // Convert accelerometer values (-1 to 1) to slider positions (0 to 100%)
            const x = ((tiltX + 1) / 2) * 100;
            const y = ((accelY + 1) / 2) * 100;
            const z = ((accelZ + 1) / 2) * 100;
            
            // Get actual container width for proper positioning
            const sliderWidth = document.querySelector('.slider-container')?.offsetWidth || 100;
            const thumbWidth = 21;
            const maxLeft = ((sliderWidth - thumbWidth) / sliderWidth) * 100;
            
            const thumb1 = document.getElementById('tiltThumb1');
            const thumb2 = document.getElementById('tiltThumb2');
            const thumb3 = document.getElementById('tiltThumb3');
            
            if (thumb1) thumb1.style.left = Math.max(0, Math.min(maxLeft, x)) + '%';
            if (thumb2) thumb2.style.left = Math.max(0, Math.min(maxLeft, y)) + '%';
            if (thumb3) thumb3.style.left = Math.max(0, Math.min(maxLeft, z)) + '%';
            
            // Animate tap button on beat
            if (currentData.beat) {
                animateTap();
            }
            
            // Update palette selection
            if (currentData.currentPalette !== undefined) {
                document.querySelectorAll('.palette-card').forEach(card => card.classList.remove('active'));
                const activeCard = document.querySelector(`[data-index="${currentData.currentPalette}"]`);
                if (activeCard) {
                    activeCard.classList.add('active');
                }
            }
            
            // Update pattern selection
            if (currentData.currentPattern !== undefined) {
                const patterns = ['rainbow', 'breathing', 'chase', 'sparkle', 'strobe', 'fade'];
                document.querySelectorAll('[data-pattern]').forEach(card => card.classList.remove('active'));
                const activePattern = document.querySelector(`[data-pattern="${patterns[currentData.currentPattern] || 'rainbow'}"]`);
                if (activePattern) {
                    activePattern.classList.add('active');
                }
            }
        }
        
        // Close modal when clicking outside
        document.getElementById('customPaletteModal').addEventListener('click', (e) => {
            if (e.target.classList.contains('modal')) {
                document.getElementById('customPaletteModal').classList.remove('show');
            }
        });
        
        // Prevent scrolling on mobile when interacting with dials
        document.addEventListener('touchmove', (e) => {
            if (isDragging) {
                e.preventDefault();
            }
        }, { passive: false });
    </script>
</body>
</html>
)rawliteral";

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
void setLEDColorRGB(int led, uint8_t r, uint8_t g, uint8_t b);
void updateLEDs();
void checkIdleTimeout();
void checkBatteryLevel();
void showBatteryLevel();
int calculateBatteryPercentage(float voltage);
void setupWiFi();
void setupWebServer();
void processWebCommand(String command);
String getStatusJSON();
void triggerRotationSparkle();
void checkPaletteSpin(float gyroZ);
void checkAnimationFlip(float gyroX);


// New light customization functions
void updateAnimations();
void applyColorPalette();
void updateBreathingEffect();
void updateChaseEffect();
void updateSparkleEffect();
void updateFadeEffect();
uint32_t interpolateColor(uint32_t color1, uint32_t color2, float factor);
uint32_t adjustColorTemperature(uint32_t color, float temperature);
uint32_t hexToColor(String hex);

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
  
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();
  
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission();
  
  Serial.println("🚀 Ready for liquid physics with 7 LEDs!");
}

void readMPU() {
  if (!mpuAvailable) return;
  
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(mpuAddress, (byte)14); // Read accel + gyro
  
  if (Wire.available() >= 14) {
    // Accelerometer
    int16_t rawX = Wire.read() << 8 | Wire.read();
    int16_t rawY = Wire.read() << 8 | Wire.read();
    int16_t rawZ = Wire.read() << 8 | Wire.read();
    
    // Skip temperature (2 bytes)
    Wire.read(); Wire.read();
    
    // Gyroscope
    int16_t rawGX = Wire.read() << 8 | Wire.read();
    int16_t rawGY = Wire.read() << 8 | Wire.read();
    int16_t rawGZ = Wire.read() << 8 | Wire.read();
    
    accelX = rawX / 16384.0;
    accelY = rawY / 16384.0;
    accelZ = rawZ / 16384.0;
    
    tiltAngle = constrain(accelX, -1.0, 1.0);
    
    // Gyroscope for rotation detection
   float gyroX = rawGX / 131.0; // X-axis rotation (Do a Barrel Roll!)
   float gyroZ = rawGZ / 131.0; // Z-axis rotation (Oh we Oh - Tail Spin!)

    // Detect rotation for palette cycling
   checkPaletteSpin(gyroZ);     // Z-axis always changes colors
   checkAnimationFlip(gyroX);   // X-axis always changes animations
    
    // Original motion detection
    float currentMagnitude = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
    float motionDelta = abs(currentMagnitude - lastAccelMagnitude);
    
    if (motionDelta > motionThreshold) {
      isMoving = true;
      lastMotionTime = millis();
    } else if (millis() - lastMotionTime > motionTimeout) {
      isMoving = false;
    }
    
    lastAccelMagnitude = currentMagnitude;
    checkAnyMovement();
    checkDeviceTap();
  }
}

void checkAnyMovement() {
  if (!mpuAvailable) return;
  
  float currentMagnitude = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
  float motionDelta = abs(currentMagnitude - lastAccelMagnitude);
  
  if (motionDelta > motionThreshold && (millis() - lastTriggerTime) > shakeDebounce) {
    lastTriggerTime = millis();
    handleMovementTrigger();
  }
}

void handleMovementTrigger() {
  lastActivity = millis();
  unsigned long currentTime = millis();
  
  if (liquidMode) {
    Serial.println("🌊➡️🎵 TAP! Switching to tempo mode!");
    liquidMode = false;
    tempoModeActive = true;
    tempoModeStartTime = currentTime;
    pressCount = 0;
    tempoLocked = false;
    autoStrobing = false;
  }
  
  if (!liquidMode) {

 if (autoStrobing) {
      autoStrobing = false;
      pressCount = 0;
    }

    pressCount++;
    pressHistory[pressCount - 1] = currentTime;  // Store current tap time
    
    Serial.print("🎵 Tempo trigger "); Serial.print(pressCount);
    
    startStrobe();

    // START TEMPO PREDICTION ON 3RD TAP, adjust continuously after
    if (pressCount >= 3) {
      calculateAndUpdateTempo(currentTime);
    } else {
      Serial.println(" - Need one more tap for tempo prediction...");
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
        tapThreshold = newThreshold * 16;
        Serial.print("🎛️ Motion threshold set to: ");
        Serial.print(motionThreshold, 3);
        Serial.print(" | Tap threshold: ");
        Serial.println(tapThreshold, 3);
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
  float totalAccel = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
  
  tapHistory[0] = tapHistory[1];
  tapHistory[1] = tapHistory[2];
  tapHistory[2] = tapHistory[3];
  tapHistory[3] = tapHistory[4];
  tapHistory[4] = totalAccel;
  
  float avgAccel = (tapHistory[0] + tapHistory[1] + tapHistory[2] + tapHistory[3] + tapHistory[4]) / 5.0;
  float spikeAboveBaseline = totalAccel - avgAccel;
  
  if (spikeAboveBaseline > tapThreshold && (currentTime - lastTapTime) > tapDebounce) {
    Serial.print("👆 Tap detected! Spike: ");
    Serial.print(spikeAboveBaseline);
    Serial.print(" | Total: ");
    Serial.println(totalAccel);
    
    lastTapTime = currentTime;
    handleMovementTrigger();
  }
  
  lastTotalAccel = totalAccel;
}

void checkPaletteSpin(float gyroZ) {
  unsigned long currentTime = millis();
  
  if (abs(gyroZ) > 1) { // Spinning fast enough
    cumulativeZRotation += gyroZ * 0.01;
    lastZRotationTime = currentTime;
    
    if (abs(cumulativeZRotation) >= 180.0) {
      if (!hasTriggeredZRoll) {
        hasTriggeredZRoll = true;
        
        Serial.println("🌀 Z-SPIN DETECTED! Cycling palette!");
        
        if (cumulativeZRotation > 0) {
          currentPaletteIndex = (currentPaletteIndex + 1) % totalPaletteCount;
        } else {
          currentPaletteIndex = (currentPaletteIndex - 1 + totalPaletteCount) % totalPaletteCount;
        }
        
        Serial.println("🎨 Palette changed to: " + String(currentPaletteIndex));
        triggerRotationSparkle();
        cumulativeZRotation = 0;
      }
    }
  }
  
  // Reset when stopped spinning
  if (abs(cumulativeZRotation) < 45.0 || (currentTime - lastZRotationTime > 1000)) {
    hasTriggeredZRoll = false;
  }
  if (currentTime - lastZRotationTime > 5000) {
    cumulativeZRotation = 0;
  }
}

void checkAnimationFlip(float gyroX) {
  unsigned long currentTime = millis();
  
  if (abs(gyroX) > 5) { // Flipping fast enough
    cumulativeXRotation += gyroX * 0.01;
    lastXRotationTime = currentTime;
    
    if (abs(cumulativeXRotation) >= 180.0) {
      if (!hasTriggeredXRoll) {
        hasTriggeredXRoll = true;
        
        Serial.println("🛞 X-FLIP DETECTED! Cycling animation!");
        
        if (cumulativeXRotation > 0) {
          currentPattern = (AnimationPattern)((currentPattern + 1) % 6);
        } else {
          currentPattern = (AnimationPattern)((currentPattern - 1 + 6) % 6);
        }
        
        String patternNames[] = {"Rainbow", "Breathing", "Chase", "Sparkle", "Strobe", "Fade"};
        Serial.println("✨ Animation changed to: " + patternNames[currentPattern]);
        triggerRotationSparkle();
        cumulativeXRotation = 0;
      }
    }
  }
  
  // Reset when stopped flipping
  if (abs(cumulativeXRotation) < 45.0 || (currentTime - lastXRotationTime > 1000)) {
    hasTriggeredXRoll = false;
  }
  if (currentTime - lastXRotationTime > 5000) {
    cumulativeXRotation = 0;
  }
}

void triggerRotationSparkle() {
  // Quick sparkle effect for rotation feedback
  for(int i = 0; i < NUM_LEDS; i++) {
    sparkleStates[i] = true;
    sparkleTimers[i] = millis();
    liquidLevels[i] = 1.0;
  }
  Serial.println("✨ Rotation sparkle triggered!");
}

void calculateAndUpdateTempo(unsigned long currentTime) {
  unsigned long avgInterval = 0;
  int intervalCount = 0;

  // TAP 3: First prediction (use average of first 2 intervals)
  if (pressCount == 3) {
    unsigned long interval1 = pressHistory[1] - pressHistory[0];
    unsigned long interval2 = pressHistory[2] - pressHistory[1];
    avgInterval = (interval1 + interval2) / 2;
    intervalCount = 2;
    Serial.println("🎯 FIRST PREDICTION from 3 taps!");

  // TAP 4+: Continuous adjustment with sliding window
  } else if (pressCount >= 4) {
    // Use last 4 taps for sliding window (better than just 3)
    if (pressCount == 4) {
      // First time with 4 taps - use all 3 intervals
      unsigned long interval1 = pressHistory[1] - pressHistory[0];
      unsigned long interval2 = pressHistory[2] - pressHistory[1];
      unsigned long interval3 = pressHistory[3] - pressHistory[2];

      // Weight recent intervals more heavily (exponential weighting)
      avgInterval = (interval1 + interval2 * 2 + interval3 * 4) / 7;
      intervalCount = 3;
      Serial.println("🎯 ADJUSTING tempo (tap 4)");

    } else {
      // TAP 5+: Shift window and continuously recalculate
      // Shift array left to make room for new tap
      for(int i = 0; i < 3; i++) {
        pressHistory[i] = pressHistory[i + 1];
      }
      pressHistory[3] = currentTime;
      pressCount = 4; // Keep at 4 for sliding window

      // Recalculate with new sliding window
      unsigned long interval1 = pressHistory[1] - pressHistory[0];
      unsigned long interval2 = pressHistory[2] - pressHistory[1];
      unsigned long interval3 = pressHistory[3] - pressHistory[2];

      // Exponential weighting: most recent tap has highest influence
      avgInterval = (interval1 + interval2 * 2 + interval3 * 4) / 7;
      intervalCount = 3;
      Serial.println("🔄 CONTINUOUSLY ADJUSTING tempo!");
      tempoLocked = true;
    }
  }

  // Update tempo values
  tempoInterval = avgInterval;
  bpm = 60000 / avgInterval;

  // Clamp BPM to reasonable range
  if(bpm < 30) { bpm = 30; tempoInterval = 60000 / 30; }
  if(bpm > 300) { bpm = 300; tempoInterval = 60000 / 300; }

  Serial.print("📊 BPM: "); Serial.print(bpm);
  Serial.print(" ("); Serial.print(tempoInterval); Serial.print("ms)");
  Serial.print(" | Window: "); Serial.print(intervalCount); Serial.println(" intervals");

  // Only initialize beat timing on FIRST prediction (tap 3)
  if (!autoStrobing) {
    autoStrobing = true;
    nextBeatTime = currentTime + tempoInterval;
    Serial.println("🎵 Starting beat sync!");
  } else {
    // For subsequent adjustments, smoothly transition without jarring the beat
    // Don't reset nextBeatTime - let drift correction handle it
    Serial.println("✨ Tempo adjusted dynamically!");
  }

  lastTempoTime = currentTime;

  // Update tempo-reactive colors
  if (tempoColorReactive) {
    temperatureShift = map(bpm, 30, 300, -1.0, 1.0);
  }
}

void stopSequence() {
  strobing = false;
  autoStrobing = false;
  pressCount = 0;
  bpm = 0;
  liquidMode = true;
  tempoModeActive = false;
  temperatureShift = 0;
  
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
  
  if (!mpuAvailable) {
    for(int i = 0; i < NUM_LEDS; i++) {
      targetLevels[i] = dimBrightness;
    }
    targetLevels[3] = 1.0;
  } else {
    for(int i = 0; i < NUM_LEDS; i++) {
      targetLevels[i] = dimBrightness;
    }
    
    if (abs(tiltAngle) < 0.15) {
      targetLevels[3] = 1.0;
    } else {
      float ledPosition = 3.0 + tiltAngle * 3.0;
      ledPosition = constrain(ledPosition, 0, 6);
      
      int mainLED = round(ledPosition);
      mainLED = constrain(mainLED, 0, NUM_LEDS-1);
      
      targetLevels[mainLED] = 1.0;
      
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
  
  float smoothingFactor = 0.08;
  for(int i = 0; i < NUM_LEDS; i++) {
    liquidLevels[i] += (targetLevels[i] - liquidLevels[i]) * smoothingFactor;
  }
  
  checkLiquidBatteryTrigger();
}

void checkLiquidBatteryTrigger() {
  if (!liquidMode || showingBatteryLevel) return;
  
 // if (liquidLevels[6] > 0.8) {
 //   Serial.println("🔋 Liquid reached end! Showing battery level...");
 //   checkBatteryLevel();
 //   showBatteryLevel();
 // }
}

void doRippleEffect() {
  wavePosition += waveSpeed;
  
  for(int i = 0; i < NUM_LEDS; i++) {
    float distance = abs(i - wavePosition);
    
    float rippleBrightness = 0;
    if (distance <= trailLength) {
      rippleBrightness = cos(distance * PI / (trailLength * 2)) * maxBrightness;
      if (rippleBrightness < 0) rippleBrightness = 0;
      liquidLevels[i] = max(liquidLevels[i], rippleBrightness);
    }
    
    if (distance > trailLength) {
      liquidLevels[i] *= 0.85;
      if (liquidLevels[i] < dimBrightness) liquidLevels[i] = dimBrightness;
    }
  }
  
  globalHueShift += 1.5;
  if (globalHueShift >= 360) globalHueShift -= 360;
}

// New color customization functions
void updateAnimations() {
  unsigned long currentTime = millis();
  if (currentTime - lastAnimationUpdate < animationInterval) return;
  
  lastAnimationUpdate = currentTime;
  
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
      // Handled in tempo system
      break;
    case PATTERN_RAINBOW_CYCLE:
    default:
      // Default rainbow behavior
      globalHueShift += 0.5;
      if (globalHueShift >= 360) globalHueShift -= 360;
      break;
  }
}

void updateBreathingEffect() {
  breathPhase += 0.05;
  float pulse = 0.3 + 0.7 * (sin(breathPhase) + 1) / 2;
  
  for(int i = 0; i < NUM_LEDS; i++) {
    liquidLevels[i] = liquidLevels[i] * pulse;
  }
}

void updateChaseEffect() {
  // Clear all LEDs
  for(int i = 0; i < NUM_LEDS; i++) {
    liquidLevels[i] = dimBrightness;
  }
  
  // Set chase LED
  liquidLevels[chasePosition] = 1.0;
  
  // Move chase position
  if (chaseDirection) {
    chasePosition++;
    if (chasePosition >= NUM_LEDS) {
      chasePosition = NUM_LEDS - 1;
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
  for(int i = 0; i < NUM_LEDS; i++) {
    if (!sparkleStates[i] && random(100) < 5) { // 5% chance per frame
      sparkleStates[i] = true;
      sparkleTimers[i] = millis();
      liquidLevels[i] = 1.0;
    }
    
    // Fade out sparkles
    if (sparkleStates[i] && millis() - sparkleTimers[i] > 500) {
      sparkleStates[i] = false;
      liquidLevels[i] = dimBrightness;
    }
  }
}

void updateFadeEffect() {
  static float fadePhase = 0;
  fadePhase += 0.02;
  
  for(int i = 0; i < NUM_LEDS; i++) {
    float phase = fadePhase + (i * 0.3);
    liquidLevels[i] = 0.2 + 0.8 * (sin(phase) + 1) / 2;
  }
}

void applyColorPalette() {
  ColorPalette* palette;
  if (currentPaletteIndex < paletteCount) {
    palette = &palettes[currentPaletteIndex];
  } else {
    palette = &customPalettes[currentPaletteIndex - paletteCount];  // Fixed index calculation
  }
  
  // Handle tilt-based palette switching
  if (useTiltPalettes && mpuAvailable) {
    int targetPalette = currentPaletteIndex;
    
    for(int i = 0; i < 3; i++) {
      if (tiltAngle >= tiltZones[i].tiltMin && tiltAngle <= tiltZones[i].tiltMax) {
        targetPalette = tiltZones[i].paletteIndex;
        break;
      }
    }
    
    // Smooth transition between palettes
    if (targetPalette != currentPaletteIndex) {
      static float transitionProgress = 0;
      transitionProgress += tiltTransitionSmoothing;
      
      if (transitionProgress >= 1.0) {
        currentPaletteIndex = targetPalette;
        transitionProgress = 0;
      }
    }
    
    palette = &palettes[currentPaletteIndex];
  }
  
  // Apply colors to LEDs
  for(int i = 0; i < NUM_LEDS; i++) {
    uint32_t color;
    
    if (useCustomColors && customLEDColors[i] != 0) {
      color = customLEDColors[i];
    } else {
      int colorIndex = (i * palette->colorCount) / NUM_LEDS;
      colorIndex = constrain(colorIndex, 0, palette->colorCount - 1);
      color = palette->colors[colorIndex];
      
      // Apply hue shift for rainbow mode
      if (currentPaletteIndex == 0) { // Rainbow palette
        float hue = (i * 51.4 + globalHueShift);
        setLEDColorHSV(i, hue, liquidLevels[i]);
        continue;
      }
    }
    
    // Apply tempo-reactive color temperature
    if (tempoColorReactive) {
      color = adjustColorTemperature(color, temperatureShift);
    }
    
    // Extract RGB and apply brightness
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    float brightness = liquidLevels[i] * maxBrightness;
    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);
    
    setLEDColorRGB(i, r, g, b);
  }
}

uint32_t adjustColorTemperature(uint32_t color, float temperature) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  if (temperature > 0) { // Warmer (more red/yellow)
    r = min(255, (int)(r * (1.0 + temperature * 0.3)));
    g = min(255, (int)(g * (1.0 + temperature * 0.1)));
    b = max(0, (int)(b * (1.0 - temperature * 0.2)));
  } else { // Cooler (more blue)
    temperature = -temperature;
    r = max(0, (int)(r * (1.0 - temperature * 0.2)));
    g = max(0, (int)(g * (1.0 - temperature * 0.1)));
    b = min(255, (int)(b * (1.0 + temperature * 0.3)));
  }
  
  return (r << 16) | (g << 8) | b;
}

uint32_t hexToColor(String hex) {
  hex.replace("#", "");
  long number = strtol(hex.c_str(), NULL, 16);
  return (uint32_t)number;
}

void setLEDColorRGB(int led, uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(led, r, g, b);
}

void setLEDColorHSV(int led, float hue, float brightness) {
  brightness = constrain(brightness * maxBrightness, 0.0, 1.0);
  hue = fmod(hue, 360.0);
  
  float c = brightness;
  float x = c * (1 - abs(fmod(hue / 60.0, 2) - 1));
  float r, g, b;
  
  if (hue < 60) { r = c; g = x; b = 0; }
  else if (hue < 120) { r = x; g = c; b = 0; }
  else if (hue < 180) { r = 0; g = c; b = x; }
  else if (hue < 240) { r = 0; g = x; b = c; }
  else if (hue < 300) { r = x; g = 0; b = c; }
  else { r = c; g = 0; b = x; }
  
  strip.setPixelColor(led, (int)(r * 255), (int)(g * 255), (int)(b * 255));
}

void updateLEDs() {
  if (showingBatteryLevel) return;
  
  // Update animations first
  updateAnimations();
  
  // Apply color palette
  applyColorPalette();
  
  strip.show();
}

void checkIdleTimeout() {
  if (tempoModeActive && (millis() - tempoModeStartTime) > tempoModeTimeout) {
    Serial.println("⏰ 60 seconds of tempo mode - returning to liquid mode");
    stopSequence();
    return;
  }
  
  if (!liquidMode && millis() - lastActivity > idleTimeout) {
    Serial.println("⏰ 5 minutes idle - returning to liquid mode");
    stopSequence();
  }
}

int calculateBatteryPercentage(float voltage) {
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.3) return 0;
  return (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);
}

void checkBatteryLevel() {
  unsigned long currentTime = millis();
  int rawReading = analogRead(BATTERY_PIN);
  
  // Better voltage calculation with smoothing
  static float smoothedVoltage = 0;
  float currentVoltage = (rawReading / 4095.0) * 3.3 * 2.0;
  
  if (smoothedVoltage == 0) smoothedVoltage = currentVoltage; // Initialize
  smoothedVoltage = smoothedVoltage * 0.9 + currentVoltage * 0.1; // Smooth readings
  
  batteryVoltage = smoothedVoltage;
  batteryPercentage = calculateBatteryPercentage(batteryVoltage);
  
  // More conservative low battery warning
  if (batteryPercentage <= 15 && !lowBatteryWarning) {
    lowBatteryWarning = true;
    Serial.println("⚠️ LOW BATTERY WARNING! ⚠️");
    Serial.print("Raw ADC: "); Serial.print(rawReading);
    Serial.print(" | Voltage: "); Serial.print(batteryVoltage, 2);
    Serial.print("V | Percent: "); Serial.print(batteryPercentage);
    Serial.println("%");
  } else if (batteryPercentage > 20) {
    lowBatteryWarning = false;
  }
  
  // Debug battery readings every minute
  static unsigned long lastBatteryDebug = 0;
  if (currentTime - lastBatteryDebug > 60000) {
    Serial.print("🔋 Battery check - Raw: "); Serial.print(rawReading);
    Serial.print(" | Voltage: "); Serial.print(batteryVoltage, 2);
    Serial.print("V | Percent: "); Serial.print(batteryPercentage); Serial.println("%");
    lastBatteryDebug = currentTime;
  }
}

void showBatteryLevel() {
  showingBatteryLevel = true;
  strip.clear();
  
  if (batteryPercentage >= 80) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0, 255, 0);
    }
  } else if (batteryPercentage >= 20) {
    int ledsToLight = map(batteryPercentage, 20, 79, 1, NUM_LEDS);
    for (int i = 0; i < ledsToLight; i++) {
      strip.setPixelColor(i, 255, 255, 0);
    }
  } else {
    int ledsToLight = map(batteryPercentage, 1, 19, 1, NUM_LEDS);
    for (int i = 0; i < ledsToLight; i++) {
      strip.setPixelColor(i, 255, 0, 0);
    }
  }
  
  strip.show();
  delay(2000);
  
  Serial.print("🔋 Battery: ");
  Serial.print(batteryPercentage);
  Serial.print("% (");
  Serial.print(batteryVoltage, 2);
  Serial.println("V)");
  
  showingBatteryLevel = false;
}

String getStatusJSON() {
  StaticJsonDocument<512> doc;
  
  doc["bpm"] = bpm;
  doc["mode"] = liquidMode ? "liquid" : "tempo";
  doc["batteryPercent"] = batteryPercentage;
  doc["batteryVoltage"] = batteryVoltage;
  doc["tilt"] = tiltAngle;
  doc["isMoving"] = isMoving;
  doc["autoStrobing"] = autoStrobing;
  doc["pressCount"] = pressCount;
  doc["motionThreshold"] = motionThreshold;
  doc["maxBrightness"] = maxBrightness;
  doc["currentPalette"] = currentPaletteIndex;
  doc["currentPattern"] = (int)currentPattern;
  doc["useTiltPalettes"] = useTiltPalettes;
  doc["tempoColorReactive"] = tempoColorReactive;
  doc["useCustomColors"] = useCustomColors;
  doc["accelX"] = accelX;
  doc["accelY"] = accelY; 
  doc["accelZ"] = accelZ;


  JsonArray ledArray = doc.createNestedArray("leds");
  for(int i = 0; i < NUM_LEDS; i++) {
    ledArray.add(liquidLevels[i]);
  }
  
  static bool beatTriggered = false;
  if (strobing && millis() - lastStrobeTime < 100) {
    beatTriggered = true;
  }
  doc["beat"] = beatTriggered;
  beatTriggered = false;
  
  String output;
  serializeJson(doc, output);
  return output;
}

void processWebCommand(String command) {
  Serial.println("📱 Web command: " + command);
  
  if (command == "tap") {
    handleMovementTrigger();
  } else if (command == "reset") {
    stopSequence();
  } else if (command.startsWith("threshold=")) {
    float newThreshold = command.substring(10).toFloat();
    if (newThreshold > 0 && newThreshold < 1.0) {
      motionThreshold = newThreshold;
      tapThreshold = newThreshold * 16;
      Serial.println("🎛️ Threshold updated via web: " + String(motionThreshold, 3));
    }
  } else if (command.startsWith("brightness=")) {
    float newBrightness = command.substring(11).toFloat();
    if (newBrightness >= 0.1 && newBrightness <= 1.0) {
      maxBrightness = newBrightness;
      Serial.println("💡 Brightness updated via web: " + String(maxBrightness, 1));
    }
  } else if (command.startsWith("bpm=")) {
    int newBPM = command.substring(4).toInt();
    if (newBPM >= 30 && newBPM <= 300) {
      bpm = newBPM;
      tempoInterval = 60000 / bpm;
      autoStrobing = true;
      liquidMode = false;
      tempoModeActive = true;
      tempoModeStartTime = millis();
      lastTempoTime = millis();
      Serial.println("🎵 BPM set via web: " + String(bpm));
    }
} else if (command.startsWith("palette=")) {
    int paletteIndex = command.substring(8).toInt();
    if (paletteIndex >= 0 && paletteIndex < paletteCount) {  // Use paletteCount instead of totalPaletteCount
      currentPaletteIndex = paletteIndex;
      Serial.println("🎨 Palette changed to: " + palettes[paletteIndex].name);
    }
  } else if (command.startsWith("pattern=")) {
    String pattern = command.substring(8);
    if (pattern == "rainbow") currentPattern = PATTERN_RAINBOW_CYCLE;
    else if (pattern == "breathing") currentPattern = PATTERN_BREATHING;
    else if (pattern == "chase") currentPattern = PATTERN_CHASE;
    else if (pattern == "sparkle") currentPattern = PATTERN_SPARKLE;
    else if (pattern == "strobe") currentPattern = PATTERN_STROBE;
    else if (pattern == "fade") currentPattern = PATTERN_FADE;
    Serial.println("✨ Animation pattern changed to: " + pattern);
    // Add this inside your processWebCommand function, after the existing else if statements:
} else if (command.startsWith("customPalette=")) {
  String paletteJson = command.substring(14);
  StaticJsonDocument<512> doc;
  
  if (deserializeJson(doc, paletteJson) == DeserializationError::Ok) {
    String name = doc["name"];
    JsonArray colors = doc["colors"];
    
    if (customPaletteCount < 10 && colors.size() == 7) {
      customPalettes[customPaletteCount].name = name;
      customPalettes[customPaletteCount].colorCount = 7;
      
      for (int i = 0; i < 7; i++) {
        String hexColor = colors[i];
        customPalettes[customPaletteCount].colors[i] = strtol(hexColor.c_str(), NULL, 16);
      }
      
      customPaletteCount++;
      totalPaletteCount = paletteCount + customPaletteCount;  // Fixed calculation
      Serial.println("🎨 Custom palette '" + name + "' saved!");
      
      // Select the new custom palette immediately
      currentPaletteIndex = paletteCount + customPaletteCount - 1;
    }
  }
  } else if (command.startsWith("animationSpeed=")) {
    int speed = command.substring(15).toInt();
    if (speed >= 10 && speed <= 200) {
      animationInterval = speed;
      Serial.println("⚡ Animation speed set to: " + String(speed));
    }
  } else if (command.startsWith("tiltPalettes=")) {
    useTiltPalettes = (command.substring(13) == "true");
    Serial.println("🌊 Tilt palettes: " + String(useTiltPalettes ? "ON" : "OFF"));
  } else if (command.startsWith("tempoColors=")) {
    tempoColorReactive = (command.substring(12) == "true");
    Serial.println("🎵 Tempo colors: " + String(tempoColorReactive ? "ON" : "OFF"));
  } else if (command.startsWith("customLEDs=")) {
    useCustomColors = (command.substring(11) == "true");
    if (!useCustomColors) {
      // Clear custom colors
      for(int i = 0; i < NUM_LEDS; i++) {
        customLEDColors[i] = 0;
      }
    }
    Serial.println("💡 Custom LED colors: " + String(useCustomColors ? "ON" : "OFF"));
  } else if (command.startsWith("ledColor=")) {
    // Format: ledColor=0,#FF0000
    int commaIndex = command.indexOf(',');
    if (commaIndex > 0) {
      int ledIndex = command.substring(9, commaIndex).toInt();
      String colorHex = command.substring(commaIndex + 1);
      
      if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
        customLEDColors[ledIndex] = hexToColor(colorHex);
        Serial.println("🎨 LED " + String(ledIndex) + " color set to: " + colorHex);
      }
    }
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", dashboard_html);
  });
  
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String status = getStatusJSON();
    request->send(200, "application/json", status);
  });
  
  server.on("/command", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      String body = String((char*)data).substring(0, len);
      
      StaticJsonDocument<128> doc;
      DeserializationError error = deserializeJson(doc, body);
      
      if (!error) {
        String command = doc["command"];
        processWebCommand(command);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
      }
    });
  
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Not found");
  });
  
  server.begin();
  Serial.println("🌐 Web server started!");
}

void setupWiFi() {
  Serial.println("🔧 Starting WiFi setup...");
  WiFi.mode(WIFI_AP);
  delay(100);
  
  bool result = WiFi.softAP(ssid, password);
  Serial.println("WiFi softAP result: " + String(result));
  
  IPAddress IP = WiFi.softAPIP();
  
  Serial.println("🔥 Ctenophore hotspot created!");
  Serial.println("📶 Network: " + String(ssid));
  Serial.println("🔑 Password: " + String(password));
  Serial.println("🌐 Dashboard: http://" + IP.toString());
  Serial.println("💡 Usually http://192.168.4.1");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // ===== POWER MANAGEMENT FIXES =====
  // Disable automatic light sleep to prevent random shutdowns
  esp_pm_config_t pm_config;
  pm_config.max_freq_mhz = 160;          // Max CPU frequency
  pm_config.min_freq_mhz = 10;           // Min CPU frequency  
  pm_config.light_sleep_enable = false;  // DISABLE automatic sleep
  esp_pm_configure(&pm_config);
  
  // Disable watchdog timer that might be causing resets
  // esp_task_wdt_deinit();
  
  // Keep WiFi power management off
  WiFi.setSleep(false);
  
  Serial.println("🔧 Power management configured - no auto-sleep");
  
  pinMode(BATTERY_PIN, INPUT);
  
  strip.begin();
  strip.setBrightness(255);
  strip.clear();
  strip.show();
  
  Serial.println("🌊✨ CTENOPHORE v2.0 - FULL LIGHT CONTROL ✨🌊");
  Serial.println("Features:");
  Serial.println(" 🌊 Real liquid tilt physics via MPU6050");
  Serial.println(" 👆 Device tap detection");
  Serial.println(" 🏃‍♂️ 3-trigger mode switching");
  Serial.println(" 🎵 Automatic tempo detection");
  Serial.println(" 🌈 Advanced color palette system");
  Serial.println(" ✨ Multiple animation patterns");
  Serial.println(" 🎨 Individual LED color control");
  Serial.println(" 🌊 Smooth tilt-based palettes");
  Serial.println(" 🎵 Tempo-reactive coloring");
  Serial.println(" 💡 Battery display when liquid reaches end");
  Serial.println(" 🔋 Battery level monitoring");
  Serial.println(" 🎛️ Adjustable motion sensitivity");
  Serial.println(" 🌐 WiFi web dashboard control");
  Serial.println("");
  
  setupWiFi();
  setupWebServer();
  
  Serial.println("📋 Serial Commands:");
  Serial.println("  threshold=0.08  - Set motion sensitivity");
  Serial.println("  reset          - Return to liquid mode");
  Serial.println("  battery        - Show battery level");
  Serial.println("  help           - Show command menu");
  Serial.println("");
  
  initMPU();
  lastActivity = millis();
  
  // checkBatteryLevel();
  // showBatteryLevel();
  
  Serial.println("🪄 Ready! Tilt for liquid, single tap for tempo!");
  Serial.println("💡 Tempo mode auto-returns to liquid after 60 seconds");
  Serial.println("📱 Connect to WiFi hotspot for advanced light control!");
  Serial.println("🎨 New: 6 color palettes, 6 animation patterns, individual LED control!");
}

void loop() {
  static unsigned long lastMPURead = 0;
  static unsigned long lastDebug = 0;
  unsigned long currentTime = millis();
  
  processSerialCommands();
  
  if (currentTime - lastMPURead > 10) {
    readMPU();
    lastMPURead = currentTime;
  }
  
  // if (currentTime - lastBatteryCheck > batteryCheckInterval) {
  //  checkBatteryLevel();
  //  lastBatteryCheck = currentTime;
  // }
  
  if (liquidMode) {
    updateLiquidPhysics();
  }
  
  if (strobing && currentTime - lastStrobeTime >= strobeInterval) {
    doRippleEffect();
    lastStrobeTime = currentTime;
  }
  
  if (autoStrobing && tempoInterval > 0) {
    if (currentTime >= nextBeatTime) {
      Serial.print("🎵 Auto-beat "); Serial.print(pressCount);
      Serial.print(" ("); Serial.print(bpm); Serial.println(" BPM)");
      
      startStrobe();
      lastActivity = currentTime;
      
      // ADDITIVE TIMING - this prevents drift!
      nextBeatTime += tempoInterval;
      
      // Drift correction: if we're getting too far ahead of real time,
      // gradually sync back (this handles edge cases)
      if (currentTime - lastDriftCorrection > 10000) {  // Every 10 seconds
        long drift = (long)nextBeatTime - (long)currentTime;
        if (abs(drift) > tempoInterval / 4) {  // If drift > 25% of beat interval
          Serial.print("⚡ Drift correction: "); Serial.print(drift); Serial.println("ms");
          nextBeatTime = currentTime + tempoInterval;  // Resync
        }
        lastDriftCorrection = currentTime;
      }
      
      // Safety check: if nextBeatTime is way in the past, resync
      if (nextBeatTime < currentTime - tempoInterval) {
        Serial.println("⚡ Major resync needed");
        nextBeatTime = currentTime + tempoInterval;
      }
    }
  }
  
  checkIdleTimeout();
  updateLEDs();
  
   if (currentTime - lastDebug > 5000) {
    Serial.print("🌊 Mode: "); Serial.print(liquidMode ? "LIQUID" : "TEMPO");
    if (liquidMode && mpuAvailable) {
      Serial.print(" | Tilt: "); Serial.print(tiltAngle, 2);
    } else if (!liquidMode) {
      Serial.print(" | BPM: "); Serial.print(bpm);
      if (autoStrobing) {
        long nextBeat = (long)nextBeatTime - (long)currentTime;
        Serial.print(" | Next: "); Serial.print(nextBeat); Serial.print("ms");
      }
    }
    Serial.print(" | 🔋 "); Serial.print(batteryPercentage); Serial.print("%");
    Serial.print(" | Palette: "); Serial.print(palettes[currentPaletteIndex].name);
    
    // Enhanced pattern display
    String patternNames[] = {"Rainbow", "Breathing", "Chase", "Sparkle", "Strobe", "Fade"};
    Serial.print(" | Pattern: "); Serial.print(patternNames[currentPattern]);
    
    // Show rotation state
   // Show rotation state
if (abs(cumulativeZRotation) > 10 || abs(cumulativeXRotation) > 10) {
  Serial.print(" | Z: "); Serial.print(cumulativeZRotation, 1);
  Serial.print("° X: "); Serial.print(cumulativeXRotation, 1); Serial.print("°");
}

    Serial.println();
    lastDebug = currentTime;
  }
  
  delay(20);
} 