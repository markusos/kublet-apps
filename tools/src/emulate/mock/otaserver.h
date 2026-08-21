#pragma once

#include "Arduino.h"
#include "ArduinoJSON.h"
#include <fstream>
#include <sstream>
#include <string>

#ifndef EMU_APP_DIR
#define EMU_APP_DIR ""
#endif

// Stub Preferences — loads defaults from assets/preferences.json
class Preferences {
public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  String getString(const char* key, const String& def = "") {
    std::string appDir = EMU_APP_DIR;
    if (appDir.empty()) return def;

    std::ifstream f(appDir + "/assets/preferences.json");
    if (!f.is_open()) return def;

    std::ostringstream ss;
    ss << f.rdbuf();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, ss.str());
    if (err) {
      printf("[EMU] WARNING: Failed to parse preferences.json: %s\n", err.c_str());
      return def;
    }

    const char* val = doc[key];
    if (!val) {
      printf("[EMU] WARNING: Preference key '%s' not found in preferences.json\n", key);
      return def;
    }
    return String(val);
  }
  unsigned long getULong(const char* key, unsigned long def = 0) {
    String val = getString(key, "");
    if (val.length() == 0) return def;
    return strtoul(val.c_str(), nullptr, 10);
  }
  void putString(const char*, const String&) {}
  void putULong(const char*, unsigned long) {}
};

inline Preferences pref;

// Forward-declare the global server from WebServer.h
class WebServer;
extern WebServer server;

#define OTA_GIF_MAX_BYTES (80 * 1024)

// Mock of the pushed-GIF channel. Set KUBLET_EMU_GIF to a file path and the
// emulator hands that GIF to the app once, the same way a real push does.
inline uint8_t* _emu_gif_buffer = nullptr;
inline size_t _emu_gif_length = 0;
inline bool _emu_gif_loaded = false;

class OTAServer {
public:
  void init() { Serial.println("[EMU] OTAServer init (no-op)"); }
  void start() {}
  void run();  // defined in WebServer_impl.cpp — starts TCP listener
  void handle();  // defined in WebServer_impl.cpp — calls server.handleClient()
  void stop() {}
  void connectWiFi() { Serial.println("[EMU] WiFi simulated — connected"); }

  bool gifReady() {
    if (_emu_gif_loaded) return _emu_gif_buffer != nullptr;
    _emu_gif_loaded = true;

    const char* path = getenv("KUBLET_EMU_GIF");
    if (!path || !*path) return false;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
      printf("[EMU] WARNING: cannot open KUBLET_EMU_GIF '%s'\n", path);
      return false;
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (size <= 0 || size > OTA_GIF_MAX_BYTES) {
      printf("[EMU] WARNING: gif is %lld bytes, limit is %d\n",
             (long long)size, OTA_GIF_MAX_BYTES);
      return false;
    }

    _emu_gif_buffer = (uint8_t*)malloc((size_t)size);
    if (!_emu_gif_buffer) return false;
    f.read((char*)_emu_gif_buffer, size);
    _emu_gif_length = (size_t)size;
    printf("[EMU] Pushed gif loaded: %s (%lld bytes)\n", path, (long long)size);
    return true;
  }

  const uint8_t* gifData() { return _emu_gif_buffer; }
  size_t gifLength() { return _emu_gif_length; }
  uint32_t gifDurationMs() {
    const char* s = getenv("KUBLET_EMU_GIF_SECONDS");
    if (s && *s) {
      long seconds = strtol(s, nullptr, 10);
      if (seconds > 0 && seconds <= 120) return (uint32_t)seconds * 1000;
    }
    return 12000;
  }
  void gifRelease() {
    if (_emu_gif_buffer) {
      free(_emu_gif_buffer);
      _emu_gif_buffer = nullptr;
    }
    _emu_gif_length = 0;
  }
};

