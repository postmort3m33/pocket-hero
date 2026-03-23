// stats.h
// ─────────────────────────────────────────────────────────────────────────────
//  PlayerStats — all character data in one place, zero rendering code.
//
//  Shared by: ui.h (menus), battle system, inventory, save/load, etc.
//  Nothing in this file should #include TFT_eSPI or depend on hardware.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
//  PlayerStats struct
// ─────────────────────────────────────────────────────────────────────────────
struct PlayerStats {
    char     name[12];   // display name (null-terminated)
    uint8_t  level;
    uint16_t xp;
    uint16_t xpToNext;
    uint16_t hp;
    uint16_t hpMax;
    uint8_t  str;        // Strength
    uint8_t  intel;      // Intelligence  (INT is a C++ reserved word)
    uint8_t  agi;        // Agility
    uint8_t  vit;        // Vitality
};

// ─────────────────────────────────────────────────────────────────────────────
//  Factory — returns default stats for a fresh character
// ─────────────────────────────────────────────────────────────────────────────
inline PlayerStats createDefaultStats() {
    PlayerStats s;
    strncpy(s.name, "JOSH", sizeof(s.name));
    s.level    = 1;
    s.xp       = 30;
    s.xpToNext = 100;
    s.hp       = 100;
    s.hpMax    = 100;
    s.str      = 10;
    s.intel    = 10;
    s.agi      = 10;
    s.vit      = 10;
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  XP helper — adds XP and handles level-up.
//  Returns true if the player leveled up (caller can trigger a level-up screen).
//
//  Level-up formula: each level requires 100 more XP than the previous.
//    Level 1→2 : 100 XP    Level 2→3 : 200 XP    etc.
//  Stat gains on level-up: +2 to all stats, +20 to hpMax, hp fully restored.
// ─────────────────────────────────────────────────────────────────────────────
inline bool addXP(PlayerStats& p, uint16_t amount) {
    p.xp += amount;
    if (p.xp < p.xpToNext) return false;

    // Level up
    p.xp      -= p.xpToNext;
    p.level   += 1;
    p.xpToNext = (uint16_t)p.level * 100;  // 100, 200, 300 …

    p.hpMax += 20;
    p.hp     = p.hpMax;   // full heal on level-up
    return true;
}
