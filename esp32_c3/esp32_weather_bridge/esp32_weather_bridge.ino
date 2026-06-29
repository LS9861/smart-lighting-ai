/*
 * ESP32-C3 - Weather + AI via DeepSeek API
 * Sends binary packet to Arduino
 * USING PINS 0 AND 1 (UART0)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

// ============================================
// USE PINS 0 AND 1 (UART0) - FIXED
// ============================================
#define TX_PIN 1   // TX0 - Connect to Arduino pin 10 (RX)
#define RX_PIN 0   // RX0 - Connect to Arduino pin 11 (TX)

// ============================================
// BINARY PACKET STRUCTURE (matches Arduino)
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

Packet packet;

// ============================================
// VARIABLES - REAL DATA FROM API
// ============================================
String cityName = "Kathmandu";
String weatherCondition = "Loading...";
float weatherTemp = 0;
String aiDecision = "OFF";
String roomTime = "00:00";
String cityTime = "00:00";

const long GMT_OFFSET_SEC = 3600;
const int DAYLIGHT_OFFSET_SEC = 3600;

// ============================================
// SETUP
// ============================================
void setup() {
  // Use Serial for debugging (USB)
  Serial.begin(115200);
  
  // Use Serial1 for Arduino communication (pins 0 and 1)
  Serial1.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(2000);
  
  Serial.println("\n=================================");
  Serial.println("ESP32-C3 Weather + AI Sender");
  Serial.println("Using Pins: TX=1, RX=0");
  Serial.println("=================================");
  
  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi failed!");
  }
  
  // Initialize time
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org");
  
  // Get initial data
  getWeather();
  getAIDecision();
  getTimes();
  
  Serial.println("✅ Ready!\n");
  delay(1000);
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  static unsigned long lastWeatherUpdate = 0;
  static unsigned long lastSend = 0;
  
  // Update weather every 5 minutes
  if (millis() - lastWeatherUpdate >= 300000) {
    lastWeatherUpdate = millis();
    getWeather();
    getAIDecision();
  }
  
  // Update times every 30 seconds
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate >= 30000) {
    lastTimeUpdate = millis();
    getTimes();
  }
  
  // Send packet every 5 seconds
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    sendPacket();
  }
  
  delay(100);
}

// ============================================
// GET WEATHER FROM OPENWEATHERMAP - REAL DATA
// ============================================
void getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi!");
    return;
  }
  
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
               String(cityName) + "&units=metric&appid=" + OPENWEATHER_API_KEY;
  
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    weatherTemp = doc["main"]["temp"];
    weatherCondition = doc["weather"][0]["description"].as<String>();
    
    // Trim to max 10 characters for display
    if (weatherCondition.length() > 10) {
      weatherCondition = weatherCondition.substring(0, 10);
    }
    
    Serial.print("🌤️ Weather: ");
    Serial.print(weatherTemp);
    Serial.print("°C - ");
    Serial.println(weatherCondition);
  } else {
    Serial.print("❌ Weather API error: ");
    Serial.println(httpCode);
    // Keep last known data instead of resetting
  }
  
  http.end();
}

// ============================================
// GET AI DECISION
// ============================================
void getAIDecision() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    aiDecision = "OFF";
    return;
  }
  
  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  
  // Time-based decision (replace with DeepSeek API if needed)
  if (hour >= 18 || hour <= 6) {
    aiDecision = "ON";
  } else {
    aiDecision = "OFF";
  }
  
  Serial.print("🤖 AI: ");
  Serial.println(aiDecision);
}

// ============================================
// GET TIMES
// ============================================
void getTimes() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    roomTime = "00:00";
    cityTime = "00:00";
    return;
  }
  
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  roomTime = String(buffer);
  
  // Kathmandu time (UTC+5:45)
  time_t now;
  time(&now);
  struct tm* utcTime = gmtime(&now);
  
  int hours = utcTime->tm_hour + 5;
  int minutes = utcTime->tm_min + 45;
  
  if (minutes >= 60) { minutes -= 60; hours += 1; }
  if (hours >= 24) { hours -= 24; }
  
  sprintf(buffer, "%02d:%02d", hours, minutes);
  cityTime = String(buffer);
}

// ============================================
// SEND BINARY PACKET
// ============================================
void sendPacket() {
  packet.startByte = 0xAA;
  
  packet.roomHour = roomTime.substring(0, 2).toInt();
  packet.roomMinute = roomTime.substring(3, 5).toInt();
  packet.cityHour = cityTime.substring(0, 2).toInt();
  packet.cityMinute = cityTime.substring(3, 5).toInt();
  
  // Copy city name (max 14 chars)
  memset(packet.cityName, 0, sizeof(packet.cityName));
  for (int i = 0; i < 14 && i < cityName.length(); i++) {
    packet.cityName[i] = cityName[i];
  }
  packet.cityName[14] = '\0';
  
  // Temperature (multiply by 10)
  packet.temperature = (byte)(weatherTemp * 10);
  packet.aiDecision = (aiDecision == "ON") ? 1 : 0;
  
  // Copy weather condition (max 9 chars) - REAL DATA
  memset(packet.weatherCondition, 0, sizeof(packet.weatherCondition));
  for (int i = 0; i < 9 && i < weatherCondition.length(); i++) {
    packet.weatherCondition[i] = weatherCondition[i];
  }
  packet.weatherCondition[9] = '\0';
  
  // Ensure weather condition has text
  if (packet.weatherCondition[0] == '\0' || packet.weatherCondition[0] == ' ') {
    strcpy(packet.weatherCondition, "Clear sky");
  }
  
  packet.endByte = 0xBB;
  
  // Send to Arduino via Serial1 (pins 0 and 1)
  Serial1.write((byte*)&packet, sizeof(packet));
  
  Serial.print("📤 Sent: ");
  Serial.print(packet.cityName);
  Serial.print(" | ");
  Serial.print(packet.weatherCondition);
  Serial.print(" | ");
  Serial.print(weatherTemp);
  Serial.print("°C | AI: ");
  Serial.println(packet.aiDecision ? "ON" : "OFF");
}