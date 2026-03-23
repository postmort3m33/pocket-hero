// ui.h
// ─────────────────────────────────────────────────────────────────────────────
//  UI / HUD rendering — ESP32 + ST7735 160×128 landscape.
//
//  All drawing targets a TFT_eSprite canvas. Caller does pushSprite().
//
//  RENDERING RULES (do not change):
//   • fillRect() and drawPixel() on TFT_eSprite byte-swap internally.
//   • Always pass RAW standard RGB565 — no pre-swapping.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <TFT_eSPI.h>
#include "stats.h"
#include "sprites.h"   // SPRITE_W, SPRITE_H, SPRITE_TRANSPARENT, spriteFrames

// ─────────────────────────────────────────────────────────────────────────────
//  Colour palette — one place to retheme the whole UI
// ─────────────────────────────────────────────────────────────────────────────
namespace UIColor {
    constexpr uint16_t BG          = 0x0821;  // dark teal-black
    constexpr uint16_t BLACK       = 0x0000;
    constexpr uint16_t BORDER_LO   = 0x4208;  // dim border / dividers
    constexpr uint16_t BORDER_HI   = 0xC618;  // light-gray outer border
    constexpr uint16_t HEADER_BG   = 0x000F;  // navy
    constexpr uint16_t GOLD        = 0xFFE0;
    constexpr uint16_t LABEL       = 0x7BEF;  // mid-gray
    constexpr uint16_t WHITE       = 0xFFFF;
    constexpr uint16_t HP_FILL     = 0x07E0;  // green
    constexpr uint16_t HP_EMPTY    = 0x2800;  // dark red
    constexpr uint16_t HP_LABEL    = 0xFFFF;  // red
    constexpr uint16_t XP_FILL     = 0xfbc0;  // purple
    constexpr uint16_t XP_EMPTY    = 0x1008;  // dark purple
    constexpr uint16_t XP_LABEL    = 0xFFFF;  // pink-purple
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

// Progress bar (no outline)
static inline void _uiFillBar(TFT_eSprite& spr,
                               int16_t x, int16_t y, int16_t w, int16_t h,
                               uint16_t current, uint16_t maximum,
                               uint16_t fillCol, uint16_t emptyCol) {
    spr.fillRect(x, y, w, h, emptyCol);
    if (maximum > 0 && current > 0) {
        int16_t filled = (int32_t)w * current / maximum;
        if (filled > w) filled = w;
        if (filled > 0) spr.fillRect(x, y, filled, h, fillCol);
    }
}

// Sprite pixel loop — mirrors drawTile() in main.cpp
// Renders one sprite frame at (sx, sy) onto canvas, skipping transparent pixels.
static inline void _uiDrawSprite(TFT_eSprite& canvas,
                                  int sx, int sy,
                                  const uint16_t* frameData) {
    if (!frameData) return;
    for (int row = 0; row < SPRITE_H; row++) {
        int screenY = sy + row;
        if (screenY < 0 || screenY >= 128) continue;
        for (int col = 0; col < SPRITE_W; col++) {
            int screenX = sx + col;
            if (screenX < 0 || screenX >= 160) continue;
            uint16_t px = pgm_read_word(&frameData[row * SPRITE_W + col]);
            if (px == SPRITE_TRANSPARENT) continue;
            canvas.drawPixel(screenX, screenY, px);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawStatsMenu
//
//  animDir   — pass the current player direction (shows correct facing)
//  animFrame — pass the current walk frame
//
//  Layout:
//    y  0-13  Header: "✦ POCKET HERO ✦"
//    y 14-24  HP bar (full width)
//    y 25-35  XP bar (full width)
//    y 36     Horizontal divider
//    y 37-127 Left pane (x 0-69):  black bg + sprite centered
//             Right pane (x 70-159): name, level, stats, footer
// ─────────────────────────────────────────────────────────────────────────────
inline void drawStatsMenu(TFT_eSprite& canvas,
                           const PlayerStats& p,
                           uint8_t animDir,
                           uint8_t animFrame) {
    using namespace UIColor;

    // ── Background ────────────────────────────────────────────────────────────
    canvas.fillSprite(BG);
    canvas.drawRect(0, 0, 160, 128, BORDER_HI);

    // ── Header  y=0-13 ───────────────────────────────────────────────────────
    canvas.fillRect(1, 1, 158, 13, HEADER_BG);
    canvas.drawFastHLine(1, 13, 158, BORDER_HI);

    canvas.setTextFont(1);
    canvas.setTextSize(1);
    canvas.setTextColor(GOLD);
    // "* POCKET HERO *" = 16 chars x 6px = 96px  -> centre at 80 -> x=32
    canvas.setCursor(68, 4);
    canvas.print("HERO");

    // ── HP bar  y=14-24 ──────────────────────────────────────────────────────
    canvas.setTextColor(HP_LABEL);
    canvas.setCursor(3, 16);
    canvas.print("HP");

    canvas.drawRect(17, 14, 83, 11, BORDER_LO);  // width=83: leaves room for "9999/9999"
    _uiFillBar(canvas, 18, 15, 81, 9, p.hp, p.hpMax, HP_FILL, HP_EMPTY);

    {
        char buf[10];
        snprintf(buf, sizeof(buf), "%d/%d", p.hp, p.hpMax);
        canvas.setTextColor(WHITE);
        canvas.setCursor(157 - canvas.textWidth(buf), 16);
        canvas.print(buf);
    }

    // ── XP bar  y=25-35 ──────────────────────────────────────────────────────
    canvas.setTextColor(XP_LABEL);
    canvas.setCursor(3, 27);
    canvas.print("XP");

    canvas.drawRect(17, 25, 83, 11, BORDER_LO);
    _uiFillBar(canvas, 18, 26, 81, 9, p.xp, p.xpToNext, XP_FILL, XP_EMPTY);

    {
        char buf[10];
        snprintf(buf, sizeof(buf), "%d/%d", p.xp, p.xpToNext);
        canvas.setTextColor(WHITE);
        canvas.setCursor(157 - canvas.textWidth(buf), 27);
        canvas.print(buf);
    }

    // ── Horizontal divider  y=36 ─────────────────────────────────────────────
    canvas.drawFastHLine(1, 36, 158, BORDER_HI);

    // ── Left pane  x=0-69, y=37-127  (black bg + sprite) ────────────────────
    canvas.fillRect(1, 37, 69, 90, BLACK);
    canvas.drawFastVLine(69, 37, 90, BORDER_HI);

    // Sprite centred in 70×90 pane
    //   x = 1 + (70 - SPRITE_W) / 2 = 1 + 7 = 8
    //   y = 37 + (90 - SPRITE_H) / 2 = 37 + 17 = 54
    {
        const uint16_t* const* dirFrames =
            (const uint16_t* const*)pgm_read_ptr(&spriteFrames[animDir]);
        const uint16_t* frameData =
            (const uint16_t*)pgm_read_ptr(&dirFrames[animFrame]);
        _uiDrawSprite(canvas, 8, 54, frameData);
    }

    // ── Right pane  x=70-159, y=37-127 ───────────────────────────────────────
    //   Name row  y=42
    canvas.setTextFont(1);
    canvas.setTextSize(1);

    canvas.setTextColor(LABEL);
    canvas.setCursor(74, 42);
    canvas.print("NAME");

    canvas.setTextColor(WHITE);
    canvas.setCursor(104, 42);
    canvas.print(p.name);

    //   Level row  y=52
    canvas.setTextColor(LABEL);
    canvas.setCursor(74, 52);
    canvas.print("LVL");

    canvas.setTextColor(GOLD);
    canvas.setCursor(104, 52);
    canvas.print(p.level);

    //   Thin divider  y=62
    canvas.drawFastHLine(73, 62, 85, BORDER_LO);

    //   Stat rows — label in accent colour, value in white
    struct StatRow { int16_t y; const char* label; uint8_t val; uint16_t col; };
    const StatRow rows[4] = {
        { 67, "STR", p.str,   0xFDA0 },   // orange
        { 77, "INT", p.intel, 0x07FF },   // cyan
        { 87, "AGI", p.agi,   0x07E0 },   // green
        { 97, "VIT", p.vit,   0xF800 },   // red
    };

    for (const auto& r : rows) {
        canvas.setTextColor(r.col);
        canvas.setCursor(74, r.y);
        canvas.print(r.label);

        canvas.setTextColor(WHITE);
        canvas.setCursor(104, r.y);
        canvas.print(r.val);
    }

    // ── Footer  y=118 ─────────────────────────────────────────────────────────
    canvas.drawFastHLine(70, 114, 88, BORDER_LO);
    canvas.setTextColor(LABEL);
    // "[B] CLOSE" = 9 chars x 6px = 54px -> centre in 90px pane at x=70 -> x=88
    canvas.setCursor(88, 118);
    canvas.print("[B] CLOSE");
}