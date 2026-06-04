"""
ESP32 Simulator with REAL DeepSeek AI and REAL Weather Data
Asks for city name at startup - displays on Arduino OLED
"""

import serial
import time
import sys
import os
import requests
from datetime import datetime
from dotenv import load_dotenv

# Load configuration
load_dotenv()

# ============================================
# CONFIGURATION
# ============================================
ARDUINO_PORT = os.getenv('ARDUINO_PORT', 'COM3')
BAUD_RATE = 9600

# DeepSeek API Key
DEEPSEEK_API_KEY = os.getenv('DEEPSEEK_API_KEY', '')

# OpenWeatherMap API Key (FREE - sign up at openweathermap.org)
OPENWEATHER_API_KEY = os.getenv('OPENWEATHER_API_KEY', '')

# Timing intervals
AI_INTERVAL = 10
TIME_INTERVAL = 30
WEATHER_INTERVAL = 300  # 5 minutes

# ============================================
# ASK USER FOR CITY NAME
# ============================================

def get_city_from_user():
    """Ask user for city name at startup"""
    print("\n" + "=" * 60)
    print("🌍 CITY SELECTION")
    print("=" * 60)
    print("Enter the city name for weather data")
    print("Examples: Stuttgart, London, New York, Tokyo, Paris")
    print("=" * 60)
    
    city = input("\n📍 Enter city name: ").strip()
    
    if not city:
        city = "Stuttgart"
        print(f"   Using default city: {city}")
    
    print(f"✅ Weather for: {city}\n")
    return city

# ============================================
# REAL WEATHER FUNCTION
# ============================================

def get_real_weather(city_name):
    """
    Get REAL weather for specified city from OpenWeatherMap
    """
    if not OPENWEATHER_API_KEY:
        print("⚠️ No OpenWeather API key! Using mock data.")
        return get_mock_weather()
    
    try:
        # Using city name
        url = f"http://api.openweathermap.org/data/2.5/weather?q={city_name}&appid={OPENWEATHER_API_KEY}&units=metric"
        
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            temp = round(data['main']['temp'])
            condition = data['weather'][0]['description']
            # Capitalize first letter of condition
            condition = condition[0].upper() + condition[1:] if condition else condition
            return f"{temp}|{condition}"
        else:
            print(f"⚠️ Weather API error: {response.status_code}")
            print(f"   City '{city_name}' may not be found. Using mock data.")
            return get_mock_weather()
            
    except Exception as e:
        print(f"⚠️ Weather fetch error: {e}")
        return get_mock_weather()

def get_mock_weather():
    """Fallback mock weather if real API fails"""
    conditions = ["Clear sky", "Sunny", "Cloudy", "Light rain", "Snow"]
    temp = 10 + (hash(str(time.time())) % 25)
    condition = conditions[int(time.time() / 60) % len(conditions)]
    return f"{temp}|{condition}"

# ============================================
# DEEPSEEK AI FUNCTION
# ============================================

def get_ai_decision(light_value=None, time_str=None, weather=None, city=None):
    """Call REAL DeepSeek API for intelligent decision"""
    
    if not DEEPSEEK_API_KEY:
        print("⚠️ No DeepSeek API key!")
        current_hour = datetime.now().hour
        return "ON" if (current_hour >= 18 or current_hour <= 6) else "OFF"
    
    if light_value is None:
        light_value = 500
    
    prompt = f"""You are controlling a smart light system for a room in {city or "Stuttgart"}.
Current time: {time_str or datetime.now().strftime('%H:%M')}
Light sensor reading: {light_value} (0=dark, 1023=bright)
Current weather: {weather or "unknown"}

Based on this information, should the light be turned ON or OFF?
Reply with ONLY the word 'ON' or 'OFF'. No explanation."""

    headers = {
        "Authorization": f"Bearer {DEEPSEEK_API_KEY}",
        "Content-Type": "application/json"
    }
    
    data = {
        "model": "deepseek-chat",
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": 10,
        "temperature": 0.1
    }
    
    try:
        print("🧠 Calling DeepSeek AI...")
        response = requests.post(
            "https://api.deepseek.com/v1/chat/completions",
            headers=headers,
            json=data,
            timeout=10
        )
        
        if response.status_code == 200:
            result = response.json()
            decision = result['choices'][0]['message']['content'].strip().upper()
            if decision in ["ON", "OFF"]:
                print(f"🤖 DeepSeek AI decision: {decision}")
                return decision
    except Exception as e:
        print(f"❌ DeepSeek API error: {e}")
    
    current_hour = datetime.now().hour
    fallback = "ON" if (current_hour >= 18 or current_hour <= 6) else "OFF"
    return fallback

# ============================================
# READ ARDUINO DATA
# ============================================

def read_arduino_data(ser):
    """Read and parse Arduino sensor data"""
    if ser.in_waiting:
        line = ser.readline().decode('utf-8').strip()
        if "Light:" in line:
            parts = line.split()
            for i, part in enumerate(parts):
                if part == "Light:" and i+1 < len(parts):
                    try:
                        return int(parts[i+1])
                    except:
                        pass
    return None

# ============================================
# MAIN SIMULATION
# ============================================

def main():
    # Ask user for city first
    city_name = get_city_from_user()
    
    print("=" * 60)
    print("ESP32 SIMULATOR - REAL DeepSeek AI + REAL Weather")
    print("=" * 60)
    print(f"Arduino Port: {ARDUINO_PORT}")
    print(f"Selected City: {city_name}")
    print(f"DeepSeek AI: {'✅ Enabled' if DEEPSEEK_API_KEY else '❌ DISABLED'}")
    print(f"Weather: {'✅ REAL' if OPENWEATHER_API_KEY else '❌ Mock data'}")
    print("=" * 60)
    
    # Connect to Arduino
    try:
        ser = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        print(f"\n✅ Connected to Arduino on {ARDUINO_PORT}\n")
    except Exception as e:
        print(f"\n❌ Cannot connect to Arduino: {e}")
        return
    
    # Send initial city name to Arduino
    ser.write((f"CITY:{city_name}\n").encode())
    print(f"📍 Sent city: {city_name}\n")
    
    last_time_sent = 0
    last_weather_sent = 0
    last_ai_sent = 0
    last_light_read = 0
    current_light = 500
    current_weather = "unknown"
    
    print("📡 Sending REAL data to Arduino...")
    print("   - City: user selected")
    print("   - Time: from system clock")
    print("   - Weather: REAL data")
    print("   - AI: REAL DeepSeek decisions")
    print("Press Ctrl+C to stop\n")
    
    try:
        while True:
            current_time_val = time.time()
            
            # Read light level from Arduino
            if current_time_val - last_light_read >= 5:
                light_val = read_arduino_data(ser)
                if light_val is not None:
                    current_light = light_val
                    print(f"📊 Light level: {current_light}")
                last_light_read = current_time_val
            
            # Send current time
            if current_time_val - last_time_sent >= TIME_INTERVAL:
                time_str = datetime.now().strftime("%H:%M")
                ser.write((f"TIME:{time_str}\n").encode())
                print(f"🕐 Sent: TIME:{time_str}")
                last_time_sent = current_time_val
            
            # Send REAL weather
            if current_time_val - last_weather_sent >= WEATHER_INTERVAL:
                weather_data = get_real_weather(city_name)
                current_weather = weather_data
                # Format: WEATHER:temp|condition
                ser.write((f"WEATHER:{weather_data}\n").encode())
                print(f"🌤️ Sent: WEATHER:{weather_data}")
                last_weather_sent = current_time_val
            
            # Send REAL AI decision
            if current_time_val - last_ai_sent >= AI_INTERVAL:
                now = datetime.now()
                weather_condition = current_weather.split('|')[1] if '|' in current_weather else "unknown"
                ai_decision = get_ai_decision(
                    light_value=current_light,
                    time_str=now.strftime("%H:%M"),
                    weather=weather_condition,
                    city=city_name
                )
                ser.write((f"AI:{ai_decision}\n").encode())
                print(f"🤖 Sent: AI:{ai_decision}")
                last_ai_sent = current_time_val
            
            time.sleep(0.5)
            
    except KeyboardInterrupt:
        print("\n\n👋 Stopping ESP32 simulator")
        ser.close()
        print("✅ Disconnected")

if __name__ == "__main__":
    try:
        import serial
    except ImportError:
        print("❌ Install: pip install pyserial python-dotenv requests")
        sys.exit(1)
    
    if not DEEPSEEK_API_KEY:
        print("\n⚠️ WARNING: DeepSeek API key not found in .env file!")
    
    main()