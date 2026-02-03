# ChirpStack Basic Station Mode Configuration Guide

## Overview

Basic Station is a more modern gateway protocol than the legacy Packet Forwarder. It uses WebSockets instead of UDP and provides better security, reliability, and features.

## Current vs Target Architecture

**Current (Packet Forwarder):**
```
SenseCAP M2 → UDP (port 1700) → ChirpStack
```

**Target (Basic Station):**
```
SenseCAP M2 → WebSocket (port 3001) → ChirpStack
```

**Arduino sketch:** No changes needed - LoRaWAN packets are identical

---

## Part 1: Configure ChirpStack for Basic Station

### Step 1: Enable Basic Station in ChirpStack

ChirpStack supports both modes simultaneously, so you don't need to disable Packet Forwarder.

**Edit ChirpStack configuration:**

```bash
# SSH into your ChirpStack server (or edit locally)
ssh glen@rubberduck.local

# Edit the ChirpStack configuration file
sudo nano /etc/chirpstack/chirpstack.toml
```

**Find the `[gateway]` section and ensure Basic Station is enabled:**

```toml
[gateway]
  # Gateway backend configuration.
  [gateway.backend]
    # Backend type.
    # Valid options are:
    #  * semtech_udp: Semtech UDP packet-forwarder backend (default, port 1700)
    #  * basic_station: Basic Station backend (WebSocket, port 3001)
    type="semtech_udp"

  # Basic Station backend.
  [gateway.backend.basic_station]
    # ip:port to bind the Basic Station backend to.
    bind=":3001"

    # Region.
    # Please refer to the LoRaWAN Regional Parameters specification
    # for the complete list of regions.
    region="AU915"

    # Minimal frequency (Hz).
    frequency_min=915000000

    # Maximum frequency (Hz).
    frequency_max=928000000

    # CA certificate (path).
    # Leave blank to disable TLS (not recommended for production)
    ca_cert=""

    # TLS certificate (path).
    # Leave blank to disable TLS
    tls_cert=""

    # TLS key (path).
    # Leave blank to disable TLS
    tls_key=""
```

**For testing without TLS (simpler):**
- Leave `ca_cert`, `tls_cert`, and `tls_key` blank
- Basic Station will connect on `ws://` instead of `wss://`

**Save and restart ChirpStack:**

```bash
sudo systemctl restart chirpstack
sudo systemctl status chirpstack

# Check that port 3001 is listening
sudo netstat -tlnp | grep 3001
# Should show: tcp6  0  0 :::3001  :::*  LISTEN  <pid>/chirpstack
```

### Step 2: Verify ChirpStack is Ready

```bash
# Check ChirpStack logs for Basic Station backend
sudo journalctl -u chirpstack -f

# You should see something like:
# INFO basic_station: starting Basic Station backend bind=":3001"
```

---

## Part 2: Configure SenseCAP M2 for Basic Station

### Step 3: Access the SenseCAP Gateway

**Option A: Web Interface**
```
http://<gateway-ip>:8080
Username: admin
Password: (your gateway password)
```

**Option B: SSH (if enabled)**
```bash
ssh root@<gateway-ip>
```

### Step 4: Switch to Basic Station Mode

#### Via Web Interface:

1. **Navigate to:** Network Server → Basic Station
2. **Enable Basic Station**
3. **Configure connection:**
   ```
   Server: ws://rubberduck.local:3001
   (or ws://192.168.55.192:3001)
   ```
4. **Region:** AU915
5. **Save and restart gateway**

#### Via SSH:

```bash
# Edit Basic Station config
vi /etc/basicstation/station.conf

# Set the following:
{
  "station_conf": {
    "log_level": "INFO",
    "log_file": "stderr",
    "radio_init": "/etc/basicstation/radio.conf",
    "TC_URI": "ws://192.168.55.192:3001",
    "TC_TRUST": "",
    "TC_KEY": ""
  }
}

# Edit radio config for AU915
vi /etc/basicstation/radio.conf

{
  "SX1302_conf": {
    "com_type": "SPI",
    "com_path": "/dev/spidev0.0",
    "lorawan_public": true,
    "clksrc": 0,
    "full_duplex": false,
    "antenna_gain": 0,
    "radio_0": {
      "enable": true,
      "type": "SX1250",
      "freq": 923300000,
      "rssi_offset": -215.4,
      "rssi_tcomp": {"coeff_a": 0, "coeff_b": 0, "coeff_c": 20.41, "coeff_d": 2162.56, "coeff_e": 0},
      "tx_enable": true
    },
    "radio_1": {
      "enable": true,
      "type": "SX1250",
      "freq": 921900000,
      "rssi_offset": -215.4,
      "rssi_tcomp": {"coeff_a": 0, "coeff_b": 0, "coeff_c": 20.41, "coeff_d": 2162.56, "coeff_e": 0},
      "tx_enable": false
    },
    "chan_multiSF_0": {"enable": true, "radio": 0, "if": -400000},
    "chan_multiSF_1": {"enable": true, "radio": 0, "if": -200000},
    "chan_multiSF_2": {"enable": true, "radio": 0, "if": 0},
    "chan_multiSF_3": {"enable": true, "radio": 0, "if": 200000},
    "chan_multiSF_4": {"enable": true, "radio": 1, "if": -300000},
    "chan_multiSF_5": {"enable": true, "radio": 1, "if": -100000},
    "chan_multiSF_6": {"enable": true, "radio": 1, "if": 100000},
    "chan_multiSF_7": {"enable": true, "radio": 1, "if": 300000},
    "chan_Lora_std":  {"enable": true, "radio": 0, "if": 300000, "bandwidth": 500000, "spread_factor": 8, "implicit_hdr": false, "implicit_payload_length": 0, "implicit_crc_en": false, "implicit_coderate": 0},
    "chan_FSK":       {"enable": false}
  }
}

# Restart Basic Station service
/etc/init.d/basicstation restart

# Check status
logread -f | grep station
```

---

## Part 3: Verification

### Step 5: Check Gateway Connection

**In ChirpStack Web UI:**
1. Navigate to **Gateways**
2. Your gateway should show as **connected**
3. Last seen should update every ~30 seconds

**In ChirpStack logs:**
```bash
sudo journalctl -u chirpstack -f

# You should see:
# INFO basic_station: gateway connected gateway_id=2cf7f1177440004b
# INFO basic_station: uplink received
```

**In Gateway logs:**
```bash
# Via web interface: System → Logs
# Or via SSH:
logread -f

# You should see:
# INFO: [main] Station started, using protocol 2
# INFO: [main] Connecting to CUPS...
# INFO: [main] Connected to CUPS
```

### Step 6: Test with Your Arduino

Your Arduino doesn't need any changes. Just power it on and verify:

1. **ChirpStack shows uplinks:**
   - Go to Devices → Tank-Sensor-01 → LoRaWAN frames
   - You should see UnconfirmedDataUp frames arriving

2. **Gateway shows RX packets:**
   - Gateway web UI → Dashboard
   - RX packet count should increment

3. **Dashboard updates:**
   - http://rubberduck.local:8080
   - Should show live data

---

## Troubleshooting

### Gateway Not Connecting to ChirpStack

**Check firewall:**
```bash
# On ChirpStack server
sudo ufw status
# Port 3001 should be allowed

# If not:
sudo ufw allow 3001/tcp
sudo ufw reload
```

**Check ChirpStack is listening:**
```bash
sudo netstat -tlnp | grep 3001
# Should show chirpstack listening on port 3001
```

**Check gateway can reach ChirpStack:**
```bash
# From gateway (SSH)
telnet 192.168.55.192 3001
# Should connect (Ctrl+C to exit)
```

### Packets Not Being Received

**Verify gateway is transmitting:**
```bash
# Gateway logs should show:
# INFO: [up] packet received
# INFO: [main] station::service_loop: uplink frame sent
```

**Verify ChirpStack is receiving:**
```bash
sudo journalctl -u chirpstack -f
# Should show: uplink received
```

**Check device keys match:**
- Device EUI: `6c8daba7367fe214`
- Join EUI: `77f70cb666cf39ee`
- App Key should match between Arduino and ChirpStack

### Switching Back to Packet Forwarder

If Basic Station doesn't work, you can easily switch back:

**On gateway:**
```bash
# Via web interface: disable Basic Station, enable Packet Forwarder
# Or via SSH:
/etc/init.d/basicstation stop
/etc/init.d/lora-pkt-fwd start
```

ChirpStack continues listening on both protocols, so no server changes needed.

---

## Benefits of Basic Station

Once working, Basic Station provides:

1. **Better reliability:** WebSocket maintains persistent connection vs UDP fire-and-forget
2. **Lower latency:** No UDP handshake delays
3. **Security:** Can use WSS (WebSocket Secure) with TLS certificates
4. **Remote management:** CUPS (Configuration and Update Protocol) support
5. **Better debugging:** More detailed logs and error messages

---

## Advanced: Enable TLS (Production)

For production deployments, enable TLS encryption:

### Generate certificates:

```bash
# On ChirpStack server
cd /etc/chirpstack/certs

# Generate CA certificate
openssl genrsa -out ca.key 4096
openssl req -new -x509 -days 3650 -key ca.key -out ca.pem

# Generate server certificate
openssl genrsa -out server.key 4096
openssl req -new -key server.key -out server.csr
openssl x509 -req -days 3650 -in server.csr -CA ca.pem -CAkey ca.key -set_serial 01 -out server.crt

# Set permissions
chmod 640 *.key
chown chirpstack:chirpstack *.key *.crt *.pem
```

### Update ChirpStack config:

```toml
[gateway.backend.basic_station]
  ca_cert="/etc/chirpstack/certs/ca.pem"
  tls_cert="/etc/chirpstack/certs/server.crt"
  tls_key="/etc/chirpstack/certs/server.key"
```

### Update gateway config:

```
TC_URI: "wss://192.168.55.192:3001"
TC_TRUST: "/path/to/ca.pem"
```

---

## Summary Checklist

- [ ] ChirpStack listening on port 3001
- [ ] SenseCAP M2 configured for Basic Station
- [ ] Gateway connects to ChirpStack (check logs)
- [ ] Gateway shows as connected in ChirpStack UI
- [ ] Arduino uplinks appear in ChirpStack
- [ ] Dashboard at rubberduck.local:8080 updating
- [ ] (Optional) TLS enabled for production

---

## Reference

- ChirpStack Basic Station docs: https://www.chirpstack.io/docs/chirpstack/gateway-management.html
- SenseCAP M2 docs: https://wiki.seeedstudio.com/SenseCAP_M2_Multi_Platform/
- Basic Station GitHub: https://github.com/lorabasics/basicstation

**Arduino code:** No changes required! ✅
