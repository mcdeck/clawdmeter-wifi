#include "theme_palette.h"
#include <stdio.h>
#include <string.h>

// Dark defaults — same values as the original hardcoded theme.h macros.
static uint32_t s_palette[TH_COUNT] = {
    0x000000,   // TH_BG
    0x1f1f1e,   // TH_PANEL
    0xfaf9f5,   // TH_TEXT
    0xb0aea5,   // TH_DIM
    0xd97757,   // TH_ACCENT
    0x788c5d,   // TH_GREEN
    0xd97757,   // TH_AMBER
    0xc0392b,   // TH_RED
    0x2a2a28,   // TH_BAR_BG
};

void theme_palette_set(const uint32_t colors[TH_COUNT]) {
    for (int i = 0; i < TH_COUNT; i++) s_palette[i] = colors[i];
}

uint32_t theme_hex(ThemeColor c) {
    if (c < 0 || c >= TH_COUNT) return 0x000000;
    return s_palette[c];
}

uint16_t theme_rgb565(ThemeColor c) {
    uint32_t rgb = theme_hex(c);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8)  & 0xFF;
    uint8_t b =  rgb        & 0xFF;
    return ((uint16_t)(r >> 3) << 11) |
           ((uint16_t)(g >> 2) <<  5) |
           ((uint16_t)(b >> 3));
}

void theme_parse_hex(const char* hex54, uint32_t out[TH_COUNT]) {
    for (int i = 0; i < TH_COUNT; i++) {
        uint32_t v = 0;
        for (int j = 0; j < 6; j++) {
            char c = hex54[i * 6 + j];
            uint8_t nib;
            if      (c >= '0' && c <= '9') nib = c - '0';
            else if (c >= 'a' && c <= 'f') nib = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') nib = c - 'A' + 10;
            else                           nib = 0;
            v = (v << 4) | nib;
        }
        out[i] = v;
    }
}

void theme_format_hex(const uint32_t colors[TH_COUNT], char out55[55]) {
    for (int i = 0; i < TH_COUNT; i++) {
        snprintf(out55 + i * 6, 7, "%06x", (unsigned)colors[i]);
    }
    out55[TH_COUNT * 6] = '\0';
}
