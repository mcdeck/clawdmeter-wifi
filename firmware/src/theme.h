#pragma once
#include <lvgl.h>
#include "theme_palette.h"

// Dynamic design tokens: each call resolves from the live palette set by
// theme_palette_set(). Hardcoded Dark defaults live in theme_palette.cpp.
static inline lv_color_t th_col(ThemeColor c) { return lv_color_hex(theme_hex(c)); }

#define THEME_BG       th_col(TH_BG)
#define THEME_PANEL    th_col(TH_PANEL)
#define THEME_TEXT     th_col(TH_TEXT)
#define THEME_DIM      th_col(TH_DIM)
#define THEME_ACCENT   th_col(TH_ACCENT)
#define THEME_GREEN    th_col(TH_GREEN)
#define THEME_AMBER    th_col(TH_AMBER)
#define THEME_RED      th_col(TH_RED)
#define THEME_BAR_BG   th_col(TH_BAR_BG)
