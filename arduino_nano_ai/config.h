/*
 * config.h - Configuration file for Arduino Nano AI Lighting System
 * 
 * This file contains all your settings - pins, thresholds, modes, etc.
 * Separate from main code so you can easily change settings without
 * searching through hundreds of lines of code.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// PIN DEFINITIONS (Hardware connections)
// ============================================
#define PIN_LDR        A0          // Light sensor (analog input)
#define PIN_RELAY      2           // Relay control (digital output)
#define ONE_WIRE_BUS   3           // DS18B20 temperature sensor (data pin)

// OLED I2C pins (fixed for Arduino Nano)
#define OLED_SDA       A4
#define OLED_SCL       A5

// ============================================
// OLED DISPLAY ADDRESSES
// ============================================
#define OLED_ADDR_ROOM    0x3C      // First OLED (Room temperature)
#define OLED_ADDR_CITY    0x3D      // Second OLED (City temperature)

// ============================================
// ESP32 COMMUNICATION
// ============================================
#define ESP_RX_PIN       10        // D10 - Receive from ESP32
#define ESP_TX_PIN       11        // D11 - Transmit to ESP32
#define ESP_TIMEOUT_MS   2000      // Wait 2 seconds for ESP32 response

// ============================================
// SYSTEM MODES
// ============================================
// MODE_AI    = Decisions from DeepSeek via ESP32
// MODE_LOCAL = Decisions based on light threshold
enum SystemMode { MODE_AI, MODE_LOCAL };
#define DEFAULT_MODE MODE_AI       // Change to MODE_LOCAL if ESP32 not available

// ============================================
// LOCAL MODE THRESHOLDS
// ============================================
// Light sensor: 0 (very dark) to 1023 (very bright)
#define LIGHT_DARK_THRESHOLD    300    // Below this = dark (turn ON)
#define LIGHT_BRIGHT_THRESHOLD  600    // Above this = bright (turn OFF)

// ============================================
// TEMPERATURE LABELS (Celsius)
// ============================================
#define TEMP_HOT      30.0        // Above 30°C = 🔥 HOT
#define TEMP_WARM     25.0        // 25-30°C   = 😊 WARM
#define TEMP_COOL     20.0        // 20-25°C   = 😐 COOL
#define TEMP_COLD     15.0        // 15-20°C   = ❄️ COLD
// Below 15°C = 🥶 FREEZING

// ============================================
// TIMING INTERVALS (milliseconds)
// ============================================
#define SENSOR_INTERVAL    2000    // Read sensors every 2 seconds
#define SCREEN_INTERVAL           5000    // Switch screens every 5 seconds  // ← ADD THIS LINE
#define WEATHER_REQUEST_INTERVAL 10000  // Request city weather every 10 seconds

// ============================================
// MANUAL SWITCH PIN
// ============================================
#define MANUAL_SWITCH_PIN         4       // Toggle switch for manual override

// ============================================
// OLED DISPLAY SETTINGS
// ============================================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#endif  // CONFIG_H