"""
Test DeepSeek API via OpenRouter (free)
Run this before hardware arrives to verify API key works.
"""

import requests
import json

API_KEY = "sk-or-v1-xxxxxxxxxxxxx"  # ← CHANGE THIS
API_URL = "https://openrouter.ai/api/v1/chat/completions"

def test_lighting_decision(light_value, city_temp=22, weather="clear"):
    """Test AI lighting decision with context"""
    
    light_percent = (light_value / 1023.0) * 100
    
    prompt = f"""
    Light sensor: {light_value} ({light_percent:.1f}%).
    City weather: {city_temp}°C, {weather}.
    Should I turn the light ON or OFF? Reply ONLY 'ON' or 'OFF'.
    """
    
    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    }
    
    data = {
        "model": "deepseek/deepseek-chat:free",
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": 10,
        "temperature": 0.1
    }
    
    response = requests.post(API_URL, headers=headers, json=data)
    
    if response.status_code == 200:
        result = response.json()
        reply = result['choices'][0]['message']['content']
        print(f"Light: {light_value} ({light_percent:.0f}%) → AI says: {reply}")
        return reply
    else:
        print(f"Error: {response.status_code}")
        return None

def run_tests():
    print("=" * 50)
    print("DeepSeek AI Lighting Decision Tests")
    print("=" * 50)
    
    # Test different light levels
    test_values = [100, 300, 500, 700, 900]
    
    for value in test_values:
        test_lighting_decision(value)
    
    print("=" * 50)

if __name__ == "__main__":
    run_tests()