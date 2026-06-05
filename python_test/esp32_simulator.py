"""
ESP32 Simulator - Working Version with CORRECT AI decision (uses ROOM time)
"""

import serial
import time
import requests
from datetime import datetime, timezone, timedelta
from dotenv import load_dotenv
import os

load_dotenv()

# ============================================
# CONFIGURATION
# ============================================
ARDUINO_PORT = os.getenv('ARDUINO_PORT', 'COM3')
BAUD_RATE = 9600
OPENWEATHER_API_KEY = os.getenv('OPENWEATHER_API_KEY', '')

# Timezone offsets
ROOM_OFFSET = 1.0  # Germany = UTC+1
CITY_OFFSETS = {
    "kathmandu": 5.75,
    "stuttgart": 1.0,
    "berlin": 1.0,
    "london": 0,
    "new york": -5,
    "tokyo": 9,
    "paris": 1,
    "mumbai": 5.5,
    "dubai": 4,
    "singapore": 8,
    "sydney": 10,
}

# ============================================
# FUNCTIONS
# ============================================

def get_local_time(offset_hours):
    """Get local time for a timezone offset"""
    utc_now = datetime.now(timezone.utc)
    hours = int(offset_hours)
    minutes = int((offset_hours - hours) * 60)
    local_time = utc_now + timedelta(hours=hours, minutes=minutes)
    return local_time.replace(tzinfo=None)

def get_weather(city_name):
    """Get real weather from OpenWeatherMap"""
    if not OPENWEATHER_API_KEY:
        return "22|Clear sky"
    
    try:
        url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name}&appid={OPENWEATHER_API_KEY}&units=metric"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            temp = round(data['main']['temp'])
            condition = data['weather'][0]['description']
            condition = condition[0].upper() + condition[1:]
            return f"{temp}|{condition}"
        else:
            return "22|Clear sky"
    except:
        return "22|Clear sky"

def get_ai_decision(light_value, room_time_str, city_time_str, city):
    """
    AI decision based on ROOM time (where Arduino is located)
    room_time_str: Germany local time (HH:MM)
    """
    hour = int(room_time_str.split(':')[0])
    
    print(f"   🤔 Decision: Room={room_time_str} | City={city_time_str} | Light={light_value}")
    
    # Night time in Germany (after 6 PM or before 6 AM)
    if hour >= 18 or hour <= 6:
        print(f"      → Night in Germany ({room_time_str}) → ON")
        return "ON"
    
    # Day time in Germany - check light sensor
    if light_value < 300:
        print(f"      → Day but dark (light={light_value}) → ON")
        return "ON"
    
    print(f"      → Day and bright (light={light_value}) → OFF")
    return "OFF"

# ============================================
# MAIN
# ============================================

def main():
    print("\n" + "=" * 60)
    print("ESP32 SIMULATOR - AI uses ROOM time (Germany)")
    print("=" * 60)
    
    # Get city from user
    city = input("Enter city name (e.g., Kathmandu, Stuttgart): ").strip()
    if not city:
        city = "Stuttgart"
    city = city.title()
    
    city_lower = city.lower()
    city_offset = CITY_OFFSETS.get(city_lower, 1.0)
    
    print(f"\n📍 Room location: Germany (UTC+{ROOM_OFFSET})")
    print(f"📍 City selected: {city} (UTC+{city_offset})")
    print(f"🔌 COM Port: {ARDUINO_PORT}")
    print("\n" + "=" * 60)
    
    # Connect to Arduino
    try:
        ser = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        print(f"\n✅ Connected to Arduino\n")
    except Exception as e:
        print(f"❌ Cannot connect: {e}")
        return
    
    # Send city name immediately
    ser.write(f"CITY:{city}\n".encode())
    print(f"📤 Sent: CITY:{city}")
    time.sleep(0.5)
    
    # Send initial weather
    weather = get_weather(city)
    ser.write(f"WEATHER:{weather}\n".encode())
    print(f"📤 Sent: WEATHER:{weather}")
    time.sleep(0.5)
    
    # Send initial AI decision (using ROOM time)
    room_time = get_local_time(ROOM_OFFSET)
    city_time = get_local_time(city_offset)
    ai = get_ai_decision(500, room_time.strftime("%H:%M"), city_time.strftime("%H:%M"), city)
    ser.write(f"AI:{ai}\n".encode())
    print(f"📤 Sent: AI:{ai}")
    
    print("\n📡 Sending updates...")
    print("   AI decisions based on GERMANY time (where light is)")
    print("Press Ctrl+C to stop\n")
    
    last_time_sent = 0
    last_weather_sent = 0
    last_ai_sent = 0
    current_light = 500
    
    try:
        while True:
            now = time.time()
            
            # Read light level from Arduino
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if "Light:" in line:
                        parts = line.split()
                        for i, p in enumerate(parts):
                            if p == "Light:" and i+1 < len(parts):
                                current_light = int(parts[i+1])
                except:
                    pass
            
            # Send times (every 30 seconds)
            if now - last_time_sent >= 30:
                room_time = get_local_time(ROOM_OFFSET)
                city_time = get_local_time(city_offset)
                
                ser.write(f"ROOM_TIME:{room_time.strftime('%H:%M')}\n".encode())
                ser.write(f"CITY_TIME:{city_time.strftime('%H:%M')}\n".encode())
                
                print(f"🕐 ROOM time: {room_time.strftime('%H:%M')} (Germany)")
                print(f"   CITY time: {city_time.strftime('%H:%M')} ({city})")
                last_time_sent = now
            
            # Send weather (every 60 seconds)
            if now - last_weather_sent >= 60:
                weather = get_weather(city)
                ser.write(f"WEATHER:{weather}\n".encode())
                print(f"🌤️ Weather: {weather}")
                last_weather_sent = now
            
            # Send AI decision (every 15 seconds) - using ROOM time!
            if now - last_ai_sent >= 15:
                room_time = get_local_time(ROOM_OFFSET)
                city_time = get_local_time(city_offset)
                ai = get_ai_decision(current_light, room_time.strftime("%H:%M"), city_time.strftime("%H:%M"), city)
                ser.write(f"AI:{ai}\n".encode())
                print(f"🤖 AI Decision: {ai}")
                last_ai_sent = now
            
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n\n👋 Stopping simulator")
        ser.close()
        print("✅ Disconnected")

if __name__ == "__main__":
    if not OPENWEATHER_API_KEY:
        print("\n⚠️ WARNING: No OpenWeather API key found!")
        print("   Weather will use mock data.\n")
    
    main()