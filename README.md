# Ctenophore LED Controller

A sophisticated ESP32-C3 based NeoPixel controller featuring liquid physics simulation, motion sensing, tempo detection, and WiFi control interface.

![Version](https://img.shields.io/badge/version-2.0-blue)
![Platform](https://img.shields.io/badge/platform-ESP32--C3-green)
![License](https://img.shields.io/badge/license-MIT-orange)

## Features

### 🌊 Liquid Physics Simulation
- Real-time 7-LED liquid level simulation
- Tilt-reactive physics using MPU-6050 accelerometer
- Smooth transitions and realistic fluid dynamics
- Ripple effects triggered by motion

### 🎵 Tempo Detection
- Tap-based BPM detection and locking
- Auto-strobing synchronized to detected tempo
- Drift correction for stable beat tracking
- 60-second tempo mode with automatic timeout
- Immediate tempo switching (no delay)

### 🎨 Color Palettes
8 built-in palettes:
- **Rainbow** - Classic spectrum
- **Ocean** - Deep blues to cyan
- **Fire** - Black to white through reds/oranges
- **Ctenophore** - Deep blue to cyan gradient
- **Sunset** - Purple to red gradient
- **Cyberpunk** - Magenta/cyan neon
- **Peppermint** - Red and white stripes
- **Aesthetic** - Blue/red/yellow retro

**Custom Features:**
- Store up to 10 custom palettes
- Individual LED color overrides
- Tilt-based palette zone switching
- Random palette mode
- Tempo-reactive color shifts

### 🎭 Animation Patterns
- **Rainbow Cycle** - Smooth spectrum rotation
- **Breathing** - Gentle pulsing effect
- **Chase** - Light chasing back and forth
- **Sparkle** - Random LED sparkles
- **Strobe** - Tempo-synced strobing
- **Fade** - Smooth fading transitions

### 🎢 Motion Detection
**MPU-6050 Integration:**
- Tilt angle detection (-1.0 to 1.0)
- Motion and shake detection
- Rotation tracking on X and Z axes
- Barrel roll detection (X-axis) → Animation changes
- Spin detection (Z-axis) → Palette changes
- Configurable sensitivity thresholds

### 📡 WiFi Control Interface
**Hotspot Mode:**
- SSID: `Ctenophore-Control`
- Password: `tempo123`
- Access point on `192.168.4.1`

**Web Dashboard:**
- Live LED color preview
- BPM display and tap button
- Battery level monitoring
- Palette selector
- Animation pattern controls
- Real-time status updates
- Responsive mobile-first design

### 🔋 Battery Management
- Voltage monitoring on A0 pin
- Percentage calculation
- Low battery warnings
- 10-second check intervals
- Visual battery level display

### ⚡ Power Management
- Auto-return to liquid mode after 5 minutes idle
- Configurable brightness levels (max: 60%, dim: 2%)
- ESP32 power management integration
- Watchdog timer protection

## Hardware Requirements

### Core Components
- **Board:** Seeed Xiao ESP32-C3
- **LEDs:** 7x NeoPixel (WS2812B) strip on pin D10
- **Accelerometer:** MPU-6050 (I2C)
- **Battery:** LiPo with voltage divider on A0

### Wiring
```
ESP32-C3          Component
--------          ---------
D10        →      NeoPixel DIN
A0         →      Battery + (through voltage divider)
SDA        →      MPU-6050 SDA
SCL        →      MPU-6050 SCL
GND        →      Common Ground
```

## Software Setup

### PlatformIO Configuration
```ini
[env:seeed_xiao_esp32c3]
platform = espressif32
board = seeed_xiao_esp32c3
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit NeoPixel@^1.15.1
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    bblanchon/ArduinoJson@^6.21.3
```

### Installation
1. Clone this repository
2. Open in PlatformIO
3. Build and upload to ESP32-C3
4. Connect to `Ctenophore-Control` WiFi network
5. Navigate to `192.168.4.1` in browser

## Usage

### Tap Detection
- **Single tap:** Activate tempo mode
- **Multiple taps:** Calculate and lock BPM
- **Configurable threshold:** Default 0.6 (lower = more sensitive)

### Rotation Gestures
- **Barrel roll (X-axis):** Cycle through animation patterns
- **Spin (Z-axis):** Change color palettes
- **360° rotation threshold:** Triggers effect changes

### Serial Commands
Monitor at 115200 baud for debug output and configuration.

### Web Interface
Access full control panel at `http://192.168.4.1` after connecting to WiFi hotspot.

## Configuration

### Key Parameters
```cpp
// Motion sensitivity
float motionThreshold = 0.05;
float tapThreshold = 0.6;

// Visual settings
float maxBrightness = 0.6;
float dimBrightness = 0.02;
float waveSpeed = 0.4;

// Timing
unsigned long tempoModeTimeout = 60000;  // 60s
unsigned long idleTimeout = 300000;      // 5min
```

## Project Structure

```
ct00/
├── src/
│   └── main.cpp              # Main application (2500+ lines)
├── test/                     # Development history/versions
│   ├── V1 7 LEDs_liquid sim.cpp
│   ├── LAST WORKING [Jul 28] main copy.cpp
│   └── ...
├── include/                  # Header files
├── lib/                      # Custom libraries
└── platformio.ini            # Build configuration
```

## Performance

- **Refresh Rate:** 20 FPS (50ms update interval)
- **Tempo Range:** Dynamic BPM detection
- **LED Update:** Hardware-accelerated NeoPixel driver
- **WiFi:** Async web server for responsive control

## Troubleshooting

**LEDs not lighting:**
- Check pin D10 connection
- Verify 5V power supply for LEDs
- Check NeoPixel data line signal quality

**MPU-6050 not detected:**
- Verify I2C connections (SDA/SCL)
- Check address (default: 0x68)
- Enable I2C pull-up resistors if needed

**WiFi not connecting:**
- Search for `Ctenophore-Control` network
- Password: `tempo123`
- Some devices may take 30s to detect new hotspot

**Battery reading incorrect:**
- Check voltage divider ratio
- Adjust `batteryVoltage` calculation in code
- Verify A0 pin connection

## Development History

Check the `/test` directory for previous versions and experimental features including:
- Power saving modes
- WiFi variants
- Dashboard prototypes
- Earlier liquid simulations

## License

MIT License - feel free to modify and use in your projects.

## Credits

Built with regular Claude Sonnet and upgraded to Sonnet 4.5.

**Technologies:**
- ESP32-C3 Arduino Framework
- Adafruit NeoPixel Library
- ESPAsyncWebServer
- ArduinoJson

---

*Inspired by the bioluminescence of ctenophores (comb jellies)* 🌊✨
