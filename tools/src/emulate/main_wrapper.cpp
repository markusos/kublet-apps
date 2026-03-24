#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "mock/Arduino.h"
#include "mock/TFT_eSPI.h"

// stb_image_write for screenshots
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ---------------------------------------------------------------------------
// App entry points (defined in the app's main.cpp)
// ---------------------------------------------------------------------------
extern void setup();
extern void loop();

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static SDL_Window*   g_window   = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture*  g_texture  = nullptr;
static int g_scale = 2;
static bool g_running = true;

static const char* g_screenshot_path = nullptr;
static int g_screenshot_after_frames = 0;  // 0 = don't auto-screenshot
static int g_frame_count = 0;

// GIF capture state
static const char* g_gif_path = nullptr;
static const char* g_gif_frame_dir = nullptr;
static int g_gif_frames = 0;
static int g_gif_start_frame = 0;    // skip initial frames (setup)
static int g_gif_frame_interval = 1; // capture every Nth frame
static int g_gif_captured = 0;

// Scripted button presses: times in milliseconds {start_ms, duration_ms}
struct ButtonEvent { uint32_t start_ms; uint32_t duration_ms; };
static ButtonEvent g_button_events[32];
static int g_button_event_count = 0;

// Called from digitalRead to check if a scripted button press is active
bool _emu_scripted_button_active() {
  uint32_t now = SDL_GetTicks();
  for (int i = 0; i < g_button_event_count; i++) {
    uint32_t start = g_button_events[i].start_ms;
    uint32_t end = start + g_button_events[i].duration_ms;
    if (now >= start && now < end) {
      return true;
    }
  }
  return false;
}

// Display registration — TFT_eSPI::begin() calls this to register itself.
// We keep only the first one (KGFX::t), not sprites.
static TFT_eSPI* g_display = nullptr;

void _emu_register_display(void* tft) {
  if (!g_display) g_display = static_cast<TFT_eSPI*>(tft);
}

// Forward declarations for yield-based frame rendering
static void fb565_to_rgb888(const uint16_t* src, uint8_t* dst, int w, int h);
static void captureGifFrame(const uint16_t* fb);

// ---------------------------------------------------------------------------
// Yield-based frame rendering (for GIF animation apps that decode in a loop)
// ---------------------------------------------------------------------------
static uint8_t* g_yield_rgb_buf = nullptr;
static bool g_yield_rendered = false;  // true if yield rendered this loop iteration

void _emu_yield_frame() {
  if (!g_display || !g_renderer || !g_texture) return;
  g_yield_rendered = true;

  // Lazy-allocate buffer
  if (!g_yield_rgb_buf) g_yield_rgb_buf = new uint8_t[240 * 240 * 3];

  // Render to screen
  fb565_to_rgb888(g_display->getFramebuffer(), g_yield_rgb_buf, 240, 240);
  SDL_UpdateTexture(g_texture, nullptr, g_yield_rgb_buf, 240 * 3);
  SDL_RenderClear(g_renderer);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
  SDL_RenderPresent(g_renderer);

  g_frame_count++;

  // GIF frame capture during yield
  if (g_gif_path && g_gif_frame_dir &&
      g_frame_count >= g_gif_start_frame && g_gif_captured < g_gif_frames) {
    int relative_frame = g_frame_count - g_gif_start_frame;
    if (relative_frame % g_gif_frame_interval == 0) {
      captureGifFrame(g_display->getFramebuffer());
    }
    if (g_gif_captured >= g_gif_frames) {
      printf("[EMU] Captured %d GIF frames\n", g_gif_captured);
      g_running = false;
    }
  }
}

// ---------------------------------------------------------------------------
// RGB565 → RGB888 conversion for SDL
// ---------------------------------------------------------------------------
static void fb565_to_rgb888(const uint16_t* src, uint8_t* dst, int w, int h) {
  for (int i = 0; i < w * h; i++) {
    uint16_t c = src[i];
    dst[i*3 + 0] = ((c >> 11) & 0x1F) * 255 / 31;  // R
    dst[i*3 + 1] = ((c >>  5) & 0x3F) * 255 / 63;  // G
    dst[i*3 + 2] = ( c        & 0x1F) * 255 / 31;  // B
  }
}

// ---------------------------------------------------------------------------
// Screenshot
// ---------------------------------------------------------------------------
static void takeScreenshot(const char* path, const uint16_t* fb) {
  uint8_t* rgb = new uint8_t[240 * 240 * 3];
  fb565_to_rgb888(fb, rgb, 240, 240);
  if (stbi_write_png(path, 240, 240, 3, rgb, 240 * 3)) {
    printf("[EMU] Screenshot saved: %s\n", path);
  } else {
    printf("[EMU] Failed to save screenshot: %s\n", path);
  }
  delete[] rgb;
}

// ---------------------------------------------------------------------------
// GIF frame capture
// ---------------------------------------------------------------------------
static void captureGifFrame(const uint16_t* fb) {
  char path[512];
  snprintf(path, sizeof(path), "%s/frame_%05d.png", g_gif_frame_dir, g_gif_captured);

  uint8_t* rgb = new uint8_t[240 * 240 * 3];
  fb565_to_rgb888(fb, rgb, 240, 240);
  stbi_write_png(path, 240, 240, 3, rgb, 240 * 3);
  delete[] rgb;

  g_gif_captured++;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  // Parse arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--scale") == 0 && i+1 < argc) {
      g_scale = atoi(argv[++i]);
      if (g_scale < 1) g_scale = 1;
      if (g_scale > 4) g_scale = 4;
    } else if (strcmp(argv[i], "--screenshot") == 0 && i+1 < argc) {
      g_screenshot_path = argv[++i];
      g_screenshot_after_frames = 60;
    } else if (strcmp(argv[i], "--frames") == 0 && i+1 < argc) {
      g_screenshot_after_frames = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--gif") == 0 && i+1 < argc) {
      g_gif_path = argv[++i];
    } else if (strcmp(argv[i], "--gif-frames") == 0 && i+1 < argc) {
      g_gif_frames = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--gif-frame-dir") == 0 && i+1 < argc) {
      g_gif_frame_dir = argv[++i];
    } else if (strcmp(argv[i], "--gif-start") == 0 && i+1 < argc) {
      g_gif_start_frame = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--gif-interval") == 0 && i+1 < argc) {
      g_gif_frame_interval = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--button-at") == 0 && i+1 < argc) {
      // Parse comma-separated "seconds[:duration_ms]" pairs
      // e.g. "1.5" or "1.5:200" or "1,3:200,5"
      char* spec = strdup(argv[++i]);
      char* tok = strtok(spec, ",");
      while (tok && g_button_event_count < 32) {
        float secs = 0; int dur_ms = 200; // default 200ms press
        if (char* colon = strchr(tok, ':')) {
          *colon = '\0';
          secs = atof(tok);
          dur_ms = atoi(colon + 1);
        } else {
          secs = atof(tok);
        }
        g_button_events[g_button_event_count++] = {(uint32_t)(secs * 1000), (uint32_t)dur_ms};
        tok = strtok(nullptr, ",");
      }
      free(spec);
    }
  }

  // Default GIF settings
  if (g_gif_path && g_gif_frames == 0) g_gif_frames = 120;

  // Init SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  int winW = 240 * g_scale;
  int winH = 240 * g_scale;

  g_window = SDL_CreateWindow("Kublet Emulator",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    winW, winH, SDL_WINDOW_SHOWN);
  if (!g_window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }

  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!g_renderer) {
    g_renderer = SDL_CreateRenderer(g_window, -1, 0);
  }

  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24,
    SDL_TEXTUREACCESS_STREAMING, 240, 240);

  printf("[EMU] Kublet Emulator started (%dx scale)\n", g_scale);
  printf("[EMU] Keys: Space=Button, S=Screenshot, Q=Quit\n");

  // Run app setup
  setup();

  if (!g_display) {
    fprintf(stderr, "[EMU] Warning: no display registered. Did the app call ui.init()?\n");
  }

  // Render buffer
  uint8_t* rgb_buf = new uint8_t[240 * 240 * 3];

  // Main loop
  while (g_running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          g_running = false;
          break;
        case SDL_KEYDOWN:
          if (e.key.keysym.sym == SDLK_q) g_running = false;
          else if (e.key.keysym.sym == SDLK_SPACE) _emu_set_button_state(true);
          else if (e.key.keysym.sym == SDLK_s && g_display) {
            takeScreenshot("screenshot.png", g_display->getFramebuffer());
          }
          break;
        case SDL_KEYUP:
          if (e.key.keysym.sym == SDLK_SPACE) _emu_set_button_state(false);
          break;
      }
    }

    // Run one iteration of the app loop
    g_yield_rendered = false;
    loop();

    // Render framebuffer to screen (skip if yield() already rendered)
    if (!g_yield_rendered && g_display) {
      fb565_to_rgb888(g_display->getFramebuffer(), rgb_buf, 240, 240);
      SDL_UpdateTexture(g_texture, nullptr, rgb_buf, 240 * 3);
      SDL_RenderClear(g_renderer);
      SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
      SDL_RenderPresent(g_renderer);
    }

    if (!g_yield_rendered) g_frame_count++;

    // Auto-screenshot mode
    if (g_screenshot_path && g_screenshot_after_frames > 0 &&
        g_frame_count >= g_screenshot_after_frames && g_display) {
      takeScreenshot(g_screenshot_path, g_display->getFramebuffer());
      g_running = false;
    }

    // GIF frame capture (skip if yield() already handled it)
    if (!g_yield_rendered && g_gif_path && g_gif_frame_dir && g_display &&
        g_frame_count >= g_gif_start_frame) {
      int relative_frame = g_frame_count - g_gif_start_frame;
      if (relative_frame % g_gif_frame_interval == 0) {
        captureGifFrame(g_display->getFramebuffer());
      }
      if (g_gif_captured >= g_gif_frames) {
        printf("[EMU] Captured %d GIF frames\n", g_gif_captured);
        g_running = false;
      }
    }
  }

  delete[] rgb_buf;
  delete[] g_yield_rgb_buf;
  g_yield_rgb_buf = nullptr;
  SDL_DestroyTexture(g_texture);
  SDL_DestroyRenderer(g_renderer);
  SDL_DestroyWindow(g_window);
  SDL_Quit();

  return 0;
}
