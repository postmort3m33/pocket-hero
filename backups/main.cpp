// ─────────────────────────────────────────────────────────────────────────────
//  Scrolling RPG -- ESP32 + ST7735 128x160 (rotation 1 = landscape 160x128)
//
//  5-LAYER SYSTEM
//  ══════════════
//  Layer 1 : WORLD     -- Ground tiles       (opaque, rendered first)
//  Layer 2 : WORLD2    -- Overlay tiles      (transparent, rendered BEHIND sprite)
//  Layer 3 : SPRITE    -- Player character   (transparent, rendered above layers 1-2)
//  Layer 4 : WORLD3    -- Foreground tiles   (transparent, rendered IN FRONT of sprite)
//  Layer 5 : WORLD_COL -- Collision map      (not rendered, blocks player movement)
//
//  Render order : Layer 1 -> Layer 2 -> Layer 3 -> Layer 4
//  Collision    : Layer 1 tileWalkable[] + Layer 5 WORLD_COL
//
//  All 4 rendered layers share a single drawTile() core function.
//  Pixels matching the transparent key (0xF81F) are skipped on every layer.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "sprites.h"
#include "tiles.h"
#include "overlay_tiles.h"
#include "foreground_tiles.h"
#include "input.h"
#include "stats.h"
#include "ui.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Screen / map constants
// ─────────────────────────────────────────────────────────────────────────────
#define SCR_W     160
#define SCR_H     128
#define MAP_W      16
#define MAP_H      16

#define COL_W      14   // player collision box width
#define COL_H       8   // player collision box height (feet area)

#define PLAYER_SPD  1.5f
#define DIAG_SPD    1.06f   // PLAYER_SPD / sqrt(2)
#define ANIM_MS     130

// ─────────────────────────────────────────────────────────────────────────────
//  Direction enum -- matches spriteFrames[] order in sprites.h
// ─────────────────────────────────────────────────────────────────────────────
enum Dir : uint8_t {
    DIR_SOUTH = 0, DIR_NORTH = 1,
    DIR_EAST  = 2, DIR_WEST  = 3,
    DIR_SE    = 4, DIR_SW    = 5,
    DIR_NE    = 6, DIR_NW    = 7,
};

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 1 -- WORLD (ground tiles, fully opaque)
//  Rendered first. Tile IDs reference tileData[] in tiles.h.
//  Walkability defined by tileWalkable[] in tiles.h.
//
//  0=GRASS1  1=GRASS2  2=GRASS3  3=GRASS4
//  4=GRASS_FLOWER1  5=GRASS_FLOWER2  6=GRASS_FLOWER3  7=GRASS_FLOWER4
// ─────────────────────────────────────────────────────────────────────────────
static const uint8_t WORLD[MAP_H][MAP_W] PROGMEM = {
//   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    { 0,  1,  2,  0,  1,  3,  0,  2,  1,  0,  3,  1,  0,  2,  1,  0},  // 0
    { 3,  0,  4,  5,  4,  1,  3,  0,  2,  1,  0,  3,  2,  0,  1,  2},  // 1
    { 1,  2,  5,  6,  5,  4,  0,  0,  1,  2,  0,  1,  3,  0,  2,  1},  // 2
    { 0,  3,  4,  7,  6,  5,  4,  0,  0,  1,  2,  0,  4,  5,  0,  2},  // 3
    { 2,  0,  0,  4,  5,  0,  1,  0,  3,  0,  1,  4,  5,  6,  4,  0},  // 4
    { 1,  3,  0,  2,  0,  0,  1,  2,  0,  3,  0,  5,  7,  5,  0,  3},  // 5
    { 0,  1,  2,  0,  3,  1,  0,  2,  1,  0,  3,  4,  5,  4,  0,  1},  // 6
    { 3,  0,  1,  3,  0,  2,  4,  5,  4,  1,  0,  2,  0,  3,  1,  2},  // 7
    { 1,  2,  0,  1,  2,  0,  5,  6,  5,  2,  4,  5,  4,  0,  2,  0},  // 8
    { 0,  1,  3,  0,  1,  2,  4,  7,  4,  0,  5,  6,  5,  1,  0,  3},  // 9
    { 2,  0,  1,  2,  0,  3,  1,  4,  0,  1,  4,  7,  4,  0,  1,  2},  // 10
    { 0,  3,  0,  4,  5,  4,  2,  1,  0,  3,  0,  0,  1,  2,  0,  1},  // 11
    { 1,  0,  2,  5,  6,  5,  0,  3,  1,  0,  2,  1,  0,  4,  5,  0},  // 12
    { 3,  1,  0,  4,  7,  4,  2,  0,  3,  2,  0,  1,  3,  5,  6,  4},  // 13
    { 0,  2,  1,  0,  4,  0,  3,  1,  0,  1,  3,  0,  2,  4,  7,  5},  // 14
    { 1,  0,  3,  1,  0,  2,  1,  0,  2,  0,  1,  3,  0,  1,  4,  0},  // 15
};

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 2 -- WORLD2 (overlay tiles, transparent, rendered BEHIND sprite)
//  Use for: flowers, plants, fence sections, lower arch sections.
//  Tile IDs reference overlayTileData[] in overlay_tiles.h.
//  0 = empty (nothing drawn)
//
//  1=FLOWER_WHITE1        2=FLOWER_WHITE2        3=FLOWER_WHITE3
//  4=FLOWER_YELLOW1       5=FLOWER_YELLOW2       6=FLOWER_YELLOW3
//  7=PLANT_GREEN1         8=PLANT_GREEN2         9=PLANT_GREEN3
//  10=PLANT_GREEN4
//  11=WOODARCH_FRONT_OVERLAY1  12=WOODARCH_FRONT_OVERLAY2  (collision portion of arch)
//  13=WOODARCH_FRONT_OVERLAY3  14=WOODARCH_FRONT_OVERLAY4  (Row: 11,12,13,14)
//  15=FENCE_BACK_LEFT_CORNER   16=FENCE_BACK_RIGHT_CORNER
//  17=FENCE_FRONT_LEFT_CORNER  18=FENCE_FRONT_RIGHT_CORNER
//  19=FENCE_LEFT_CONNECT       20=FENCE_LEFT_TOPBOTTOM_CONNECT
//  21=FENCE_LEFTRIGHT_CONNECT  22=FENCE_RIGHT_CONNECT
//  23=FENCE_RIGHT_TOPBOTTOM_CONNECT
//  24=WOODARCH_FRONT_OVERLAY1_FENCE_CONNECT
//  25=WOODARCH_FRONT_OVERLAY4_FENCE_CONNECT
// ─────────────────────────────────────────────────────────────────────────────
static const uint8_t WORLD2[MAP_H][MAP_W] PROGMEM = {
//   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 0
    { 0,  0,  0,  7,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0},  // 1
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0},  // 2
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 3
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  9,  0,  0},  // 4
    { 0,  0,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 5
    { 0,  0,  0, 15, 21, 21, 24, 12, 13, 25, 21, 21, 16,  0,  0,  5},  // 6
    { 0,  0,  0, 20,  0,  0,  0,  0,  0,  0,  8,  0, 23,  0,  0,  0},  // 7
    { 0,  0,  0, 20,  0,  0,  3,  0,  0,  0,  0,  0, 23,  0,  0,  0},  // 8
    { 0,  6,  0, 20,  0,  0,  0,  0,  0,  0,  0,  0, 23,  0,  0,  0},  // 9
    { 0,  0,  0, 20,  0,  0,  0,  0,  0,  0,  0,  0, 23,  0,  2,  0},  // 10
    { 0,  0,  0, 20,  0,  0,  0,  0,  0,  7,  0,  0, 23,  0,  0,  0},  // 11
    { 0,  0,  0, 17, 21, 21, 24, 12, 13, 25, 21, 21, 18,  0,  0,  0},  // 12
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4},  // 13
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  9,  0,  0,  0,  0},  // 14
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 15
};

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 3 -- SPRITE (player character)
//  Not a map array -- drawn procedurally from sprites.h.
//  Rendered above Layers 1 and 2, below Layer 4.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 4 -- WORLD3 (foreground tiles, transparent, rendered IN FRONT of sprite)
//  Use for: arch rafters, roof overhangs, bridge tops, anything above the player.
//  Tile IDs reference fgTileData[] in foreground_tiles.h.
//  0 = empty (nothing drawn)
//
//  1=WOODARCH_FRONT_FOREGROUND1  2=WOODARCH_FRONT_FOREGROUND2
//  3=WOODARCH_FRONT_FOREGROUND3  4=WOODARCH_FRONT_FOREGROUND4
//  5=WOODARCH_FRONT_FOREGROUND5  6=WOODARCH_FRONT_FOREGROUND6
//  7=WOODARCH_FRONT_FOREGROUND7  8=WOODARCH_FRONT_FOREGROUND8
//  (Arch format: Row1=1,2,3,4  Row2=5,6,7,8)
// ─────────────────────────────────────────────────────────────────────────────
static const uint8_t WORLD3[MAP_H][MAP_W] PROGMEM = {
//    0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 0
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 1
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 2
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 3
    { 0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  0,  0,  0,  0,  0,  0},  // 4
    { 0,  0,  0,  0,  0,  0,  5,  6,  7,  8,  0,  0,  0,  0,  0,  0},  // 5
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 6
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 7
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 8
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 9
    { 0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  0,  0,  0,  0,  0,  0},  // 10
    { 0,  0,  0,  0,  0,  0,  5,  6,  7,  8,  0,  0,  0,  0,  0,  0},  // 11
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 12
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 13
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 14
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 15
};

// ─────────────────────────────────────────────────────────────────────────────
//  LAYER 5 -- WORLD_COL (collision map, not rendered)
//  Blocks player movement independently of tile graphics.
//  Use for: fence posts, arch posts, trees, barrels, NPCs, any overlay
//  object that needs collision. Mark the tile(s) at foot/base level.
//  1 = blocked   0 = free
// ─────────────────────────────────────────────────────────────────────────────
static const uint8_t WORLD_COL[MAP_H][MAP_W] PROGMEM = {
//    0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 0
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 1
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 2
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 3
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 4
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 5
    { 0,  0,  0,  1,  1,  1,  1,  0,  0,  1,  1,  1,  1,  0,  0,  0},  // 6
    { 0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0},  // 7
    { 0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0},  // 8
    { 0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0},  // 9
    { 0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0},  // 10
    { 0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0},  // 11
    { 0,  0,  0,  1,  1,  1,  1,  0,  0,  1,  1,  1,  1,  0,  0,  0},  // 12
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 13
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 14
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // 15
};

// ─────────────────────────────────────────────────────────────────────────────
//  Map accessors
// ─────────────────────────────────────────────────────────────────────────────
inline uint8_t layer1Tile(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return TILE_GRASS1;
    return pgm_read_byte(&WORLD[ty][tx]);
}

inline uint8_t layer2Tile(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return 0;
    return pgm_read_byte(&WORLD2[ty][tx]);
}

inline uint8_t layer4Tile(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return 0;
    return pgm_read_byte(&WORLD3[ty][tx]);
}

inline bool layer5Blocked(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return false;
    return pgm_read_byte(&WORLD_COL[ty][tx]) != 0;
}

inline bool isTileWalkable(uint8_t id) {
    if (id >= TILE_COUNT) return false;
    return pgm_read_byte(&tileWalkable[id]);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Global state
// ─────────────────────────────────────────────────────────────────────────────
TFT_eSPI     tft;
TFT_eSprite  canvas(&tft);
InputManager input;

// ── Player / world state ─────────────────────────────────────────────────────
float    plrX = 1 * TILE_SIZE + (TILE_SIZE / 2) - (SPRITE_W / 2);
float    plrY = 1 * TILE_SIZE + (TILE_SIZE / 2) - (SPRITE_H / 2);
int      camX = 0, camY = 0;
Dir      animDir    = DIR_SOUTH;
uint8_t  animFrame  = 0;
uint32_t lastAnimMs = 0;
bool     isMoving   = false;

// ── Character stats ──────────────────────────────────────────────────────────
PlayerStats player = createDefaultStats();

// ── Menu state ───────────────────────────────────────────────────────────────
bool menuOpen = false;   // true = stats screen active, game paused

// ─────────────────────────────────────────────────────────────────────────────
//  Collision -- Layer 1 tileWalkable[] + Layer 5 WORLD_COL
// ─────────────────────────────────────────────────────────────────────────────
bool canMoveTo(float nx, float ny) {
    int fx  = (int)(nx + (SPRITE_W - COL_W) / 2);
    int fy  = (int)(ny + SPRITE_H - COL_H);
    int tx0 =  fx            / TILE_SIZE;
    int tx1 = (fx + COL_W-1) / TILE_SIZE;
    int ty0 =  fy            / TILE_SIZE;
    int ty1 = (fy + COL_H-1) / TILE_SIZE;

    if (!isTileWalkable(layer1Tile(tx0, ty0))) return false;
    if (!isTileWalkable(layer1Tile(tx1, ty0))) return false;
    if (!isTileWalkable(layer1Tile(tx0, ty1))) return false;
    if (!isTileWalkable(layer1Tile(tx1, ty1))) return false;

    if (layer5Blocked(tx0, ty0)) return false;
    if (layer5Blocked(tx1, ty0)) return false;
    if (layer5Blocked(tx0, ty1)) return false;
    if (layer5Blocked(tx1, ty1)) return false;

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawTile -- core pixel writer used by ALL 4 rendered layers
//  Reads pixel data from PROGMEM, skips pixels matching transpColor (0xF81F).
//  Ground tiles have no transparent pixels so the key is a no-op for Layer 1.
// ─────────────────────────────────────────────────────────────────────────────
void drawTile(int sx, int sy, const uint16_t* data,
              int tileW, int tileH, uint16_t transpColor) {
    if (!data) return;
    for (int row = 0; row < tileH; row++) {
        int screenY = sy + row;
        if (screenY < 0 || screenY >= SCR_H) continue;
        for (int col = 0; col < tileW; col++) {
            int screenX = sx + col;
            if (screenX < 0 || screenX >= SCR_W) continue;
            uint16_t pixel = pgm_read_word(&data[row * tileW + col]);
            if (pixel == transpColor) continue;
            canvas.drawPixel(screenX, screenY, pixel);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layer-specific draw functions -- each resolves its data pointer and calls drawTile
// ─────────────────────────────────────────────────────────────────────────────

// Layer 1 -- ground
void drawGroundTile(int sx, int sy, uint8_t id) {
    if (id >= TILE_COUNT) return;
    const uint16_t* data = (const uint16_t*)pgm_read_ptr(&tileData[id]);
    drawTile(sx, sy, data, TILE_SIZE, TILE_SIZE, OVERLAY_TRANSPARENT);
}

// Layer 2 -- overlay (behind sprite)
void drawOverlayTile(int sx, int sy, uint8_t id) {
    if (id == 0 || id >= OVERLAY_TILE_COUNT) return;
    const uint16_t* data = (const uint16_t*)pgm_read_ptr(&overlayTileData[id]);
    drawTile(sx, sy, data, OVERLAY_TILE_SIZE, OVERLAY_TILE_SIZE, OVERLAY_TRANSPARENT);
}

// Layer 3 -- sprite
void drawSprite(int sx, int sy) {
    const uint16_t* const* dirFrames =
        (const uint16_t* const*)pgm_read_ptr(&spriteFrames[animDir]);
    const uint16_t* frameData =
        (const uint16_t*)pgm_read_ptr(&dirFrames[animFrame]);
    drawTile(sx, sy, frameData, SPRITE_W, SPRITE_H, SPRITE_TRANSPARENT);
}

// Layer 4 -- foreground (in front of sprite)
void drawForegroundTile(int sx, int sy, uint8_t id) {
    if (id == 0 || id >= FOREGROUND_TILE_COUNT) return;
    const uint16_t* data = (const uint16_t*)pgm_read_ptr(&fgTileData[id]);
    drawTile(sx, sy, data, FOREGROUND_TILE_SIZE, FOREGROUND_TILE_SIZE, FOREGROUND_TRANSPARENT);
}

// ─────────────────────────────────────────────────────────────────────────────
//  renderFrame -- fills canvas with the 4-layer game world.
//  Does NOT call pushSprite — that is done in loop() so menus can use the
//  same canvas without a separate push path.
// ─────────────────────────────────────────────────────────────────────────────
void renderFrame() {
    int startTX = camX / TILE_SIZE;
    int startTY = camY / TILE_SIZE;
    int endTX   = (camX + SCR_W - 1) / TILE_SIZE + 1;
    int endTY   = (camY + SCR_H - 1) / TILE_SIZE + 1;

    // ── Layer 1: ground ───────────────────────────────────────────────────────
    for (int ty = startTY; ty <= endTY; ty++)
        for (int tx = startTX; tx <= endTX; tx++)
            drawGroundTile(tx * TILE_SIZE - camX, ty * TILE_SIZE - camY,
                           layer1Tile(tx, ty));

    // ── Layer 2: overlay (behind sprite) ─────────────────────────────────────
    for (int ty = startTY; ty <= endTY; ty++)
        for (int tx = startTX; tx <= endTX; tx++)
            drawOverlayTile(tx * TILE_SIZE - camX, ty * TILE_SIZE - camY,
                            layer2Tile(tx, ty));

    // ── Layer 3: sprite ───────────────────────────────────────────────────────
    drawSprite((int)plrX - camX, (int)plrY - camY);

    // ── Layer 4: foreground (in front of sprite) ──────────────────────────────
    for (int ty = startTY; ty <= endTY; ty++)
        for (int tx = startTX; tx <= endTX; tx++)
            drawForegroundTile(tx * TILE_SIZE - camX, ty * TILE_SIZE - camY,
                               layer4Tile(tx, ty));

    // ── Layer 5: collision (not rendered, checked in canMoveTo()) ─────────────
}

// ─────────────────────────────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    input.init();

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    canvas.createSprite(SCR_W, SCR_H);
    canvas.setSwapBytes(true);

    Serial.println("Ready");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── Single-press events (debounced) ───────────────────────────────────────
    ButtonEvent ev = input.poll();
    if (ev == BTN_EV_B) menuOpen = !menuOpen;   // B toggles stats menu

    // ── If menu is open: draw it and skip all game logic ─────────────────────
    if (menuOpen) {
        drawStatsMenu(canvas, player);
        canvas.pushSprite(0, 0);
        return;
    }

    // ── Game logic (only runs when menu is closed) ────────────────────────────

    bool up    = input.held(BTN_UP);
    bool down  = input.held(BTN_DOWN);
    bool left  = input.held(BTN_LEFT);
    bool right = input.held(BTN_RIGHT);

    isMoving = up || down || left || right;

    float dx = 0, dy = 0;

    if      (up   && right) { animDir = DIR_NE;    dx =  DIAG_SPD; dy = -DIAG_SPD; }
    else if (up   && left)  { animDir = DIR_NW;    dx = -DIAG_SPD; dy = -DIAG_SPD; }
    else if (down && right) { animDir = DIR_SE;    dx =  DIAG_SPD; dy =  DIAG_SPD; }
    else if (down && left)  { animDir = DIR_SW;    dx = -DIAG_SPD; dy =  DIAG_SPD; }
    else if (up)            { animDir = DIR_NORTH; dy = -PLAYER_SPD; }
    else if (down)          { animDir = DIR_SOUTH; dy =  PLAYER_SPD; }
    else if (right)         { animDir = DIR_EAST;  dx =  PLAYER_SPD; }
    else if (left)          { animDir = DIR_WEST;  dx = -PLAYER_SPD; }

    if (dx != 0 && canMoveTo(plrX + dx, plrY))      plrX += dx;
    if (dy != 0 && canMoveTo(plrX,      plrY + dy)) plrY += dy;

    plrX = constrain(plrX, 0.0f, (float)(MAP_W * TILE_SIZE - SPRITE_W));
    plrY = constrain(plrY, 0.0f, (float)(MAP_H * TILE_SIZE - SPRITE_H));

    if (isMoving) {
        if (now - lastAnimMs >= ANIM_MS) {
            animFrame = (animFrame + 1) % SPRITE_FRAME_COUNT;
            lastAnimMs = now;
        }
    } else {
        animFrame  = 0;
        lastAnimMs = now;
    }

    camX = (int)(plrX + SPRITE_W / 2) - SCR_W / 2;
    camY = (int)(plrY + SPRITE_H / 2) - SCR_H / 2;
    camX = constrain(camX, 0, MAP_W * TILE_SIZE - SCR_W);
    camY = constrain(camY, 0, MAP_H * TILE_SIZE - SCR_H);

    renderFrame();
    canvas.pushSprite(0, 0);
}
