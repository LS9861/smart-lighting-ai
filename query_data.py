"""
query_data.py - View and analyze data from your lighting database
Run this separately whenever you want to see statistics
"""

import sqlite3
import pandas as pd
from datetime import datetime

def show_last_readings(conn, limit=10):
    """Show the most recent sensor readings"""
    print("\n" + "=" * 60)
    print(f"📊 LAST {limit} SENSOR READINGS")
    print("=" * 60)
    
    df = pd.read_sql_query(f"""
        SELECT id, timestamp, light_value, light_status, relay_state, mode, ai_decision 
        FROM sensor_readings 
        ORDER BY id DESC 
        LIMIT {limit}
    """, conn)
    
    print(df.to_string(index=False))
    return df

def show_statistics(conn):
    """Show statistics about your data"""
    print("\n" + "=" * 60)
    print("📈 LIGHT STATISTICS")
    print("=" * 60)
    
    cursor = conn.cursor()
    
    # Light statistics
    cursor.execute("""
        SELECT 
            COUNT(*) as total,
            AVG(light_value) as avg_light,
            MIN(light_value) as min_light,
            MAX(light_value) as max_light
        FROM sensor_readings
    """)
    result = cursor.fetchone()
    
    print(f"Total readings:    {result[0]}")
    print(f"Average light:     {result[1]:.1f} (0=dark, 1023=bright)")
    print(f"Minimum light:     {result[2]}")
    print(f"Maximum light:     {result[3]}")
    
    # AI decision counts
    cursor.execute("""
        SELECT ai_decision, COUNT(*) 
        FROM sensor_readings 
        WHERE ai_decision != '---'
        GROUP BY ai_decision
    """)
    
    print("\n🤖 AI DECISIONS")
    print("-" * 30)
    for row in cursor.fetchall():
        print(f"AI {row[0]}: {row[1]} times")
    
    # Mode distribution
    cursor.execute("""
        SELECT mode, COUNT(*) 
        FROM sensor_readings 
        GROUP BY mode
    """)
    
    print("\n🎮 MODE DISTRIBUTION")
    print("-" * 30)
    for row in cursor.fetchall():
        print(f"{row[0]}: {row[1]} times")

def show_weather_history(conn):
    """Show recent weather data"""
    print("\n" + "=" * 60)
    print("🌤️ RECENT WEATHER DATA")
    print("=" * 60)
    
    df = pd.read_sql_query("""
        SELECT timestamp, city, temperature, condition 
        FROM weather_data 
        ORDER BY id DESC 
        LIMIT 10
    """, conn)
    
    print(df.to_string(index=False))

def show_hourly_trend(conn):
    """Show light trend by hour of day"""
    print("\n" + "=" * 60)
    print("📈 LIGHT TREND BY HOUR")
    print("=" * 60)
    
    cursor = conn.cursor()
    cursor.execute("""
        SELECT 
            strftime('%H', timestamp) as hour,
            AVG(light_value) as avg_light
        FROM sensor_readings
        GROUP BY hour
        ORDER BY hour
    """)
    
    print("Hour  Average Light")
    print("-" * 25)
    for row in cursor.fetchall():
        print(f"{row[0]}:00     {row[1]:.0f}")

def export_summary_report(conn):
    """Export a summary report to CSV"""
    print("\n" + "=" * 60)
    print("📄 EXPORTING SUMMARY REPORT")
    print("=" * 60)
    
    # Export summary
    summary_df = pd.read_sql_query("""
        SELECT 
            date(timestamp) as date,
            COUNT(*) as readings,
            AVG(light_value) as avg_light,
            MIN(light_value) as min_light,
            MAX(light_value) as max_light,
            SUM(CASE WHEN ai_decision = 'ON' THEN 1 ELSE 0 END) as ai_on_count
        FROM sensor_readings
        GROUP BY date(timestamp)
        ORDER BY date DESC
    """, conn)
    
    summary_df.to_csv("daily_summary.csv", index=False)
    print("✅ Exported to daily_summary.csv")

def main():
    print("\n" + "=" * 60)
    print("📊 LIGHTING DATABASE ANALYZER")
    print("=" * 60)
    
    # Connect to database
    try:
        conn = sqlite3.connect('lighting_data.db')
        print("✅ Connected to lighting_data.db")
    except Exception as e:
        print(f"❌ Error: {e}")
        print("\nMake sure you've run the logger first to collect data!")
        return
    
    # Show all the insights
    show_statistics(conn)
    show_last_readings(conn, 5)
    show_weather_history(conn)
    show_hourly_trend(conn)
    
    # Ask if user wants to export
    print("\n" + "=" * 60)
    export_choice = input("Export summary report to CSV? (y/n): ").strip().lower()
    if export_choice == 'y':
        export_summary_report(conn)
    
    conn.close()
    print("\n✅ Done!")

if __name__ == "__main__":
    main()