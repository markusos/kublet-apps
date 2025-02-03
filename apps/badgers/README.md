# Badgers

This app shows an animated gif of [Badger Badger Badger](https://en.wikipedia.org/wiki/Badgers_(animation))

Dependency: https://github.com/bitbank2/AnimatedGIF

Due to the Kublet display refresh rate the gif is not as smoth as the gif here:

![Badger Badger Badger](assets/badgers.gif?raw=true "Badgers")

To replace with your own gif, create a 240x240 px gif and use a tool like [image_to_c](https://github.com/bitbank2/image_to_c) to convert it to c code, like in `apps/badgers/src/badgers.h`

# Deploy

Load the develop app onto the kublet

Then from your PlatformIO terminal session, run

```
krate send <kublet_ip>
```

To monitor output run:

```
krate monitor
```

If connected it should print

```
Badger Badger Badger...
```
for each render cycle