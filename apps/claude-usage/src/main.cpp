#include <Arduino.h>
#include <otaserver.h>
#include <kgfx.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <AnimatedGIF.h>
#include <GIFDraw.h>

Preferences preferences;
OTAServer otaserver;
KGFX ui;
HTTPClient http;
JsonDocument json;
AnimatedGIF gif;
String serverUrl;

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
#define CENTER_X 120
#define CENTER_Y 112

#define SESSION_OR 108   // session ring, outer radius
#define SESSION_IR  92   // session ring, inner radius
#define WEEKLY_OR   84   // weekly ring, outer radius
#define WEEKLY_IR   70   // weekly ring, inner radius

#define ARC_START 45.0f  // degrees, clockwise from 6 o'clock
#define ARC_SPAN 270.0f  // leaves a gap at the bottom

// Angular step, divided by the outer radius. 28.6 degrees times a radius of 1
// pixel is one radian of arc, so this steps about half a pixel at the edge.
#define ANGLE_STEP_SCALE 28.6f
#define TIP_DEGREES 7.0f // width of the glowing tip

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------
struct Rgb {
  uint8_t r, g, b;
};

// The session ring runs cool to hot, so a full window reads as a warning
static const Rgb SESSION_RAMP[] = {
    {0, 224, 176},   // teal
    {132, 214, 60},  // lime
    {255, 190, 40},  // amber
    {255, 96, 56},   // coral
    {255, 40, 40},   // red
};

// The weekly ring uses a separate hue family, so the two rings never blur together
static const Rgb WEEKLY_RAMP[] = {
    {60, 168, 255},  // blue
    {138, 116, 255}, // indigo
    {186, 96, 246},  // violet
    {255, 92, 190},  // magenta
};

static const int SESSION_STOPS = sizeof(SESSION_RAMP) / sizeof(SESSION_RAMP[0]);
static const int WEEKLY_STOPS = sizeof(WEEKLY_RAMP) / sizeof(WEEKLY_RAMP[0]);

#define COL_BG TFT_BLACK
#define COL_TRACK 0x18E3  // dim slate, the unfilled part of a ring
#define COL_LABEL 0x8410  // grey text
#define COL_STALE 0xFDE5  // amber, the label colour after a failed fetch

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
int sessionPercent = 0;
int weeklyPercent = 0;
long sessionResetIn = 0;  // seconds until the window resets
long weeklyResetIn = 0;

// values currently on screen, so a redraw only touches what changed
float shownSession = 0.0f;
float shownWeekly = 0.0f;
int drawnSessionText = -1;
int drawnWeeklyText = -1;
char drawnSessionClock[16] = "";
char drawnWeeklyClock[16] = "";

// animation
bool animating = false;
unsigned long animStart = 0;
float animFromSession = 0.0f;
float animFromWeekly = 0.0f;
const unsigned long ANIM_MS = 900;

unsigned long lastFetch = 0;
unsigned long lastClockTick = 0;
unsigned long resetBase = 0;  // millis() when the countdowns were last refreshed
bool haveData = false;

// The countdowns keep running between fetches, so a dead server still looks
// plausible. These track the last fetch, and colour the label when it failed.
bool fetchFailed = false;
bool drawnFetchFailed = false;

int refreshTimeInSeconds = 300;

// ---------------------------------------------------------------------------
// Colour helpers
// ---------------------------------------------------------------------------
uint16_t rampColor(const Rgb* ramp, int stops, float t, float scale) {
  t = constrain(t, 0.0f, 1.0f);
  float pos = t * (stops - 1);
  int i = (int)pos;
  if (i >= stops - 1) i = stops - 2;
  float f = pos - i;

  float r = ramp[i].r + (ramp[i + 1].r - ramp[i].r) * f;
  float g = ramp[i].g + (ramp[i + 1].g - ramp[i].g) * f;
  float b = ramp[i].b + (ramp[i + 1].b - ramp[i].b) * f;

  r = constrain(r * scale, 0.0f, 255.0f);
  g = constrain(g * scale, 0.0f, 255.0f);
  b = constrain(b * scale, 0.0f, 255.0f);
  return ui.tft.color565((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

// ---------------------------------------------------------------------------
// Arc drawing
// ---------------------------------------------------------------------------
// Angles run clockwise from 6 o'clock, which matches TFT_eSPI drawSmoothArc.
void arcPoint(float angle, int radius, int& x, int& y) {
  float rad = angle * DEG_TO_RAD;
  x = CENTER_X - (int)lroundf(radius * sinf(rad));
  y = CENTER_Y + (int)lroundf(radius * cosf(rad));
}

// Draws one angular slice of a ring. The colour comes from the position in the
// whole ring, not the slice, so the gradient stays fixed while the arc grows.
void drawArcSlice(int outerR, int innerR, float fromAngle, float toAngle,
                  const Rgb* ramp, int stops, bool track, float scale) {
  if (toAngle <= fromAngle) return;

  // One pixel per integer radius, and an angular step of about half a pixel at
  // the outer edge. Drawing separate radial lines leaves pinholes, because the
  // lines are rasterised independently and do not tile the ring.
  float step = ANGLE_STEP_SCALE / (float)outerR;

  for (float a = fromAngle; a < toAngle; a += step) {
    uint16_t color = COL_TRACK;
    if (!track) {
      float frac = (a - ARC_START) / ARC_SPAN;
      color = rampColor(ramp, stops, frac, scale);
    }
    float rad = a * DEG_TO_RAD;
    float sinA = sinf(rad);
    float cosA = cosf(rad);
    for (int r = innerR; r <= outerR; r++) {
      int x = CENTER_X - (int)lroundf(r * sinA);
      int y = CENTER_Y + (int)lroundf(r * cosA);
      ui.tft.drawPixel(x, y, color);
    }
  }
}

float angleFor(float percent) {
  return ARC_START + ARC_SPAN * constrain(percent, 0.0f, 100.0f) / 100.0f;
}

// Grows or shrinks a ring to a new value, redrawing only the part that changed.
void updateRing(int outerR, int innerR, const Rgb* ramp, int stops,
                float fromPct, float toPct) {
  float fromAngle = angleFor(fromPct);
  float toAngle = angleFor(toPct);

  if (toAngle > fromAngle) {
    drawArcSlice(outerR, innerR, fromAngle, toAngle, ramp, stops, false, 1.0f);
  } else if (toAngle < fromAngle) {
    // value dropped, for example after a window reset — paint the track back
    drawArcSlice(outerR, innerR, toAngle, fromAngle, ramp, stops, true, 1.0f);
  }
}

// A brighter band at the leading edge, pulsed to show the display is live.
void drawTip(int outerR, int innerR, const Rgb* ramp, int stops, float pct,
             float scale) {
  if (pct <= 0.0f) return;
  float end = angleFor(pct);
  float start = end - TIP_DEGREES;
  if (start < ARC_START) start = ARC_START;
  drawArcSlice(outerR, innerR, start, end, ramp, stops, false, scale);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
void formatCountdown(long seconds, char* buf, size_t len) {
  if (seconds <= 0) {
    snprintf(buf, len, "now");
    return;
  }
  long days = seconds / 86400;
  long hours = (seconds % 86400) / 3600;
  long mins = (seconds % 3600) / 60;

  if (days > 0) {
    snprintf(buf, len, "%ldd %ldh", days, hours);
  } else if (hours > 0) {
    snprintf(buf, len, "%ldh %ldm", hours, mins);
  } else {
    snprintf(buf, len, "%ldm", mins > 0 ? mins : 1);
  }
}

void drawBoltIcon(int x, int y, uint16_t color) {
  ui.tft.fillTriangle(x + 4, y, x, y + 6, x + 5, y + 5, color);
  ui.tft.fillTriangle(x + 3, y + 5, x + 8, y + 6, x + 4, y + 14, color);
}

void drawCalendarIcon(int x, int y, uint16_t color) {
  ui.tft.drawRect(x, y + 3, 10, 9, color);
  ui.tft.fillRect(x, y + 3, 10, 3, color);
  ui.tft.fillRect(x + 2, y, 2, 4, color);
  ui.tft.fillRect(x + 6, y, 2, 4, color);
}

// Centres text and clears only a narrow box first. KGFX::drawCentered clears the
// full width of the screen, which would erase both rings on every redraw.
#define TEXT_BOX_X 62
#define TEXT_BOX_W 116

void drawCenteredNarrow(const char* text, const tftfont_t& font, uint16_t color,
                        int y, int clearH) {
  ui.tft.TTFdestination(&ui.tft);
  ui.tft.setTTFFont(font);
  if (clearH > 0) ui.tft.fillRect(TEXT_BOX_X, y - 2, TEXT_BOX_W, clearH, COL_BG);
  ui.tft.setTextColor(color, COL_BG);
  int w = ui.tft.TTFtextWidth(text);
  ui.tft.setCursor((240 - w) / 2, y);
  ui.tft.print(text);
}

void drawPercentages(int session, int weekly) {
  char buf[8];
  uint16_t sessionColor = rampColor(SESSION_RAMP, SESSION_STOPS, session / 100.0f, 1.0f);
  uint16_t weeklyColor = rampColor(WEEKLY_RAMP, WEEKLY_STOPS, weekly / 100.0f, 1.0f);

  if (session != drawnSessionText) {
    snprintf(buf, sizeof(buf), "%d%%", session);
    drawCenteredNarrow(buf, Arial_32_Bold, sessionColor, 72, 42);
    drawnSessionText = session;
  }

  if (weekly != drawnWeeklyText) {
    snprintf(buf, sizeof(buf), "%d%%", weekly);
    drawCenteredNarrow(buf, Arial_20_Bold, weeklyColor, 118, 28);
    drawnWeeklyText = weekly;
  }
}

// The two countdown readouts along the bottom.
void drawClocks(bool force) {
  char sessionBuf[16];
  char weeklyBuf[16];
  long elapsed = (long)((millis() - resetBase) / 1000);
  formatCountdown(sessionResetIn - elapsed, sessionBuf, sizeof(sessionBuf));
  formatCountdown(weeklyResetIn - elapsed, weeklyBuf, sizeof(weeklyBuf));

  bool sessionChanged = force || strcmp(sessionBuf, drawnSessionClock) != 0;
  bool weeklyChanged = force || strcmp(weeklyBuf, drawnWeeklyClock) != 0;
  if (!sessionChanged && !weeklyChanged) return;

  uint16_t sessionColor =
      rampColor(SESSION_RAMP, SESSION_STOPS, sessionPercent / 100.0f, 1.0f);
  uint16_t weeklyColor =
      rampColor(WEEKLY_RAMP, WEEKLY_STOPS, weeklyPercent / 100.0f, 1.0f);

  ui.tft.setTTFFont(Arial_12_Bold);

  // Centre the icon and its text together inside each half of the screen. The
  // countdown text changes width as it counts down, so this recentres it.
  const int half = 120;
  const int iconW = 10;
  const int gap = 6;

  if (sessionChanged) {
    ui.tft.fillRect(0, 216, half, 24, COL_BG);
    int textW = ui.tft.TTFtextWidth(sessionBuf);
    int startX = (half - (iconW + gap + textW)) / 2;
    drawBoltIcon(startX, 220, sessionColor);
    ui.tft.setTextColor(sessionColor, COL_BG);
    ui.tft.setCursor(startX + iconW + gap, 222);
    ui.tft.print(sessionBuf);
    strncpy(drawnSessionClock, sessionBuf, sizeof(drawnSessionClock) - 1);
  }

  if (weeklyChanged) {
    ui.tft.fillRect(half, 216, half, 24, COL_BG);
    int textW = ui.tft.TTFtextWidth(weeklyBuf);
    int startX = half + (half - (iconW + gap + textW)) / 2;
    drawCalendarIcon(startX, 220, weeklyColor);
    ui.tft.setTextColor(weeklyColor, COL_BG);
    ui.tft.setCursor(startX + iconW + gap, 222);
    ui.tft.print(weeklyBuf);
    strncpy(drawnWeeklyClock, weeklyBuf, sizeof(drawnWeeklyClock) - 1);
  }
}

void drawStatic() {
  ui.tft.fillScreen(COL_BG);

  // empty tracks for both rings
  drawArcSlice(SESSION_OR, SESSION_IR, ARC_START, ARC_START + ARC_SPAN,
               SESSION_RAMP, SESSION_STOPS, true, 1.0f);
  drawArcSlice(WEEKLY_OR, WEEKLY_IR, ARC_START, ARC_START + ARC_SPAN,
               WEEKLY_RAMP, WEEKLY_STOPS, true, 1.0f);

  ui.drawCentered("RESETS IN", Arial_11, COL_LABEL, 200, COL_BG, 0);
  ui.tft.drawFastVLine(120, 218, 20, COL_TRACK);
}

// The label doubles as a health light. It turns amber while fetches fail, so
// counted-down values cannot pass for fresh ones. The text keeps its width and
// position, and drawCentered paints the background behind each glyph, so this
// needs no clear.
void drawFetchState() {
  if (fetchFailed == drawnFetchFailed) return;
  ui.drawCentered("RESETS IN", Arial_11, fetchFailed ? COL_STALE : COL_LABEL, 200,
                  COL_BG, 0);
  drawnFetchFailed = fetchFailed;
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
float easeOut(float t) {
  float inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

void startAnimation() {
  animFromSession = shownSession;
  animFromWeekly = shownWeekly;
  animStart = millis();
  animating = true;
}

void stepAnimation() {
  unsigned long elapsed = millis() - animStart;
  float t = (float)elapsed / (float)ANIM_MS;
  bool last = false;
  if (t >= 1.0f) {
    t = 1.0f;
    last = true;
  }

  float p = easeOut(t);
  float session = animFromSession + (sessionPercent - animFromSession) * p;
  float weekly = animFromWeekly + (weeklyPercent - animFromWeekly) * p;

  updateRing(SESSION_OR, SESSION_IR, SESSION_RAMP, SESSION_STOPS, shownSession, session);
  updateRing(WEEKLY_OR, WEEKLY_IR, WEEKLY_RAMP, WEEKLY_STOPS, shownWeekly, weekly);
  shownSession = session;
  shownWeekly = weekly;

  drawPercentages((int)lroundf(session), (int)lroundf(weekly));

  if (last) {
    animating = false;
    drawClocks(true);
  }
}

// Breathing highlight on both arc tips while the app waits for the next fetch.
void stepPulse() {
  const float twoPi = 6.28318530718f;
  float phase = (millis() % 2400) / 2400.0f;
  float wave = 0.5f + 0.5f * sinf(phase * twoPi);
  float scale = 1.0f + 0.55f * wave;

  drawTip(SESSION_OR, SESSION_IR, SESSION_RAMP, SESSION_STOPS, shownSession, scale);
  drawTip(WEEKLY_OR, WEEKLY_IR, WEEKLY_RAMP, WEEKLY_STOPS, shownWeekly, scale);
}

// ---------------------------------------------------------------------------
// Pushed GIF
// ---------------------------------------------------------------------------
// Rebuilds the whole metrics screen. The rings sweep up again from zero, which
// makes the return from a GIF read as a deliberate transition.
void restoreMetrics() {
  drawStatic();
  drawnFetchFailed = false;  // drawStatic painted the label grey again
  drawFetchState();
  shownSession = 0.0f;
  shownWeekly = 0.0f;
  drawnSessionText = -1;
  drawnWeeklyText = -1;
  drawnSessionClock[0] = '\0';
  drawnWeeklyClock[0] = '\0';
  drawClocks(true);
  startAnimation();
}

// Plays a GIF that arrived on POST /gif, then hands the screen back.
void playPushedGif() {
  const uint8_t* data = otaserver.gifData();
  size_t length = otaserver.gifLength();
  uint32_t durationMs = otaserver.gifDurationMs();

  Serial.printf("Gif: playing %u bytes for %u ms, heap free %u, largest block %u\n",
                (unsigned)length, (unsigned)durationMs,
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());

  GIFDrawSetTFT(&ui.t);
  gif.begin(BIG_ENDIAN_PIXELS);
  ui.t.fillScreen(TFT_BLACK);

  unsigned long started = millis();
  unsigned long frames = 0;

  if (gif.open((uint8_t*)data, (int)length, GIFDraw)) {
    Serial.printf("Gif: opened %dx%d\n", gif.getCanvasWidth(),
                  gif.getCanvasHeight());

    // GIFDraw writes each line with setAddrWindow and pushPixels, which need an
    // open SPI transaction. Without this the frames decode but never reach the
    // panel, and the screen stays black.
    ui.t.startWrite();

    while (millis() - started < durationMs) {
      // playFrame does not pace itself when the delay pointer is null, so it
      // returns the frame delay and this waits for it.
      int frameDelay = 0;
      unsigned long frameStart = millis();
      if (gif.playFrame(false, &frameDelay)) {
        frames++;
        if (frameDelay < 20) frameDelay = 20;
        if (frameDelay > 500) frameDelay = 500;

        // Decoding and drawing a frame costs real time. Wait only the rest of
        // the frame delay, otherwise playback runs slower than it was encoded.
        unsigned long spent = millis() - frameStart;
        if ((unsigned long)frameDelay > spent) {
          delay(frameDelay - spent);
        }
      } else {
        gif.reset();  // loop until the time is up
        if (frames == 0) {
          Serial.println("Gif: no frames decoded, stopping");
          break;
        }
      }
      otaserver.handle(); // DO NOT EDIT
    }
    ui.t.endWrite();
    gif.close();
    Serial.printf("Gif: drew %lu frames in %lu ms\n", frames, millis() - started);
  } else {
    Serial.printf("Gif: could not decode the file, error %d\n",
                  gif.getLastError());
  }

  otaserver.gifRelease();
  restoreMetrics();
}

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------
void fetchUsageData() {
  if (serverUrl.length() == 0) {
    Serial.println("Usage: no server_url stored, run ./tools/dev config");
    fetchFailed = true;
    drawFetchState();
    return;
  }

  http.begin(serverUrl + "/api/usage");
  int httpResponseCode = http.GET();
  bool ok = false;

  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DeserializationError error = deserializeJson(json, payload);
    if (!error) {
      sessionPercent = constrain((int)(json["session"]["percent"] | 0), 0, 100);
      weeklyPercent = constrain((int)(json["weekly"]["percent"] | 0), 0, 100);
      sessionResetIn = (long)(json["session"]["resets_in"] | 0);
      weeklyResetIn = (long)(json["weekly"]["resets_in"] | 0);
      resetBase = millis();
      haveData = true;
      ok = true;
      startAnimation();
    } else {
      Serial.printf("Usage: bad JSON, %s\n", error.c_str());
    }
  } else {
    Serial.printf("Usage: HTTP %d\n", httpResponseCode);
  }

  http.end();

  fetchFailed = !ok;
  drawFetchState();
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(460800);
  Serial.println("Starting claude-usage app");
  otaserver.connectWiFi(); // DO NOT EDIT.
  otaserver.run(); // DO NOT EDIT

  ui.init();
  ui.clear();

  preferences.begin("app", true);
  serverUrl = preferences.getString("server_url");
  preferences.end();

  drawStatic();
  drawClocks(true);
}

void loop() {
  if ((WiFi.status() == WL_CONNECTED)) {
    otaserver.handle(); // DO NOT EDIT

    if (otaserver.gifReady()) {
      playPushedGif();
    }

    unsigned long now = millis();
    if (lastFetch == 0 || (now - lastFetch) > (unsigned long)refreshTimeInSeconds * 1000) {
      lastFetch = now;
      fetchUsageData();
    }

    if (animating) {
      stepAnimation();
    } else if (haveData) {
      stepPulse();
      if (now - lastClockTick > 1000) {
        lastClockTick = now;
        drawClocks(false);
      }
    }
  }

  delay(16);
}
