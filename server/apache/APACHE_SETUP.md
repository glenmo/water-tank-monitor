# Apache2 Setup for Water Tank Sensor

## Installation Steps

### 1. Copy CGI Script to Apache

```bash
# Copy the CGI script to Apache's cgi-bin directory
sudo cp sensor-data.cgi /usr/lib/cgi-bin/
sudo chmod +x /usr/lib/cgi-bin/sensor-data.cgi
sudo chown www-data:www-data /usr/lib/cgi-bin/sensor-data.cgi
```

### 2. Enable CGI Module

```bash
# Enable CGI module in Apache2
sudo a2enmod cgi
```

### 3. Configure Apache Virtual Host

Create or edit your site configuration:

```bash
sudo nano /etc/apache2/sites-available/000-default.conf
```

Add the following inside the `<VirtualHost *:8080>` section:

```apache
# Enable CGI for /api path
ScriptAlias /api /usr/lib/cgi-bin

<Directory "/usr/lib/cgi-bin">
    AllowOverride None
    Options +ExecCGI
    Require all granted
    AddHandler cgi-script .cgi
</Directory>
```

### 4. Configure Apache to Listen on Port 8080

Edit the ports configuration:

```bash
sudo nano /etc/apache2/ports.conf
```

Ensure it includes:

```apache
Listen 8080
```

### 5. Create Log File (Optional)

```bash
# Create log file with proper permissions
sudo touch /var/log/water-tank-sensor.log
sudo chown www-data:www-data /var/log/water-tank-sensor.log
sudo chmod 644 /var/log/water-tank-sensor.log
```

**Note:** If you skip this step, the script will automatically log to `/tmp/water-tank-sensor.log` instead.

### 6. Restart Apache

```bash
sudo systemctl restart apache2
```

### 7. Check Apache Status

```bash
sudo systemctl status apache2
```

## Testing the Endpoint

### Test with curl:

```bash
curl -X POST http://192.168.55.192:8080/api/sensor-data.cgi \
  -H "Content-Type: application/json" \
  -d '{"voltage":2.5,"pressure_kpa":5.2,"water_depth_m":0.530,"volume_liters":25.5}'
```

Expected response:
```json
{
  "status": "success",
  "message": "Sensor data received",
  "data": {
    "voltage": 2.5,
    "pressure_kpa": 5.2,
    "water_depth_m": 0.530,
    "volume_liters": 25.5
  }
}
```

### View logs:

```bash
# If using /var/log
sudo tail -f /var/log/water-tank-sensor.log

# If using /tmp
tail -f /tmp/water-tank-sensor.log
```

## Troubleshooting

### Check Apache error logs:
```bash
sudo tail -f /var/log/apache2/error.log
```

### Check file permissions:
```bash
ls -l /usr/lib/cgi-bin/sensor-data.cgi
```

Should show:
```
-rwxr-xr-x 1 www-data www-data ... sensor-data.cgi
```

### Common issues:

1. **Permission denied**: Check file ownership and execute permissions
2. **Module not loaded**: Ensure CGI module is enabled with `sudo a2enmod cgi`
3. **500 Internal Server Error**: Check Apache error logs for details
4. **Script not found**: Verify ScriptAlias path matches your cgi-bin location

## Security Notes

- This is a basic setup for local network use
- For production/internet-facing servers, consider:
  - Adding authentication (API keys)
  - Using HTTPS
  - Rate limiting
  - Input validation
  - Database storage instead of flat files
