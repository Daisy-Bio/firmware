# Mira PCR Firmware

Firmware for the Mira portable PCR (Polymerase Chain Reaction) device, running on an ESP32 microcontroller with a US2066 OLED display, rotary encoder UI, and SD card storage.

## Hardware Requirements

| Component   | Specification                        |
| ----------- | ------------------------------------ |
| **MCU**     | ESP32 (any variant with WiFi/BT)     |
| **Display** | US2066 4x20 OLED (I2C, address 0x3C) |
| **Input**   | Rotary encoder with push button      |
| **Storage** | SD card (SPI)                        |

### Pin Configuration

```
Rotary Encoder:
  - A Pin:  GPIO 33
  - B Pin:  GPIO 32
  - SW Pin: GPIO 25

I2C (OLED):
  - SDA: GPIO 21
  - SCL: GPIO 22
  - RST: GPIO 13

SPI (SD Card):
  - CS:   GPIO 2
  - MOSI: GPIO 17
  - MISO: GPIO 19
  - SCLK: GPIO 18
```

## Project Structure

```
firmware/
├── Mira_Firmware.ino      # Main sketch (setup/loop)
├── config.h               # Pin definitions and constants
├── test_params.h          # PCR test parameter structure
├── oled.h / oled.cpp      # US2066 OLED display driver
├── encoder.h / encoder.cpp  # Rotary encoder + button handling
├── sd_storage.h / sd_storage.cpp  # SD card operations
├── ui_state.h / ui_state.cpp      # UI state machine
├── ui_draw.h / ui_draw.cpp        # Screen drawing functions
└── LICENSE.md             # MIT License
```

### Module Descriptions

| Module             | Description                                                     |
| ------------------ | --------------------------------------------------------------- |
| `config.h`         | Hardware pin definitions, OLED constants, encoder settings      |
| `test_params.h`    | `TestParams` struct with default PCR cycle parameters           |
| `oled.h/cpp`       | Low-level I2C driver for US2066 character OLED                  |
| `encoder.h/cpp`    | Quadrature decoder with debounced single/double-click detection |
| `sd_storage.h/cpp` | Test file CRUD operations, parameter serialization              |
| `ui_state.h/cpp`   | Finite state machine, global UI state, transition logic         |
| `ui_draw.h/cpp`    | Menu and screen rendering functions                             |

## Building

### Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (1.8.x or 2.x)
2. Add ESP32 board support:
   - File → Preferences → Additional Board Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Tools → Board → Board Manager → Search "esp32" → Install
3. Open `Mira_Firmware.ino`
4. Select board: **Tools → Board → ESP32 Dev Module**
5. Click **Verify** to compile

### PlatformIO

```ini
; platformio.ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
```

```bash
pio run          # Compile
pio run -t upload  # Upload to device
```

### Arduino CLI

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 .
```

## UI Navigation

The device uses a rotary encoder for all navigation:

| Action           | Function                   |
| ---------------- | -------------------------- |
| **Rotate**       | Move cursor / adjust value |
| **Single Click** | Select / confirm           |
| **Double Click** | Accept filename / back     |

### Menu Structure

```
Main Menu
├── Run Test
│   ├── Start → Live Test Screen
│   └── Back
├── Create Test
│   ├── Init Denat Temp (25-125°C)
│   ├── Init Denat Time (0-600s)
│   ├── Denat Temp/Time
│   ├── Anneal Temp/Time
│   ├── Extension Temp/Time
│   ├── Number of Cycles (1-99)
│   ├── Final Ext Temp/Time
│   ├── Save Test → Name Entry
│   └── Back
└── SD Card
    ├── [Test Files...]
    │   ├── Load Test
    │   ├── View Test
    │   ├── Delete Test
    │   └── Back
    └── Back
```

## PCR Test Parameters

| Parameter                 | Default | Range  | Unit |
| ------------------------- | ------- | ------ | ---- |
| Initial Denaturation Temp | 95      | 25-125 | °C   |
| Initial Denaturation Time | 120     | 0-600  | s    |
| Denaturation Temp         | 95      | 25-125 | °C   |
| Denaturation Time         | 10      | 0-600  | s    |
| Annealing Temp            | 60      | 25-125 | °C   |
| Annealing Time            | 20      | 0-600  | s    |
| Extension Temp            | 72      | 25-125 | °C   |
| Extension Time            | 20      | 0-600  | s    |
| Number of Cycles          | 45      | 1-99   | -    |
| Final Extension Temp      | 72      | 25-125 | °C   |
| Final Extension Time      | 240     | 0-600  | s    |

## SD Card File Format

Tests are stored as `.TXT` files in `/TESTS/` directory:

```
Temp_Init_Denat=95
Time_Init_Denat=120
Temp_Denat=95
Time_Denat=10
Temp_Anneal=60
Time_Anneal=20
Temp_Extension=72
Time_Extension=20
Num_Cycles=45
Temp_Final_Ext=72
Time_Final_Ext=240
```

## Development

### Adding a New UI Screen

1. Add state to `UIState` enum in `ui_state.h`
2. Add draw function declaration in `ui_draw.h`
3. Implement draw function in `ui_draw.cpp`
4. Add state transition in `enterState()` in `ui_state.cpp`
5. Add rotation/button handling in `loop()` in `Mira_Firmware.ino`

### Code Style

- Use `static constexpr` for compile-time constants
- Prefix global variables with `g_`
- Use `IRAM_ATTR` for ISR functions (ESP32 requirement)
- Keep functions under 50 lines where possible

## License

This firmware is released under the [MIT License](LICENSE.md).

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Commit changes (`git commit -m 'Add my feature'`)
4. Push to branch (`git push origin feature/my-feature`)
5. Open a Pull Request
