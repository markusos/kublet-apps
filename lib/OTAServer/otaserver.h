#pragma once

#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include <WiFi.h>
#include <Preferences.h>

inline Preferences pref;

// Largest GIF the device accepts on POST /gif. The whole file sits in heap
// next to WiFi and the display driver while it plays.
//
// Measured on hardware before this allocation: 198 KB free, but only 110 KB in
// one contiguous block, and malloc needs contiguous memory. 96 KB fits with
// room left for the decoder's own buffers.
#define OTA_GIF_MAX_BYTES (96 * 1024)

class OTAServer {
  private:

  public:
    void init();
    void start();
    void run();
    void handle();
    void stop();

    void connectWiFi();

    // A pushed GIF waits here until the app collects it. The app owns the
    // playback, because only the app knows how to restore its own screen.
    bool gifReady();
    const uint8_t* gifData();
    size_t gifLength();
    uint32_t gifDurationMs();
    void gifRelease();
};
