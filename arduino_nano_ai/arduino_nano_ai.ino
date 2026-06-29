/*
 * Arduino Uno - AI Lighting System
 * ABSOLUTE MINIMAL - Guaranteed to fit
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include "config.h"

SoftwareSerial espSerial(10, 11);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_ADDR_ROOM);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

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

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  
  while (espSerial.available()) espSerial.read();
  
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(MANUAL_SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(PIN_RELAY, LOW);
  
  // OLED INIT - MINIMAL
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_ROOM)) {
    Serial.println(F("OLED failed!"));
    while (1);
  }
  
  display.clearDisplay();
  display.display();
  
  sensors.begin();
  tempSensorFound = (sensors.getDeviceCount() > 0);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("Ready"));
  display.display();
}

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

void readESP32Data() {
  if (espSerial.available() >= sizeof(Packet)) {
    byte buffer[sizeof(Packet)];
    for (int i = 0; i < sizeof(Packet); i++) buffer[i] = espSerial.read();
    
    if (buffer[0] == 0xAA && buffer[sizeof(Packet) - 1] == 0xBB) {
      Packet* pkt = (Packet*)buffer;
      sprintf(roomTime, "%02d:%02d", pkt->roomHour, pkt->roomMinute);
      sprintf(cityTime, "%02d:%02d", pkt->cityHour, pkt->cityMinute);
      
      for (int i = 0; i < 14; i++) cityName[i] = pkt->cityName[i];
      cityName[14] = '\0';
      
      float temp = pkt->temperature / 10.0;
      dtostrf(temp, 4, 1, cityTemp);
      
      for (int i = 0; i < 9; i++) cityCondition[i] = pkt->weatherCondition[i];
      cityCondition[9] = '\0';
      if (cityCondition[0] == '\0') strcpy(cityCondition, "Clear");
      
      strcpy(lastAIDecision, pkt->aiDecision ? "ON" : "OFF");
    }
  }
}

void readSensors() {
  lastLightValue = analogRead(PIN_LDR);
  if (lastLightValue < LIGHT_ON_THRESHOLD) lightStatusText = "DRK";
  else if (lastLightValue > LIGHT_OFF_THRESHOLD) lightStatusText = "BRT";
  else lightStatusText = "NRM";
  
  if (tempSensorFound) {
    sensors.requestTemperatures();
    delay(100);
    float temp = sensors.getTempCByIndex(0);
    if (temp != -127.0 && temp < 100) lastTemperature = temp;
  } else {
    static float simTemp = 23.5;
    simTemp += 0.01;
    if (simTemp > 24.5) simTemp = 22.5;
    lastTemperature = simTemp;
  }
}

void makeLightingDecision() {
  if (manualSwitchState) return;
  bool on = relayState;
  
  if (strcmp(lastAIDecision, "ON") == 0) on = true;
  else if (strcmp(lastAIDecision, "OFF") == 0) on = false;
  else {
    if (!relayState && lastLightValue < LIGHT_ON_THRESHOLD) on = true;
    else if (relayState && lastLightValue > LIGHT_OFF_THRESHOLD) on = false;
  }
  
  if (on && !relayState) { digitalWrite(PIN_RELAY, HIGH); relayState = true; }
  else if (!on && relayState) { digitalWrite(PIN_RELAY, LOW); relayState = false; }
}

void checkManualSwitch() {
  bool pressed = (digitalRead(MANUAL_SWITCH_PIN) == LOW);
  if (pressed && !manualSwitchState) {
    manualSwitchState = true;
    if (!relayState) { digitalWrite(PIN_RELAY, HIGH); relayState = true; }
  } else if (!pressed && manualSwitchState) {
    manualSwitchState = false;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (screenState == 0) {
    // ROOM SCREEN
    display.setCursor(0, 0);
    display.print(F("ROOM"));
    if (roomTime[0] != '-') { display.setCursor(85, 0); display.print(roomTime); }
    
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print(lastTemperature, 1); display.print(F("C"));
    
    display.setTextSize(1);
    display.setCursor(0, 44);
    display.print(F("L:")); display.print(lightStatusText);
    display.setCursor(55, 44);
    display.print(F("R:")); display.print(relayState ? F("ON") : F("OFF"));
    
    display.setCursor(0, 54);
    display.print(F("AI:")); display.print(lastAIDecision);
    display.setCursor(55, 54);
    display.print(manualSwitchState ? F("MAN") : F("AUTO"));
  } else {
    // CITY SCREEN
    display.setCursor(0, 0);
    if (cityName[0] != '\0') display.print(cityName);
    else display.print(F("Kathmandu"));
    
    if (cityTime[0] != '-') {
      int x = 128 - (strlen(cityTime) * 6);
      display.setCursor(x, 0);
      display.print(cityTime);
    }
    
    display.setTextSize(2);
    display.setCursor(0, 22);
    if (cityTemp[0] != '-' && cityTemp[0] != '\0') display.print(cityTemp);
    else display.print(F("--"));
    display.print(F("C"));
    
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.print(cityCondition);
    display.setCursor(0, 58);
    display.print(F("ESP32"));
  }
  
  display.display();
}

void handleSerialCommands() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); cmd.toUpperCase();
  
  if (cmd == F("STATUS")) {
    Serial.print(F("LDR:")); Serial.print(lastLightValue);
    Serial.print(F(" R:")); Serial.print(relayState ? "ON" : "OFF");
    Serial.print(F(" AI:")); Serial.println(lastAIDecision);
  }
  else if (cmd == F("TOGGLE")) {
    if (!manualSwitchState) {
      relayState = !relayState;
      digitalWrite(PIN_RELAY, relayState ? HIGH : LOW);
    }
  }
  else if (cmd == F("HELP")) {
    Serial.println(F("STATUS, TOGGLE, HELP"));
  }
}