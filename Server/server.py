from flask import Flask, request, jsonify
from flask_cors import CORS
from datetime import datetime
import sqlite3

app = Flask(__name__)
CORS(app)

# Simplified command state
current_command = "none"

def init_db():
    conn = sqlite3.connect('sensor_data.db')
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS sensor_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            temperature REAL,
            humidity REAL,
            light INTEGER,
            angle INTEGER,
            opening REAL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    conn.commit()
    conn.close()

@app.route('/', methods=['GET'])
def update_data():
    try:
        temperature = float(request.args.get('temp'))
        humidity = float(request.args.get('hum'))
        light = int(request.args.get('light'))
        angle = int(request.args.get('angle'))
        opening = float(request.args.get('opening'))

        conn = sqlite3.connect('sensor_data.db')
        c = conn.cursor()
        c.execute('''
            INSERT INTO sensor_readings (temperature, humidity, light, angle, opening)
            VALUES (?, ?, ?, ?, ?)
        ''', (temperature, humidity, light, angle, opening))
        conn.commit()
        conn.close()

        return jsonify({
            "status": "success",
            "message": f"Data received: temp={temperature}, humidity={humidity}",
            "command": current_command
        })
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500
    @app.route('/data', methods=['GET'])
def get_data():
    try:
        conn = sqlite3.connect('sensor_data.db')
        c = conn.cursor()
        c.execute('''
            SELECT temperature, humidity, light, timestamp
            FROM sensor_readings
            ORDER BY timestamp DESC
            LIMIT 50
        ''')
        readings = c.fetchall()
        conn.close()
        return jsonify([{
            'temperature': r[0],
            'humidity': r[1],
            'light': r[2],
            'timestamp': r[3]
        } for r in readings])
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/control', methods=['GET', 'POST'])
def control_blinds():
    global current_command
    
    if request.method == 'POST':
        try:
            command = request.json.get('command')
            if command not in ["open", "close", "none"]:
                return jsonify({"status": "error", "message": "Invalid command"}), 400
            
            # Simplified response
            current_command = command
            return jsonify({
                "command": command,
                "status": "success"
            })
            
        except Exception as e:
            return jsonify({"status": "error", "message": str(e)}), 500
    
    elif request.method == 'GET':
        return jsonify({
            "command": current_command
        })

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True)