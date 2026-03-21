# Badgers

This app shows an animated gif of [Badger Badger Badger](https://en.wikipedia.org/wiki/Badgers_(animation))

Dependency: https://github.com/bitbank2/AnimatedGIF

Due to the Kublet display refresh rate the gif is not as smoth as the gif here:

![Badger Badger Badger](assets/badgers.gif?raw=true "Badgers")

To replace with your own gif, create a 240x240 px gif and use a tool like [image_to_c](https://github.com/bitbank2/image_to_c) to convert it to c code, like in `apps/badgers/src/badgers.h`

## Dependencies

```
bodmer/TFT_eSPI@^2.5.0
kublet/OTAServer@^1.0.4
bitbank2/AnimatedGIF@^2.1.1
```

## Build & Deploy

```bash
./tools/dev build badgers       # Compile
./tools/dev deploy badgers      # OTA deploy to device
./tools/dev init                # First-time USB flash + WiFi setup
./tools/dev logs                # Stream serial output
```

Serial output prints `Badger Badger Badger...` for each render cycle.