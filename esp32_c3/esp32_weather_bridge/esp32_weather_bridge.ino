/*
 * ESP32-C3 - Master Controller
 * FULL WEATHER DISPLAY - Two lines for long conditions
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "secrets.h"

#define SENSOR_INTERVAL         2000
#define SCREEN_INTERVAL         5000
#define LIGHT_ON_THRESHOLD      250
#define LIGHT_OFF_THRESHOLD     350

#define OLED_SDA    5
#define OLED_SCL    6
#define OLED_ADDR   0x3C
#define ONE_WIRE_BUS 7

Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

bool relayState = false;
float lastTemperature = 23.5;
bool tempSensorFound = false;

char roomTime[6] = "--:--";
char cityName[20] = "Kathmandu";
char cityTime[6] = "--:--";
char cityTemp[6] = "--";
char cityCondition[35] = "Waiting...";
char lastAIDecision[4] = "---";

int screenState = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastSensorRead = 0;
const char* lightStatusText = "NRM";

// GERMANY (Room) - For AI decision
String roomCity = "Berlin";
int roomSunriseHour = 6, roomSunriseMinute = 0;
int roomSunsetHour = 18, roomSunsetMinute = 0;

// KATHMANDU (City) - For display only
String displayCity = "Kathmandu";

bool wifiConnected = false;
bool manualMode = false;
String ldrStatus = "N";
String relayStatus = "0";

const long GMT_OFFSET_SEC = 3600;
const int DAYLIGHT_OFFSET_SEC = 3600;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 20, 21);
  
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 Starting...");
  display.display();
  delay(500);
  
  display.ssd1306_command(SSD1306_DISPLAYON);
  
  sensors.begin();
  tempSensorFound = (sensors.getDeviceCount() > 0);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org");
    getWeather(roomCity, true);
    getWeather(displayCity, false);
  } else {
    wifiConnected = false;
    roomSunriseHour = 6;
    roomSunriseMinute = 0;
    roomSunsetHour = 18;
    roomSunsetMinute = 0;
  }
  
  getTimes();
  updateAI();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(wifiConnected ? "Ready - WiFi OK" : "FALLBACK MODE");
  display.display();
  
  lastScreenSwitch = millis();
  
  Serial.println("ESP32 Ready");
  Serial.print("Room (Germany): ");
  Serial.println(roomCity);
  Serial.print("Display (City): ");
  Serial.println(displayCity);
}

void loop() {
  unsigned long now = millis();
  
  // Check WiFi
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org");
      getWeather(roomCity, true);
      getWeather(displayCity, false);
    }
  } else {
    if (wifiConnected) {
      wifiConnected = false;
    }
  }
  
  // Read Arduino status
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    
    if (msg.length() >= 5) {
      ldrStatus = msg.substring(0, 1);
      manualMode = (msg.substring(2, 3) == "1");
      relayStatus = msg.substring(4, 5);
      relayState = (relayStatus == "1");
      
      if (ldrStatus == "D") lightStatusText = "DRK";
      else if (ldrStatus == "B") lightStatusText = "BRT";
      else lightStatusText = "NRM";
      
      Serial.print("📥 ");
      Serial.print("LDR:");
      Serial.print(ldrStatus);
      Serial.print(" | MAN:");
      Serial.print(manualMode ? "ON" : "OFF");
      Serial.print(" | RLY:");
      Serial.println(relayState ? "ON" : "OFF");
    }
  }
  
  // Read temperature
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
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
    updateAI();
    sendRelayCommand();
  }
  
  // Update time
  static unsigned long lastTimeUpdate = 0;
  if (now - lastTimeUpdate >= 5000) {
    lastTimeUpdate = now;
    getTimes();
  }
  
  // Screen switching
  if (now - lastScreenSwitch >= SCREEN_INTERVAL) {
    lastScreenSwitch = now;
    screenState = (screenState == 0) ? 1 : 0;
  }
  
  // Update weather every 5 minutes
  static unsigned long lastWeatherUpdate = 0;
  if (wifiConnected && (now - lastWeatherUpdate >= 300000)) {
    lastWeatherUpdate = now;
    getWeather(roomCity, true);
    getWeather(displayCity, false);
    updateAI();
  }
  
  // Update display
  updateDisplay();
  
  delay(50);
}

void getWeather(String city, bool isRoom) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
               city + "&units=metric&appid=" + OPENWEATHER_API_KEY;
  
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    deserializeJson(doc, payload);
    
    if (isRoom) {
      long tz = doc["timezone"].as<long>();
      long sr = doc["sys"]["sunrise"].as<long>() + tz;
      long ss = doc["sys"]["sunset"].as<long>() + tz;
      
      roomSunriseHour = (sr % 86400) / 3600;
      roomSunriseMinute = ((sr % 86400) % 3600) / 60;
      roomSunsetHour = (ss % 86400) / 3600;
      roomSunsetMinute = ((ss % 86400) % 3600) / 60;
      
      Serial.println("=================================");
      Serial.print("🇩🇪 GERMANY (Room) - Sunrise/Sunset:");
      Serial.print("  Sunrise: ");
      Serial.print(roomSunriseHour);
      Serial.print(":");
      if (roomSunriseMinute < 10) Serial.print("0");
      Serial.println(roomSunriseMinute);
      Serial.print("  Sunset: ");
      Serial.print(roomSunsetHour);
      Serial.print(":");
      if (roomSunsetMinute < 10) Serial.print("0");
      Serial.println(roomSunsetMinute);
      Serial.println("=================================");
      
    } else {
      float temp = doc["main"]["temp"];
      dtostrf(temp, 4, 1, cityTemp);
      String cond = doc["weather"][0]["description"].as<String>();
      
      // Store FULL condition (no truncation)
      strcpy(cityCondition, cond.c_str());
      
      Serial.print("🇳🇵 KATHMANDU (City) - Weather: ");
      Serial.print(cityTemp);
      Serial.print("°C ");
      Serial.println(cityCondition);
    }
    
  } else {
    Serial.print("❌ Weather API error for ");
    Serial.print(city);
    Serial.print(": ");
    Serial.println(httpCode);
    
    if (isRoom) {
      roomSunriseHour = 6;
      roomSunriseMinute = 0;
      roomSunsetHour = 21;
      roomSunsetMinute = 30;
    }
  }
  http.end();
}

void updateAI() {
  if (manualMode) {
    Serial.println("🔧 MANUAL MODE - AI disabled");
    return;
  }
  
  if (!wifiConnected) {
    strcpy(lastAIDecision, (ldrStatus == "D") ? "ON" : "OFF");
    return;
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    strcpy(lastAIDecision, (ldrStatus == "D") ? "ON" : "OFF");
    return;
  }
  
  int current = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int sunrise = roomSunriseHour * 60 + roomSunriseMinute;
  int sunset = roomSunsetHour * 60 + roomSunsetMinute;
  
  bool isNight = (current < sunrise || current >= sunset);
  
  if (isNight) {
    strcpy(lastAIDecision, "ON");
    Serial.print("🌙 NIGHTTIME - AI: ON");
  } else {
    strcpy(lastAIDecision, "OFF");
    Serial.print("☀️ DAYTIME - AI: OFF");
  }
  
  Serial.print(" | Time: ");
  Serial.print(timeinfo.tm_hour);
  Serial.print(":");
  if (timeinfo.tm_min < 10) Serial.print("0");
  Serial.print(timeinfo.tm_min);
  Serial.print(" | Sunrise: ");
  Serial.print(roomSunriseHour);
  Serial.print(":");
  if (roomSunriseMinute < 10) Serial.print("0");
  Serial.print(roomSunriseMinute);
  Serial.print(" | Sunset: ");
  Serial.print(roomSunsetHour);
  Serial.print(":");
  if (roomSunsetMinute < 10) Serial.print("0");
  Serial.println(roomSunsetMinute);
}

void sendRelayCommand() {
  if (manualMode) return;
  
  static char lastSent[4] = "";
  if (strcmp(lastAIDecision, lastSent) != 0) {
    strcpy(lastSent, lastAIDecision);
    Serial1.println(lastAIDecision);
    Serial.print("📤 Sent to Uno: ");
    Serial.println(lastAIDecision);
  }
}

void getTimes() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    strftime(roomTime, 6, "%H:%M", &timeinfo);
  } else {
    strcpy(roomTime, "--:--");
  }
  
  time_t now;
  time(&now);
  struct tm* utc = gmtime(&now);
  int h = utc->tm_hour + 5;
  int m = utc->tm_min + 45;
  if (m >= 60) { m -= 60; h += 1; }
  if (h >= 24) h -= 24;
  sprintf(cityTime, "%02d:%02d", h, m);
}

void updateDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (screenState == 0) {
    // ==========================================
    // ROOM SCREEN - Germany
    // ==========================================
    display.setCursor(0, 0);
    display.print("ROOM");
    if (roomTime[0] != '-') {
      display.setCursor(85, 0);
      display.print(roomTime);
    }
    
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print(lastTemperature, 1);
    display.print("C");
    
    display.setTextSize(1);
    display.setCursor(0, 44);
    display.print("L:");
    display.print(lightStatusText);
    display.setCursor(55, 44);
    display.print("R:");
    display.print(relayState ? "ON" : "OFF");
    
    display.setCursor(0, 54);
    display.print("AI:");
    display.print(lastAIDecision);
    display.setCursor(55, 54);
    
    if (manualMode) {
      display.print("MAN");
    } else if (!wifiConnected) {
      display.print("FALLBACK");
    } else {
      display.print("AUTO");
    }
    
  } else {
    // ==========================================
    // CITY SCREEN - Kathmandu with FULL weather
    // ==========================================
    display.setCursor(0, 0);
    display.print(displayCity);
    if (cityTime[0] != '-') {
      int x = 128 - (strlen(cityTime) * 6);
      display.setCursor(x, 0);
      display.print(cityTime);
    }
    
    display.setTextSize(2);
    display.setCursor(0, 22);
    if (cityTemp[0] != '-' && cityTemp[0] != '\0') {
      display.print(cityTemp);
    } else {
      display.print("--");
    }
    display.print("C");
    
    // ==========================================
    // WEATHER CONDITION - FULL TEXT (2 lines)
    // ==========================================
    display.setTextSize(1);
    String condStr = String(cityCondition);
    condStr.trim();
    
    if (condStr.length() > 20) {
      // Split at first space near the middle
      int splitPos = 16;
      for (int i = 10; i < condStr.length() && i < 20; i++) {
        if (condStr.charAt(i) == ' ') {
          splitPos = i;
          break;
        }
      }
      
      String line1 = condStr.substring(0, splitPos);
      String line2 = condStr.substring(splitPos + 1);
      
      // If line2 is still too long, truncate
      if (line2.length() > 18) {
        line2 = line2.substring(0, 18);
      }
      
      display.setCursor(0, 46);
      display.print(line1);
      display.setCursor(0, 54);
      display.print(line2);
      
    } else {
      // Short weather - one line
      display.setCursor(0, 46);
      display.print(condStr);
      // Show ESP32 status on second line if weather is short
      display.setCursor(0, 54);
      display.print(wifiConnected ? "ESP32" : "OFFLINE");
    }
  }
  
  display.display();
}