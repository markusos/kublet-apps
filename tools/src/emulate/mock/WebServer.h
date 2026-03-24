#pragma once
// Stub for OTAServer dependency
class WebServer {
public:
  WebServer(int = 80) {}
  void begin() {}
  void handleClient() {}
  void on(const char*, int, void(*)(), void(*)()) {}
  void on(const char*, void(*)()) {}
  void send(int, const char* = "", const char* = "") {}
  void sendHeader(const char*, const char*) {}
  bool hasArg(const char*) { return false; }
  class Upload { public: int status = 0; const uint8_t* buf = nullptr; size_t currentSize = 0; size_t totalSize = 0; };
  Upload upload() { return Upload(); }
};
