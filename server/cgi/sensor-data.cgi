#!/usr/bin/env python3
"""
CGI script to receive sensor data from Arduino water tank monitor
Place this in your Apache2 cgi-bin directory
"""

import sys
import json
import os
from datetime import datetime

# Set content type header
print("Content-Type: application/json")
print()

def log_data(data):
    """Log sensor data to file"""
    log_file = "/var/log/water-tank-sensor.log"
    timestamp = datetime.now().isoformat()

    try:
        with open(log_file, 'a') as f:
            log_entry = {
                "timestamp": timestamp,
                "data": data
            }
            f.write(json.dumps(log_entry) + "\n")
        return True
    except Exception as e:
        # Fallback to /tmp if /var/log is not writable
        try:
            log_file = "/tmp/water-tank-sensor.log"
            with open(log_file, 'a') as f:
                log_entry = {
                    "timestamp": timestamp,
                    "data": data
                }
                f.write(json.dumps(log_entry) + "\n")
            return True
        except:
            return False

def main():
    # Check if POST request
    request_method = os.environ.get('REQUEST_METHOD', '')

    if request_method != 'POST':
        print(json.dumps({
            "error": "Only POST requests are supported",
            "method": request_method
        }))
        sys.exit(0)

    # Read POST data
    try:
        content_length = int(os.environ.get('CONTENT_LENGTH', 0))
        if content_length > 0:
            post_data = sys.stdin.read(content_length)
            sensor_data = json.loads(post_data)

            # Log the data
            if log_data(sensor_data):
                print(json.dumps({
                    "status": "success",
                    "message": "Sensor data received",
                    "data": sensor_data
                }))
            else:
                print(json.dumps({
                    "status": "warning",
                    "message": "Data received but logging failed",
                    "data": sensor_data
                }))
        else:
            print(json.dumps({
                "error": "No data received",
                "content_length": content_length
            }))

    except json.JSONDecodeError as e:
        print(json.dumps({
            "error": "Invalid JSON data",
            "details": str(e)
        }))
    except Exception as e:
        print(json.dumps({
            "error": "Server error",
            "details": str(e)
        }))

if __name__ == "__main__":
    main()
