#pragma once
#include "data.h"
#include "ble.h"
#include <lvgl.h>

enum screen_t {
    SCREEN_SPLASH,
    SCREEN_USAGE,
    SCREEN_SETTINGS,
    SCREEN_BLUETOOTH,
    SCREEN_WIFI,
    SCREEN_COUNT,
};

typedef enum {
    UI_STATUS_NONE,
    UI_STATUS_INFO,
    UI_STATUS_WARN,
    UI_STATUS_ERROR,
} ui_status_level_t;

void ui_init(void);
void ui_update(const UsageData* data);
void ui_set_status(ui_status_level_t level, const char* msg);
// Wi-Fi overlay path: caches the status and suppresses it while an alert is active.
void ui_set_wifi_status(ui_status_level_t level, const char* msg);
void ui_tick_anim(void);
void ui_show_screen(screen_t screen);
void ui_cycle_screen(void);
void ui_toggle_splash(void);
screen_t ui_get_current_screen(void);
void ui_update_ble_status(ble_state_t state, const char* name, const char* mac);
void ui_update_battery(int percent, bool charging);
void ui_update_wifi_creds(bool portal_active);
bool ui_hotspot_requested(void);
void ui_set_nav_locked(bool locked);

// Re-sync the settings screen's widgets (brightness slider, chime toggle)
// from their backing stores. Call after an external change (e.g.
// `chime on|off` over serial).
void ui_settings_refresh(void);

// Reset the cached-minute counter so the clock title updates on the next tick.
// Call after changing clock preferences.
void ui_clock_invalidate(void);

// Show or hide the pace-marker ticks on the usage bars. Applies immediately
// using the last received usage data.
void ui_set_pace_marker(bool on);

// Re-apply all LVGL style properties from the live theme palette to every
// persistent object on every screen. Call after theme_palette_set().
void ui_repaint_theme(void);

// Director bridge — implemented in ui.cpp; extern'd by clawd_director.cpp
lv_obj_t* ui_mascot_parent(void);         // SCREEN_USAGE root container
bool      ui_meters_live(void);           // true when SCREEN_USAGE active + data valid
void      ui_set_alert_text(const char* msg);  // director attention overlay
