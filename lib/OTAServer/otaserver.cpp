#include "otaserver.h"

WebServer server(80);

static volatile bool _otaPendingRestart = false;
static unsigned long _otaRestartRequestedAt = 0;

/* NVS namespace that apps read their config from, and the NVS key size limit */
static const char* CONFIG_NAMESPACE = "app";
static const unsigned int NVS_KEY_MAX_LENGTH = 15;

/***************************************************************************************
** Function name:           init
** Description:             Initialize OTA server and endpoint
***************************************************************************************/
void OTAServer::init() {
if (!MDNS.begin("esp32")) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  /* health check endpoint for deploy tool verification */
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
  });

  /* Collect custom header for firmware MD5 verification */
  const char* headerKeys[] = {"X-Firmware-MD5"};
  server.collectHeaders(headerKeys, 1);

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
      // Set expected MD5 if provided by deploy tool
      if (server.hasHeader("X-Firmware-MD5")) {
        String md5 = server.header("X-Firmware-MD5");
        if (md5.length() == 32) {
          Update.setMD5(md5.c_str());
          Serial.printf("OTA: MD5 verification enabled: %s\n", md5.c_str());
        }
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        _otaPendingRestart = true;
        _otaRestartRequestedAt = millis();
      } else {
        Serial.println("OTA: Update FAILED (possible MD5 mismatch or write error)");
        Update.printError(Serial);
      }
    }
  });

  /* read the app config, so a client can verify what the device stores */
  server.on("/config", HTTP_GET, []() {
    pref.begin(CONFIG_NAMESPACE, true);
    String serverUrl = pref.getString("server_url", "");
    pref.end();
    server.send(200, "application/json", "{\"server_url\":\"" + serverUrl + "\"}");
  });

  /* write the app config over WiFi, so a USB re-init is not needed */
  server.on("/config", HTTP_POST, []() {
    int written = 0;
    int skipped = 0;
    bool restart = true;

    pref.begin(CONFIG_NAMESPACE, false);
    for (int i = 0; i < server.args(); i++) {
      String key = server.argName(i);
      String value = server.arg(i);

      if (key == "restart") {
        restart = !(value == "0" || value == "false");
        continue;
      }
      if (key.length() == 0 || key.length() > NVS_KEY_MAX_LENGTH) {
        Serial.printf("Config: skipped invalid key '%s'\n", key.c_str());
        skipped++;
        continue;
      }

      pref.putString(key.c_str(), value);
      Serial.printf("Config: %s = %s\n", key.c_str(), value.c_str());
      written++;
    }
    pref.end();

    server.sendHeader("Connection", "close");
    if (written == 0) {
      server.send(400, "text/plain", "no valid keys");
      return;
    }

    Serial.printf("Config: wrote %d key(s), skipped %d\n", written, skipped);
    server.send(200, "text/plain", restart ? "OK restarting" : "OK");

    /* apps read NVS in setup(), so a restart applies the new values */
    if (restart) {
      _otaPendingRestart = true;
      _otaRestartRequestedAt = millis();
    }
  });
}

/***************************************************************************************
** Function name:           start
** Description:             Start OTA server
***************************************************************************************/
void OTAServer::start() {
  server.begin();
}

/***************************************************************************************
** Function name:           run
** Description:             Initialize and start OTA server
***************************************************************************************/
void OTAServer::run() {
  init();
  start();
}

/***************************************************************************************
** Function name:           handle
** Description:             Handle incoming connections from client sending OTA firmware
***************************************************************************************/
void OTAServer::handle() {
  server.handleClient();
  if (_otaPendingRestart && (millis() - _otaRestartRequestedAt > 1000)) {
    Serial.println("OTA: deferred restart");
    delay(100);
    ESP.restart();
  }
}

/***************************************************************************************
** Function name:           stop
** Description:             Stop OTA server
***************************************************************************************/
void OTAServer::stop() {
  server.stop();
}

/***************************************************************************************
** Function name:           connectWifi
** Description:             Connects to WiFi
***************************************************************************************/
void OTAServer::connectWiFi() {
  pref.begin("core");
  std::string ssid = pref.getString("ssid").c_str();
  std::string pw = pref.getString("pw").c_str();
  pref.end();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pw.c_str());
  Serial.print("Connected to ");
  Serial.println(ssid.c_str());
  while((WiFi.status() != WL_CONNECTED)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
}
