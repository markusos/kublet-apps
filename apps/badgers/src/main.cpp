#include <Arduino.h>
#include <otaserver.h>
#include <AnimatedGIF.h>
#include <TFT_eSPI.h>

#include "badgers.h"
#include "globals.h"
#include "GIFDraw.h"

#define GIF_IMAGE badgers

OTAServer otaserver;
Preferences preferences;

unsigned long lastTime = 0;
unsigned long timerDelay = 600000;

TFT_eSPI tft = TFT_eSPI();
AnimatedGIF gif;

void setup()
{
  Serial.begin(460800);
  Serial.println("Starting app");

  otaserver.connectWiFi(); // DO NOT EDIT.
  otaserver.run();         // DO NOT EDIT

  tft.begin();
  tft.setRotation(0);        // Set the screen rotation
  tft.fillScreen(TFT_BLACK); // Clear the screen with black color

  gif.begin(BIG_ENDIAN_PIXELS);
}

void loop()
{
  if ((WiFi.status() == WL_CONNECTED))
  {
    otaserver.handle(); // DO NOT EDIT

    if (gif.open((uint8_t *)GIF_IMAGE, sizeof(GIF_IMAGE), GIFDraw))
    {
      tft.startWrite();
      while (gif.playFrame(true, NULL))
      {
        yield();
      }
      Serial.println("Badger Badger Badger...");
      gif.close();
      tft.endWrite();
    }
  }
}
