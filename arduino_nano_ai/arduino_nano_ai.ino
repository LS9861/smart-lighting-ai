/*
 * Arduino Uno - AI Lighting System
 * FIXED: Weather condition display
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include "config.h"

// ============================================
// SOFTWARE SERIAL - ESP32 on pins 10 and 11
// ============================================
SoftwareSerial espSerial(10, 11);

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
// BINARY PACKET STRUCTURE
// ============================================
struct Packet {
  byte startByte;
  byte roomHour;
  byte roomMinute;
  byte cityHour;
  byte cityMinute;
  char cityName[15];
  byte temperature;
  byte aiDecision;
  char weatherCondition[10];
  byte endByte;
};

// ============================================
// SYSTEM VARIABLES
// ============================================
bool relayState = false;
int lastLightValue = 0;
float lastTemperature = 23.5;
bool tempSensorFound = false;
bool manualSwitchState = false;

char roomTime[6] = "--:--";
char cityName[20] = "Kathmandu";
char cityTime[6] = "--:--";
char cityTemp[6] = "--";
char cityCondition[25] = "Waiting...";
char lastAIDecision[4] = "---";

int screenState = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastSensorRead = 0;
const char* lightStatusText = "NRM";

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  
  while (espSerial.available()) {
    espSerial.read();
  }
  
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(MANUAL_SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(PIN_RELAY, LOW);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_ROOM)) {
    Serial.println(F("OLED failed!"));
    while (1);
  }
  
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(0xFF);
  display.clearDisplay();
  display.display();
  
  sensors.begin();
  tempSensorFound = (sensors.getDeviceCount() > 0);
  
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
  
  readESP32Data();
  
  if (now - lastScreenSwitch >= SCREEN_INTERVAL) {
    lastScreenSwitch = now;
    screenState = (screenState == 0) ? 1 : 0;
  }
  
  updateDisplay();
  handleSerialCommands();
  delay(50);
}

// ============================================
// READ ESP32 DATA
// ============================================
void readESP32Data() {
  if (espSerial.available() >= sizeof(Packet)) {
    byte buffer[sizeof(Packet)];
    
    for (int i = 0; i < sizeof(Packet); i++) {
      buffer[i] = espSerial.read();
    }
    
    if (buffer[0] == 0xAA && buffer[sizeof(Packet) - 1] == 0xBB) {
      Packet* pkt = (Packet*)buffer;
      
      sprintf(roomTime, "%02d:%02d", pkt->roomHour, pkt->roomMinute);
      sprintf(cityTime, "%02d:%02d", pkt->cityHour, pkt->cityMinute);
      
      // Copy city name
      memset(cityName, 0, sizeof(cityName));
      for (int i = 0; i < 14 && i < sizeof(pkt->cityName); i++) {
        cityName[i] = pkt->cityName[i];
      }
      cityName[14] = '\0';
      
      // Temperature
      float temp = pkt->temperature / 10.0;
      dtostrf(temp, 4, 1, cityTemp);
      
      // Copy weather condition - FIXED
      memset(cityCondition, 0, sizeof(cityCondition));
      for (int i = 0; i < 9 && i < sizeof(pkt->weatherCondition); i++) {
        cityCondition[i] = pkt->weatherCondition[i];
      }
      cityCondition[9] = '\0';
      
      // If empty, use default
      if (cityCondition[0] == '\0' || cityCondition[0] == ' ') {
        strcpy(cityCondition, "Clear sky");
      }
      
      if (pkt->aiDecision == 1) {
        strcpy(lastAIDecision, "ON");
      } else {
        strcpy(lastAIDecision, "OFF");
      }
      
      Serial.print("📨 ");
      Serial.print(cityName);
      Serial.print(" | ");
      Serial.print(cityCondition);
      Serial.print(" | ");
      Serial.println(cityTemp);
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
  
  if (strcmp(lastAIDecision, "ON") == 0) {
    shouldLightBeOn = true;
  } else if (strcmp(lastAIDecision, "OFF") == 0) {
    shouldLightBeOn = false;
  } else {
    if (!relayState && lastLightValue < LIGHT_ON_THRESHOLD) {
      shouldLightBeOn = true;
    } else if (relayState && lastLightValue > LIGHT_OFF_THRESHOLD) {
      shouldLightBeOn = false;
    }
  }
  
  if (shouldLightBeOn && !relayState) {
    digitalWrite(PIN_RELAY, HIGH);
    relayState = true;
    Serial.println(F("🔦 LIGHT ON"));
  } else if (!shouldLightBeOn && relayState) {
    digitalWrite(PIN_RELAY, LOW);
    relayState = false;
    Serial.println(F("💡 LIGHT OFF"));
  }
}

// ============================================
// MANUAL SWITCH
// ============================================
void checkManualSwitch() {
  bool switchPressed = (digitalRead(MANUAL_SWITCH_PIN) == LOW);
  
  if (switchPressed && !manualSwitchState) {
    manualSwitchState = true;
    Serial.println(F("🔧 MANUAL ON"));
    if (!relayState) {
      digitalWrite(PIN_RELAY, HIGH);
      relayState = true;
    }
  } 
  else if (!switchPressed && manualSwitchState) {
    manualSwitchState = false;
    Serial.println(F("🔄 AUTO"));
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

// ============================================
// DRAW CITY SCREEN
// ============================================
void drawCityScreen() {
  // City name
  display.setCursor(0, 0);
  if (cityName[0] != '\0' && cityName[0] != ' ') {
    display.print(cityName);
  } else {
    display.print(F("Kathmandu"));
  }
  
  // City time
  if (cityTime[0] != '-') {
    int timeX = 128 - (strlen(cityTime) * 6);
    display.setCursor(timeX, 0);
    display.print(cityTime);
  }
  
  // Temperature
  display.setTextSize(2);
  display.setCursor(0, 22);
  if (cityTemp[0] != '-' && cityTemp[0] != '\0') {
    display.print(cityTemp);
  } else {
    display.print(F("--"));
  }
  display.print(F("C"));
  
  // Weather condition
  display.setTextSize(1);
  display.setCursor(0, 46);
  
  // Check if weather condition has valid text
  bool hasValidText = false;
  for (int i = 0; i < 10 && cityCondition[i] != '\0'; i++) {
    if (cityCondition[i] >= 'A' && cityCondition[i] <= 'z') {
      hasValidText = true;
      break;
    }
  }
  
  if (hasValidText) {
    display.print(cityCondition);
  } else {
    display.print(F("Clear sky"));
  }
  
  display.setCursor(0, 58);
  display.print(F("ESP32"));
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
    Serial.print(F("Temp: "));
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