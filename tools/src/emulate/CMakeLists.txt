cmake_minimum_required(VERSION 3.16)
project(kublet-emulate CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# App directory — set by the build script: cmake -DAPP_DIR=.../apps/hello
set(APP_DIR "" CACHE PATH "Absolute path to the app directory")
if(NOT APP_DIR OR NOT EXISTS "${APP_DIR}/src/main.cpp")
  message(FATAL_ERROR "APP_DIR must point to a valid app directory (got: '${APP_DIR}')")
endif()

# Paths
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(KGFX_DIR "${REPO_ROOT}/lib/KGFX")
set(GIFDRAW_DIR "${REPO_ROOT}/lib/GIFDraw")
set(ANIMGIF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/AnimatedGIF")

# Find SDL2
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sdl2")
find_package(SDL2 REQUIRED)

# Mock headers take priority over everything
include_directories(BEFORE SYSTEM
  "${CMAKE_CURRENT_SOURCE_DIR}/mock"
)

# Vendored AnimatedGIF (must come after mock but before system includes)
include_directories("${ANIMGIF_DIR}")

# KGFX, GIFDraw, app source headers
include_directories(
  "${KGFX_DIR}"
  "${GIFDRAW_DIR}"
  "${APP_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

# --- Detect which optional libraries the app needs ---
file(READ "${APP_DIR}/src/main.cpp" APP_SOURCE)

# Base sources (always compiled)
set(SOURCES
  main_wrapper.cpp
  mock/TFT_eSPI_impl.cpp
  mock/Arduino_impl.cpp
  mock/WebServer_impl.cpp
  "${APP_DIR}/src/main.cpp"
  "${KGFX_DIR}/kgfx.cpp"
  "${KGFX_DIR}/font_Arial.c"
  "${KGFX_DIR}/font_ArialBold.c"
)

# AnimatedGIF + GIFDraw (if app uses them)
if(APP_SOURCE MATCHES "#include.*<AnimatedGIF.h>")
  list(APPEND SOURCES
    "${ANIMGIF_DIR}/AnimatedGIF.cpp"
    "${GIFDRAW_DIR}/GIFDraw.cpp"
  )
  message(STATUS "App uses AnimatedGIF — adding GIF support")
endif()

# TJpg_Decoder (if app uses it)
if(APP_SOURCE MATCHES "#include.*<TJpg_Decoder.h>")
  list(APPEND SOURCES mock/TJpg_Decoder_impl.cpp)
  message(STATUS "App uses TJpg_Decoder — adding JPEG support")
endif()

add_executable(emulate ${SOURCES})

target_include_directories(emulate PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(emulate SDL2::SDL2)

# Pass APP_DIR as compile definition for HTTP fixture loading
target_compile_definitions(emulate PRIVATE
  EMU_APP_DIR="${APP_DIR}"
)

# Force-include Arduino.h for C++ files (Arduino framework does this automatically)
# Note: .c font files must NOT get this (they can't include C++ headers)
set(CPP_SOURCES
  main_wrapper.cpp
  mock/TFT_eSPI_impl.cpp
  mock/Arduino_impl.cpp
  mock/WebServer_impl.cpp
  "${APP_DIR}/src/main.cpp"
  "${KGFX_DIR}/kgfx.cpp"
)

# Add GIFDraw.cpp to force-include list if used
if(APP_SOURCE MATCHES "#include.*<AnimatedGIF.h>")
  list(APPEND CPP_SOURCES "${GIFDRAW_DIR}/GIFDraw.cpp")
endif()

set_source_files_properties(${CPP_SOURCES}
  PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/mock/Arduino.h"
)

# Suppress warnings from app code and KGFX (they're written for ESP32/Arduino)
target_compile_options(emulate PRIVATE
  -Wno-unused-variable
  -Wno-unused-function
  -Wno-sign-compare
  -Wno-missing-field-initializers
  -Wno-deprecated-declarations
)
