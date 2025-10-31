# Ctenophore LED Controller - Refactoring Log

## Session 1 - 2025-10-31

### Goals
1. **Tempo Detection Improvements**
   - Start tempo prediction on 3rd tap (not 4th)
   - Continuously adjust tempo with every subsequent tap
   - Example: Tap 3 = first prediction, Tap 4 = adjust, Tap 5 = adjust, etc.
   - Reflect speed changes dynamically (slow down on 4, speed up on 5-6, stabilize on 7+)

2. **Increase Tap Sensitivity**
   - Current threshold: `0.6` (already lowered from 1.2)
   - Make more responsive to lighter taps

3. **Major Refactoring**
   - Split 2,520-line `main.cpp` into modular architecture
   - Create proper state machine for mode management
   - Unify command parsing (serial + web)
   - Externalize HTML dashboard
   - Extract all magic numbers to constants

---

### Architecture Plan

#### Target Structure
```
src/
├── main.cpp (coordinator - ~100 lines)
├── config/
│   └── Constants.h              // All magic numbers and thresholds
├── hardware/
│   ├── MPUSensor.h/cpp          // MPU-6050 I2C management
│   ├── LEDController.h/cpp      // NeoPixel wrapper
│   └── BatteryMonitor.h/cpp     // Battery voltage monitoring
├── motion/
│   ├── GestureDetector.h/cpp    // Tap, tilt, rotation detection
│   └── MotionState.h            // Acceleration data structures
├── effects/
│   ├── AnimationEngine.h/cpp    // Pattern rendering
│   ├── LiquidPhysics.h/cpp      // Tilt-based liquid simulation
│   └── PaletteManager.h/cpp     // Unified palette management
├── tempo/
│   ├── TempoDetector.h/cpp      // BPM calculation with continuous adjustment
│   └── BeatSynchronizer.h/cpp   // Drift-free beat timing
└── control/
    ├── DeviceMode.h             // State machine enum
    ├── ModeController.h/cpp     // Mode state machine
    ├── CommandParser.h/cpp      // Unified serial/web command parsing
    └── WiFiServer.h/cpp         // Web dashboard server
```

---

### Changes Made

#### ✅ Completed
- [x] Created `REFACTORING.md` tracking file
- [x] **Fixed tempo detection** - Now starts prediction on **3rd tap** (not 2nd)
  - Tap 3: First prediction using average of 2 intervals
  - Tap 4+: Continuous adjustment with exponential weighting (most recent tap = highest influence)
  - Sliding window maintains last 4 taps for smooth tempo tracking
- [x] **Increased tap sensitivity**
  - `tapThreshold`: `0.6` → `0.4` (33% more sensitive)
  - `tapDebounce`: `250ms` → `200ms` (faster response)
- [x] Created `config/Constants.h` - All magic numbers extracted
  - Hardware, MPU, Motion, Rotation, Tempo, Effects, Battery, WiFi, System, Palette configs
- [x] Created `control/DeviceMode.h` - State machine implementation
  - Enum: `LIQUID_IDLE`, `LIQUID_TILTING`, `TEMPO_DETECTING`, `TEMPO_PLAYING`, `BATTERY_DISPLAY`, `ROTATION_EFFECT`
  - `ModeController` class with timeout management and activity tracking
- [x] Created `hardware/MPUSensor.h` - MPU-6050 sensor wrapper
  - I2C initialization, accel/gyro reading, tilt angle calculation
  - Motion magnitude and delta calculations
- [x] Created `hardware/LEDController.h` - NeoPixel wrapper
  - RGB/HSV color control, interpolation, temperature adjustment
  - Hex string parsing, utility functions
- [x] Created `hardware/BatteryMonitor.h` - Battery monitoring
  - Voltage reading with exponential smoothing
  - Percentage calculation, low battery warnings
- [x] Created `motion/GestureDetector.h` - Unified gesture detection
  - `RotationDetector` class (reusable for X and Z axis)
  - Tap, motion, shake detection
  - Callback-based architecture

#### 🚧 In Progress
- [ ] Extract `effects/` module (AnimationEngine, ColorPalette)

#### 📋 Pending
- [ ] Extract `tempo/` module (TempoDetector, BeatSynchronizer)
- [ ] Extract `control/` module (CommandParser, WiFiServer)
- [ ] Refactor `main.cpp` to coordinate modules
- [ ] Move HTML dashboard to external file (SPIFFS/LittleFS)
- [ ] Add custom palette persistence
- [ ] Test and verify all functionality

---

### Key Issues Identified

#### Critical
1. **No unified state machine** - 3 global bools manage mode (`liquidMode`, `tempoModeActive`, `autoStrobing`)
2. **Palette index arithmetic fragile** - Mixing two arrays with manual math
3. **Command parsing duplicated** - 200+ lines in serial + web handlers

#### High Priority
4. **Magic numbers scattered** - Rotation thresholds (180°, 45°), timeouts (5000ms), etc.
5. **Tight coupling** - All systems depend on global state mutations
6. **1200-line HTML embedded** in source code

#### Medium Priority
7. **Gesture detection duplicated** - `checkPaletteSpin()` and `checkAnimationFlip()` nearly identical
8. **LED rendering implicit ordering** - `updateAnimations()` must run before `applyColorPalette()`
9. **Static variables hidden** - State spread across function-local statics

---

### Testing Checklist (Post-Refactor)
- [ ] Liquid physics responds to tilt
- [ ] Tap detection triggers tempo mode
- [ ] 3-tap tempo prediction works
- [ ] Continuous tempo adjustment (4+ taps)
- [ ] Barrel roll cycles animations
- [ ] Spin cycles palettes
- [ ] Web dashboard connects and controls
- [ ] Serial commands work
- [ ] Battery monitoring displays correctly
- [ ] Mode timeout returns to liquid
- [ ] All 6 animation patterns render
- [ ] Custom palettes save/load (if implemented)

---

### Notes
- Original `main.cpp`: **2,520 lines**, ~145 functions
- Target: Modular architecture with clear separation of concerns
- Maintain Arduino/PlatformIO compatibility
- Preserve all existing functionality during refactor
