/*
 * ESP32-C3 - SMART AI with Sunrise/Sunset
 * ROOM: Germany (for AI decision)
 * CITY: Kathmandu (for display only)
 * Pins: TX=1, RX=0
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define TX_PIN 1
#define RX_PIN 0

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

// ==========================================
// TWO CITIES: Room (Germany) + City (Kathmandu)
// ==========================================
String roomCity = "Berlin";      // For AI decision (Germany)
String displayCity = "Kathmandu"; // For CITY screen display

String weatherCondition = "Loading...";
float weatherTemp = 0;
String aiDecision = "OFF";
String roomTime = "00:00";
String cityTime = "00:00";

// Germany sunrise/sunset for AI
int sunriseHour = 6;
int sunriseMinute = 0;
int sunsetHour = 18;
int sunsetMinute = 0;

const long GMT_OFFSET_SEC = 3600;
const int DAYLIGHT_OFFSET_SEC = 3600;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(2000);
  
  Serial.println("\n=================================");
  Serial.println("ESP32-C3 SMART AI - ROOM(Germany) + CITY(Kathmandu)");
  Serial.println("=================================");
  
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
  } else {
    Serial.println("\n❌ WiFi failed!");
  }
  
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org");
  
  // Get weather for BOTH cities
  getWeather(roomCity);      // Germany - for AI decision
  getWeather(displayCity);   // Kathmandu - for display
  
  delay(500);
  getAIDecision();
  getTimes();
  
  Serial.println("✅ Ready!\n");
  delay(1000);
}

void loop() {
  static unsigned long lastWeatherUpdate = 0;
  static unsigned long lastSend = 0;
  
  if (millis() - lastWeatherUpdate >= 300000) {
    lastWeatherUpdate = millis();
    getWeather(roomCity);      // Update Germany weather
    getWeather(displayCity);   // Update Kathmandu weather
    getAIDecision();
  }
  
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate >= 30000) {
    lastTimeUpdate = millis();
    getTimes();
    getAIDecision();
  }
  
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    sendPacket();
  }
  
  delay(100);
}

// ============================================
// GET WEATHER FOR A SPECIFIC CITY
// ============================================
void getWeather(String city) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi!");
    return;
  }
  
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
               city + "&units=metric&appid=" + OPENWEATHER_API_KEY;
  
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    // ==========================================
    // IF THIS IS GERMANY (roomCity) - Get sunrise/sunset for AI
    // ==========================================
    if (city == roomCity) {
      long sunriseUnix = doc["sys"]["sunrise"];
      long sunsetUnix = doc["sys"]["sunset"];
      long timezoneOffset = doc["timezone"].as<long>();
      
      long sunriseLocal = sunriseUnix + timezoneOffset;
      long sunsetLocal = sunsetUnix + timezoneOffset;
      
      long sunriseSeconds = sunriseLocal % 86400;
      long sunsetSeconds = sunsetLocal % 86400;
      
      sunriseHour = sunriseSeconds / 3600;
      sunriseMinute = (sunriseSeconds % 3600) / 60;
      sunsetHour = sunsetSeconds / 3600;
      sunsetMinute = (sunsetSeconds % 3600) / 60;
      
      Serial.println("=================================");
      Serial.println("🇩🇪 GERMANY (Room) - Sunrise/Sunset for AI:");
      Serial.print("   City: ");
      Serial.println(city);
      Serial.print("   Sunrise: ");
      Serial.print(sunriseHour);
      Serial.print(":");
      if (sunriseMinute < 10) Serial.print("0");
      Serial.println(sunriseMinute);
      Serial.print("   Sunset: ");
      Serial.print(sunsetHour);
      Serial.print(":");
      if (sunsetMinute < 10) Serial.print("0");
      Serial.println(sunsetMinute);
      
      int dayLengthMinutes = (sunsetHour * 60 + sunsetMinute) - (sunriseHour * 60 + sunriseMinute);
      if (dayLengthMinutes < 0) dayLengthMinutes += 1440;
      Serial.print("   Day length: ");
      Serial.print(dayLengthMinutes / 60);
      Serial.print("h ");
      Serial.print(dayLengthMinutes % 60);
      Serial.println("m");
      Serial.println("=================================");
    }
    
    // ==========================================
    // IF THIS IS KATHMANDU (displayCity) - Get weather for display
    // ==========================================
    if (city == displayCity) {
      weatherTemp = doc["main"]["temp"];
      weatherCondition = doc["weather"][0]["description"].as<String>();
      
      if (weatherCondition.length() > 10) {
        weatherCondition = weatherCondition.substring(0, 10);
      }
      
      Serial.print("🇳🇵 KATHMANDU (City) - Weather: ");
      Serial.print(weatherTemp);
      Serial.print("°C - ");
      Serial.println(weatherCondition);
    }
    
  } else {
    Serial.print("❌ Weather API error for ");
    Serial.print(city);
    Serial.print(": ");
    Serial.println(httpCode);
    
    // Fallback for Germany
    if (city == roomCity) {
      sunriseHour = 5;
      sunriseMinute = 0;
      sunsetHour = 21;
      sunsetMinute = 30;
    }
    // Fallback for Kathmandu
    if (city == displayCity) {
      weatherTemp = 22.5;
      weatherCondition = "Clear sky";
    }
  }
  
  http.end();
}

// ============================================
// SMART AI DECISION - Based on GERMANY times
// ============================================
void getAIDecision() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    aiDecision = "OFF";
    return;
  }
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  
  int currentMinutes = currentHour * 60 + currentMinute;
  int sunriseMinutes = sunriseHour * 60 + sunriseMinute;
  int sunsetMinutes = sunsetHour * 60 + sunsetMinute;
  
  // ==========================================
  // Daytime/Nighttime detection using GERMANY times
  // ==========================================
  bool isDaytime = false;
  
  if (sunriseMinutes < sunsetMinutes) {
    if (currentMinutes >= sunriseMinutes && currentMinutes < sunsetMinutes) {
      isDaytime = true;
    }
  } else {
    if (currentMinutes >= sunriseMinutes || currentMinutes < sunsetMinutes) {
      isDaytime = true;
    }
  }
  
  // ==========================================
  // 30-minute buffer for smooth transitions
  // ==========================================
  int duskStart = sunsetMinutes - 30;
  int dawnEnd = sunriseMinutes + 30;
  
  if (duskStart < 0) duskStart += 1440;
  if (dawnEnd > 1440) dawnEnd -= 1440;
  
  bool isDusk = false;
  bool isDawn = false;
  
  if (sunsetMinutes > sunriseMinutes) {
    if (currentMinutes >= sunsetMinutes - 30 && currentMinutes < sunsetMinutes) {
      isDusk = true;
    }
    if (currentMinutes >= sunriseMinutes && currentMinutes < sunriseMinutes + 30) {
      isDawn = true;
    }
  }
  
  if (isDusk) {
    aiDecision = "ON";
    Serial.println("🌆 DUSK (30min before sunset) - AI: ON");
  } else if (isDawn) {
    aiDecision = "OFF";
    Serial.println("🌅 DAWN (30min after sunrise) - AI: OFF");
  } else if (isDaytime) {
    aiDecision = "OFF";
    Serial.print("☀️ GERMANY DAYTIME (");
    Serial.print(currentHour);
    Serial.print(":");
    if (currentMinute < 10) Serial.print("0");
    Serial.print(currentMinute);
    Serial.print(") - Sunrise: ");
    Serial.print(sunriseHour);
    Serial.print(":");
    if (sunriseMinute < 10) Serial.print("0");
    Serial.print(sunriseMinute);
    Serial.print(" - Sunset: ");
    Serial.print(sunsetHour);
    Serial.print(":");
    if (sunsetMinute < 10) Serial.print("0");
    Serial.print(sunsetMinute);
    Serial.println(" → AI: OFF");
  } else {
    aiDecision = "ON";
    Serial.print("🌙 GERMANY NIGHTTIME (");
    Serial.print(currentHour);
    Serial.print(":");
    if (currentMinute < 10) Serial.print("0");
    Serial.print(currentMinute);
    Serial.print(") - Sunrise: ");
    Serial.print(sunriseHour);
    Serial.print(":");
    if (sunriseMinute < 10) Serial.print("0");
    Serial.print(sunriseMinute);
    Serial.print(" - Sunset: ");
    Serial.print(sunsetHour);
    Serial.print(":");
    if (sunsetMinute < 10) Serial.print("0");
    Serial.print(sunsetMinute);
    Serial.println(" → AI: ON");
  }
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
  roomTime = String(buffer);  // Germany time (Room)
  
  // Kathmandu time (UTC+5:45) - for City display
  time_t now;
  time(&now);
  struct tm* utcTime = gmtime(&now);
  
  int hours = utcTime->tm_hour + 5;
  int minutes = utcTime->tm_min + 45;
  
  if (minutes >= 60) { minutes -= 60; hours += 1; }
  if (hours >= 24) { hours -= 24; }
  
  sprintf(buffer, "%02d:%02d", hours, minutes);
  cityTime = String(buffer);  // Kathmandu time (City)
}

// ============================================
// SEND BINARY PACKET
// ============================================
void sendPacket() {
  packet.startByte = 0xAA;
  
  // Room = Germany time
  packet.roomHour = roomTime.substring(0, 2).toInt();
  packet.roomMinute = roomTime.substring(3, 5).toInt();
  
  // City = Kathmandu time
  packet.cityHour = cityTime.substring(0, 2).toInt();
  packet.cityMinute = cityTime.substring(3, 5).toInt();
  
  // City name for display
  memset(packet.cityName, 0, sizeof(packet.cityName));
  for (int i = 0; i < 14 && i < displayCity.length(); i++) {
    packet.cityName[i] = displayCity[i];
  }
  packet.cityName[14] = '\0';
  
  // Kathmandu weather for display
  packet.temperature = (byte)(weatherTemp * 10);
  packet.aiDecision = (aiDecision == "ON") ? 1 : 0;
  
  memset(packet.weatherCondition, 0, sizeof(packet.weatherCondition));
  for (int i = 0; i < 9 && i < weatherCondition.length(); i++) {
    packet.weatherCondition[i] = weatherCondition[i];
  }
  packet.weatherCondition[9] = '\0';
  
  if (packet.weatherCondition[0] == '\0' || packet.weatherCondition[0] == ' ') {
    strcpy(packet.weatherCondition, "Clear sky");
  }
  
  packet.endByte = 0xBB;
  
  Serial1.write((byte*)&packet, sizeof(packet));
  
  Serial.print("📤 Sent: Room(Germany): ");
  Serial.print(roomTime);
  Serial.print(" | City(Kathmandu): ");
  Serial.print(cityTime);
  Serial.print(" | ");
  Serial.print(displayCity);
  Serial.print(": ");
  Serial.print(weatherTemp);
  Serial.print("°C ");
  Serial.print(weatherCondition);
  Serial.print(" | AI: ");
  Serial.println(packet.aiDecision ? "ON" : "OFF");
}