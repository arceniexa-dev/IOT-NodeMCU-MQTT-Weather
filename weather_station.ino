#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

// ---------------- DHT22 ----------------
#define DHTPIN  D5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ---------------- Thresholds ----------------
float TEMP_MAX = DEFAULT_TEMP_MAX;
float TEMP_MIN = DEFAULT_TEMP_MIN;
float HUM_MAX  = DEFAULT_HUM_MAX;
float HUM_MIN  = DEFAULT_HUM_MIN;

// ---------------- MQTT topics ----------------
String base_topic = String("weather/") + DEVICE_ID;
String temperature_topic, humidity_topic, data_topic, status_topic, alert_topic, control_topic;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;
unsigned long lastTelegramPoll = 0;
unsigned long lastReconnectAttempt = 0;
uint8_t reconnectFailCount = 0;
const uint8_t MAX_RECONNECT_FAIL = 12;

bool alertActiveTemp = false;
bool alertActiveHum  = false;
float lastTemperature = NAN;
float lastHumidity    = NAN;
long telegramLastUpdateId = 0;

// ================================================================
//  TELEGRAM HELPERS
// ================================================================
String urlEncode(const String& str) {
  String encoded = "";
  encoded.reserve(str.length() * 3 + 1);
  char c, code0, code1;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) encoded += c;
    else if (c == ' ') encoded += "%20";
    else if (c == '\n') encoded += "%0A";
    else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%'; encoded += code0; encoded += code1;
    }
  }
  return encoded;
}

void sendTelegramMessage(const String& text) {
  if (WiFi.status() != WL_CONNECTED) return;

  std::unique_ptr<BearSSL::WiFiClientSecure> secureClient(new BearSSL::WiFiClientSecure);
  secureClient->setInsecure();
  secureClient->setBufferSizes(1024, 1024);

  HTTPClient https;
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN +
               "/sendMessage?chat_id=" + TELEGRAM_CHAT_ID +
               "&parse_mode=HTML&text=" + urlEncode(text);

  if (https.begin(*secureClient, url)) {
    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK) {
      Serial.print("Telegram send failed: ");
      Serial.println(httpCode);
    }
    https.end();
  }
}

// ---------------- Telegram command handler ----------------
void handleTelegramCommand(const String& rawText) {
  String cmd = rawText;
  cmd.trim();

  if (cmd == "/status" || cmd == "/temperature" || cmd == "/humidity") {
    String reply = "===Weather Station===\n";
    reply += "<b>" + String(DEVICE_LABEL) + "</b> 🌤️\n\n";
    if (cmd == "/status" || cmd == "/temperature")
      reply += "🌡️ <b>Temperature:</b> " + String(lastTemperature, 1) + " C\n";
    if (cmd == "/status" || cmd == "/humidity")
      reply += "💧 <b>Humidity:</b> " + String(lastHumidity, 1) + " %RH\n";
    reply += "\n⏱️ <b>Uptime:</b> " + String(millis() / 1000) + " s";
    sendTelegramMessage(reply);

  } else if (cmd == "/threshold") {
    String reply = "===Weather Station===\n";
    reply += "<b>Thresholds</b> ⚙️\n\n";
    reply += "🌡️ Temp: " + String(TEMP_MIN, 1) + " - " + String(TEMP_MAX, 1) + " C\n";
    reply += "💧 Humidity: " + String(HUM_MIN, 1) + " - " + String(HUM_MAX, 1) + " %RH\n\n";
    reply += "<i>Use /setthreshold Tmax Tmin Hmax Hmin</i>";
    sendTelegramMessage(reply);

  } else if (cmd.startsWith("/setthreshold")) {
    String args = cmd.substring(13);
    args.trim();
    float t_max, t_min, h_max, h_min;
    if (sscanf(args.c_str(), "%f %f %f %f", &t_max, &t_min, &h_max, &h_min) == 4) {
      TEMP_MAX = t_max; TEMP_MIN = t_min;
      HUM_MAX = h_max; HUM_MIN = h_min;
      sendTelegramMessage("<b>Thresholds updated ✅</b>\n🌡️ " +
        String(TEMP_MIN, 1) + " - " + String(TEMP_MAX, 1) + " C\n💧 " +
        String(HUM_MIN, 1) + " - " + String(HUM_MAX, 1) + " %RH");
    } else {
      sendTelegramMessage("⚠️ Format: <code>/setthreshold 35 10 80 20</code>");
    }

  } else {
    String reply = "===Weather Station===\n";
    reply += "Hello! 🤖\nAvailable commands:\n\n";
    reply += "📊 /status - All metrics\n";
    reply += "🌡️ /temperature - Temperature only\n";
    reply += "💧 /humidity - Humidity only\n";
    reply += "⚙️ /threshold - Show thresholds\n";
    reply += "🛠️ /setthreshold - Change thresholds";
    sendTelegramMessage(reply);
  }
}

// ---------------- Telegram polling ----------------
void pollTelegramUpdates() {
  if (WiFi.status() != WL_CONNECTED) return;

  std::unique_ptr<BearSSL::WiFiClientSecure> secureClient(new BearSSL::WiFiClientSecure);
  secureClient->setInsecure();
  secureClient->setBufferSizes(1024, 1024);

  HTTPClient https;
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN +
               "/getUpdates?offset=" + String(telegramLastUpdateId + 1) +
               "&limit=1&timeout=0";

  if (!https.begin(*secureClient, url)) return;
  if (https.GET() != HTTP_CODE_OK) { https.end(); return; }

  String payload = https.getString();
  https.end();

  DynamicJsonDocument doc(768);
  if (deserializeJson(doc, payload)) return;

  JsonArray results = doc["result"].as<JsonArray>();
  if (results.size() == 0) return;

  JsonObject update = results[0];
  telegramLastUpdateId = update["update_id"].as<long>();

  if (!update.containsKey("message")) return;
  JsonObject message = update["message"];
  String fromChatId = message["chat"]["id"].as<String>();
  String text = message["text"] | "";

  if (fromChatId != String(TELEGRAM_CHAT_ID)) return;
  handleTelegramCommand(text);
}

// ---------------- WiFi ----------------
void setup_wifi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

// ---------------- NTP ----------------
void setup_time() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.print("NTP sync");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) { delay(300); Serial.print("."); now = time(nullptr); }
  Serial.println(" done");
}

String isoTimestamp() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

// ---------------- MQTT callback ----------------
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();

  if (String(topic) == control_topic && message.startsWith("threshold:")) {
    String vals = message.substring(10);
    float t_max, t_min, h_max, h_min;
    if (sscanf(vals.c_str(), "%f,%f,%f,%f", &t_max, &t_min, &h_max, &h_min) == 4) {
      TEMP_MAX = t_max; TEMP_MIN = t_min;
      HUM_MAX = h_max; HUM_MIN = h_min;
      Serial.println("Thresholds updated via MQTT");
    }
  }
}

// ---------------- MQTT reconnect ----------------
bool reconnect_once() {
  String clientId = String("ESP8266-") + DEVICE_ID;
  bool ok = client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD,
                           status_topic.c_str(), 1, true, "offline");
  if (ok) {
    Serial.println("MQTT connected");
    client.publish(status_topic.c_str(), "online", true);
    client.subscribe(control_topic.c_str());
    reconnectFailCount = 0;
  } else {
    reconnectFailCount++;
    Serial.print("MQTT fail rc="); Serial.println(client.state());
  }
  return ok;
}

void handleReconnect() {
  unsigned long nowMs = millis();
  if (nowMs - lastReconnectAttempt < 5000) return;
  lastReconnectAttempt = nowMs;
  if (!reconnect_once() && reconnectFailCount >= MAX_RECONNECT_FAIL) {
    Serial.println("Force restart");
    ESP.restart();
  }
}

// ---------------- Threshold checks ----------------
void checkThresholds(float t, float h, const String& ts) {
  StaticJsonDocument<192> doc;
  char buf[192];
  bool tempOut = (t > TEMP_MAX || t < TEMP_MIN);
  bool humOut  = (h > HUM_MAX  || h < HUM_MIN);

  if (tempOut && !alertActiveTemp) {
    doc["rack"] = DEVICE_ID; doc["type"] = "temperature"; doc["value"] = t;
    doc["limit_max"] = TEMP_MAX; doc["limit_min"] = TEMP_MIN;
    doc["state"] = "breach"; doc["timestamp"] = ts;
    serializeJson(doc, buf);
    client.publish(alert_topic.c_str(), buf, true);
    sendTelegramMessage("<b>🌡️ Temperature alert</b>\nValue: " + String(t, 1) +
      " C\nNormal: " + String(TEMP_MIN, 1) + " - " + String(TEMP_MAX, 1) + " C");
    alertActiveTemp = true;
  } else if (!tempOut && alertActiveTemp) {
    alertActiveTemp = false;
    sendTelegramMessage("<b>🌡️ Temperature normal</b>\nValue: " + String(t, 1) + " C");
  }

  if (humOut && !alertActiveHum) {
    doc["rack"] = DEVICE_ID; doc["type"] = "humidity"; doc["value"] = h;
    doc["limit_max"] = HUM_MAX; doc["limit_min"] = HUM_MIN;
    doc["state"] = "breach"; doc["timestamp"] = ts;
    serializeJson(doc, buf);
    client.publish(alert_topic.c_str(), buf, true);
    sendTelegramMessage("<b>💧 Humidity alert</b>\nValue: " + String(h, 1) +
      " %RH\nNormal: " + String(HUM_MIN, 1) + " - " + String(HUM_MAX, 1) + " %RH");
    alertActiveHum = true;
  } else if (!humOut && alertActiveHum) {
    alertActiveHum = false;
    sendTelegramMessage("<b>💧 Humidity normal</b>\nValue: " + String(h, 1) + " %RH");
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);
  temperature_topic = base_topic + "/temp";
  humidity_topic    = base_topic + "/hum";
  data_topic        = base_topic + "/data";
  status_topic      = base_topic + "/status";
  alert_topic       = base_topic + "/alert";
  control_topic     = base_topic + "/ctrl";

  setup_wifi();
  setup_time();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  dht.begin();
}

// ---------------- Main loop ----------------
void loop() {
  if (!client.connected()) handleReconnect();
  else client.loop();

  unsigned long nowMs = millis();

  if (nowMs - lastTelegramPoll >= TELEGRAM_POLL_INTERVAL_MS) {
    lastTelegramPoll = nowMs;
    pollTelegramUpdates();
  }

  if (nowMs - lastPublish < PUBLISH_INTERVAL_MS) return;
  lastPublish = nowMs;

  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22 read failed");
    return;
  }

  lastTemperature = temperature;
  lastHumidity = humidity;
  String ts = isoTimestamp();

  char temp_buffer[8]; dtostrf(temperature, 4, 2, temp_buffer);
  char hum_buffer[8];  dtostrf(humidity, 4, 2, hum_buffer);

  if (client.connected()) {
    client.publish(temperature_topic.c_str(), temp_buffer);
    client.publish(humidity_topic.c_str(), hum_buffer);

    StaticJsonDocument<192> doc;
    doc["device"] = DEVICE_ID;
    doc["temp"] = temperature;
    doc["hum"] = humidity;
    doc["timestamp"] = ts;
    char json_buffer[192];
    serializeJson(doc, json_buffer);
    client.publish(data_topic.c_str(), json_buffer);
  }

  checkThresholds(temperature, humidity, ts);

  Serial.print("T: "); Serial.print(temp_buffer);
  Serial.print(" C, H: "); Serial.print(hum_buffer);
  Serial.println(" %");
}
