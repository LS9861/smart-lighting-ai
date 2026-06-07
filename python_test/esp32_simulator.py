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
# ============================================
# TIMEZONE OFFSETS BY COUNTRY (not by city!)
# ============================================
COUNTRY_OFFSETS = {
    "NP": 5.75,    # Nepal
    "DE": 1.0,     # Germany
    "GB": 0,       # United Kingdom
    "US": -5,      # USA (Eastern Time)
    "FR": 1.0,     # France
    "JP": 9,       # Japan
    "IN": 5.5,     # India
    "AU": 10,      # Australia
    "AE": 4,       # UAE (Dubai)
    "SG": 8,       # Singapore
    "CN": 8,       # China
    "BR": -3,      # Brazil
    "ZA": 2,       # South Africa
    "RU": 3,       # Russia (Moscow)
    "CA": -5,      # Canada (Eastern)
    "MX": -6,      # Mexico
    "IT": 1.0,     # Italy
    "ES": 1.0,     # Spain
    "TR": 3,       # Turkey
    "TH": 7,       # Thailand
    "VN": 7,       # Vietnam
    "MY": 8,       # Malaysia
    "PH": 8,       # Philippines
    "PK": 5,       # Pakistan
    "BD": 6,       # Bangladesh
    "LK": 5.5,     # Sri Lanka
}
# Room location offset (Germany)
ROOM_OFFSET = 1.0  # Germany = UTC+1

def get_timezone_from_city(city_name):
    """Automatically get timezone offset by looking up city's country"""
    if not OPENWEATHER_API_KEY:
        return 1.0
    
    try:
        # First try to get city info from weather API
        url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name}&appid={OPENWEATHER_API_KEY}&units=metric"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            country_code = data['sys']['country']
            city_found = data['name']
            
            print(f"   📍 Found: {city_found}, {country_code}")
            
            # Look up country in our dictionary
            if country_code in COUNTRY_OFFSETS:
                offset = COUNTRY_OFFSETS[country_code]
                print(f"   🕐 Timezone: UTC+{offset}")
                return offset
            else:
                print(f"   ⚠️ Unknown country code: {country_code}, using UTC+0")
                return 0
        else:
            print(f"   ⚠️ Could not find city '{city_name}'")
            return 1.0
            
    except Exception as e:
        print(f"   ⚠️ Error: {e}")
        return 1.0

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


# ============================================
# NEW: IMPROVED WEATHER FUNCTIONS (Add these)
# ============================================

def get_city_coordinates(city_name, country_code=None):
    """Get latitude and longitude for any city using Geocoding API"""
    if not OPENWEATHER_API_KEY:
        return None, None
    
    try:
        # Build search query
        query = city_name
        if country_code:
            query = f"{city_name},{country_code}"
        
        url = f"http://api.openweathermap.org/geo/1.0/direct?q={query}&limit=1&appid={OPENWEATHER_API_KEY}"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200 and response.json():
            data = response.json()[0]
            lat = data.get('lat')
            lon = data.get('lon')
            city_found = data.get('name')
            country_found = data.get('country')
            print(f"   📍 Found: {city_found}, {country_found}")
            return lat, lon
        else:
            print(f"   ⚠️ Could not find coordinates for '{city_name}'")
            return None, None
            
    except Exception as e:
        print(f"   ⚠️ Geocoding error: {e}")
        return None, None

def get_weather_by_coordinates(lat, lon):
    """Get weather using coordinates (most accurate method)"""
    try:
        url = f"http://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={OPENWEATHER_API_KEY}&units=metric"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            temp = round(data['main']['temp'])
            condition = data['weather'][0]['description']
            condition = condition[0].upper() + condition[1:]
            city_name = data.get('name', 'Unknown')
            print(f"   ✅ Weather: {temp}°C, {condition}")
            return f"{temp}|{condition}"
        else:
            return "22|Clear sky"
    except Exception as e:
        print(f"   ⚠️ Weather error: {e}")
        return "22|Clear sky"

def get_weather(city_name):
    """Get real weather - works for ANY city using multiple methods"""
    if not OPENWEATHER_API_KEY:
        return "22|Clear sky"
    
    print(f"\n🔍 Looking up weather for '{city_name}'...")
    
    # METHOD 1: Try direct city name first
    try:
        url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name}&appid={OPENWEATHER_API_KEY}&units=metric"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            temp = round(data['main']['temp'])
            condition = data['weather'][0]['description']
            condition = condition[0].upper() + condition[1:]
            print(f"   ✅ Found: {data['name']}, {data['sys']['country']}")
            return f"{temp}|{condition}"
    except:
        pass
    
    # METHOD 2: Try with common country codes
    common_countries = {
        "nepal": "NP",
        "germany": "DE",
        "india": "IN", 
        "usa": "US",
        "uk": "GB",
        "france": "FR",
        "japan": "JP",
        "australia": "AU"
    }
    
    city_lower = city_name.lower()
    for country_name, code in common_countries.items():
        if country_name in city_lower or city_lower in country_name:
            try:
                url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name},{code}&appid={OPENWEATHER_API_KEY}&units=metric"
                response = requests.get(url, timeout=10)
                if response.status_code == 200:
                    data = response.json()
                    temp = round(data['main']['temp'])
                    condition = data['weather'][0]['description']
                    condition = condition[0].upper() + condition[1:]
                    print(f"   ✅ Found with country code {code}: {data['name']}, {data['sys']['country']}")
                    return f"{temp}|{condition}"
            except:
                pass
    
    # METHOD 3: Try Geocoding API (find by coordinates)
    print(f"   🔍 Trying geocoding for '{city_name}'...")
    lat, lon = get_city_coordinates(city_name)
    if lat and lon:
        return get_weather_by_coordinates(lat, lon)
    
    # METHOD 4: If all fails, ask user for country code
    print(f"   ⚠️ '{city_name}' not found automatically.")
    country_code = input(f"   Enter country code for {city_name} (e.g., NP, DE, US): ").strip().upper()
    
    if country_code:
        try:
            url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name},{country_code}&appid={OPENWEATHER_API_KEY}&units=metric"
            response = requests.get(url, timeout=10)
            if response.status_code == 200:
                data = response.json()
                temp = round(data['main']['temp'])
                condition = data['weather'][0]['description']
                condition = condition[0].upper() + condition[1:]
                print(f"   ✅ Found: {data['name']}, {data['sys']['country']}")
                return f"{temp}|{condition}"
        except:
            pass
    
    print(f"   ⚠️ Could not find '{city_name}' in weather database")
    print(f"   Using mock data: 22°C, Clear sky")
    return "22|Clear sky"


# ============================================
# ORIGINAL get_ai_decision FUNCTION (keep as is)
# ============================================

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
    city_offset = get_timezone_from_city(city)
    
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