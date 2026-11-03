#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Usa la configurazione attiva di TFT_eSPI. Sul computer è già impostata
// per la CYD (ILI9341_2, GPIO 12/13/14/15/2, retroilluminazione GPIO 21).
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH
#include <TFT_eSPI.h>

// Wi-Fi richiesto. Cambiare qui SSID e password se necessario.
const char* WIFI_SSID = "M1";
const char* WIFI_PASSWORD = "M1testM1";

// Coordinate di Roma: Open-Meteo non richiede una chiave API.
const char* WEATHER_URL = "https://api.open-meteo.com/v1/forecast?latitude=41.9028&longitude=12.4964&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=Europe%2FRome";
const uint32_t WEATHER_INTERVAL = 15UL * 60UL * 1000UL;
const uint32_t WEATHER_RETRY_INTERVAL = 30UL * 1000UL;
const uint32_t WIFI_RETRY_INTERVAL = 10UL * 1000UL;

TFT_eSPI tft;
float temperatureC = NAN;
float windKmh = NAN;
int humidity = -1;
int weatherCode = -1;
String weatherStatus = "Attendo dati meteo";
uint32_t lastWeatherFetch = 0;
uint32_t lastDraw = 0;
uint32_t lastWiFiRetry = 0;
uint32_t weatherWait = WEATHER_RETRY_INTERVAL;
bool clockReady = false;

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}

const char* italianDay(int day) {
  const char* days[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
  return days[day % 7];
}

const char* italianMonth(int month) {
  const char* months[] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};
  return months[month % 12];
}

String descriptionForCode(int code) {
  if (code == 0) return "Sereno";
  if (code == 1 || code == 2) return "Poco nuvoloso";
  if (code == 3) return "Nuvoloso";
  if (code == 45 || code == 48) return "Nebbia";
  if (code >= 51 && code <= 57) return "Pioviggine";
  if (code >= 61 && code <= 67) return "Pioggia";
  if (code >= 71 && code <= 77) return "Neve";
  if (code >= 80 && code <= 82) return "Rovesci";
  if (code >= 85 && code <= 86) return "Neve";
  if (code >= 95) return "Temporale";
  return "Meteo variabile";
}

void connectWiFi() {
  Serial.printf("[WiFi] Connessione alla rete '%s'\n", WIFI_SSID);
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawCentreString("OROLOGIO METEO", 120, 90, 4);
  tft.drawCentreString("Connessione Wi-Fi...", 120, 150, 2);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    delay(350);
    tft.fillRect(35, 185, 190, 8, TFT_NAVY);
    tft.fillRect(35, 185, map(millis() - started, 0, 20000, 0, 190), 8, TFT_CYAN);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connesso, IP=%s, RSSI=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
  } else {
    weatherStatus = "NO WIFI - controlla rete";
    Serial.printf("[WiFi] Connessione fallita, status=%d\n", (int)WiFi.status());
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    weatherStatus = "NO WIFI - riconnessione";
    Serial.printf("[Meteo] Saltato: Wi-Fi status=%d\n", (int)WiFi.status());
    return false;
  }
  Serial.println("[Meteo] Richiesta a Open-Meteo...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, WEATHER_URL)) {
    weatherStatus = "Errore apertura servizio";
    return false;
  }
  http.setTimeout(12000);
  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    weatherStatus = "HTTP " + String(status);
    Serial.printf("[Meteo] Errore HTTP: %d (%s)\n", status, http.errorToString(status).c_str());
    http.end();
    return false;
  }
  String payload = http.getString();
  Serial.printf("[Meteo] HTTP 200, risposta %u byte\n", (unsigned)payload.length());
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  http.end();
  if (error || !doc["current"].is<JsonObject>()) {
    weatherStatus = "ERRORE DATI METEO";
    Serial.printf("[Meteo] JSON non valido: %s\n", error ? error.c_str() : "campo current assente");
    return false;
  }
  JsonObject current = doc["current"].as<JsonObject>();
  temperatureC = current["temperature_2m"] | NAN;
  humidity = current["relative_humidity_2m"] | -1;
  windKmh = current["wind_speed_10m"] | NAN;
  weatherCode = current["weather_code"] | -1;
  weatherStatus = descriptionForCode(weatherCode);
  Serial.printf("[Meteo] OK: %.1f C, umidita %d%%, vento %.1f km/h, codice %d\n", temperatureC, humidity, windKmh, weatherCode);
  return true;
}

void drawScreen() {
  const uint16_t bg = color565(5, 11, 20);
  const uint16_t panel = color565(11, 23, 38);
  const uint16_t edge = color565(24, 54, 75);
  const uint16_t cyan = color565(40, 220, 245);
  const uint16_t muted = color565(122, 153, 174);
  const uint16_t white = color565(225, 242, 250);
  tft.fillScreen(bg);
  tft.fillRect(0, 0, 320, 34, color565(8, 20, 34));
  tft.drawFastHLine(0, 34, 320, edge);
  tft.fillRoundRect(10, 9, 4, 16, cyan, 2);
  tft.setTextColor(white, color565(8, 20, 34));
  tft.drawString("NOVA", 22, 8, 2);
  tft.setTextColor(muted, color565(8, 20, 34));
  tft.drawString("/ METEO", 67, 8, 2);
  uint16_t wifiColor = WiFi.status() == WL_CONNECTED ? color565(48, 230, 160) : color565(255, 166, 70);
  tft.fillCircle(294, 17, 3, wifiColor);
  tft.setTextColor(wifiColor, color565(8, 20, 34));
  tft.drawRightString(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE", 284, 10, 1);

  struct tm timeinfo;
  char timeText[8] = "--:--";
  char dateText[24] = "IN ATTESA DEL WI-FI";
  clockReady = getLocalTime(&timeinfo, 20);
  if (clockReady) {
    strftime(timeText, sizeof(timeText), "%H:%M", &timeinfo);
    snprintf(dateText, sizeof(dateText), "%s  %02d %s %04d", italianDay(timeinfo.tm_wday), timeinfo.tm_mday, italianMonth(timeinfo.tm_mon), timeinfo.tm_year + 1900);
  }

  // Ora e data: area principale a sinistra.
  tft.setTextColor(muted, bg);
  tft.drawString("ORA LOCALE  /  ROMA", 16, 49, 1);
  tft.setTextColor(cyan, bg);
  tft.drawString(timeText, 12, 64, 7);
  tft.drawFastHLine(16, 132, 148, edge);
  tft.fillRoundRect(16, 145, 4, 13, cyan, 2);
  tft.setTextColor(white, bg);
  tft.drawString(dateText, 27, 145, 1);
  tft.setTextColor(muted, bg);
  tft.drawString("SINCRONIZZAZIONE NTP", 16, 177, 1);

  // Scheda meteo con icona geometrica e temperatura.
  tft.fillRoundRect(180, 45, 128, 137, panel, 10);
  tft.drawRoundRect(180, 45, 128, 137, edge, 10);
  tft.setTextColor(muted, panel);
  tft.drawString("METEO ATTUALE", 191, 54, 1);
  if (weatherCode == 0 || weatherCode == 1) {
    tft.drawCircle(205, 91, 12, color565(255, 190, 70));
    tft.fillCircle(205, 91, 8, color565(255, 190, 70));
    for (int a = 0; a < 8; a++) {
      float angle = a * 0.785398f;
      tft.drawLine(205 + cos(angle) * 16, 91 + sin(angle) * 16, 205 + cos(angle) * 20, 91 + sin(angle) * 20, color565(255, 190, 70));
    }
  } else {
    uint16_t cloud = color565(125, 186, 210);
    tft.fillCircle(200, 91, 8, cloud);
    tft.fillCircle(209, 86, 10, cloud);
    tft.fillCircle(219, 92, 7, cloud);
    tft.fillRoundRect(195, 91, 29, 9, cloud, 4);
    if (weatherCode >= 51 && weatherCode <= 82) {
      tft.drawLine(202, 104, 199, 110, cyan);
      tft.drawLine(212, 104, 209, 110, cyan);
    }
  }
  tft.setTextColor(color565(255, 205, 103), panel);
  String tempText = isnan(temperatureC) ? "--.- C" : String(temperatureC, 1) + " C";
  tft.drawRightString(tempText, 298, 77, 4);
  tft.setTextColor(white, panel);
  String status = weatherStatus;
  if (status.length() > 17) status = status.substring(0, 17);
  tft.drawCentreString(status, 244, 126, 2);
  tft.setTextColor(muted, panel);
  tft.drawCentreString("ROMA  /  ITALIA", 244, 155, 1);

  // Indicatori inferiori.
  tft.fillRoundRect(12, 196, 145, 32, panel, 7);
  tft.fillRoundRect(163, 196, 145, 32, panel, 7);
  tft.drawRoundRect(12, 196, 145, 32, edge, 7);
  tft.drawRoundRect(163, 196, 145, 32, edge, 7);
  tft.setTextColor(cyan, panel);
  tft.drawString("UMIDITA", 23, 205, 1);
  tft.setTextColor(white, panel);
  tft.drawRightString((humidity < 0 ? String("--") : String(humidity)) + "%", 146, 202, 2);
  tft.setTextColor(cyan, panel);
  tft.drawString("VENTO", 174, 205, 1);
  tft.setTextColor(white, panel);
  tft.drawRightString((isnan(windKmh) ? String("--") : String(windKmh, 0)) + " km/h", 298, 202, 2);
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n[Avvio] Orologio meteo CYD2USB");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.init();
  tft.setRotation(1); // orizzontale, 320 x 240
  tft.fillScreen(TFT_BLACK);
  connectWiFi();
  bool weatherOk = WiFi.status() == WL_CONNECTED && fetchWeather();
  weatherWait = weatherOk ? WEATHER_INTERVAL : WEATHER_RETRY_INTERVAL;
  lastWeatherFetch = millis();
  drawScreen();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWiFiRetry >= WIFI_RETRY_INTERVAL) {
    WiFi.reconnect();
    lastWiFiRetry = millis();
  }
  if (millis() - lastWeatherFetch >= weatherWait) {
    bool weatherOk = fetchWeather();
    weatherWait = weatherOk ? WEATHER_INTERVAL : WEATHER_RETRY_INTERVAL;
    lastWeatherFetch = millis();
  }
  if (millis() - lastDraw >= 1000) {
    drawScreen();
    lastDraw = millis();
  }
  delay(20);
}
