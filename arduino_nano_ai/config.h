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

#define MANUAL_SWITCH_PIN   4
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


#endif  // CONFIG_H