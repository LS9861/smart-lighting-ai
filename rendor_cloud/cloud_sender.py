"""
cloud_sender.py - Send local database data to cloud API
Run this periodically to sync your data to the cloud
"""

import sqlite3
import requests
import time
from datetime import datetime

CLOUD_API_URL = "https://YOUR-APP.onrender.com/submit"  # Change after deployment

def send_local_data_to_cloud():
    """Read local SQLite database and send to cloud"""
    
    # Connect to local database
    conn = sqlite3.connect('lighting_data.db')
    cursor = conn.cursor()
    
    # Get last 100 readings not sent
    cursor.execute("""
        SELECT light_value, light_status, relay_state, mode, ai_decision 
        FROM sensor_readings 
        ORDER BY id DESC 
        LIMIT 100
    """)
    
    readings = cursor.fetchall()
    conn.close()
    
    for reading in readings:
        data = {
            'light_value': reading[0],
            'light_status': reading[1],
            'relay_state': reading[2],
            'mode': reading[3],
            'ai_decision': reading[4]
        }
        
        try:
            response = requests.post(CLOUD_API_URL, json=data, timeout=5)
            if response.status_code == 200:
                print(f"✅ Sent: Light={reading[0]}")
            else:
                print(f"❌ Failed: {response.status_code}")
        except Exception as e:
            print(f"❌ Error: {e}")
        
        time.sleep(0.5)  # Small delay to avoid rate limiting

if __name__ == "__main__":
    send_local_data_to_cloud()