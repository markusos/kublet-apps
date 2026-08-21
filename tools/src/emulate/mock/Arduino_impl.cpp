#include "Arduino.h"
#include "SPI.h"

// ---------------------------------------------------------------------------
// Global instances
// ---------------------------------------------------------------------------
HardwareSerial Serial;
WiFiClass WiFi;
SPIClass SPI;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
static uint32_t _start_ticks = 0;

// A screenshot must show the same picture on every machine. With the real
// clock, an app draws whatever it reached in N wall seconds, so a slower host
// captures an earlier frame: a maze half carved instead of fully carved. The
// virtual clock advances only when the app asks time to pass, through delay(),
// or when the emulator finishes one pass of loop(). The app timeline then no
// longer depends on host speed.
static bool _virtual_clock = false;
static unsigned long _virtual_ms = 0;

void emuUseVirtualClock() { _virtual_clock = true; }
bool emuVirtualClock() { return _virtual_clock; }
void emuAdvanceClock(unsigned long ms) { _virtual_ms += ms; }

static void _ensure_sdl() {
  // SDL_Init is idempotent for subsystems already initialized
  if (_start_ticks == 0) {
    _start_ticks = SDL_GetTicks();
  }
}

unsigned long millis() {
  if (_virtual_clock) return _virtual_ms;
  _ensure_sdl();
  return SDL_GetTicks() - _start_ticks;
}

unsigned long micros() {
  return millis() * 1000;
}

// Defined in main_wrapper.cpp — renders framebuffer to screen + captures GIF frames
extern void _emu_yield_frame();

void yield() {
  // Pump SDL events so window stays responsive
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) exit(0);
  }
  // Render current framebuffer (enables GIF animation apps to show each frame)
  _emu_yield_frame();
}

// Defined in main_wrapper.cpp — pumps SDL events so button state updates during delays
extern void _emu_pump_events();

void delay(unsigned long ms) {
  if (_virtual_clock) {
    // Time passes for the app, not for the host. Every wait inside a read loop
    // still reaches its timeout, and the run finishes as fast as it computes.
    _virtual_ms += ms;
    _emu_pump_events();
    yield();
    return;
  }
  if (ms == 0) { yield(); return; }
  unsigned long end = SDL_GetTicks() + ms;
  while (SDL_GetTicks() < end) {
    _emu_pump_events();  // keep button state current during delay
    yield();
    SDL_Delay(1);
  }
}

void delayMicroseconds(unsigned int us) {
  if (us >= 1000) delay(us / 1000);
}

// ---------------------------------------------------------------------------
// GPIO
// ---------------------------------------------------------------------------
static bool _button_pressed = false;
static uint32_t _button_press_until = 0;  // minimum hold time (SDL ticks)
static const uint32_t BUTTON_MIN_HOLD_MS = 500;  // must exceed longest app loop period (e.g. 200ms delay + processing)

void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
void analogWrite(uint8_t, int) {}

// Defined in main_wrapper.cpp — checks scripted --button-at events
extern bool _emu_scripted_button_active();

int digitalRead(uint8_t pin) {
  // Button on GPIO 19: active LOW with pull-up
  if (pin == 19) {
    bool active = _button_pressed || SDL_GetTicks() < _button_press_until || _emu_scripted_button_active();
    return active ? 0 : 1;
  }
  return 1;  // pull-up default
}

void _emu_set_button_state(bool pressed) {
  if (pressed && !_button_pressed) {
    // Ensure button stays active for at least BUTTON_MIN_HOLD_MS
    // This survives the debounce delay(50) + second digitalRead in apps
    _button_press_until = SDL_GetTicks() + BUTTON_MIN_HOLD_MS;
  }
  _button_pressed = pressed;
}

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------
// The mock keeps its own generator state. The libc rand() state is shared with
// every other part of the process, including SDL and the notification listener
// thread, so an app that seeds from it drew a different colour or a different
// maze in about one run in four.
//
// A screenshot run starts from a fixed seed, so the captured frame is the same
// every time. An interactive run seeds from the clock, so a demo app still
// shows something new on each start.
static uint32_t _rng_state = 0;  // 0 means not seeded yet

static uint32_t _next_random() {
  if (_rng_state == 0) {
    _rng_state = _virtual_clock ? 1u : (uint32_t)time(nullptr);
    if (_rng_state == 0) _rng_state = 1;
  }
  // xorshift32
  _rng_state ^= _rng_state << 13;
  _rng_state ^= _rng_state >> 17;
  _rng_state ^= _rng_state << 5;
  return _rng_state;
}

uint32_t esp_random() { return _next_random(); }

long random(long max) {
  if (max <= 0) return 0;
  return (long)(_next_random() % (uint32_t)max);
}

long random(long min, long max) {
  if (min >= max) return min;
  return min + (long)(_next_random() % (uint32_t)(max - min));
}

void randomSeed(unsigned long seed) {
  _rng_state = (uint32_t)seed;
  if (_rng_state == 0) _rng_state = 1;
}
