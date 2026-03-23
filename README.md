## Pocket Hero - ESP32 Handheld RPG

A scrolling RPG game built for ESP32 + ST7735 1.8" SPI TFT display (128×160), designed as a physical handheld device.

## Hardware
- ESP32D dev board
- KMR-1.8 SPI ST7735 128×160 TFT display
- 6 push buttons (directional + A/B)
- Built and flashed with PlatformIO

## Features
- Smooth 8-directional scrolling movement with diagonal speed normalization
- Camera follow system clamped to world edges
- 5-layer rendering pipeline:
  - **Layer 1** — Opaque ground tiles
  - **Layer 2** — Transparent overlay tiles (plants, fences, structures) rendered behind the player
  - **Layer 3** — Animated player sprite (8 directions, 6-frame walk cycle)
  - **Layer 4** — Transparent foreground tiles rendered in front of the player (arch rafters, roofs)
  - **Layer 5** — Collision map (invisible, blocks movement independently of graphics)
- Axis-separated collision so the player slides along walls
- Tile-based world map with manual collision layer for overlay objects

## Asset Pipeline
Python converters transform source art into C headers stored in flash (PROGMEM):
- `gif_to_rgb565.py` — Converts Pixellab AI walk cycle GIFs to `sprites.h`
- `png_to_tiles.py` — Converts ground tile PNGs to `tiles.h`
- `png_to_overlay.py` — Converts transparent overlay PNGs to `overlay_tiles.h`
- `png_to_foreground.py` — Converts transparent foreground PNGs to `foreground_tiles.h`

## Dependencies
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
- PlatformIO with espressif32 platform
- Python + Pillow (for asset conversion)
