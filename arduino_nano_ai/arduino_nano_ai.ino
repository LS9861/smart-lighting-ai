/*
 * Arduino Nano - AI Lighting System
 * TWO Screens: ROOM (Germany) and CITY (User selected)
 * FIXED: Weather parsing working
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// ============================================
// OLED DISPLAY
// ============================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_ADDR_ROOM);

// ============================================
// SENSORS
// ============================================
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ============================================
// SYSTEM VARIABLES
// ============================================
bool relayState = false;
int lastLightValue = 0;
float lastTemperature = 23.5;
bool tempSensorFound = false;
bool manualSwitchState = false;

// Data from Python/ESP32
char roomTime[6] = "--:--";
char cityName[20] = "Stuttgart";
char cityTime[6] = "--:--";
char cityTemp[6] = "--";
char cityCondition[25] = "Waiting...";
char lastAIDecision[4] = "---";

// Display state
int screenState = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastSensorRead = 0;

const char* lightStatusText = "NRM";

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(9600);
  
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(MANUAL_SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(PIN_RELAY, LOW);
  
  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_ROOM)) {
    Serial.println(F("OLED failed!"));
    while (1);
  }
  
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(0xFF);
  display.clearDisplay();
  display.display();
  
  // Initialize temperature sensor
  sensors.begin();
  tempSensorFound = (sensors.getDeviceCount() > 0);
  
  if (tempSensorFound) {
    Serial.println(F("✅ DS18B20 found"));
  } else {
    Serial.println(F("⚠️ No DS18B20 - using simulated temp"));
  }
  
  Serial.println(F("\n================================="));
  Serial.println(F("AI Lighting System Ready"));
  Serial.println(F("=================================\n"));
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("AI Lighting"));
  display.setCursor(0, 20);
  display.print(F("Ready..."));
  display.display();
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
  
  readSerialData();
  
  if (now - lastScreenSwitch >= SCREEN_INTERVAL) {
    lastScreenSwitch = now;
    screenState = (screenState == 0) ? 1 : 0;
  }
  
  updateDisplay();
  handleSerialCommands();
  delay(50);
}

// ============================================
// READ SERIAL DATA - FIXED
// ============================================
void readSerialData() {
  if (Serial.available()) {
    String response = Serial.readStringUntil('\n');
    response.trim();
    
    // Debug - see what's coming in
    Serial.print(F("📨 RX: '"));
    Serial.print(response);
    Serial.println(F("'"));
    
    if (response.startsWith(F("ROOM_TIME:"))) {
      response.substring(10).toCharArray(roomTime, 6);
      Serial.print(F("🏠 Room Time: "));
      Serial.println(roomTime);
    }
    else if (response.startsWith(F("CITY_TIME:"))) {
      response.substring(10).toCharArray(cityTime, 6);
      Serial.print(F("🌍 City Time: "));
      Serial.println(cityTime);
    }
    else if (response.startsWith(F("CITY:"))) {
      response.substring(5).toCharArray(cityName, 20);
      Serial.print(F("📍 City: "));
      Serial.println(cityName);
    }
    else if (response.startsWith(F("WEATHER:"))) {
      // Extract after WEATHER:
      String data = response.substring(8);
      data.trim();
      
      Serial.print(F("🌡️ Weather data: '"));
      Serial.print(data);
      Serial.println(F("'"));
      
      int sep = data.indexOf('|');
      if (sep > 0) {
        String temp = data.substring(0, sep);
        String condition = data.substring(sep + 1);
        temp.toCharArray(cityTemp, 6);
        condition.toCharArray(cityCondition, 25);
        Serial.print(F("   ✅ Temp: "));
        Serial.print(cityTemp);
        Serial.print(F("C Condition: "));
        Serial.println(cityCondition);
      } else {
        Serial.print(F("   ⚠️ No '|' found"));
      }
    }
    else if (response.startsWith(F("AI:"))) {
      String decision = response.substring(3);
      decision.trim();
      decision.toUpperCase();
      decision.toCharArray(lastAIDecision, 4);
      Serial.print(F("🤖 AI: "));
      Serial.println(lastAIDecision);
    }
  }
}

// ============================================
// SENSOR READING
// ============================================
void readSensors() {
  lastLightValue = analogRead(PIN_LDR);
  
  if (lastLightValue < LIGHT_ON_THRESHOLD) {
    lightStatusText = "DRK";
  } else if (lastLightValue > LIGHT_OFF_THRESHOLD) {
    lightStatusText = "BRT";
  } else {
    lightStatusText = "NRM";
  }
  
  if (tempSensorFound) {
    sensors.requestTemperatures();
    delay(100);
    float temp = sensors.getTempCByIndex(0);
    if (temp != -127.0 && temp < 100 && temp > -20) {
      lastTemperature = temp;
    }
  } else {
    // Simulated temperature
    static float simulatedTemp = 23.5;
    simulatedTemp += 0.01;
    if (simulatedTemp > 24.5) simulatedTemp = 22.5;
    lastTemperature = simulatedTemp;
  }
  
  Serial.print(F("Light: "));
  Serial.print(lastLightValue);
  Serial.print(F(" ["));
  Serial.print(lightStatusText);
  Serial.print(F("] Relay: "));
  Serial.print(relayState ? "ON" : "OFF");
  Serial.print(F(" AI: "));
  Serial.print(lastAIDecision);
  Serial.print(F(" Temp: "));
  Serial.println(lastTemperature, 1);
}

// ============================================
// LIGHTING CONTROL
// ============================================
void makeLightingDecision() {
  if (manualSwitchState) return;
  
  bool shouldLightBeOn = relayState;
  
  if (!relayState && lastLightValue < LIGHT_ON_THRESHOLD) {
    shouldLightBeOn = true;
  } else if (relayState && lastLightValue > LIGHT_OFF_THRESHOLD) {
    shouldLightBeOn = false;
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
// MANUAL SWITCH
// ============================================
void checkManualSwitch() {
  bool switchPressed = (digitalRead(MANUAL_SWITCH_PIN) == LOW);
  
  if (switchPressed && !manualSwitchState) {
    manualSwitchState = true;
    Serial.println(F("MANUAL ON"));
    if (!relayState) {
      digitalWrite(PIN_RELAY, HIGH);
      relayState = true;
    }
  } 
  else if (!switchPressed && manualSwitchState) {
    manualSwitchState = false;
    Serial.println(F("AUTO"));
  }
}

// ============================================
// OLED DISPLAY
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
  display.setCursor(0, 0);
  display.print(F("ROOM"));
  if (roomTime[0] != '-') {
    display.setCursor(85, 0);
    display.print(roomTime);
  }
  
  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print(lastTemperature, 1);
  display.print(F("C"));
  
  display.setTextSize(1);
  display.setCursor(0, 44);
  display.print(F("L:"));
  display.print(lightStatusText);
  
  display.setCursor(55, 44);
  display.print(F("R:"));
  display.print(relayState ? F("ON") : F("OFF"));
  
  display.setCursor(0, 54);
  display.print(F("AI:"));
  display.print(lastAIDecision);
  
  display.setCursor(80, 54);
  display.print(manualSwitchState ? F("MAN") : F("AUTO"));
}

void drawCityScreen() {
  // City name
  String cityStr = String(cityName);
  if (cityStr.length() > 12) {
    cityStr = cityStr.substring(0, 10) + "..";
  }
  display.setCursor(0, 0);
  display.print(cityStr);
  
  // City time
  if (cityTime[0] != '-') {
    int timeX = 128 - (strlen(cityTime) * 6);
    display.setCursor(timeX, 0);
    display.print(cityTime);
  }
  
  // Temperature
  display.setTextSize(2);
  display.setCursor(0, 22);
  
  if (cityTemp[0] != '-' && atoi(cityTemp) != 0) {
    display.print(cityTemp);
  } else {
    display.print(F("--"));
  }
  display.print(F("C"));
  
  // Weather condition
  display.setTextSize(1);
  display.setCursor(0, 46);
  
  String condStr = String(cityCondition);
  if (condStr.length() > 18) {
    condStr = condStr.substring(0, 16);
  }
  display.print(condStr);
  
  // Update hint
  display.setCursor(0, 58);
  display.print(F("Updates:5min"));
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
    Serial.println(F("\n========== STATUS =========="));
    Serial.print(F("Light: "));
    Serial.print(lastLightValue);
    Serial.print(F(" ["));
    Serial.print(lightStatusText);
    Serial.println(F("]"));
    Serial.print(F("Relay: "));
    Serial.println(relayState ? F("ON") : F("OFF"));
    Serial.print(F("AI: "));
    Serial.println(lastAIDecision);
    Serial.print(F("Room Temp: "));
    Serial.print(lastTemperature, 1);
    Serial.println(F("C"));
    Serial.print(F("City: "));
    Serial.print(cityName);
    Serial.print(F(" Time: "));
    Serial.print(cityTime);
    Serial.print(F(" Weather: "));
    Serial.print(cityTemp);
    Serial.print(F("C "));
    Serial.println(cityCondition);
    Serial.println(F("===========================\n"));
  }
  else if (cmd == F("TOGGLE")) {
    if (!manualSwitchState) {
      relayState = !relayState;
      digitalWrite(PIN_RELAY, relayState ? HIGH : LOW);
      Serial.print(F("Toggle: "));
      Serial.println(relayState ? F("ON") : F("OFF"));
    }
  }
  else if (cmd == F("HELP")) {
    Serial.println(F("Commands: STATUS, TOGGLE, HELP"));
  }
}