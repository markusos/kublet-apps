#pragma once

#include "Arduino.h"
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
    // Try loading from assets/preferences.json
    std::string appDir = EMU_APP_DIR;
    if (appDir.empty()) return def;

    std::ifstream f(appDir + "/assets/preferences.json");
    if (!f.is_open()) return def;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    // Simple JSON key lookup: "key": "value"
    std::string needle = std::string("\"") + key + "\"";
    size_t ki = content.find(needle);
    if (ki == std::string::npos) return def;
    size_t colon = content.find(':', ki + needle.size());
    if (colon == std::string::npos) return def;
    size_t q1 = content.find('"', colon + 1);
    if (q1 == std::string::npos) return def;
    size_t q2 = content.find('"', q1 + 1);
    if (q2 == std::string::npos) return def;
    return String(content.substr(q1 + 1, q2 - q1 - 1).c_str());
  }
  void putString(const char*, const String&) {}
};

inline Preferences pref;

// Forward-declare the global server from WebServer.h
class WebServer;
extern WebServer server;

class OTAServer {
public:
  void init() { Serial.println("[EMU] OTAServer init (no-op)"); }
  void start() {}
  void run();  // defined in WebServer_impl.cpp — starts TCP listener
  void handle();  // defined in WebServer_impl.cpp — calls server.handleClient()
  void stop() {}
  void connectWiFi() { Serial.println("[EMU] WiFi simulated — connected"); }
};

