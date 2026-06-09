"""
Cloud API for Smart Lighting System
Deploy to Render.com
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime
import os

app = Flask(__name__)
CORS(app)

# Database configuration
database_url = os.environ.get('DATABASE_URL', 'sqlite:///lighting.db')
app.config['SQLALCHEMY_DATABASE_URI'] = database_url
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

# ============================================
# DATABASE MODELS
# ============================================

class SensorReading(db.Model):
    __tablename__ = 'sensor_readings'
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.String(50), nullable=False)
    light_value = db.Column(db.Integer)
    light_status = db.Column(db.String(10))
    relay_state = db.Column(db.String(5))
    mode = db.Column(db.String(10))
    ai_decision = db.Column(db.String(5))
    
    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp,
            'light_value': self.light_value,
            'light_status': self.light_status,
            'relay_state': self.relay_state,
            'mode': self.mode,
            'ai_decision': self.ai_decision
        }

class WeatherData(db.Model):
    __tablename__ = 'weather_data'
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.String(50), nullable=False)
    city = db.Column(db.String(50))
    temperature = db.Column(db.Float)
    condition = db.Column(db.String(100))
    
    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp,
            'city': self.city,
            'temperature': self.temperature,
            'condition': self.condition
        }

# ============================================
# API ENDPOINTS
# ============================================

@app.route('/', methods=['GET'])
def home():
    return jsonify({
        'name': 'Smart Lighting Cloud API',
        'version': '1.0',
        'endpoints': {
            '/readings': 'GET - Get all sensor readings',
            '/readings/latest': 'GET - Get latest reading',
            '/weather': 'GET - Get weather data',
            '/stats': 'GET - Get statistics',
            '/submit': 'POST - Submit sensor data',
            '/dashboard': 'GET - View web dashboard'
        }
    })

@app.route('/submit', methods=['POST'])
def submit_data():
    """Receive data from your local logger"""
    data = request.json
    
    if 'light_value' in data:
        reading = SensorReading(
            timestamp=datetime.now().isoformat(),
            light_value=data.get('light_value'),
            light_status=data.get('light_status', 'UNK'),
            relay_state=data.get('relay_state', 'OFF'),
            mode=data.get('mode', 'AUTO'),
            ai_decision=data.get('ai_decision', '---')
        )
        db.session.add(reading)
        db.session.commit()
    
    if 'temperature' in data:
        weather = WeatherData(
            timestamp=datetime.now().isoformat(),
            city=data.get('city', 'Unknown'),
            temperature=data.get('temperature'),
            condition=data.get('condition', '')
        )
        db.session.add(weather)
        db.session.commit()
    
    return jsonify({'status': 'success', 'message': 'Data saved'})

@app.route('/readings', methods=['GET'])
def get_readings():
    """Get all sensor readings"""
    limit = request.args.get('limit', 100, type=int)
    readings = SensorReading.query.order_by(SensorReading.id.desc()).limit(limit).all()
    return jsonify([r.to_dict() for r in readings])

@app.route('/readings/latest', methods=['GET'])
def get_latest():
    """Get the latest sensor reading"""
    latest = SensorReading.query.order_by(SensorReading.id.desc()).first()
    if latest:
        return jsonify(latest.to_dict())
    return jsonify({'error': 'No data yet'})

@app.route('/weather', methods=['GET'])
def get_weather():
    """Get weather data"""
    limit = request.args.get('limit', 50, type=int)
    weather = WeatherData.query.order_by(WeatherData.id.desc()).limit(limit).all()
    return jsonify([w.to_dict() for w in weather])

@app.route('/stats', methods=['GET'])
def get_stats():
    """Get statistics"""
    total = SensorReading.query.count()
    if total == 0:
        return jsonify({'total': 0})
    
    from sqlalchemy import func
    avg_light = db.session.query(func.avg(SensorReading.light_value)).scalar()
    min_light = db.session.query(func.min(SensorReading.light_value)).scalar()
    max_light = db.session.query(func.max(SensorReading.light_value)).scalar()
    
    ai_on = SensorReading.query.filter_by(ai_decision='ON').count()
    ai_off = SensorReading.query.filter_by(ai_decision='OFF').count()
    
    return jsonify({
        'total_readings': total,
        'avg_light': round(avg_light, 1) if avg_light else 0,
        'min_light': min_light or 0,
        'max_light': max_light or 0,
        'ai_on': ai_on,
        'ai_off': ai_off
    })

@app.route('/dashboard', methods=['GET'])
def dashboard():
    """Serve the HTML dashboard from external file"""
    try:
        with open('dashboard.html', 'r', encoding='utf-8') as f:
            return f.read()
    except FileNotFoundError:
        return """
        <html>
        <body>
            <h1>Dashboard file not found</h1>
            <p>Please ensure dashboard.html is uploaded to the server.</p>
            <p><a href="/">Back to API</a></p>
        </body>
        </html>
        """, 404

# ============================================
# CREATE DATABASE TABLES (MUST run on startup)
# ============================================

with app.app_context():
    db.create_all()
    print("✅ Database tables created/verified")

# ============================================
# RUN
# ============================================

if __name__ == '__main__':
    port = int(os.environ.get('PORT', 5000))
    app.run(host='0.0.0.0', port=port)