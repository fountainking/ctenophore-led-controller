#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define LED_PIN 10 // D10 on Xiao ESP32-C3
#define BATTERY_PIN A0 // A0 for battery voltage reading (top left pin)
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

// Rotation Variables
float lastGyroZ = 0;
float cumulativeRotation = 0;
float rotationThreshold = 360.0; // Full rotation in degrees
unsigned long lastRotationTime = 0;
bool rotationMode = false;

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
float tapThreshold = 0.8; // Much more sensitive - lowered from 1.2
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
    <title>Ctenophore v2.0 - Light Control</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 50%, #16213e 100%);
            color: #ffffff; min-height: 100vh; overflow-x: hidden;
        }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .header { text-align: center; margin-bottom: 30px; padding: 20px;
            background: rgba(255, 255, 255, 0.05); border-radius: 20px;
            backdrop-filter: blur(10px); border: 1px solid rgba(255, 255, 255, 0.1);
        }
        .header h1 { font-size: 2.5rem;
            background: linear-gradient(45deg, #ff6b6b, #4ecdc4, #45b7d1, #96ceb4);
            -webkit-background-clip: text; -webkit-text-fill-color: transparent;
            background-clip: text; margin-bottom: 10px;
            animation: shimmer 3s ease-in-out infinite; background-size: 300% 300%;
        }
        @keyframes shimmer { 0%, 100% { background-position: 0% 50%; } 50% { background-position: 100% 50%; } }
        .status-bar { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 30px; }
        .status-card { background: rgba(255, 255, 255, 0.08); border-radius: 15px; padding: 20px;
            text-align: center; border: 1px solid rgba(255, 255, 255, 0.1); transition: all 0.3s ease;
        }
        .status-card:hover { transform: translateY(-2px); border-color: rgba(255, 255, 255, 0.2); }
        .connection-status { display: flex; align-items: center; justify-content: center; gap: 10px; font-weight: 500; }
        .status-dot { width: 12px; height: 12px; border-radius: 50%; background: #2ed573; animation: pulse 2s infinite; }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }
        .main-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 25px; margin-bottom: 30px; }
        .panel { background: rgba(255, 255, 255, 0.05); border-radius: 20px; padding: 25px;
            border: 1px solid rgba(255, 255, 255, 0.1); backdrop-filter: blur(10px); transition: all 0.3s ease;
        }
        .panel:hover { transform: translateY(-5px); border-color: rgba(255, 255, 255, 0.2); box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3); }
        .panel h3 { margin-bottom: 20px; font-size: 1.3rem; color: #4ecdc4; text-align: center; }
        
        /* Tempo Display */
        .tempo-display { text-align: center; margin-bottom: 30px; }
        .bpm-number { font-size: 4rem; font-weight: bold; color: #ff6b6b; margin-bottom: 10px; text-shadow: 0 0 20px rgba(255, 107, 107, 0.5); }
        .bpm-label { font-size: 1.2rem; color: #e2e8f0; margin-bottom: 20px; }
        .metronome { width: 80px; height: 80px; margin: 0 auto 20px; border-radius: 50%;
            background: linear-gradient(45deg, #4ecdc4, #45b7d1); display: flex; align-items: center;
            justify-content: center; position: relative; overflow: hidden;
        }
        .metronome.beat { animation: beat 0.3s ease-out; }
        @keyframes beat { 0% { transform: scale(1); } 50% { transform: scale(1.2); box-shadow: 0 0 30px rgba(78, 205, 196, 0.8); } 100% { transform: scale(1); } }
        .metronome::after { content: '♪'; font-size: 2rem; color: white; }
        
        /* LED Display */
        .led-display { display: flex; justify-content: space-between; margin-bottom: 25px; padding: 20px;
            background: rgba(0, 0, 0, 0.3); border-radius: 15px; border: 1px solid rgba(255, 255, 255, 0.1);
        }
        .led { width: 35px; height: 35px; border-radius: 50%; background: #333; border: 2px solid #555;
            transition: all 0.3s ease; position: relative; overflow: hidden; cursor: pointer;
        }
        .led.active { box-shadow: 0 0 20px currentColor; }
        .led:hover { transform: scale(1.1); border-color: #4ecdc4; }
        
        /* Color Palette Selector - FIXED RESPONSIVE */
        .palette-grid { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); 
            gap: 12px; 
            margin-bottom: 20px; 
        }
        .palette-card { 
            background: rgba(255, 255, 255, 0.05); 
            border-radius: 10px; 
            padding: 12px;
            cursor: pointer; 
            transition: all 0.3s ease; 
            border: 2px solid transparent;
            min-width: 0; /* Prevents overflow */
            max-width: 100%; /* Ensures it fits */
        }
        .palette-card:hover { border-color: #4ecdc4; transform: translateY(-2px); }
        .palette-card.active { border-color: #ff6b6b; background: rgba(255, 107, 107, 0.1); }
        .palette-preview { 
            display: flex; 
            justify-content: space-between; 
            margin-bottom: 8px; 
            flex-wrap: wrap; /* Allow wrapping on very small screens */
        }
        .palette-color { 
            width: 14px; 
            height: 14px; 
            border-radius: 50%; 
            margin: 1px; 
            flex-shrink: 0; /* Prevent shrinking */
        }
        .palette-name { 
            text-align: center; 
            font-size: 0.85rem; 
            color: #e2e8f0; 
            white-space: nowrap; 
            overflow: hidden; 
            text-overflow: ellipsis; 
        }
        
        /* Animation Pattern Selector */
        .pattern-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px; margin-bottom: 20px; }
        .pattern-btn { background: rgba(255, 255, 255, 0.1); border: 2px solid transparent; border-radius: 10px;
            padding: 12px; cursor: pointer; transition: all 0.3s ease; text-align: center; color: #e2e8f0;
            font-size: 0.9rem; min-width: 0;
        }
        .pattern-btn:hover { border-color: #4ecdc4; background: rgba(78, 205, 196, 0.1); }
        .pattern-btn.active { border-color: #ff6b6b; background: rgba(255, 107, 107, 0.1); color: #fff; }
        
        /* Controls */
        .btn { background: linear-gradient(45deg, #667eea 0%, #764ba2 100%); color: white; border: none;
            padding: 12px 24px; border-radius: 25px; cursor: pointer; font-size: 1rem; font-weight: 500;
            transition: all 0.3s ease; margin: 5px; width: 100%;
        }
        .btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0, 0, 0, 0.3); }
        .btn.primary { background: linear-gradient(45deg, #ff6b6b, #4ecdc4); }
        .btn.secondary { background: linear-gradient(45deg, #a8edea, #fed6e3); color: #333; }
        .tap-btn { width: 120px; height: 120px; border-radius: 50%; font-size: 1.5rem; margin: 10px auto; display: flex; align-items: center; justify-content: center; }
        .mode-indicator { display: flex; justify-content: center; gap: 10px; margin-bottom: 20px; }
        .mode-badge { padding: 8px 16px; border-radius: 20px; font-size: 0.9rem; font-weight: 500;
            border: 2px solid transparent; transition: all 0.3s ease;
        }
        .mode-badge.active { background: linear-gradient(45deg, #4ecdc4, #45b7d1); color: white; border-color: rgba(255, 255, 255, 0.3); }
        .mode-badge.inactive { background: rgba(255, 255, 255, 0.1); color: #888; }
        .battery-display { display: flex; align-items: center; justify-content: center; gap: 10px; }
        .battery-level { width: 60px; height: 25px; border: 2px solid #fff; border-radius: 8px; position: relative; overflow: hidden; }
        .battery-fill { height: 100%; background: linear-gradient(90deg, #ff4757, #ffa502, #2ed573); transition: width 0.5s ease; }
        .control-group { margin-bottom: 20px; }
        .control-group label { display: block; margin-bottom: 8px; font-weight: 500; color: #e2e8f0; }
        .slider { width: 100%; height: 8px; border-radius: 5px; background: rgba(255, 255, 255, 0.1); outline: none; -webkit-appearance: none; appearance: none; }
        .slider::-webkit-slider-thumb { appearance: none; width: 20px; height: 20px; border-radius: 50%; background: #4ecdc4; cursor: pointer; border: 2px solid #fff; box-shadow: 0 2px 10px rgba(0, 0, 0, 0.3); }
        .value-display { text-align: center; margin-top: 8px; font-size: 0.9rem; color: #4ecdc4; font-weight: 500; }
        .tilt-display { margin-bottom: 20px; }
        .tilt-bar { width: 100%; height: 30px; background: rgba(255, 255, 255, 0.1); border-radius: 15px; position: relative; overflow: hidden; }
        .tilt-indicator { width: 20px; height: 100%; background: linear-gradient(45deg, #4ecdc4, #45b7d1); border-radius: 10px;
            position: absolute; top: 0; transition: left 0.3s ease; box-shadow: 0 0 10px rgba(78, 205, 196, 0.5);
        }
        .toggle { display: flex; align-items: center; gap: 10px; margin-bottom: 15px; }
        .toggle input[type="checkbox"] { width: 50px; height: 25px; appearance: none; background: rgba(255, 255, 255, 0.1);
            border-radius: 25px; position: relative; cursor: pointer; transition: all 0.3s ease;
        }
        .toggle input[type="checkbox"]:checked { background: #4ecdc4; }
        .toggle input[type="checkbox"]::before { content: ''; width: 21px; height: 21px; border-radius: 50%;
            background: white; position: absolute; top: 2px; left: 2px; transition: all 0.3s ease;
        }
        .toggle input[type="checkbox"]:checked::before { transform: translateX(25px); }
        .color-picker { width: 50px; height: 50px; border: none; border-radius: 50%; cursor: pointer; margin: 5px; }
        
        /* Mobile responsiveness */
        @media (max-width: 768px) {
            .container { padding: 15px; }
            .header h1 { font-size: 2rem; }
            .bpm-number { font-size: 3rem; }
            .panel { padding: 20px; }
            .palette-grid { grid-template-columns: repeat(auto-fit, minmax(100px, 1fr)); gap: 8px; }
            .pattern-grid { grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); }
            .led { width: 30px; height: 30px; }
        }
        
        @media (max-width: 480px) {
            .palette-grid { grid-template-columns: repeat(auto-fit, minmax(90px, 1fr)); }
            .palette-card { padding: 8px; }
            .palette-color { width: 12px; height: 12px; }
            .palette-name { font-size: 0.75rem; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌊 CTENOPHORE v2.0 🎨</h1>
            <p>Advanced Light Control & Tempo System</p>
        </div>

        <div class="status-bar">
            <div class="status-card">
                <div class="connection-status">
                    <div class="status-dot" id="connectionDot"></div>
                    <span id="connectionText">Connected</span>
                </div>
            </div>
            <div class="status-card">
                <div class="battery-display">
                    <div class="battery-level">
                        <div class="battery-fill" id="batteryFill" style="width: 75%"></div>
                    </div>
                    <span id="batteryText">75%</span>
                </div>
            </div>
            <div class="status-card">
                <div class="mode-indicator">
                    <div class="mode-badge active" id="liquidMode">🌊 LIQUID</div>
                    <div class="mode-badge inactive" id="tempoMode">🎵 TEMPO</div>
                </div>
            </div>
        </div>

        <div class="main-grid">
            <div class="panel">
                <h3>🎵 Tempo Control</h3>
                
                <div class="tempo-display">
                    <div class="bpm-number" id="bpmDisplay">0</div>
                    <div class="bpm-label">BPM</div>
                    <div class="metronome" id="metronome"></div>
                </div>

                <button class="btn primary tap-btn" id="tapBtn">👆 TAP</button>
                
                <div class="control-group">
                    <label>Manual BPM Override</label>
                    <div class="slider-container">
                        <input type="range" class="slider" id="bpmSlider" min="30" max="300" value="120">
                        <div class="value-display" id="bpmSliderValue">120 BPM</div>
                    </div>
                </div>

                <button class="btn secondary" id="resetBtn">🔄 Reset to Liquid</button>
            </div>

            <div class="panel">
                <h3>🎨 Color Palettes</h3>
                
                <div class="palette-grid" id="paletteGrid">
                    <!-- Palettes will be populated by JavaScript -->
                </div>
                
                <div class="control-group">
                    <label>🎨 Custom Palette Creator</label>
                    <div style="display: flex; gap: 5px; margin-bottom: 10px; flex-wrap: wrap;">
                        <input type="color" class="color-picker" id="color0" value="#FF0000" style="width: 35px; height: 35px; display: inline-block;">
                        <input type="color" class="color-picker" id="color1" value="#FF7F00" style="width: 35px; height: 35px; display: inline-block;">
                        <input type="color" class="color-picker" id="color2" value="#FFFF00" style="width: 35px; height: 35px; display: inline-block;">
                        <input type="color" class="color-picker" id="color3" value="#00FF00" style="width: 35px; height: 35px; display: inline-block;">
                        <input type="color" class="color-picker" id="color4" value="#0000FF" style="width: 35px; height: 35px; display: inline-block;">
                        <input type="color" class="color-picker" id="color5" value="#4B0082" style="width: 35px; height: 35px; display: inline-block;">
                        <input type="color" class="color-picker" id="color6" value="#9400D3" style="width: 35px; height: 35px; display: inline-block;">
                    </div>
                    <input type="text" id="customPaletteName" placeholder="Enter palette name..." 
                           style="width: 100%; padding: 8px; border-radius: 5px; border: 1px solid rgba(255,255,255,0.2); 
                                  background: rgba(255,255,255,0.05); color: white; margin-bottom: 10px;">
                    <button class="btn secondary" id="createCustomPalette">✨ Create Custom Palette</button>
                </div>
                
                <div class="toggle">
                    <input type="checkbox" id="tiltPalettes">
                    <label>Tilt-Based Palettes</label>
                </div>
                
                <div class="toggle">
                    <input type="checkbox" id="tempoColors">
                    <label>Tempo-Reactive Colors</label>
                </div>
            </div>

            <div class="panel">
                <h3>✨ Animation Patterns</h3>
                
                <div class="pattern-grid">
                    <div class="pattern-btn active" data-pattern="rainbow">🌈 Rainbow</div>
                    <div class="pattern-btn" data-pattern="breathing">💨 Breathing</div>
                    <div class="pattern-btn" data-pattern="chase">🏃 Chase</div>
                    <div class="pattern-btn" data-pattern="sparkle">✨ Sparkle</div>
                    <div class="pattern-btn" data-pattern="strobe">⚡ Strobe</div>
                    <div class="pattern-btn" data-pattern="fade">🌅 Fade</div>
                </div>
                
                <div class="control-group">
                    <label>Animation Speed</label>
                    <div class="slider-container">
                        <input type="range" class="slider" id="animationSpeed" min="10" max="200" value="50">
                        <div class="value-display" id="animationSpeedValue">Normal</div>
                    </div>
                </div>
            </div>

            <div class="panel">
                <h3>💡 LED Control</h3>
                
                <div class="led-display" id="ledDisplay">
                    <div class="led" data-led="0"></div>
                    <div class="led" data-led="1"></div>
                    <div class="led" data-led="2"></div>
                    <div class="led" data-led="3"></div>
                    <div class="led" data-led="4"></div>
                    <div class="led" data-led="5"></div>
                    <div class="led" data-led="6"></div>
                </div>

                <div class="toggle">
                    <input type="checkbox" id="customLEDs">
                    <label>Individual LED Control</label>
                </div>
                
                <input type="color" class="color-picker" id="ledColorPicker" value="#ff6b6b" style="display: none;">

                <div class="tilt-display">
                    <label>Tilt Angle</label>
                    <div class="tilt-bar">
                        <div class="tilt-indicator" id="tiltIndicator"></div>
                    </div>
                    <div class="value-display" id="tiltValue">0.00</div>
                </div>

                <div class="control-group">
                    <label>Motion Threshold</label>
                    <div class="slider-container">
                        <input type="range" class="slider" id="thresholdSlider" min="0.01" max="0.20" step="0.01" value="0.05">
                        <div class="value-display" id="thresholdValue">0.05</div>
                    </div>
                </div>

                <div class="control-group">
                    <label>Brightness</label>
                    <div class="slider-container">
                        <input type="range" class="slider" id="brightnessSlider" min="0.1" max="1.0" step="0.1" value="0.6">
                        <div class="value-display" id="brightnessValue">60%</div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        let currentData = {};
        let selectedLED = -1;
        
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
        updateUI();
        startPolling();
        initializePalettes();
        setupEventListeners();
        
        function initializePalettes() {
            const paletteGrid = document.getElementById('paletteGrid');
            palettes.forEach((palette, index) => {
                const paletteCard = document.createElement('div');
                paletteCard.className = 'palette-card' + (index === 0 ? ' active' : '');
                paletteCard.dataset.index = index;
                
                const preview = document.createElement('div');
                preview.className = 'palette-preview';
                palette.colors.forEach(color => {
                    const colorDiv = document.createElement('div');
                    colorDiv.className = 'palette-color';
                    colorDiv.style.backgroundColor = color;
                    preview.appendChild(colorDiv);
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
            document.getElementById('tapBtn').addEventListener('click', () => sendCommand('tap'));
            document.getElementById('resetBtn').addEventListener('click', () => sendCommand('reset'));
            
            document.getElementById('bpmSlider').addEventListener('input', (e) => {
                const value = e.target.value;
                document.getElementById('bpmSliderValue').textContent = value + ' BPM';
                sendCommand('bpm=' + value);
            });
            
            document.getElementById('thresholdSlider').addEventListener('input', (e) => {
                const value = parseFloat(e.target.value);
                document.getElementById('thresholdValue').textContent = value.toFixed(2);
                sendCommand('threshold=' + value);
            });
            
            document.getElementById('brightnessSlider').addEventListener('input', (e) => {
                const value = parseFloat(e.target.value);
                document.getElementById('brightnessValue').textContent = Math.round(value * 100) + '%';
                sendCommand('brightness=' + value);
            });
            
            document.getElementById('animationSpeed').addEventListener('input', (e) => {
                const value = parseInt(e.target.value);
                let speed = 'Normal';
                if (value < 30) speed = 'Slow';
                else if (value > 100) speed = 'Fast';
                document.getElementById('animationSpeedValue').textContent = speed;
                sendCommand('animationSpeed=' + value);
            });
            
            document.getElementById('tiltPalettes').addEventListener('change', (e) => {
                sendCommand('tiltPalettes=' + e.target.checked);
            });
            
            document.getElementById('tempoColors').addEventListener('change', (e) => {
                sendCommand('tempoColors=' + e.target.checked);
            });
            
            document.getElementById('customLEDs').addEventListener('change', (e) => {
                const colorPicker = document.getElementById('ledColorPicker');
                colorPicker.style.display = e.target.checked ? 'block' : 'none';
                sendCommand('customLEDs=' + e.target.checked);
            });
            
            // Custom palette creator
            document.getElementById('createCustomPalette').addEventListener('click', () => {
                const name = document.getElementById('customPaletteName').value.trim();
                if (!name) {
                    alert('Please enter a palette name!');
                    return;
                }
                
                const colors = [];
                for (let i = 0; i < 7; i++) {
                    colors.push(document.getElementById(`color${i}`).value);
                }
                
                // Add to palettes array
                palettes.push({name: name, colors: colors});
                
                // Rebuild palette grid
                document.getElementById('paletteGrid').innerHTML = '';
                initializePalettes();
                
                // Send to ESP32
                const paletteData = {
                    name: name,
                    colors: colors.map(c => c.replace('#', ''))
                };
                sendCommand('customPalette=' + JSON.stringify(paletteData));
                
                // Clear form
                document.getElementById('customPaletteName').value = '';
                
                alert(`🎨 "${name}" palette created!`);
            });
            
            // Pattern selection
            document.querySelectorAll('.pattern-btn').forEach(btn => {
                btn.addEventListener('click', (e) => {
                    document.querySelectorAll('.pattern-btn').forEach(b => b.classList.remove('active'));
                    e.target.classList.add('active');
                    sendCommand('pattern=' + e.target.dataset.pattern);
                });
            });
            
            // LED individual control
            document.querySelectorAll('.led').forEach((led, index) => {
                led.addEventListener('click', () => {
                    if (document.getElementById('customLEDs').checked) {
                        selectedLED = index;
                        document.getElementById('ledColorPicker').click();
                    }
                });
            });
            
            document.getElementById('ledColorPicker').addEventListener('change', (e) => {
                if (selectedLED >= 0) {
                    const color = e.target.value;
                    sendCommand('ledColor=' + selectedLED + ',' + color);
                }
            });
        }
        
        function selectPalette(index) {
            document.querySelectorAll('.palette-card').forEach(card => card.classList.remove('active'));
            document.querySelector(`[data-index="${index}"]`).classList.add('active');
            sendCommand('palette=' + index);
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
            }, 200);
        }

        function updateUI() {
            if (!currentData) return;
            
            document.getElementById('bpmDisplay').textContent = currentData.bpm || 0;
            
            const battery = currentData.batteryPercent || 75;
            document.getElementById('batteryFill').style.width = battery + '%';
            document.getElementById('batteryText').textContent = battery + '%';
            
            const isLiquid = currentData.mode === 'liquid';
            document.getElementById('liquidMode').className = 'mode-badge ' + (isLiquid ? 'active' : 'inactive');
            document.getElementById('tempoMode').className = 'mode-badge ' + (isLiquid ? 'inactive' : 'active');
            
            const tilt = currentData.tilt || 0;
            const tiltPercent = ((tilt + 1) / 2) * 100;
            document.getElementById('tiltIndicator').style.left = Math.max(0, Math.min(80, tiltPercent - 10)) + '%';
            document.getElementById('tiltValue').textContent = tilt.toFixed(2);
            
            const leds = document.querySelectorAll('.led');
            const ledStates = currentData.leds || [];
            leds.forEach((led, i) => {
                if (ledStates[i] && ledStates[i] > 0.1) {
                    led.style.background = 'hsl(' + (i * 51.4) + ', 70%, 60%)';
                    led.classList.add('active');
                } else {
                    led.style.background = '#333';
                    led.classList.remove('active');
                }
            });
            
            if (currentData.beat) {
                const metronome = document.getElementById('metronome');
                metronome.classList.add('beat');
                setTimeout(() => metronome.classList.remove('beat'), 300);
            }
        }
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
    float gyroZ = rawGZ / 131.0; // Convert to degrees/second
    
    // Detect rotation for palette cycling
    checkRotationGesture(gyroZ);
    
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
  
  if (liquidMode) {
    Serial.println("🌊➡️🎵 TAP! Switching to tempo mode!");
    liquidMode = false;
    tempoModeActive = true;
    tempoModeStartTime = millis();
    pressCount = 0;
  }
  
  if (!liquidMode) {
    pressCount++;
    Serial.print("🎵 Tempo trigger "); Serial.print(pressCount);
    
    startStrobe();
    
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

void checkRotationGesture(float gyroZ) {
  unsigned long currentTime = millis();
  
  // Only detect rotation when device is relatively level
  if (abs(tiltAngle) < 0.3) {
    // Accumulate rotation
    if (abs(gyroZ) > 30) { // Rotating fast enough
      cumulativeRotation += gyroZ * 0.01; // Scale by time delta
      lastRotationTime = currentTime;
      rotationMode = true;
      
      // Check for full rotation
      if (abs(cumulativeRotation) >= rotationThreshold) {
        // Cycle palette
        if (cumulativeRotation > 0) {
          // Clockwise - next palette
          currentPaletteIndex = (currentPaletteIndex + 1) % totalPaletteCount;
        } else {
          // Counter-clockwise - previous palette
          currentPaletteIndex = (currentPaletteIndex - 1 + totalPaletteCount) % totalPaletteCount;
        }
        
        Serial.println("🌀 Rotation palette change: " + String(currentPaletteIndex));
        
        // Visual feedback - quick sparkle
        triggerRotationSparkle();
        
        // Reset rotation counter
        cumulativeRotation = 0;
      }
    }
  }
  
  // Reset rotation if stopped turning
  if (currentTime - lastRotationTime > 1000) {
    cumulativeRotation = 0;
    rotationMode = false;
  }
}

void calculateAndUpdateTempo(unsigned long currentTime) {
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
  
  if(bpm < 30) bpm = 30;
  if(bpm > 300) bpm = 300;
  tempoInterval = 60000 / bpm;
  
  Serial.print("BPM: "); Serial.print(bpm);
  Serial.print(" ("); Serial.print(tempoInterval); Serial.println("ms)");
  
  autoStrobing = true;
  lastTempoTime = millis();
  
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
  
  if (liquidLevels[6] > 0.8) {
    Serial.println("🔋 Liquid reached end! Showing battery level...");
    checkBatteryLevel();
    showBatteryLevel();
  }
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
if (currentPaletteIndex < 8) {
  palette = &palettes[currentPaletteIndex];
} else {
  palette = &customPalettes[currentPaletteIndex - 6];
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
  int rawReading = analogRead(BATTERY_PIN);
  batteryVoltage = (rawReading / 4095.0) * 3.3 * 2.0;
  batteryPercentage = calculateBatteryPercentage(batteryVoltage);
  
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
   if (paletteIndex >= 0 && paletteIndex < totalPaletteCount) {
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
      totalPaletteCount = 6 + customPaletteCount;
      Serial.println("🎨 Custom palette '" + name + "' saved!");
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
    request->send_P(200, "text/html", dashboard_html);
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
  
  checkBatteryLevel();
  showBatteryLevel();
  
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
  
  if (currentTime - lastBatteryCheck > batteryCheckInterval) {
    checkBatteryLevel();
    lastBatteryCheck = currentTime;
  }
  
  if (liquidMode) {
    updateLiquidPhysics();
  }
  
  if (strobing && currentTime - lastStrobeTime >= strobeInterval) {
    doRippleEffect();
    lastStrobeTime = currentTime;
  }
  
  if (autoStrobing && tempoInterval > 0) {
    if (currentTime - lastTempoTime >= tempoInterval) {
      Serial.print("🎵 Auto-beat "); Serial.print(pressCount);
      Serial.print(" ("); Serial.print(bpm); Serial.println(" BPM)");
      startStrobe();
      lastTempoTime = currentTime;
      lastActivity = currentTime;
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
    }
    Serial.print(" | 🔋 "); Serial.print(batteryPercentage); Serial.print("%");
    Serial.print(" | Palette: "); Serial.print(palettes[currentPaletteIndex].name);
    Serial.print(" | Pattern: "); Serial.print((int)currentPattern);
    Serial.println();
    lastDebug = currentTime;
  }
  
  delay(5);
}