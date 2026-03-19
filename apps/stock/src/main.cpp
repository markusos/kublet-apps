#include <Arduino.h>
#include <otaserver.h>
#include <kgfx.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Change this to track a different ticker (e.g. "SPY", "QQQ")
#define TICKER "VOO"

Preferences preferences;
OTAServer otaserver;
KGFX ui;
HTTPClient http;
JsonDocument json;
String serverUrl;

int refreshTimeInSeconds = 300;
unsigned long lastTime = 0;

// Chart data
#define MAX_POINTS 78
float points[MAX_POINTS];
bool pointValid[MAX_POINTS];
int numPoints = 0;
float openPrice = 0.0;
float lastPrice = 0.0;
float pctChange = 0.0;
bool marketOpen = false;

// Layout constants
#define CHART_X      20
#define CHART_Y      38
#define CHART_W      210
#define CHART_H      168
#define CHART_BOTTOM (CHART_Y + CHART_H)

#define COLOR_GREEN  0x07E0
#define COLOR_RED    0xF800
#define COLOR_GRAY   0x4208
#define COLOR_DGRAY  0x2104

void drawUI() {
  ui.tft.fillScreen(TFT_BLACK);

  // --- Header ---
  uint16_t pctColor = (pctChange >= 0) ? COLOR_GREEN : COLOR_RED;

  // Ticker name (left)
  ui.tft.setTTFFont(Arial_14_Bold);
  ui.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  ui.tft.setCursor(8, 6);
  ui.tft.print(TICKER);

  // "CLOSED" indicator
  if (!marketOpen) {
    ui.tft.setTTFFont(Arial_12);
    ui.tft.setTextColor(COLOR_GRAY, TFT_BLACK);
    int tw = ui.tft.TTFtextWidth(TICKER);
    ui.tft.setCursor(8 + tw + 6, 8);
    ui.tft.print("CLOSED");
  }

  // Last price (center)
  char priceStr[16];
  snprintf(priceStr, sizeof(priceStr), "$%.2f", lastPrice);
  ui.tft.setTTFFont(Arial_14_Bold);
  ui.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int pw = ui.tft.TTFtextWidth(priceStr);
  ui.tft.setCursor((240 - pw) / 2, 6);
  ui.tft.print(priceStr);

  // Percent change (right)
  char pctStr[16];
  snprintf(pctStr, sizeof(pctStr), "%+.2f%%", pctChange);
  ui.tft.setTTFFont(Arial_14_Bold);
  ui.tft.setTextColor(pctColor, TFT_BLACK);
  int pcw = ui.tft.TTFtextWidth(pctStr);
  ui.tft.setCursor(232 - pcw, 6);
  ui.tft.print(pctStr);

  // --- Separator line ---
  ui.tft.drawFastHLine(0, 30, 240, COLOR_DGRAY);

  // --- Chart ---
  if (numPoints < 2) {
    ui.tft.setTTFFont(Arial_14_Bold);
    ui.tft.setTextColor(COLOR_GRAY, TFT_BLACK);
    ui.tft.setCursor(60, 120);
    ui.tft.print("No data");
    return;
  }

  // Find min/max delta for Y scaling
  float minDelta = 0, maxDelta = 0;
  for (int i = 0; i < MAX_POINTS; i++) {
    if (!pointValid[i]) continue;
    if (points[i] < minDelta) minDelta = points[i];
    if (points[i] > maxDelta) maxDelta = points[i];
  }

  // Ensure zero line is always visible
  if (minDelta > 0) minDelta = 0;
  if (maxDelta < 0) maxDelta = 0;

  // Add 10% padding
  float range = maxDelta - minDelta;
  if (range < 0.01f) range = 1.0f;
  minDelta -= range * 0.1f;
  maxDelta += range * 0.1f;
  range = maxDelta - minDelta;

  // Draw zero (open price) reference line - dotted
  int zeroY = CHART_BOTTOM - (int)((0.0f - minDelta) / range * CHART_H);
  if (zeroY >= CHART_Y && zeroY <= CHART_BOTTOM) {
    for (int x = CHART_X; x < CHART_X + CHART_W; x += 4) {
      ui.tft.drawPixel(x, zeroY, COLOR_GRAY);
    }
  }

  // Draw chart line
  uint16_t lineColor = (pctChange >= 0) ? COLOR_GREEN : COLOR_RED;

  int prevX = -1, prevY = -1;
  for (int i = 0; i < MAX_POINTS; i++) {
    if (!pointValid[i]) continue;

    int x = CHART_X + i * CHART_W / 77;
    int y = CHART_BOTTOM - (int)((points[i] - minDelta) / range * CHART_H);

    // Clamp to chart area
    if (y < CHART_Y) y = CHART_Y;
    if (y > CHART_BOTTOM) y = CHART_BOTTOM;

    if (prevX >= 0) {
      ui.tft.drawLine(prevX, prevY, x, y, lineColor);
    }
    prevX = x;
    prevY = y;
  }

  // --- X-axis time labels ---
  ui.tft.setTTFFont(Arial_12);
  ui.tft.setTextColor(COLOR_GRAY, TFT_BLACK);

  // 9:30
  ui.tft.setCursor(CHART_X, CHART_BOTTOM + 8);
  ui.tft.print("9:30");

  // 12:00 (index 30 of 78)
  int midX = CHART_X + 30 * CHART_W / 77;
  ui.tft.setCursor(midX - 10, CHART_BOTTOM + 8);
  ui.tft.print("12:00");

  // 16:00
  int rw = ui.tft.TTFtextWidth("16:00");
  ui.tft.setCursor(CHART_X + CHART_W - rw, CHART_BOTTOM + 8);
  ui.tft.print("16:00");
}

void fetchChartData() {
  if (serverUrl.length() == 0) {
    return;
  }

  String url = serverUrl + "/api/chart?ticker=" TICKER;
  Serial.printf("Fetching: %s\n", url.c_str());
  http.begin(url);
  int httpResponseCode = http.GET();
  Serial.printf("HTTP response: %d\n", httpResponseCode);

  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.printf("Payload size: %d\n", payload.length());
    DeserializationError error = deserializeJson(json, payload);
    Serial.printf("JSON parse: %s\n", error ? error.c_str() : "ok");
    if (!error) {
      openPrice = json["open"] | 0.0f;
      lastPrice = json["last"] | 0.0f;
      pctChange = json["pct"] | 0.0f;
      marketOpen = json["market_open"] | false;

      // Reset points
      numPoints = 0;
      memset(pointValid, 0, sizeof(pointValid));

      JsonArray pts = json["points"].as<JsonArray>();
      for (JsonArray pt : pts) {
        int idx = pt[0].as<int>();
        if (idx >= 0 && idx < MAX_POINTS) {
          points[idx] = pt[1].as<float>();
          pointValid[idx] = true;
          if (idx + 1 > numPoints) numPoints = idx + 1;
        }
      }
    }
  }

  http.end();
}

void setup() {
  Serial.begin(460800);
  Serial.println("Starting stock app");

  otaserver.connectWiFi(); // DO NOT EDIT.
  otaserver.run(); // DO NOT EDIT

  preferences.begin("app", true);
  serverUrl = preferences.getString("server_url");
  preferences.end();

  ui.init();
  ui.clear();
}

void loop() {
  if ((WiFi.status() == WL_CONNECTED)) {
    otaserver.handle(); // DO NOT EDIT

    if (((millis() - lastTime) > (unsigned long)refreshTimeInSeconds * 1000) || lastTime == 0) {
      lastTime = millis();
      fetchChartData();
      drawUI();
    }
  }

  delay(1);
}
