"""
ESP32 Simulator - Working Version with CORRECT AI decision (uses ROOM time and SUNSET)
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

# ============================================
# CLOUD API FUNCTION
# ============================================

def send_to_cloud(light_value=None, light_status=None, relay_state=None, mode=None, ai_decision=None, temperature=None, city=None, condition=None, city_time=None):
    """Send real-time data to cloud API"""
    try:
        url = "https://smart-lighting-ai.onrender.com/submit"
        data = {}
        
        if light_value is not None:
            data["light_value"] = light_value
        if light_status is not None:
            data["light_status"] = light_status
        if relay_state is not None:
            data["relay_state"] = relay_state
        if mode is not None:
            data["mode"] = mode
        if ai_decision is not None:
            data["ai_decision"] = ai_decision
        if temperature is not None:
            data["temperature"] = temperature
        if city is not None:
            data["city"] = city
        if condition is not None:
            data["condition"] = condition
        if city_time is not None:
            data["city_time"] = city_time
        
        if data:
            response = requests.post(url, json=data, timeout=2)
            if response.status_code == 200:
                if light_value is not None:
                    print(f"   ☁️ Cloud: Light={light_value}, AI={ai_decision}")
                elif temperature is not None:
                    print(f"   ☁️ Cloud: Weather={city} {temperature}°C, {condition}")
    except Exception as e:
        pass

# ============================================
# TIMEZONE OFFSETS BY COUNTRY
# ============================================
COUNTRY_OFFSETS = {
    "NP": 5.75, "DE": 1.0, "GB": 0, "US": -5, "FR": 1.0, "JP": 9,
    "IN": 5.5, "AU": 10, "AE": 4, "SG": 8, "CN": 8, "BR": -3,
    "ZA": 2, "RU": 3, "CA": -5, "MX": -6, "IT": 1.0, "ES": 1.0,
    "TR": 3, "TH": 7, "VN": 7, "MY": 8, "PH": 8, "PK": 5, "BD": 6, "LK": 5.5,
}

simulated_room_temp = 23.5

# ============================================
# TIME FUNCTIONS
# ============================================

def get_timezone_from_city(city_name):
    """Automatically get timezone offset by looking up city's country"""
    if not OPENWEATHER_API_KEY:
        return 1.0
    
    try:
        url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name}&appid={OPENWEATHER_API_KEY}&units=metric"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            country_code = data['sys']['country']
            city_found = data['name']
            print(f"   📍 Found: {city_found}, {country_code}")
            
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


def get_room_time():
    """Get current Germany time from system (handles DST automatically)"""
    return datetime.now()


def get_city_time(city_offset):
    """Get city time using offset from UTC"""
    utc_now = datetime.now(timezone.utc)
    hours = int(city_offset)
    minutes = int((city_offset - hours) * 60)
    local_time = utc_now + timedelta(hours=hours, minutes=minutes)
    return local_time.replace(tzinfo=None)

# ============================================
# WEATHER FUNCTIONS WITH SUNSET
# ============================================

def get_city_coordinates(city_name, country_code=None):
    """Get latitude and longitude for any city using Geocoding API"""
    if not OPENWEATHER_API_KEY:
        return None, None
    
    try:
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
            sunset_timestamp = data['sys']['sunset']
            sunset_time = datetime.fromtimestamp(sunset_timestamp)
            sunset_hour = sunset_time.hour
            sunset_minute = sunset_time.minute
            print(f"   ✅ Weather: {temp}°C, {condition}, Sunset: {sunset_hour}:{sunset_minute:02d}")
            return f"{temp}|{condition}|{sunset_hour}|{sunset_minute}"
        else:
            return "22|Clear sky|20|0"
    except Exception as e:
        print(f"   ⚠️ Weather error: {e}")
        return "22|Clear sky|20|0"


def get_weather(city_name):
    """Get real weather with sunset time"""
    if not OPENWEATHER_API_KEY:
        return "22|Clear sky|20|0"
    
    print(f"\n🔍 Looking up weather for '{city_name}'...")
    
    # Try direct city name first
    try:
        url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name}&appid={OPENWEATHER_API_KEY}&units=metric"
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            temp = round(data['main']['temp'])
            condition = data['weather'][0]['description']
            condition = condition[0].upper() + condition[1:]
            sunset_timestamp = data['sys']['sunset']
            sunset_time = datetime.fromtimestamp(sunset_timestamp)
            sunset_hour = sunset_time.hour
            sunset_minute = sunset_time.minute
            print(f"   ✅ Found: {data['name']}, {data['sys']['country']}")
            print(f"   🌅 Sunset: {sunset_hour}:{sunset_minute:02d}")
            return f"{temp}|{condition}|{sunset_hour}|{sunset_minute}"
    except:
        pass
    
    # Fallback
    print(f"   ⚠️ Could not find '{city_name}' in weather database")
    print(f"   Using mock data: 22°C, Clear sky, Sunset 20:00")
    return "22|Clear sky|20|0"


def get_ai_decision(light_value, room_time_str, city_time_str, city, sunset_hour=None, sunset_minute=None):
    """
    AI decision based on ROOM time and ACTUAL SUNSET time.
    
    Logic:
    - Light should be ON after sunset (when it gets dark)
    - Light should be OFF during daylight hours
    - Light sensor provides backup if API fails
    """
    hour = int(room_time_str.split(':')[0])
    minute = int(room_time_str.split(':')[1])
    current_time_minutes = hour * 60 + minute
    
    # Use actual sunset time from weather API (if available)
    if sunset_hour is not None:
        sunset_minutes = sunset_hour * 60 + (sunset_minute or 0)
        # Turn ON 30 minutes BEFORE sunset (as it starts getting dark)
        # Turn OFF after sunrise (you can add sunrise logic too)
        if current_time_minutes >= (sunset_minutes - 30):
            print(f"      → Sunset at {sunset_hour}:{sunset_minute or 0:02d}, current {room_time_str} - turning ON")
            return "ON"
        else:
            print(f"      → Before sunset ({sunset_hour}:{sunset_minute or 0:02d}), current {room_time_str} - keeping OFF")
            return "OFF"
    
    # Fallback: Use light sensor if no sunset data
    if light_value < 300:
        print(f"      → Dark detected (light={light_value}), turning ON")
        return "ON"
    
    print(f"      → Daytime, bright (light={light_value}), turning OFF")
    return "OFF"

# ============================================
# MAIN
# ============================================

def main():
    global simulated_room_temp
    
    print("\n" + "=" * 60)
    print("ESP32 SIMULATOR - AI uses ROOM time + ACTUAL SUNSET")
    print("=" * 60)
    
    # Get city from user
    city = input("Enter city name (e.g., Kathmandu, Stuttgart): ").strip()
    if not city:
        city = "Stuttgart"
    city = city.title()
    
    city_lower = city.lower()
    city_offset = get_timezone_from_city(city)
    
    print(f"\n📍 Room location: Germany (using system time)")
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
    
    # Send weather to cloud
    if '|' in weather:
        parts = weather.split('|')
        temp = float(parts[0])
        condition = parts[1]
        send_to_cloud(temperature=temp, city=city, condition=condition)
    
    time.sleep(0.5)
    
    # Send initial AI decision (using ROOM time)
    room_time = get_room_time()
    city_time = get_city_time(city_offset)
    ai = get_ai_decision(500, room_time.strftime("%H:%M"), city_time.strftime("%H:%M"), city)
    ser.write(f"AI:{ai}\n".encode())
    print(f"📤 Sent: AI:{ai}")
    
    print("\n📡 Sending updates...")
    print("   AI decisions based on ACTUAL SUNSET time in Germany")
    print("Press Ctrl+C to stop\n")
    
    last_time_sent = 0
    last_weather_sent = 0
    last_ai_sent = 0
    current_light = 500
    current_relay = "OFF"
    current_ai = "---"
    current_sunset_hour = None
    current_sunset_minute = None
    
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
                            if p == "Relay:" and i+1 < len(parts):
                                current_relay = parts[i+1]
                            if p == "AI:" and i+1 < len(parts):
                                current_ai = parts[i+1]
                        
                        # Simulate room temperature variation
                        simulated_room_temp += 0.01
                        if simulated_room_temp > 24.5:
                            simulated_room_temp = 22.5
                        
                        # Send to cloud
                        send_to_cloud(
                            light_value=current_light, 
                            light_status="NRM", 
                            relay_state=current_relay, 
                            mode="AUTO", 
                            ai_decision=current_ai,
                            temperature=round(simulated_room_temp, 1)
                        )
                except:
                    pass
            
            # Send times (every 30 seconds)
            if now - last_time_sent >= 30:
                room_time = get_room_time()
                city_time = get_city_time(city_offset)
                
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
                
                # Parse sunset from weather
                if '|' in weather:
                    parts = weather.split('|')
                    if len(parts) >= 4:
                        temp = float(parts[0])
                        condition = parts[1]
                        current_sunset_hour = int(parts[2])
                        current_sunset_minute = int(parts[3])
                        print(f"   🌅 Sunset today: {current_sunset_hour}:{current_sunset_minute:02d}")
                        
                        # Send weather to cloud
                        city_time_obj = get_city_time(city_offset)
                        city_time_str = city_time_obj.strftime("%H:%M:%S")
                        send_to_cloud(temperature=temp, city=city, condition=condition, city_time=city_time_str)
                
                last_weather_sent = now
            
            # Send AI decision (every 15 seconds) - based on SUNSET!
            if now - last_ai_sent >= 15:
                room_time = get_room_time()
                city_time = get_city_time(city_offset)
                ai = get_ai_decision(
                    current_light, 
                    room_time.strftime("%H:%M"), 
                    city_time.strftime("%H:%M"), 
                    city,
                    current_sunset_hour,
                    current_sunset_minute
                )
                ser.write(f"AI:{ai}\n".encode())
                print(f"🤖 AI Decision: {ai}")
                current_ai = ai
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