#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

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

#endif // DASHBOARD_HTML_H
