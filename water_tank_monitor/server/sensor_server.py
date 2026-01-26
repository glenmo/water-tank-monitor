#!/usr/bin/env python3
"""
HTTP server with web dashboard for Arduino water tank monitor
Run on your server: python3 sensor_server.py
"""

from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs
import json
from datetime import datetime
from collections import deque
import os

PORT = 8080
LOG_FILE = "/tmp/water-tank-sensor.log"
MAX_READINGS = 100  # Keep last 100 readings in memory

# Store recent readings in memory
recent_readings = deque(maxlen=MAX_READINGS)

class SensorHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        parsed_path = urlparse(self.path)

        # Serve dashboard at root
        if parsed_path.path == '/':
            self.serve_dashboard()

        # API endpoint for Arduino sensor data
        elif parsed_path.path == '/api/sensor-data':
            self.handle_sensor_data(parsed_path.query)

        # API endpoint to get recent readings (for dashboard)
        elif parsed_path.path == '/api/readings':
            self.serve_readings()

        # API endpoint to get latest reading
        elif parsed_path.path == '/api/latest':
            self.serve_latest()

        else:
            self.send_error(404, "Endpoint not found")

    def serve_dashboard(self):
        """Serve the HTML dashboard"""
        html = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Water Tank Monitor Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        h1 {
            color: white;
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .stat-card {
            background: white;
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            transition: transform 0.3s ease;
        }
        .stat-card:hover {
            transform: translateY(-5px);
        }
        .stat-label {
            color: #666;
            font-size: 0.9em;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .stat-value {
            font-size: 2.5em;
            font-weight: bold;
            color: #667eea;
        }
        .stat-unit {
            font-size: 0.6em;
            color: #999;
            font-weight: normal;
        }
        .chart-container {
            background: white;
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            margin-bottom: 20px;
        }
        .chart-wrapper {
            position: relative;
            height: 400px;
        }
        .status {
            background: white;
            border-radius: 15px;
            padding: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .status-indicator {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #4ade80;
            animation: pulse 2s infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .last-update {
            color: #666;
            font-size: 0.9em;
        }
        .loading {
            text-align: center;
            color: white;
            font-size: 1.2em;
            margin-top: 50px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>💧 Water Tank Monitor</h1>

        <div class="stats-grid">
            <div class="stat-card">
                <div class="stat-label">Water Volume</div>
                <div class="stat-value" id="volume">--<span class="stat-unit">L</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Water Depth</div>
                <div class="stat-value" id="depth">--<span class="stat-unit">m</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Pressure</div>
                <div class="stat-value" id="pressure">--<span class="stat-unit">kPa</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Sensor Voltage</div>
                <div class="stat-value" id="voltage">--<span class="stat-unit">V</span></div>
            </div>
        </div>

        <div class="chart-container">
            <h2 style="margin-bottom: 20px; color: #333;">Water Volume Over Time</h2>
            <div class="chart-wrapper">
                <canvas id="volumeChart"></canvas>
            </div>
        </div>

        <div class="chart-container">
            <h2 style="margin-bottom: 20px; color: #333;">Pressure & Depth</h2>
            <div class="chart-wrapper">
                <canvas id="pressureChart"></canvas>
            </div>
        </div>

        <div class="status">
            <div class="status-indicator">
                <div class="status-dot"></div>
                <span>Live Monitoring Active</span>
            </div>
            <div class="last-update" id="lastUpdate">Last update: --</div>
        </div>
    </div>

    <script>
        // Chart.js configuration
        const commonOptions = {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: true,
                    position: 'top'
                }
            },
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time'
                    }
                },
                y: {
                    display: true,
                    beginAtZero: false
                }
            }
        };

        // Initialize volume chart
        const volumeCtx = document.getElementById('volumeChart').getContext('2d');
        const volumeChart = new Chart(volumeCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Water Volume (L)',
                    data: [],
                    borderColor: '#667eea',
                    backgroundColor: 'rgba(102, 126, 234, 0.1)',
                    borderWidth: 2,
                    tension: 0.4,
                    fill: true
                }]
            },
            options: {
                ...commonOptions,
                scales: {
                    ...commonOptions.scales,
                    y: {
                        ...commonOptions.scales.y,
                        title: {
                            display: true,
                            text: 'Volume (Liters)'
                        }
                    }
                }
            }
        });

        // Initialize pressure chart
        const pressureCtx = document.getElementById('pressureChart').getContext('2d');
        const pressureChart = new Chart(pressureCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Pressure (kPa)',
                        data: [],
                        borderColor: '#f59e0b',
                        backgroundColor: 'rgba(245, 158, 11, 0.1)',
                        borderWidth: 2,
                        tension: 0.4,
                        yAxisID: 'y'
                    },
                    {
                        label: 'Depth (m)',
                        data: [],
                        borderColor: '#10b981',
                        backgroundColor: 'rgba(16, 185, 129, 0.1)',
                        borderWidth: 2,
                        tension: 0.4,
                        yAxisID: 'y1'
                    }
                ]
            },
            options: {
                ...commonOptions,
                scales: {
                    x: commonOptions.scales.x,
                    y: {
                        type: 'linear',
                        display: true,
                        position: 'left',
                        title: {
                            display: true,
                            text: 'Pressure (kPa)'
                        }
                    },
                    y1: {
                        type: 'linear',
                        display: true,
                        position: 'right',
                        title: {
                            display: true,
                            text: 'Depth (m)'
                        },
                        grid: {
                            drawOnChartArea: false
                        }
                    }
                }
            }
        });

        function formatTime(timestamp) {
            const date = new Date(timestamp);
            return date.toLocaleTimeString();
        }

        function updateDashboard(data) {
            if (!data || data.length === 0) return;

            // Update stat cards with latest reading
            const latest = data[data.length - 1];
            document.getElementById('volume').innerHTML =
                `${latest.volume_liters.toFixed(2)}<span class="stat-unit">L</span>`;
            document.getElementById('depth').innerHTML =
                `${latest.water_depth_m.toFixed(3)}<span class="stat-unit">m</span>`;
            document.getElementById('pressure').innerHTML =
                `${latest.pressure_kpa.toFixed(3)}<span class="stat-unit">kPa</span>`;
            document.getElementById('voltage').innerHTML =
                `${latest.voltage.toFixed(3)}<span class="stat-unit">V</span>`;
            document.getElementById('lastUpdate').textContent =
                `Last update: ${formatTime(latest.timestamp)}`;

            // Prepare chart data (show last 50 points)
            const recentData = data.slice(-50);
            const labels = recentData.map(r => formatTime(r.timestamp));

            // Update volume chart
            volumeChart.data.labels = labels;
            volumeChart.data.datasets[0].data = recentData.map(r => r.volume_liters);
            volumeChart.update('none');

            // Update pressure chart
            pressureChart.data.labels = labels;
            pressureChart.data.datasets[0].data = recentData.map(r => r.pressure_kpa);
            pressureChart.data.datasets[1].data = recentData.map(r => r.water_depth_m);
            pressureChart.update('none');
        }

        async function fetchData() {
            try {
                const response = await fetch('/api/readings');
                const data = await response.json();
                updateDashboard(data);
            } catch (error) {
                console.error('Error fetching data:', error);
            }
        }

        // Initial load
        fetchData();

        // Update every 5 seconds
        setInterval(fetchData, 5000);
    </script>
</body>
</html>"""

        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(html.encode())

    def handle_sensor_data(self, query):
        """Handle sensor data from Arduino"""
        params = parse_qs(query)

        try:
            # Extract sensor data
            data = {
                'voltage': float(params.get('voltage', [0])[0]),
                'pressure_kpa': float(params.get('pressure_kpa', [0])[0]),
                'water_depth_m': float(params.get('water_depth_m', [0])[0]),
                'volume_liters': float(params.get('volume_liters', [0])[0]),
                'timestamp': datetime.now().isoformat()
            }

            # Store in memory
            recent_readings.append(data)

            # Log to file
            self.log_sensor_data(data)

            # Send success response
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()

            response = {
                'status': 'success',
                'message': 'Sensor data received',
                'data': data
            }
            self.wfile.write(json.dumps(response).encode())

            # Print to console
            print(f"[{data['timestamp']}] Received: "
                  f"V={data['voltage']:.3f}V, "
                  f"P={data['pressure_kpa']:.3f}kPa, "
                  f"D={data['water_depth_m']:.3f}m, "
                  f"Vol={data['volume_liters']:.2f}L")

        except (ValueError, IndexError) as e:
            self.send_error(400, f"Invalid parameters: {e}")

    def serve_readings(self):
        """Return all recent readings as JSON"""
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(list(recent_readings)).encode())

    def serve_latest(self):
        """Return the latest reading as JSON"""
        if recent_readings:
            latest = recent_readings[-1]
        else:
            latest = {}

        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(latest).encode())

    def log_sensor_data(self, data):
        """Log sensor data to file"""
        try:
            with open(LOG_FILE, 'a') as f:
                f.write(json.dumps(data) + '\n')
        except Exception as e:
            print(f"Warning: Could not write to log file: {e}")

    def log_message(self, format, *args):
        """Override to customize logging"""
        # Only log errors, not every request
        if '404' in str(args) or '400' in str(args):
            super().log_message(format, *args)

def run_server():
    """Start the HTTP server"""
    server_address = ('', PORT)
    httpd = HTTPServer(server_address, SensorHandler)

    print(f"=== Water Tank Sensor Server ===")
    print(f"Starting server on port {PORT}...")
    print(f"Dashboard: http://localhost:{PORT}/")
    print(f"Logging data to: {LOG_FILE}")
    print(f"Press Ctrl+C to stop\n")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\nShutting down server...")
        httpd.shutdown()
        print("Server stopped.")

if __name__ == '__main__':
    run_server()
