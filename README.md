# Smart Lighting AI System

Arduino Nano based smart lighting system with AI decision making.

## Features
- LDR light sensor with hysteresis (no flickering)
- DS18B20 temperature sensor (optional)
- 0.96" OLED display with dual screens (ROOM / CITY)
- Manual toggle switch override
- Real-time Germany time (ROOM) and selected city time (CITY)
- Real weather data from OpenWeatherMap
- DeepSeek AI decisions based on time + light level
- ESP32 ready (Python simulator included)

## Pin Connections
| Component | Arduino Nano Pin |
|-----------|------------------|
| LDR | A0 |
| Relay | D2 |
| DS18B20 | D3 |
| Manual Switch | D4 |
| OLED SDA | A4 |
| OLED SCL | A5 |

## Usage
1. Upload Arduino code
2. Run Python simulator: `python esp32_simulator.py`
3. Enter city name for weather
4. Watch OLED display and AI decisions

## Commands (Serial Monitor)
- `STATUS` - Show system status
- `TOGGLE` - Manual relay control
- `HELP` - Show commands

## Hardware
- Arduino Nano
- 0.96" OLED display (SSD1306, I2C)
- LDR (Light Dependent Resistor)
- 10kΩ resistor for LDR
- Relay module
- Toggle switch (optional)
- DS18B20 temperature sensor (optional)