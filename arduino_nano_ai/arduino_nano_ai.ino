/*
 * Arduino Uno - Relay + LDR
 * Sends status every 2 seconds (not just on change)
 */

#include <SoftwareSerial.h>
#include "config.h"

SoftwareSerial espSerial(10, 11);

bool relayState = false;
bool manualSwitchState = false;
int lastLightValue = 0;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(MANUAL_SWITCH_PIN, INPUT_PULLUP);
  
  // Start OFF (Active LOW: HIGH = OFF)
  digitalWrite(PIN_RELAY, HIGH);
  relayState = false;
  
  Serial.println("Uno Ready - Active LOW");
}

void loop() {
  // Read LDR
  lastLightValue = analogRead(PIN_LDR);
  
  // Check manual switch
  bool pressed = (digitalRead(MANUAL_SWITCH_PIN) == LOW);
  if (pressed && !manualSwitchState) {
    manualSwitchState = true;
    digitalWrite(PIN_RELAY, LOW);   // ON
    relayState = true;
    Serial.println("🔧 MANUAL ON");
  } else if (!pressed && manualSwitchState) {
    manualSwitchState = false;
    Serial.println("🔧 MANUAL OFF");
    
    // Restore last ESP32 command (default OFF)
    digitalWrite(PIN_RELAY, HIGH);  // OFF
    relayState = false;
    Serial.println("   → Restored: OFF");
  }
  
  // Read ESP32 commands
  if (espSerial.available()) {
    String cmd = espSerial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (!manualSwitchState) {
      if (cmd == "ON") {
        digitalWrite(PIN_RELAY, LOW);   // ON
        relayState = true;
        Serial.println("📨 ESP: ON");
      } else if (cmd == "OFF") {
        digitalWrite(PIN_RELAY, HIGH);  // OFF
        relayState = false;
        Serial.println("📨 ESP: OFF");
      }
    }
  }
  
  // ==========================================
  // SEND STATUS EVERY 2 SECONDS (not just on change)
  // ==========================================
  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 2000) {
    lastSend = millis();
    
    String ldrStatus = (lastLightValue < 250) ? "D" : 
                       (lastLightValue > 350) ? "B" : "N";
    String status = String(ldrStatus) + "|" + 
                    (manualSwitchState ? "1" : "0") + "|" +
                    (relayState ? "1" : "0");
    
    espSerial.println(status);
    Serial.print("📤 ");
    Serial.println(status);
  }
  
  // Debug
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug >= 3000) {
    lastDebug = millis();
    Serial.print("📊 ");
    Serial.print("LDR:");
    Serial.print(lastLightValue);
    Serial.print(" | MAN:");
    Serial.print(manualSwitchState ? "ON" : "OFF");
    Serial.print(" | RLY:");
    Serial.println(relayState ? "ON" : "OFF");
  }
  
  delay(50);
}