/*
 * config.h - Configuration file for Arduino Nano AI Lighting System
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// PIN DEFINITIONS (Hardware connections)
// ============================================
#define PIN_LDR             A0
#define PIN_RELAY           2
#define ONE_WIRE_BUS        3
#define MANUAL_SWITCH_PIN   4

// OLED I2C pins (fixed for Arduino Nano)
#define OLED_SDA            A4
#define OLED_SCL            A5

// ============================================
// OLED DISPLAY ADDRESS
// ============================================
#define OLED_ADDR_ROOM      0x3C

// ============================================
// HYSTERESIS THRESHOLDS
// ============================================
#define LIGHT_ON_THRESHOLD      250
#define LIGHT_OFF_THRESHOLD     350

// ============================================
// TIMING INTERVALS (milliseconds)
// ============================================
#define SENSOR_INTERVAL         2000
#define SCREEN_INTERVAL         5000
#define WEATHER_REQUEST_INTERVAL 10000

// ============================================
// OLED DISPLAY SETTINGS
// ============================================
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           64

#endif  // CONFIG_H