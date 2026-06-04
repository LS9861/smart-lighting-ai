/*
 * ESP32-C3 - Weather + AI Bridge
 * 
 * Features:
 * - Fetches city weather from OpenWeatherMap API
 * - Calls DeepSeek AI for lighting decisions
 * - Communicates with Arduino Nano via Serial
 * 
 * Pin Connections:
 * - D0 (RX): To Arduino Nano TX
 * - D1 (TX): To Arduino Nano RX
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// ========== SERIAL COMMUNICATION ==========
const int BAUD_RATE = 9600;
String incomingData = "";

// ========== WEATHER DATA ==========
String cityName = "Berlin";  // Change to your city
float cityTemp = 0;
String weatherCondition = "";
String sunriseTime = "";
String sunsetTime = "";
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_FETCH_INTERVAL = 300000;  // 5 minutes

// ========== AI DECISION ==========
String lastAIDecision = "OFF";
unsigned long lastAICall = 0;
const unsigned long AI_COOLDOWN = 5000;  // 5 seconds

void setup() {
  Serial.begin(115200);      // Debug console
  Serial1.begin(BAUD_RATE);  // Communication with Nano
  
  Serial.println("=================================");
  Serial.println("ESP32-C3 - Weather + AI Bridge");
  Serial.println("=================================");
  
  connectToWiFi();
  fetchWeatherData();  // Initial fetch
  
  Serial.println("Ready to receive commands from Arduino Nano");
  Serial.println("Commands: GET_WEATHER, LIGHT:value");
  Serial.println();
}

void loop() {
  // Handle serial data from Arduino Nano
  if (Serial1.available()) {
    String command = Serial1.readStringUntil('\n');
    command.trim();
    
    Serial.print("📥 Received: ");
    Serial.println(command);
    
    if (command == "GET_WEATHER") {
      sendWeatherData();
    } else if (command.startsWith("LIGHT:")) {
      int lightValue = command.substring(6).toInt();
      String decision = getAIDecision(lightValue);
      Serial1.println(decision);
      Serial.print("📤 Sent AI decision: ");
      Serial.println(decision);
    }
  }
  
  // Periodic weather update
  unsigned long now = millis();
  if (now - lastWeatherFetch >= WEATHER_FETCH_INTERVAL) {
    lastWeatherFetch = now;
    fetchWeatherData();
  }
  
  delay(10);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi connection failed!");
  }
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected - cannot fetch weather");
    return;
  }
  
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
               String(cityName) + "&appid=" + String(OPENWEATHER_API_KEY) + 
               "&units=metric";
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    parseWeatherJSON(payload);
    Serial.print("🌍 Weather updated: ");
    Serial.print(cityTemp, 1);
    Serial.print("°C, ");
    Serial.println(weatherCondition);
  } else {
    Serial.print("❌ Weather API error: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

void parseWeatherJSON(String json) {
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, json);
  
  cityTemp = doc["main"]["temp"];
  weatherCondition = doc["weather"][0]["description"].as<String>();
  
  // Convert sunrise/sunset timestamps to readable time
  unsigned long sunriseUnix = doc["sys"]["sunrise"];
  unsigned long sunsetUnix = doc["sys"]["sunset"];
  
  sunriseTime = convertUnixTime(sunriseUnix);
  sunsetTime = convertUnixTime(sunsetUnix);
}

String convertUnixTime(unsigned long unixTime) {
  time_t rawTime = unixTime;
  struct tm *timeInfo = localtime(&rawTime);
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", timeInfo);
  return String(buffer);
}

void sendWeatherData() {
  // Format: "WEATHER:city|temp|condition|sunrise|sunset"
  String weatherStr = "WEATHER:" + String(cityName) + "|" + 
                      String(cityTemp, 1) + "|" + weatherCondition + "|" +
                      sunriseTime + "|" + sunsetTime;
  
  Serial1.println(weatherStr);
  Serial.println("📤 Sent weather data to Nano");
  lastWeatherFetch = millis();  // Reset timer
}

String getAIDecision(int lightValue) {
  // Check cooldown
  if (millis() - lastAICall < AI_COOLDOWN) {
    return lastAIDecision;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi disconnected - cannot call AI");
    return "OFF";
  }
  
  HTTPClient http;
  
  // Calculate light percentage
  float lightPercent = (lightValue / 1023.0) * 100.0;
  
  // Build prompt with weather context
  String prompt = "Light sensor: " + String(lightValue) + 
                  " (" + String(lightPercent, 1) + "%). ";
  prompt += "City weather: " + String(cityTemp, 1) + "°C, " + weatherCondition + ". ";
  prompt += "Sunrise: " + sunriseTime + ", Sunset: " + sunsetTime + ". ";
  prompt += "Should I turn the light ON or OFF? Reply ONLY 'ON' or 'OFF'.";
  
  http.begin(OPENROUTER_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(OPENROUTER_API_KEY));
  
  String body = "{\"model\":\"deepseek/deepseek-chat:free\",";
  body += "\"messages\":[{\"role\":\"user\",\"content\":\"" + prompt + "\"}],";
  body += "\"max_tokens\":10,\"temperature\":0.1}";
  
  Serial.print("🤖 Calling DeepSeek AI... ");
  int httpCode = http.POST(body);
  
  if (httpCode == 200) {
    String response = http.getString();
    http.end();
    
    if (response.indexOf("ON") > 0) {
      Serial.println("Decision: ON");
      lastAIDecision = "ON";
      lastAICall = millis();
      return "ON";
    } else {
      Serial.println("Decision: OFF");
      lastAIDecision = "OFF";
      lastAICall = millis();
      return "OFF";
    }
  } else {
    Serial.print("❌ API Error: ");
    Serial.println(httpCode);
    http.end();
    return lastAIDecision;
  }
}