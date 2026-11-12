#pragma once
#include <stdint.h>

// Theme color slots — order must match the live palette array.
enum ThemeColor {
    TH_BG     = 0,
    TH_PANEL  = 1,
    TH_TEXT   = 2,
    TH_DIM    = 3,
    TH_ACCENT = 4,
    TH_GREEN  = 5,
    TH_AMBER  = 6,
    TH_RED    = 7,
    TH_BAR_BG = 8,
    TH_COUNT  = 9,
};

// Set the live palette from an array of TH_COUNT 0xRRGGBB values.
void theme_palette_set(const uint32_t colors[TH_COUNT]);

// Return the 0xRRGGBB value for a slot from the live palette.
uint32_t theme_hex(ThemeColor c);

// Return the RGB565 value for a slot from the live palette.
uint16_t theme_rgb565(ThemeColor c);

// Parse a 54-char hex string (9 × 6 hex digits, no separators) into colors[].
// On malformed input the output is undefined; caller should validate length first.
void theme_parse_hex(const char* hex54, uint32_t out[TH_COUNT]);

// Format colors[] into a 54-char hex string + null terminator (55 bytes).
void theme_format_hex(const uint32_t colors[TH_COUNT], char out55[55]);
