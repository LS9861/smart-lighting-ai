/*
 * Arduino Nano - AI Lighting System with Manual Override
 * Uses existing config.h thresholds
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include "config.h"

// ============================================
// Use existing thresholds from config.h
// ============================================
#ifndef LIGHT_ON_THRESHOLD
#define LIGHT_ON_THRESHOLD     LIGHT_DARK_THRESHOLD
#endif
#ifndef LIGHT_OFF_THRESHOLD
#define LIGHT_OFF_THRESHOLD    LIGHT_BRIGHT_THRESHOLD
#endif

// ============================================
// OLED DISPLAY (Single OLED)
// ============================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_ADDR_ROOM);

// ============================================
// SENSORS
// ============================================
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

// ============================================
// SYSTEM VARIABLES
// ============================================
bool relayState = false;
int lastLightValue = 0;
float lastTemperature = 0;
bool tempSensorFound = false;
bool manualSwitchState = false;
bool oledWorking = false;

char currentTime[6] = "--:--";
char cityTemp[6] = "--";
char cityCondition[15] = "Waiting";

int screenState = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastSensorRead = 0;
unsigned long lastESPRequest = 0;

const char* lightStatusText = "NRM";

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(MANUAL_SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(PIN_RELAY, LOW);
  
  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_ROOM)) {
    Serial.println(F("OLED not found!"));
    oledWorking = false;
  } else {
    oledWorking = true;
    display.clearDisplay();
    display.display();
    Serial.println(F("OLED ready"));
  }
  
  // Initialize temperature sensor
  sensors.begin();
  if (sensors.getDeviceCount() == 0) {
    tempSensorFound = false;
    Serial.println(F("No DS18B20 sensor found"));
  } else {
    tempSensorFound = true;
    Serial.print(F("DS18B20 sensor found. Count: "));
    Serial.println(sensors.getDeviceCount());
  }
  
  Serial.println(F("\n================================="));
  Serial.println(F("AI Lighting System Ready"));
  Serial.println(F("================================="));
  Serial.print(F("Light ON: <"));
  Serial.print(LIGHT_ON_THRESHOLD);
  Serial.print(F(" OFF: >"));
  Serial.println(LIGHT_OFF_THRESHOLD);
  Serial.println(F("Switch D4: ON=Manual, OFF=Auto"));
  Serial.println(F("Commands: STATUS, TOGGLE, HELP"));
  Serial.println(F("=================================\n"));
  
  // Show welcome message
  if (oledWorking) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(F("AI Lite"));
    display.setCursor(0, 20);
    display.print(F("Ready"));
    display.display();
    delay(2000);
  }
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  unsigned long now = millis();
  
  checkManualSwitch();
  
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
    makeLightingDecision();
  }
  
  if (now - lastESPRequest >= WEATHER_REQUEST_INTERVAL) {
    lastESPRequest = now;
    espSerial.println(F("GET_DATA"));
  }
  
  handleESPCommunication();
  
  if (now - lastScreenSwitch >= SCREEN_INTERVAL) {
    lastScreenSwitch = now;
    screenState = (screenState == 0) ? 1 : 0;
  }
  
  if (oledWorking) {
    updateDisplay();
  }
  
  handleSerialCommands();
  delay(50);
}

// ============================================
// MANUAL SWITCH
// ============================================
void checkManualSwitch() {
  bool switchPressed = (digitalRead(MANUAL_SWITCH_PIN) == LOW);
  
  if (switchPressed && !manualSwitchState) {
    manualSwitchState = true;
    Serial.println(F("MANUAL MODE: Switch ON"));
    if (!relayState) {
      digitalWrite(PIN_RELAY, HIGH);
      relayState = true;
      Serial.println(F("Light ON (manual)"));
    }
  } 
  else if (!switchPressed && manualSwitchState) {
    manualSwitchState = false;
    Serial.println(F("AUTO MODE: Switch OFF"));
  }
}

// ============================================
// SENSOR READING
// ============================================
void readSensors() {
  lastLightValue = analogRead(PIN_LDR);
  
  // Determine light status using thresholds
  if (lastLightValue < LIGHT_ON_THRESHOLD) {
    lightStatusText = "DRK";
  } else if (lastLightValue > LIGHT_OFF_THRESHOLD) {
    lightStatusText = "BRT";
  } else {
    lightStatusText = "NRM";
  }
  
  // Read temperature
  if (tempSensorFound) {
    sensors.requestTemperatures();
    lastTemperature = sensors.getTempCByIndex(0);
    if (lastTemperature == -127.0) lastTemperature = -999;
  } else {
    lastTemperature = -999;
  }
  
  // Debug output
  Serial.print(F("Light: "));
  Serial.print(lastLightValue);
  Serial.print(F(" ["));
  Serial.print(lightStatusText);
  Serial.print(F("] Relay: "));
  Serial.print(relayState ? "ON" : "OFF");
  Serial.print(F(" Mode: "));
  Serial.print(manualSwitchState ? "MAN" : "AUTO");
  Serial.print(F(" Temp: "));
  if (lastTemperature != -999) Serial.println(lastTemperature, 1);
  else Serial.println(F("NO SENSOR"));
}

// ============================================
// LIGHTING CONTROL (with hysteresis)
// ============================================
void makeLightingDecision() {
  if (manualSwitchState) {
    return;
  }
  
  bool shouldLightBeOn = relayState;
  
  if (!relayState && lastLightValue < LIGHT_ON_THRESHOLD) {
    shouldLightBeOn = true;
    Serial.println(F("→ DARK -> ON"));
  } else if (relayState && lastLightValue > LIGHT_OFF_THRESHOLD) {
    shouldLightBeOn = false;
    Serial.println(F("→ BRIGHT -> OFF"));
  }
  
  if (shouldLightBeOn && !relayState) {
    digitalWrite(PIN_RELAY, HIGH);
    relayState = true;
    Serial.println(F("LIGHT ON"));
  } else if (!shouldLightBeOn && relayState) {
    digitalWrite(PIN_RELAY, LOW);
    relayState = false;
    Serial.println(F("LIGHT OFF"));
  }
}

// ============================================
// ESP32 COMMUNICATION
// ============================================
void handleESPCommunication() {
  if (espSerial.available()) {
    String response = espSerial.readStringUntil('\n');
    response.trim();
    
    if (response.startsWith(F("TIME:"))) {
      response.substring(5).toCharArray(currentTime, 6);
      Serial.print(F("Time: "));
      Serial.println(currentTime);
    } 
    else if (response.startsWith(F("WEATHER:"))) {
      String data = response.substring(8);
      int sep = data.indexOf('|');
      if (sep > 0) {
        data.substring(0, sep).toCharArray(cityTemp, 6);
        data.substring(sep + 1).toCharArray(cityCondition, 15);
        Serial.print(F("Weather: "));
        Serial.print(cityTemp);
        Serial.print(F("C "));
        Serial.println(cityCondition);
      }
    }
  }
}

// ============================================
// OLED DISPLAY (Optimized for 0.96")
// ============================================
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (screenState == 0) {
    drawRoomScreen();
  } else {
    drawCityScreen();
  }
  
  display.display();
}

void drawRoomScreen() {
  // Line 0: Title (top)
  display.setCursor(0, 0);
  display.print(F("ROOM"));
  
  // Show time if available
  if (currentTime[0] != '-') {
    display.setCursor(80, 0);
    display.print(currentTime);
  }
  
  // Temperature (BIG) - centered
  display.setTextSize(2);
  display.setCursor(0, 16);
  if (lastTemperature == -999) {
    display.print(F("--.-"));
  } else {
    display.print(lastTemperature, 1);
  }
  display.print(F("C"));
  
  // Light Status (small, shortened)
  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(F("L:"));
  display.print(lightStatusText);
  
  // Relay Status
  display.setCursor(60, 42);
  display.print(F("R:"));
  display.print(relayState ? F("ON") : F("OFF"));
  
  // Mode (bottom line)
  display.setCursor(0, 56);
  if (manualSwitchState) {
    display.print(F("MODE:MAN"));
  } else {
    display.print(F("MODE:AUTO"));
  }
}

void drawCityScreen() {
  // Title
  display.setCursor(0, 0);
  display.print(F("CITY"));
  
  // Time
  if (currentTime[0] != '-') {
    display.setCursor(80, 0);
    display.print(currentTime);
  }
  
  // Temperature (BIG)
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(cityTemp);
  display.print(F("C"));
  
  // Weather Condition
  display.setTextSize(1);
  display.setCursor(0, 46);
  display.print(cityCondition);
  
  // Update hint
  display.setCursor(0, 58);
  display.print(F("Refresh"));
}

// ============================================
// SERIAL COMMANDS
// ============================================
void handleSerialCommands() {
  if (!Serial.available()) return;
  
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();
  
  if (cmd == F("STATUS")) {
    showStatus();
  } 
  else if (cmd == F("TOGGLE")) {
    if (!manualSwitchState) {
      relayState = !relayState;
      digitalWrite(PIN_RELAY, relayState ? HIGH : LOW);
      Serial.print(F("Toggle: Light "));
      Serial.println(relayState ? F("ON") : F("OFF"));
    } else {
      Serial.println(F("Turn OFF manual switch first"));
    }
  }
  else if (cmd == F("HELP")) {
    showHelp();
  }
  else {
    Serial.println(F("Unknown. Type HELP"));
  }
}

void showStatus() {
  Serial.println(F("\n========== STATUS =========="));
  Serial.print(F("Light: "));
  Serial.print(lastLightValue);
  Serial.print(F(" ["));
  Serial.print(lightStatusText);
  Serial.println(F("]"));
  Serial.print(F("Relay: "));
  Serial.println(relayState ? F("ON") : F("OFF"));
  Serial.print(F("Mode: "));
  Serial.println(manualSwitchState ? F("MANUAL") : F("AUTO"));
  Serial.print(F("Temp: "));
  if (lastTemperature != -999) {
    Serial.print(lastTemperature, 1);
    Serial.println(F("C"));
  } else {
    Serial.println(F("NO SENSOR"));
  }
  Serial.println(F("===========================\n"));
}

void showHelp() {
  Serial.println(F("\n========== COMMANDS =========="));
  Serial.println(F("STATUS  - Show status"));
  Serial.println(F("TOGGLE  - Manual relay (AUTO)"));
  Serial.println(F("HELP    - This help"));
  Serial.println(F("\nSwitch ON = Manual"));
  Serial.println(F("Switch OFF = Auto"));
  Serial.println(F("============================\n"));
}