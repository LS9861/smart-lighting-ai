"""
Database Logger for Smart Lighting System
Stores all sensor readings, AI decisions, and weather data
"""

import sqlite3
import time
from datetime import datetime
import serial
import threading

class LightingDatabase:
    def __init__(self, db_path="lighting_data.db"):
        self.db_path = db_path
        self.init_database()
    
    def init_database(self):
        """Create all necessary tables"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # Sensor readings table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sensor_readings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                light_value INTEGER,
                light_status TEXT,
                relay_state TEXT,
                mode TEXT,
                ai_decision TEXT
            )
        ''')
        
        # Weather data table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS weather_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                city TEXT,
                temperature REAL,
                condition TEXT
            )
        ''')
        
        # AI decisions table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS ai_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                decision TEXT,
                light_value INTEGER,
                room_time TEXT,
                city_time TEXT
            )
        ''')
        
        conn.commit()
        conn.close()
        print("✅ Database initialized")
    
    def log_sensor(self, light_value, light_status, relay_state, mode, ai_decision):
        """Log a sensor reading"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO sensor_readings (timestamp, light_value, light_status, relay_state, mode, ai_decision)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (datetime.now().isoformat(), light_value, light_status, relay_state, mode, ai_decision))
        
        conn.commit()
        conn.close()
    
    def log_weather(self, city, temperature, condition):
        """Log weather data"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO weather_data (timestamp, city, temperature, condition)
            VALUES (?, ?, ?, ?)
        ''', (datetime.now().isoformat(), city, temperature, condition))
        
        conn.commit()
        conn.close()
    
    def log_ai(self, decision, light_value, room_time, city_time):
        """Log AI decision"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO ai_log (timestamp, decision, light_value, room_time, city_time)
            VALUES (?, ?, ?, ?, ?)
        ''', (datetime.now().isoformat(), decision, light_value, room_time, city_time))
        
        conn.commit()
        conn.close()
    
    def get_stats(self):
        """Get database statistics"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute("SELECT COUNT(*) FROM sensor_readings")
        total_sensors = cursor.fetchone()[0]
        
        cursor.execute("SELECT COUNT(*) FROM weather_data")
        total_weather = cursor.fetchone()[0]
        
        cursor.execute("SELECT COUNT(*) FROM ai_log")
        total_ai = cursor.fetchone()[0]
        
        conn.close()
        
        return {
            "total_readings": total_sensors,
            "total_weather": total_weather,
            "total_ai_decisions": total_ai
        }
    
    def export_to_csv(self):
        """Export all data to CSV files"""
        import csv
        conn = sqlite3.connect(self.db_path)
        
        # Export sensor readings
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM sensor_readings")
        with open("export_sensors.csv", "w", newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["id", "timestamp", "light_value", "light_status", "relay_state", "mode", "ai_decision"])
            writer.writerows(cursor.fetchall())
        
        # Export weather data
        cursor.execute("SELECT * FROM weather_data")
        with open("export_weather.csv", "w", newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["id", "timestamp", "city", "temperature", "condition"])
            writer.writerows(cursor.fetchall())
        
        conn.close()
        print("✅ Data exported to CSV files")

# ============================================
# INTEGRATE WITH YOUR SIMULATOR
# ============================================

def run_logger(arduino_port="COM3", baud_rate=9600):
    """Run the database logger alongside the simulator"""
    
    db = LightingDatabase()
    
    print("\n" + "=" * 60)
    print("📊 DATABASE LOGGER - Recording all data")
    print("=" * 60)
    
    try:
        ser = serial.Serial(arduino_port, baud_rate, timeout=1)
        time.sleep(2)
        print(f"✅ Connected to Arduino on {arduino_port}\n")
    except Exception as e:
        print(f"❌ Cannot connect: {e}")
        return
    
    print("📝 Logging data to database...")
    print("   Press Ctrl+C to stop and export data\n")
    
    try:
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # Parse sensor readings
                if "Light:" in line:
                    parts = line.split()
                    light_value = 0
                    light_status = "UNK"
                    relay_state = "OFF"
                    mode = "AUTO"
                    ai_decision = "---"
                    
                    for i, p in enumerate(parts):
                        if p == "Light:" and i+1 < len(parts):
                            light_value = int(parts[i+1])
                        elif p == "[" and i+1 < len(parts):
                            light_status = parts[i+1].replace("]", "")
                        elif p == "Relay:" and i+1 < len(parts):
                            relay_state = parts[i+1]
                        elif p == "Mode:" and i+1 < len(parts):
                            mode = parts[i+1]
                        elif p == "AI:" and i+1 < len(parts):
                            ai_decision = parts[i+1]
                    
                    db.log_sensor(light_value, light_status, relay_state, mode, ai_decision)
                    print(f"📊 Logged: Light={light_value} [{light_status}] Relay={relay_state} AI={ai_decision}")
                
                # Parse weather data
                elif "WEATHER:" in line:
                    parts = line.split("|")
                    if len(parts) >= 2:
                        temp = parts[0].replace("WEATHER:", "")
                        condition = parts[1]
                        db.log_weather("Current", float(temp), condition)
                        print(f"🌤️ Logged weather: {temp}°C, {condition}")
                
                # Parse AI decisions
                elif "AI:" in line and "AI decision" not in line:
                    decision = line.replace("AI:", "").strip()
                    db.log_ai(decision, 0, "", "")
                    print(f"🤖 Logged AI: {decision}")
            
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n\n📊 Stopping logger...")
        db.export_to_csv()
        stats = db.get_stats()
        print("\n📈 Database Statistics:")
        print(f"   Total sensor readings: {stats['total_readings']}")
        print(f"   Total weather records: {stats['total_weather']}")
        print(f"   Total AI decisions: {stats['total_ai_decisions']}")
        ser.close()
        print("✅ Done!")

if __name__ == "__main__":
    run_logger()