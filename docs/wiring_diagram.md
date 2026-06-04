# Wiring Diagram

## Arduino Nano Connections

| Component | Arduino Nano Pin | Notes |
|-----------|-----------------|-------|
| LDR Sensor | A0 | One leg to 5V, one to A0 with 10k resistor to GND |
| Relay Module | D2 | Signal to D2, VCC to 5V, GND to GND |
| Temperature Sensor (DS18B20) | D3 | Data to D3, VCC to 5V, GND to GND (with 4.7k resistor) |
| OLED 1 (Room) | A4 (SDA), A5 (SCL) | I2C address 0x3C |
| OLED 2 (City) | A4 (SDA), A5 (SCL) | I2C address 0x3D |
| ESP32-C3 (RX) | D10 | Via voltage divider (5V → 3.3V) |
| ESP32-C3 (TX) | D11 | Direct connection (3.3V → 5V safe) |

## ESP32-C3 Connections

| Component | ESP32-C3 Pin | Notes |
|-----------|-------------|-------|
| Arduino Nano (RX) | D0 (RX) | Via voltage divider |
| Arduino Nano (TX) | D1 (TX) | Direct connection |
| Power | USB or 5V pin | 5V via USB |

## Voltage Divider for ESP32 RX
