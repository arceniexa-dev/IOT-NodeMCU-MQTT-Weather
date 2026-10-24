#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  DEVICE IDENTIFICATION
// ============================================================
#define DEVICE_ID     "weather-001"
#define DEVICE_LABEL  "Portable Weather Station"

// ============================================================
//  WIFI
// ============================================================
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ============================================================
//  MQTT BROKER
// ============================================================
#define MQTT_SERVER   "mqtt.example.com"
#define MQTT_PORT     1883
#define MQTT_USERNAME "YOUR_MQTT_USER"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"

// ============================================================
//  TELEGRAM BOT
// ============================================================
#define TELEGRAM_BOT_TOKEN "123456789:REPLACE_WITH_YOUR_BOT_TOKEN"
#define TELEGRAM_CHAT_ID   "000000000"

// ============================================================
//  DEFAULT THRESHOLDS
// ============================================================
#define DEFAULT_TEMP_MAX 35.0
#define DEFAULT_TEMP_MIN 10.0
#define DEFAULT_HUM_MAX  80.0
#define DEFAULT_HUM_MIN  20.0

// ============================================================
//  TIMING
// ============================================================
#define PUBLISH_INTERVAL_MS       5000   // MQTT publish interval
#define TELEGRAM_POLL_INTERVAL_MS 5000   // Telegram command polling

#endif // CONFIG_H
