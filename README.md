# Smart Lighting AI System

Arduino Nano based smart lighting system with AI decision making.

## Features
- LDR light sensor with hysteresis (no flickering)
- DS18B20 temperature sensor (optional)
- 0.96" OLED display with dual screens (ROOM / CITY)
- Manual toggle switch override
- Real-time Germany time (ROOM) and selected city time (CITY) e.g. Kathmandu
- Real weather data from OpenWeatherMap
- DeepSeek AI decisions based on time + light level
- ESP32 ready (Python simulator included)

## Pin Connections
| Component | Arduino Nano Pin |
|-----------|------------------|
| LDR | A0 |
| Relay | D2 |
| Manual Switch | D4 |

%% new Added Temp,OLED in ESP32-C3
| OLED SDA | A4 |
| OLED SCL | A5 |
| DS18B20 | D7 |

## Usage
1. Upload Arduino code
2. Run Python simulator: if you do not have ESP32-C3, for that please link all pin to Arduino uno, `python esp32_simulator.py`
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