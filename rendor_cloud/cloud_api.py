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
            '/submit': 'POST - Submit sensor data'
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
    """HTML Dashboard"""
    return '''
    <!DOCTYPE html>
    <html>
    <head>
        <title>Smart Lighting Dashboard</title>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
        <style>
            body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; background: #f0f2f5; }
            .container { max-width: 1200px; margin: 0 auto; }
            .card { background: white; border-radius: 10px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
            .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-bottom: 20px; }
            .stat { background: #e9ecef; padding: 15px; border-radius: 8px; text-align: center; }
            .stat-number { font-size: 28px; font-weight: bold; }
            canvas { max-height: 400px; }
            button { background: #007bff; color: white; border: none; padding: 8px 16px; border-radius: 5px; cursor: pointer; margin: 5px; }
            button:hover { background: #0056b3; }
            pre { background: #f8f9fa; padding: 15px; overflow-x: auto; font-size: 12px; }
        </style>
    </head>
    <body>
        <div class="container">
            <div class="card">
                <h1>📡 Smart Lighting Cloud Dashboard</h1>
                <p>Real-time monitoring from anywhere</p>
                <button onclick="refreshData()">🔄 Refresh</button>
                <button onclick="location.reload()">📊 Reload Charts</button>
            </div>
            
            <div class="stats-grid" id="stats">
                <div class="stat"><div class="stat-number">--</div><div>Total Readings</div></div>
                <div class="stat"><div class="stat-number">--</div><div>Avg Light</div></div>
                <div class="stat"><div class="stat-number">--</div><div>AI ON</div></div>
                <div class="stat"><div class="stat-number">--</div><div>AI OFF</div></div>
            </div>
            
            <div class="card">
                <h2>📈 Light Level History</h2>
                <canvas id="lightChart"></canvas>
            </div>
            
            <div class="card">
                <h2>📊 Recent Readings</h2>
                <div id="readings" style="max-height: 300px; overflow-y: auto;">
                    <pre>Loading...</pre>
                </div>
            </div>
        </div>
        
        <script>
            let lightChart;
            
            async function getStats() {
                const response = await fetch('/stats');
                const data = await response.json();
                document.getElementById('stats').innerHTML = `
                    <div class="stat"><div class="stat-number">${data.total_readings || 0}</div><div>Total Readings</div></div>
                    <div class="stat"><div class="stat-number">${data.avg_light || 0}</div><div>Avg Light</div></div>
                    <div class="stat"><div class="stat-number" style="color:#28a745">${data.ai_on || 0}</div><div>AI ON</div></div>
                    <div class="stat"><div class="stat-number" style="color:#dc3545">${data.ai_off || 0}</div><div>AI OFF</div></div>
                `;
            }
            
            async function getReadings() {
                const response = await fetch('/readings?limit=50');
                const data = await response.json();
                document.getElementById('readings').innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                return data;
            }
            
            async function updateChart() {
                const response = await fetch('/readings?limit=50');
                const data = await response.json();
                
                if (data.length === 0) return;
                
                const labels = data.map(r => new Date(r.timestamp).toLocaleTimeString());
                const values = data.map(r => r.light_value);
                
                if (lightChart) lightChart.destroy();
                
                const ctx = document.getElementById('lightChart').getContext('2d');
                lightChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: labels.reverse(),
                        datasets: [{
                            label: 'Light Level (0-1023)',
                            data: values.reverse(),
                            borderColor: '#007bff',
                            backgroundColor: 'rgba(0,123,255,0.1)',
                            fill: true,
                            tension: 0.3
                        }]
                    },
                    options: {
                        responsive: true,
                        plugins: { legend: { position: 'top' } },
                        scales: { y: { title: { display: true, text: 'Light Value' }, min: 0, max: 1023 } }
                    }
                });
            }
            
            async function refreshData() {
                await getStats();
                await updateChart();
                await getReadings();
            }
            
            refreshData();
            setInterval(refreshData, 30000);
        </script>
    </body>
    </html>
    '''

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