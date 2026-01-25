# Water Tank Monitor System

This system monitors water tank depth using a pressure sensor connected to an Arduino UNO R4 WiFi and displays the data on a web dashboard.

## Quick Start

1. **Arduino**: Edit WiFi credentials in sketch, upload to UNO R4 WiFi
2. **Raspberry Pi**: Run `python3 tank_server.py` in the tank-monitor directory
3. **Browser**: Open http://rubberduck.local:8080 or http://192.168.55.192:8080
4. **Done!** Watch your tank level update in real-time

## Components

1. **water_tank_sensor_wifi.ino** - Arduino sketch for the UNO R4 WiFi
2. **index.html** - Web dashboard for viewing tank data
3. **tank_server.py** - Python server to run on Raspberry Pi (rubberduck.local)

## Tank Specifications

- **Diameter**: 100mm
- **Depth**: 200mm (maximum)
- **Volume**: ~1,570 ml (1.57 liters) when full
- **Surface Area**: 0.00785 m² (π × 0.05²)
- **Sensor**: Pressure sensor (0.5V - 4.5V = 0-10 kPa)

## Setup Instructions

### 1. Arduino UNO R4 WiFi Setup

**a. Install Required Library:**
- Open Arduino IDE
- Go to Sketch > Include Library > Manage Libraries
- Search for "WiFiS3" (should be pre-installed for UNO R4 WiFi)

**b. Configure WiFi Credentials:**
Edit these lines in `water_tank_sensor_wifi.ino`:
```cpp
const char* ssid = "YOUR_WIFI_SSID";        // Your WiFi network name
const char* password = "YOUR_WIFI_PASSWORD"; // Your WiFi password
```

**c. Hardware Connections:**
- Connect pressure sensor signal wire to **A0**
- Connect sensor VCC to **5V**
- Connect sensor GND to **GND**

**d. Upload Sketch:**
1. Connect Arduino to computer via USB
2. Select: Tools > Board > Arduino UNO R4 WiFi
3. Select correct COM port
4. Click Upload button

### 2. Raspberry Pi (rubberduck.local) Setup

**a. Copy Files to Raspberry Pi:**
```bash
# SSH into your Raspberry Pi
ssh pi@rubberduck.local

# Create project directory
mkdir ~/tank-monitor
cd ~/tank-monitor

# Copy index.html and tank_server.py to this directory
```

**b. Make Server Script Executable:**
```bash
chmod +x tank_server.py
```

**c. Run Server:**
```bash
python3 tank_server.py
```

Note: Port 8080 doesn't require sudo. If port 80 is needed, use `sudo python3 tank_server.py` but you'll need to stop any existing web server first.

**d. Optional: Run Server on Boot**
Create systemd service file:
```bash
sudo nano /etc/systemd/system/tank-monitor.service
```

Add this content:
```ini
[Unit]
Description=Water Tank Monitor Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/home/pi/tank-monitor
ExecStart=/usr/bin/python3 /home/pi/tank-monitor/tank_server.py
Restart=always

[Install]
WantedBy=multi-user.target
```

Enable and start service:
```bash
sudo systemctl enable tank-monitor.service
sudo systemctl start tank-monitor.service
```

Check status:
```bash
sudo systemctl status tank-monitor.service
```

### 3. Access the Dashboard

Once everything is running, access the web dashboard at:
- **http://rubberduck.local:8080** (if mDNS is working)
- **http://192.168.55.192:8080** (direct IP)

## How It Works

1. **Arduino reads pressure sensor** connected to pin A0
2. **Converts voltage to pressure** (0.5V-4.5V → 0-10 kPa)
3. **Calculates water depth** from pressure (1 kPa ≈ 0.102m water)
4. **Clamps depth** to tank maximum (200mm) to prevent overflow readings
5. **Calculates volume** using cylinder formula: V = π × r² × h
6. **Sends data to server** via HTTP GET request every 5 seconds
7. **Web dashboard fetches data** every 2 seconds and displays:
   - Visual tank representation with animated water
   - Water depth in millimeters
   - Pressure in kPa
   - Tank capacity in milliliters
   - Tank fill percentage
   - Online/offline status

## Troubleshooting

### Port 8080 already in use
If you get "Address already in use" error, check what's using port 8080:
```bash
sudo lsof -i :8080
```

Kill the process or change to a different port in both `tank_server.py` and the Arduino sketch.

### Arduino not connecting to WiFi
- Check WiFi credentials are correct
- Ensure WiFi network is 2.4GHz (UNO R4 WiFi doesn't support 5GHz)
- Check signal strength near Arduino location

### Server not receiving data
- Verify Raspberry Pi IP is 192.168.55.192
- Check firewall settings on Raspberry Pi
- Test server is running: `curl http://192.168.55.192/data`

### Dashboard shows "Offline"
- Check Arduino serial monitor for errors
- Verify server is running on Raspberry Pi
- Check network connectivity between devices

### Inaccurate readings
- Calibrate pressure sensor if needed
- Verify voltage reference (should be 5V)
- Check sensor is submerged correctly

## Serial Monitor Output

When Arduino is running, you should see output like:
```
=== Water Tank Sensor with WiFi ===
Tank diameter: 100 mm
Tank depth: 200 mm
Tank capacity: 1570.8 ml
Connecting to WiFi: YourNetwork
.....
WiFi connected!
IP address: 192.168.1.123

--- Measurement ---
Voltage: 2.345 V
Pressure: 4.61 kPa
Water Depth: 94.2 mm (0.094 m)
Tank Capacity: 739.3 ml (0.739 liters)
Tank Level: 47.1 %

Connecting to server: 192.168.55.192
Server response:
HTTP/1.0 200 OK
Data uploaded successfully!
```

## Customization

### Change Upload Interval
Edit this line in Arduino code:
```cpp
const unsigned long uploadInterval = 5000;  // milliseconds
```

### Change Tank Diameter and Depth
Edit these lines in Arduino code:
```cpp
const float TANK_DIAMETER_MM = 100.0f;  // your tank diameter in mm
const float TANK_DEPTH_MM = 200.0f;     // your tank maximum depth in mm
```

You'll also need to update the HTML page:
```javascript
// In index.html, update the max depth for visual scaling
const maxDepthM = 0.2;  // your tank depth in meters (200mm = 0.2m)
```

And update the subtitle:
```html
<p class="subtitle">100mm Ø × 200mm Deep Tank</p>  <!-- your dimensions -->
```

### Change Server Port
The default port is 8080. To use a different port, change in Arduino code:
```cpp
const int serverPort = 8080;  // your preferred port
```

And in Python server:
```python
PORT = 8080
```

**Note:** Ports below 1024 (like port 80) require sudo/root privileges.

## Data Format

Arduino sends data via HTTP GET with these parameters:
- `depth` - Water depth in meters (3 decimal places)
- `pressure` - Pressure in kPa (2 decimal places)
- `volume` - Volume in liters (3 decimal places)

Example: `/update?depth=0.094&pressure=4.61&volume=0.739`

The web dashboard converts these for display:
- Depth: meters → millimeters
- Volume: liters → milliliters

## License

Free to use and modify for personal projects.
