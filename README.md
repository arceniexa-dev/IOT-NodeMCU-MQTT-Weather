# ESP8266 Portable Weather Station

Portable weather station based on **NodeMCU ESP8266 + DHT22** with **dual-protocol communication**:
- **MQTT** for efficient cloud telemetry (periodic publish)
- **HTTP / Telegram Bot** for user-facing queries and alerts

## Features

- 🌡️ Temperature & humidity monitoring (DHT22)
- 📡 MQTT publish (temp, humidity, JSON aggregate)
- 🤖 Telegram bot (`/status`, `/temperature`, `/humidity`, `/threshold`)
- 🚨 Threshold-based Telegram alerts
- 🕒 NTP timestamping (ISO-8601)
- 🔁 Auto-reconnect for Wi-Fi and MQTT
- 🔋 Portable-friendly (low power, external power bank)

## Why Dual Protocol?

MQTT is lightweight and efficient for continuous telemetry to a cloud dashboard [citation:1]. HTTP via Telegram Bot provides a familiar, zero-install user interface for on-demand queries and alerts — ideal when the user doesn't have an MQTT client open.

## Hardware

| Component | Pin |
|-----------|-----|
| DHT22 DATA | D5 (GPIO14) |
| DHT22 VCC | 3.3V |
| DHT22 GND | GND |

> Add a 4.7kΩ–10kΩ pull-up resistor between DHT22 DATA and VCC.

## Quick Start

1. Copy `config.example.h` → `config.h`
2. Fill in Wi-Fi, MQTT, and Telegram credentials
3. Open `weather_station.ino` in Arduino IDE
4. Install libraries (see below)
5. Select board: **NodeMCU 1.0 (ESP-12E Module)**
6. Upload

### Required Libraries

- `PubSubClient` (Nick O'Leary)
- `DHT sensor library` (Adafruit)
- `Adafruit Unified Sensor`
- `ArduinoJson` (v6.x)
- ESP8266 core (WiFi, HTTPClient, WiFiClientSecure)

## Documentation

- [MQTT Topics](docs/MQTT_TOPICS.md)
- [Telegram Commands](docs/TELEGRAM_COMMANDS.md)

## Security

⚠️ Never commit `config.h` — it is git-ignored.

## License

MIT
