# Clock, Settings Web UI & Preferences Implementation Plan

## Table of Contents

- [Global Constraints](#global-constraints)
- [Review Focus](#review-focus)
- [What Was Already Implemented](#what-was-already-implemented)
- [File Map](#file-map)
- [Task 1: Pure Logic Ports](#task-1-pure-logic-ports)
- [Task 2: Dynamic Theme + UI Extensions](#task-2-dynamic-theme--ui-extensions)
- [Task 3: Preference Modules](#task-3-preference-modules)
- [Task 4: Pending Wi-Fi Credentials](#task-4-pending-wi-fi-credentials)
- [Task 5: Settings Web UI Expansion](#task-5-settings-web-ui-expansion)
- [Task 6: main.cpp Wiring](#task-6-maincpp-wiring)
- [Post-Implementation Hardware Checklist](#post-implementation-hardware-checklist)

---

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add clock display, pace marker, button remapping, runtime themes, and a full LAN settings web UI (including Wi-Fi credential updates with safety revert).

**Architecture:** Pure-logic modules (pace, button_action, theme_palette) are ported first; NVS preference modules wrap them; `ui.cpp` gains clock rendering, pace tick marks, and a theme repaint path; `settings_server.cpp` gains the full HTML form and two POST routes; `provisioning.cpp` gains a pending-credential trial API consumed by `wifi_poller.cpp`.

**Tech Stack:** ESP32 Arduino core (Preferences NVS, WebServer, ESPmDNS, WiFi, configTime/tzset), LVGL 9.

**Spec:** `docs/superpowers/specs/2026-11-11-clock-settings-webui-design.md`

## Global Constraints

- No new `lib_deps` — only Arduino core libraries already present.
- No `#ifdef BOARD_*` in shared code (use `BoardCaps` or per-board files for board differences).
- All NVS writes use the existing `"clawdmeter"` namespace.
- Build must pass for both envs: `waveshare_amoled_216` and `waveshare_amoled_18`.
- Fonts were compiled without em-dash/ellipsis. Clock strings use only ASCII digits, colon, space, A, M, P — safe.
- `theme.h` macros (`THEME_BG` etc.) are aliased as `COL_*` in `ui.cpp:112–120`. After Task 2, those aliases still work because `THEME_BG` resolves to `th_col(TH_BG)` at call time.

## Review Focus

- NTP not synced at render time (`time(nullptr) < 1000000000L`) — `lbl_title` must show "Usage", not `0:00`.
  → Task 2: guard clock rendering on `time(nullptr) > 1000000000L` before calling `localtime_r`.
- Pending trial with first-ever credentials that also fail — `provisioning_revert_pending()` leaves `ssid`/`pass` as empty strings; existing `WIFI_POLL_NO_CREDS` path and auto-portal must still activate naturally.
  → Task 4: test that `provisioning_revert_pending()` does NOT write non-empty strings when nothing was in the current slots.
- `/save-settings` with out-of-range brightness (< 10 or > 100) — must clamp, not write garbage to NVS.
  → Task 5: clamp `brt_lvl` to `[10, 100]` in the POST handler before calling `brightness_set_level()`.
- `/save-connection` with blank SSID and blank password — must treat as token-only (no pending, no reboot).
  → Task 5: check `ssid.length() == 0 && pass.length() == 0` before writing pending slots.
- Unknown theme preset id passed to `theme_pref_set_by_name()` — must silently keep current palette.
  → Task 3: `theme_pref_set_by_name()` does a linear search; if not found, return without writing NVS.

---

## What Was Already Implemented

The following parts of the spec were built during the Clawd director phase and **do not need to be re-done**:

- `settings_server.{h,cpp}` — lifecycle (`init/start/stop/tick`), mDNS, `GET /`, Security section (webpass), Event token section, Clawd section, `POST /event` hook receiver.
- `captive_portal.cpp` — calls `settings_server_stop()` before starting portal; functionally equivalent to the spec's shared-WebServer design (two separate instances, mutually exclusive modes).
- `wifi_poller.cpp` — `configTime(0, 0, "pool.ntp.org")` and `settings_server_start()` called on successful connect.
- `ui.cpp` — `SCREEN_SETTINGS` already in the nav cycle; `lbl_title` object exists on the usage screen.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `firmware/src/tz_list.h` | Create | Compile-time timezone name→POSIX array |
| `firmware/src/pace.h` | Create | `pace_elapsed_pct()`, `pace_level()` declarations |
| `firmware/src/pace.cpp` | Create | Pure math implementations |
| `firmware/src/button_action.h` | Create | `ButtonSlot`, `ButtonBinding`, `button_action_valid/decode` |
| `firmware/src/button_action.cpp` | Create | Preset decode, HID chord tables |
| `firmware/src/theme_palette.h` | Create | `ThemeColor` enum, live palette array API |
| `firmware/src/theme_palette.cpp` | Create | `theme_palette_set/hex/rgb565/parse/format_hex` |
| `firmware/src/theme_presets.h` | Create | 11 preset palette structs |
| `firmware/src/theme.h` | Modify | Replace hardcoded `lv_color_hex(0x…)` with `th_col(TH_*)` |
| `firmware/src/ui.h` | Modify | Add `ui_set_pace_marker`, `ui_clock_invalidate`, `ui_repaint_theme` |
| `firmware/src/ui.cpp` | Modify | Clock rendering, pace tick objects, `repaint_*_screen()` helpers |
| `firmware/src/clock_pref.h` | Create | `clock_pref_load/apply_tz/on/fmt/tz/set` |
| `firmware/src/clock_pref.cpp` | Create | NVS read/write + `ui_clock_invalidate()` |
| `firmware/src/pace_pref.h` | Create | `pace_pref_load/get/set` |
| `firmware/src/pace_pref.cpp` | Create | NVS read/write + `ui_set_pace_marker()` |
| `firmware/src/button_pref.h` | Create | `button_pref_load/get/set` |
| `firmware/src/button_pref.cpp` | Create | NVS read/write + validation |
| `firmware/src/theme_pref.h` | Create | `theme_pref_load/set_by_name/current_id` |
| `firmware/src/theme_pref.cpp` | Create | NVS read/write + `ui_repaint_theme()` |
| `firmware/src/provisioning.h` | Modify | Add pending Wi-Fi credential trial API |
| `firmware/src/provisioning.cpp` | Modify | Implement pending slots, trial timer, revert |
| `firmware/src/wifi_poller.cpp` | Modify | Trial timeout check in CONNECTING; `clock_pref_apply_tz()` after NTP |
| `firmware/src/settings_server.cpp` | Modify | Full settings HTML + `/save-settings` + `/save-connection` routes |
| `firmware/src/main.cpp` | Modify | Add pref loads, button decode, revert flag check |

---

## Task 1: Pure Logic Ports

**Files:**
- Create: `firmware/src/tz_list.h`
- Create: `firmware/src/pace.h`, `firmware/src/pace.cpp`
- Create: `firmware/src/button_action.h`, `firmware/src/button_action.cpp`
- Create: `firmware/src/theme_palette.h`, `firmware/src/theme_palette.cpp`
- Create: `firmware/src/theme_presets.h`

**Interfaces:**
- Produces:
  - `tz_list.h`: `extern const struct TzEntry { const char* name; const char* posix; } tz_list[]; extern const int tz_list_count;`
  - `pace.h`: `float pace_elapsed_pct(int reset_mins, int window_mins);` returns 0–100; `int pace_level(float bar_pct, float elapsed_pct);` returns 0=under/on-pace (green), 1=ahead-of-pace (amber), 2=at-limit (red, bar_pct ≥ 80).
  - `button_action.h`: `enum ButtonSlot { BTN_SLOT_PRIMARY=0, BTN_SLOT_SECONDARY=1, BTN_SLOT_PWR=2 };` `bool button_action_valid(int32_t v);` `struct ButtonBinding { int kind; uint8_t keycode; uint8_t mods; };` `ButtonBinding button_action_decode(int32_t v);` Preset map: 0=None, 1=Space(0x2C,0x00), 2=Shift+Tab(0x2B,0x02), 3=Tab(0x2B,0x00), 4=Enter(0x28,0x00), 5=Escape(0x29,0x00), 6=Brightness(kind=BTN_KIND_BRIGHTNESS, keycode=0).
  - `theme_palette.h`: `enum ThemeColor { TH_BG=0, TH_PANEL, TH_TEXT, TH_DIM, TH_ACCENT, TH_GREEN, TH_AMBER, TH_RED, TH_BAR_BG, TH_COUNT };` `void theme_palette_set(const uint32_t colors[TH_COUNT]);` `uint32_t theme_hex(ThemeColor c);` `uint16_t theme_rgb565(ThemeColor c);` `void theme_parse_hex(const char* hex66, uint32_t out[TH_COUNT]);` `void theme_format_hex(const uint32_t colors[TH_COUNT], char out67[67]);`
  - `theme_presets.h`: `struct ThemePreset { const char* id; const char* name; uint32_t colors[TH_COUNT]; };` `extern const ThemePreset theme_presets[]; extern const int theme_preset_count;` with 11 entries: `dark`, `light`, `catppuccin_mocha`, `dracula`, `gruvbox`, `monokai`, `nord`, `one_dark`, `solarized_dark`, `solarized_light`, `tokyo_night`.

- [ ] **Step 1: Create `tz_list.h`** — header-only `TzEntry` struct array with ~50 entries covering Americas, Europe, Africa, Asia, Pacific regions. Minimum required entries: `UTC0`, `EST5EDT,M3.2.0,M11.1.0` (America/New_York), `CET-1CEST,M3.5.0,M10.5.0/3` (Europe/Berlin), `JST-9` (Asia/Tokyo). `tz_list_count` as `constexpr int`.

- [ ] **Step 2: Create `pace.h` and `pace.cpp`** — no Arduino or LVGL includes. `pace_elapsed_pct`: return `((float)(window_mins - reset_mins) / (float)window_mins) * 100.0f`, clamped to [0, 100]. `pace_level`: level 2 if `bar_pct >= 80.0f`, level 1 if `bar_pct > elapsed_pct + 5.0f`, level 0 otherwise.

- [ ] **Step 3: Create `button_action.h` and `button_action.cpp`** — no Arduino or LVGL. `button_action_valid` returns true for values 0–6. `button_action_decode` uses a switch on the preset index. Define `BTN_KIND_NONE`, `BTN_KIND_HID`, `BTN_KIND_BRIGHTNESS` constants.

- [ ] **Step 4: Create `theme_palette.h` and `theme_palette.cpp`** — no Arduino or LVGL. Static `uint32_t s_palette[TH_COUNT]` initialized to Dark defaults (spec NVS table default: `""` = Dark). `theme_parse_hex` reads 9×6 hex chars from a 54-char string (not 66 — recount: 9 colors × 6 chars = 54 + null). `theme_format_hex` writes 54 chars + null.

  > **Note:** The spec says "66 hex chars — 11 × rrggbb" but TH_COUNT = 9, not 11. Use 9 × 6 = 54-char hex string. Update the NVS key description in comments to match. The settings_server POST handler (Task 5) stores this by-name, not raw hex, so the parse/format functions are only used if loading raw palette from NVS.

- [ ] **Step 5: Create `theme_presets.h`** — header-only. The 11 preset `ThemePreset` entries with their 9-color `uint32_t colors[]` arrays. Dark preset uses the existing defaults from `theme.h` (BG=0x000000, PANEL=0x1f1f1e, TEXT=0xfaf9f5, DIM=0xb0aea5, ACCENT=0xd97757, GREEN=0x788c5d, AMBER=0xd97757, RED=0xc0392b, BAR_BG=0x2a2a28). The other 10 presets use color values appropriate to each theme's design language — choose colors consistent with each theme's identity (Nord=blue-grey palette, Dracula=purple accents, etc.).

- [ ] **Step 6: Build verify**

  Run: `pio run -d firmware -e waveshare_amoled_216 -e waveshare_amoled_18`
  Expected: SUCCESS (new headers compiled in, no link errors — these files are not yet included by any existing source).

- [ ] **Step 7: Commit**

  ```bash
  git add firmware/src/tz_list.h firmware/src/pace.h firmware/src/pace.cpp \
          firmware/src/button_action.h firmware/src/button_action.cpp \
          firmware/src/theme_palette.h firmware/src/theme_palette.cpp \
          firmware/src/theme_presets.h
  git commit -m "feat: pure logic ports — pace, button_action, theme_palette, tz_list"
  ```

---

## Task 2: Dynamic Theme + UI Extensions

**Files:**
- Modify: `firmware/src/theme.h`
- Modify: `firmware/src/ui.h`
- Modify: `firmware/src/ui.cpp`

**Interfaces:**
- Consumes: `theme_palette.h` (Task 1), `pace.h` (Task 1)
- Produces:
  - `ui.h` additions: `void ui_set_pace_marker(bool on);` `void ui_clock_invalidate(void);` `void ui_repaint_theme(void);`
  - Theme macros: `THEME_BG` etc. now call `th_col(TH_BG)` at call time — **no source changes needed in `ui.cpp`** since it uses `COL_BG` = `THEME_BG`.

- [ ] **Step 1: Rewrite `theme.h`** — replace hardcoded `lv_color_hex(0x…)` macros with dynamic wrappers:

  ```cpp
  #pragma once
  #include <lvgl.h>
  #include "theme_palette.h"
  static inline lv_color_t th_col(ThemeColor c) { return lv_color_hex(theme_hex(c)); }
  #define THEME_BG      th_col(TH_BG)
  #define THEME_PANEL   th_col(TH_PANEL)
  #define THEME_TEXT    th_col(TH_TEXT)
  #define THEME_DIM     th_col(TH_DIM)
  #define THEME_ACCENT  th_col(TH_ACCENT)
  #define THEME_GREEN   th_col(TH_GREEN)
  #define THEME_AMBER   th_col(TH_AMBER)
  #define THEME_RED     th_col(TH_RED)
  #define THEME_BAR_BG  th_col(TH_BAR_BG)
  ```

- [ ] **Step 2: Add declarations to `ui.h`** — add after the existing `ui_settings_refresh()` line:

  ```cpp
  void ui_set_pace_marker(bool on);
  void ui_clock_invalidate(void);    // reset cached minute so clock updates immediately
  void ui_repaint_theme(void);       // re-apply all LVGL styles from live palette
  ```

- [ ] **Step 3: Add static state to `ui.cpp`** (after the existing static widget declarations, around line 130):

  ```cpp
  static lv_obj_t* pace_tick_session;    // 2px wide tick on bar_session
  static lv_obj_t* pace_tick_weekly;     // 2px wide tick on bar_weekly
  static bool      s_pace_on = false;
  static int       s_clock_last_min = -1;
  static UsageData s_last_usage = {};    // copy for pace marker redraws
  ```

- [ ] **Step 4: Extend `init_usage_screen()`** in `ui.cpp` — after `bar_session` and `bar_weekly` are created, create the two pace tick objects as hidden `lv_obj_t` children of the same panel as the bar, 2px wide, full bar height, `COL_DIM` fill, `lv_obj_add_flag(tick, LV_OBJ_FLAG_HIDDEN)` by default.

  Tick position is set in `ui_update()` and `ui_set_pace_marker()`. The spec says position at `pace_elapsed_pct(reset_mins, window_mins)` × bar_width.

- [ ] **Step 5: Implement clock rendering** — add to `ui_tick_anim()`, before the spinner animation block:

  ```cpp
  // Clock: update lbl_title once per minute when clock is on and NTP is synced.
  if (clock_pref_on() && time(nullptr) > 1000000000L) {
      time_t now = time(nullptr);
      struct tm t;
      localtime_r(&now, &t);
      if (t.tm_min != s_clock_last_min) {
          s_clock_last_min = t.tm_min;
          char buf[16];
          if (clock_pref_fmt() == 12) {
              int h = t.tm_hour % 12; if (!h) h = 12;
              snprintf(buf, sizeof(buf), "%d:%02d %s", h, t.tm_min,
                       t.tm_hour < 12 ? "AM" : "PM");
          } else {
              snprintf(buf, sizeof(buf), "%d:%02d", t.tm_hour, t.tm_min);
          }
          lv_label_set_text(lbl_title, buf);
      }
  } else if (!clock_pref_on()) {
      if (s_clock_last_min != -2) {   // -2 = "Usage" already set
          s_clock_last_min = -2;
          lv_label_set_text(lbl_title, "Usage");
      }
  }
  ```

  Add forward declarations at top of `ui.cpp` (before `ui_tick_anim`): `bool clock_pref_on(void); int clock_pref_fmt(void);` (these are defined in Task 3's `clock_pref.cpp` but declared here to avoid circular headers). OR: `#include "clock_pref.h"` once it exists in Task 3. Use the `#include` approach — the file doesn't exist yet but the include is harmless if guarded, OR just add the include in Task 3's step and leave a `// TODO: #include "clock_pref.h"` comment that Task 3 replaces.

  > **Simpler approach:** add `#include "clock_pref.h"`, `#include "pace_pref.h"` to `ui.cpp` in this task. The headers will be created in Task 3; the `#include` causes a compile error until then. Task 3 must be completed immediately after Task 2. (Do NOT build until after Task 3.)

- [ ] **Step 6: Implement `ui_clock_invalidate()`** in `ui.cpp`:

  ```cpp
  void ui_clock_invalidate(void) { s_clock_last_min = -1; }
  ```

- [ ] **Step 7: Implement `ui_set_pace_marker(bool on)`** in `ui.cpp`:

  ```cpp
  void ui_set_pace_marker(bool on) {
      s_pace_on = on;
      // Show/hide tick objects; redraw with last known data.
      lv_obj_set_hidden(pace_tick_session, !on);
      lv_obj_set_hidden(pace_tick_weekly, !on);
      if (on && s_last_usage.valid) ui_update(&s_last_usage);
  }
  ```

  Also in `ui_update()`: save a copy to `s_last_usage`, and when `s_pace_on` is true, position and colorize the pace ticks using `pace_elapsed_pct()` and `pace_level()`.

  Pace tick positioning (in `ui_update()`):
  ```cpp
  if (s_pace_on && data->has_session && data->session_reset_mins >= 0) {
      float ep = pace_elapsed_pct(data->session_reset_mins, 300);
      int bar_w = lv_obj_get_width(bar_session);
      lv_obj_set_x(pace_tick_session, (int)(ep / 100.0f * bar_w) - 1);
      lv_obj_set_style_bg_color(bar_session,
          pace_level(data->session_pct, ep) == 0 ? COL_GREEN :
          pace_level(data->session_pct, ep) == 1 ? COL_AMBER : COL_RED,
          LV_PART_INDICATOR);
  }
  // Same for bar_weekly with window_mins = 10080.
  ```

- [ ] **Step 8: Implement `ui_repaint_theme()`** in `ui.cpp`:

  Extract style re-application from `init_usage_screen()`, `init_settings_screen()`, `init_bluetooth_screen()`, `init_wifi_screen()` into `repaint_usage_screen()`, `repaint_settings_screen()`, `repaint_bluetooth_screen()`, `repaint_wifi_screen()` helpers that only call `lv_obj_set_style_*` and `lv_obj_invalidate` — no object creation.

  `ui_repaint_theme()` calls all four helpers then `lv_obj_invalidate(lv_screen_active())`.

  The repaint helpers must reference all persistent objects whose color comes from `COL_*` macros: panels, bars, labels, icons. Use `lv_obj_set_style_bg_color`, `lv_obj_set_style_text_color`, etc. The `COL_*` macros are already dynamic after Step 1 so just re-calling the same setter picks up the new palette.

- [ ] **Step 9: Add includes** — at top of `ui.cpp`, add:
  ```cpp
  #include "clock_pref.h"
  #include "pace_pref.h"
  #include "pace.h"
  ```
  These will cause compile errors until Task 3 creates the headers.

- [ ] **Step 10: Build verify (after Task 3 is done)** — build is deferred to Task 3 Step 5.

---

## Task 3: Preference Modules

**Files:**
- Create: `firmware/src/clock_pref.h`, `firmware/src/clock_pref.cpp`
- Create: `firmware/src/pace_pref.h`, `firmware/src/pace_pref.cpp`
- Create: `firmware/src/button_pref.h`, `firmware/src/button_pref.cpp`
- Create: `firmware/src/theme_pref.h`, `firmware/src/theme_pref.cpp`

**Interfaces:**
- Consumes: `theme_palette.h`, `theme_presets.h`, `button_action.h`, `ui.h` (Task 2 declarations)
- Produces (see spec §New files for full signatures):
  - `clock_pref.h`: `void clock_pref_load(void); void clock_pref_apply_tz(void); bool clock_pref_on(void); int clock_pref_fmt(void); const char* clock_pref_tz(void); void clock_pref_set(bool on, int fmt, const char* posix_tz);`
  - `pace_pref.h`: `void pace_pref_load(void); bool pace_pref_get(void); void pace_pref_set(bool on);`
  - `button_pref.h`: `void button_pref_load(void); int32_t button_pref_get(ButtonSlot slot); void button_pref_set(ButtonSlot slot, int32_t v);`
  - `theme_pref.h`: `void theme_pref_load(void); void theme_pref_set_by_name(const char* preset_id); const char* theme_pref_current_id(void);`

- [ ] **Step 1: Create `clock_pref.h` and `clock_pref.cpp`**

  NVS keys: `clk_on` (bool, default true), `clk_fmt` (uint8, default 24), `clk_tz` (string, default `"UTC0"`).

  `clock_pref_apply_tz()`: `setenv("TZ", clock_pref_tz(), 1); tzset();`

  `clock_pref_set()`: write NVS, update static vars, call `setenv`/`tzset`, call `ui_clock_invalidate()`.

  `clock_pref_load()`: read NVS into static vars. Does NOT call `ui_clock_invalidate()` (called at boot before `ui_init()`).

- [ ] **Step 2: Create `pace_pref.h` and `pace_pref.cpp`**

  NVS key: `pace_on` (bool, default false).

  `pace_pref_load()`: reads NVS, does NOT call `ui_set_pace_marker()` (called before `ui_init()`; the main.cpp plan in Task 6 calls both in order).

  `pace_pref_set(bool on)`: writes NVS, calls `ui_set_pace_marker(on)`.

- [ ] **Step 3: Create `button_pref.h` and `button_pref.cpp`**

  NVS keys: `btn_k1` (int32, default 1), `btn_k2` (int32, default 2), `btn_k3` (int32, default 0).

  `button_pref_set()`: validates with `button_action_valid(v)` before writing; ignores invalid values.

  `button_pref_load()`: reads NVS into three static vars. Does NOT call any UI function.

- [ ] **Step 4: Create `theme_pref.h` and `theme_pref.cpp`**

  NVS key: `theme_id` (string, default `"dark"`).

  `theme_pref_load()`: reads `theme_id` from NVS; calls `theme_pref_set_by_name()` if non-empty, otherwise loads Dark preset. Does NOT call `ui_repaint_theme()` (called before `ui_init()`; `ui_init()` builds objects with the now-live palette).

  `theme_pref_set_by_name(id)`: linear search through `theme_presets[]`; if found, calls `theme_palette_set(preset.colors)`, writes NVS, calls `ui_repaint_theme()`. If not found, returns without changing NVS.

  `theme_pref_current_id()`: returns stored `theme_id` (static var, not a fresh NVS read).

- [ ] **Step 5: Build verify**

  Run: `pio run -d firmware -e waveshare_amoled_216 -e waveshare_amoled_18`
  Expected: SUCCESS (all new headers exist; `ui.cpp` includes compile cleanly).

- [ ] **Step 6: Commit**

  ```bash
  git add firmware/src/theme.h firmware/src/ui.h firmware/src/ui.cpp \
          firmware/src/clock_pref.h firmware/src/clock_pref.cpp \
          firmware/src/pace_pref.h firmware/src/pace_pref.cpp \
          firmware/src/button_pref.h firmware/src/button_pref.cpp \
          firmware/src/theme_pref.h firmware/src/theme_pref.cpp
  git commit -m "feat: dynamic theme palette, clock/pace/button/theme preference modules, UI extensions"
  ```

---

## Task 4: Pending Wi-Fi Credentials

**Files:**
- Modify: `firmware/src/provisioning.h`
- Modify: `firmware/src/provisioning.cpp`
- Modify: `firmware/src/wifi_poller.cpp`

**Interfaces:**
- Produces (new additions to `provisioning.h`):
  ```cpp
  void provisioning_save_pending_wifi(const char* ssid, const char* pass);
  bool provisioning_has_pending_wifi(void);
  void provisioning_get_trial_creds(String& ssid_out, String& pass_out);
  void provisioning_promote_pending(void);
  void provisioning_revert_pending(void);
  bool provisioning_trial_timed_out(void);     // true when >30s since get_trial_creds() was called
  bool provisioning_take_reverted(void);        // true once after revert; clears the flag
  bool provisioning_no_revert(void);
  void provisioning_set_no_revert(bool v);
  ```
- Consumes: `provisioning.h` (new API), existing `WiFi.begin()` path in `wifi_poller.cpp`

- [ ] **Step 1: Add new API declarations to `provisioning.h`** — append the 9 declarations above after the existing `provisioning_save_webpass` line.

- [ ] **Step 2: Implement in `provisioning.cpp`**

  NVS keys: `ssid_new`, `pass_new` (pending slots), `no_revert` (bool), `reverted` (bool).

  `provisioning_save_pending_wifi(ssid, pass)`: if `provisioning_no_revert()` is true, write directly to `ssid`/`pass` current slots and return (bypasses pending flow). Otherwise write to `ssid_new`/`pass_new`.

  `provisioning_get_trial_creds(ssid_out, pass_out)`: if `ssid_new` is non-empty, return `ssid_new`/`pass_new` and record `s_trial_start = millis()`, `s_trial_active = true`. Otherwise return current `ssid`/`pass`, `s_trial_active = false`.

  `provisioning_trial_timed_out()`: returns `s_trial_active && (millis() - s_trial_start) > 30000UL`.

  `provisioning_promote_pending()`: if `ssid_new` non-empty, write `ssid_new`→`ssid`, `pass_new`→`pass`, clear `ssid_new`/`pass_new`, `s_trial_active = false`.

  `provisioning_revert_pending()`: clear `ssid_new`/`pass_new`, `s_trial_active = false`, write `reverted = true` to NVS, set `s_reverted = true`.

  `provisioning_take_reverted()`: returns `s_reverted` and sets it to false. (Also reads/clears `reverted` NVS key on `provisioning_init()`.)

  `provisioning_has_pending_wifi()`: returns `ssid_new` NVS key is non-empty (read from static var, populated at `provisioning_init()`).

- [ ] **Step 3: Update `wifi_poller.cpp`**

  In `begin_connect()`: change to call `provisioning_get_trial_creds(ssid, pass)` instead of `provisioning_get_ssid()`/`provisioning_get_pass()`.

  In `WIFI_ST_CONNECTING` branch of `wifi_poller_tick()`, before the timeout check: add:
  ```cpp
  if (provisioning_has_pending_wifi() && provisioning_trial_timed_out()) {
      WiFi.disconnect();
      provisioning_revert_pending();
      begin_connect();
      return;
  }
  ```

  In the `WIFI_ST_CONNECTING → WIFI_ST_CONNECTED` transition (where `configTime` is called): add after `configTime`:
  ```cpp
  provisioning_promote_pending();
  clock_pref_apply_tz();
  ```
  Add `#include "clock_pref.h"` to `wifi_poller.cpp`.

- [ ] **Step 4: Build verify**

  Run: `pio run -d firmware -e waveshare_amoled_216 -e waveshare_amoled_18`
  Expected: SUCCESS.

- [ ] **Step 5: Commit**

  ```bash
  git add firmware/src/provisioning.h firmware/src/provisioning.cpp \
          firmware/src/wifi_poller.cpp
  git commit -m "feat: pending Wi-Fi credential trial with 30s safety revert"
  ```

---

## Task 5: Settings Web UI Expansion

**Files:**
- Modify: `firmware/src/settings_server.cpp`

**Interfaces:**
- Consumes: `clock_pref.h`, `pace_pref.h`, `button_pref.h`, `theme_pref.h`, `theme_presets.h`, `tz_list.h`, `provisioning.h` (new API), `brightness.h`, `chime_pref.h`
- New routes registered in `settings_server_init()`:
  - `GET /settings` → same handler as `GET /`
  - `POST /save-settings` → apply clock/pace/button/theme/brightness/chime
  - `POST /save-connection` → apply token and/or pending Wi-Fi creds (may reboot)

- [ ] **Step 1: Add `#include` headers** to `settings_server.cpp`:
  ```cpp
  #include "clock_pref.h"
  #include "pace_pref.h"
  #include "button_pref.h"
  #include "theme_pref.h"
  #include "theme_presets.h"
  #include "tz_list.h"
  #include "brightness.h"
  #include "chime_pref.h"
  ```

- [ ] **Step 2: Extend `build_page()`** to generate and append HTML sections 3–8 (clock, pace, buttons, theme, brightness, chime) after the existing Clawd section. Each section is a `snprintf` block appended to the HTML string. Use `%s selected` patterns to pre-select current values.

  Clock section: checkbox `clk_on`, radio `clk_fmt` (24/12), `<select name="clk_tz">` with `<optgroup>` by region from `tz_list[]`. The currently stored POSIX string (`clock_pref_tz()`) selects the matching option.

  Pace section: checkbox `pace_on`.

  Buttons section: three `<select>` for `btn_k1/k2/k3`. Options: 0=None, 1=Space, 2=Shift+Tab, 3=Tab, 4=Enter, 5=Escape. (Omit 6=Brightness.)

  Theme section: `<select name="theme_id">` with 11 options from `theme_presets[]`. Each option includes a small inline SVG swatch of three squares (bg, panel, accent) using `theme_presets[i].colors[TH_BG]` etc. Format: `<option value="%s"%s>%s <svg …>…</svg></option>`.

  Brightness section: `<input type="range" name="brt_lvl" min="10" max="100" value="%d">`.

  Chime section: `<input type="checkbox" name="chime"%s>` pre-checked if `chime_pref_get()`.

  Sections 3–8 share one `<button type="submit">Save device settings</button>` POSTing to `/save-settings`.

- [ ] **Step 3: Add Connection section** to `build_page()` — separate `<form method="POST" action="/save-connection">` with: SSID text field (pre-filled with `provisioning_get_ssid()`), password field (empty, placeholder "leave blank to keep current"), token password field (pre-filled with first 20 chars of `provisioning_get_token()` + `"…"` if longer), safety-revert checkbox (default checked, maps to `no_revert=false`), save button "Save & Reboot" / "Save".

- [ ] **Step 4: Implement `handle_save_settings()`** handler:
  - Read `clk_on` (checkbox → present=true), `clk_fmt` (radio, "12" or "24"), `clk_tz` (select value = POSIX string). Call `clock_pref_set(on, fmt, tz)`.
  - Read `pace_on`. Call `pace_pref_set(on)`.
  - Read `btn_k1`, `btn_k2`, `btn_k3` as integers. Call `button_pref_set(slot, v)` for each (validation is inside `button_pref_set`).
  - Read `theme_id`. Call `theme_pref_set_by_name(id)`.
  - Read `brt_lvl` as int, clamp to [10, 100]. Call `brightness_set_level(val)`.
  - Read `chime` (checkbox). Call `chime_pref_set(on)`.
  - Call `ui_settings_refresh()`.
  - Redirect to `/?saved=1`.

- [ ] **Step 5: Implement `handle_save_connection()`** handler:
  - Read `ssid`, `pass`, `token`, `no_revert` (checkbox).
  - Call `provisioning_set_no_revert(!s_server.hasArg("no_revert") == false)` — checkbox present = safety on = `no_revert=false`.

    > Wait, the spec says: "Safety revert — checkbox: Revert… (checked by default; unchecked maps to `no_revert = true`)". So `no_revert = !s_server.hasArg("safety_revert")`. Use `name="safety_revert"` for the checkbox.

  - If `token` non-empty: validate starts with `"sk-ant-"`, then write directly to current token slot via `provisioning_save_wifi` or add a `provisioning_save_token(const char*)` helper. Call without changing ssid/pass.
  - If `ssid` non-empty (SSID changed): call `provisioning_save_pending_wifi(ssid.c_str(), pass.length() ? pass.c_str() : provisioning_get_pass().c_str())`. Respond with rebooting page, delay 500ms, `ESP.restart()`.
  - If only `pass` non-empty without `ssid`: treat as SSID unchanged, same pending flow with `provisioning_get_ssid()` as SSID.
  - If both blank: no wifi change; redirect to `/?saved=1`.

  Token-only path (no ssid/pass change): respond with redirect `/?saved=1`.
  Wi-Fi change path: send a "Rebooting…" HTML page then restart.

- [ ] **Step 6: Register new routes** in `settings_server_init()`:
  ```cpp
  s_server.on("/settings",         HTTP_GET,  handle_root);
  s_server.on("/save-settings",    HTTP_POST, handle_save_settings);
  s_server.on("/save-connection",  HTTP_POST, handle_save_connection);
  ```

- [ ] **Step 7: Build verify**

  Run: `pio run -d firmware -e waveshare_amoled_216 -e waveshare_amoled_18`
  Expected: SUCCESS.

- [ ] **Step 8: Commit**

  ```bash
  git add firmware/src/settings_server.cpp
  git commit -m "feat: full settings web UI — clock, pace, buttons, theme, brightness, chime, connection"
  ```

---

## Task 6: main.cpp Wiring

**Files:**
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: `clock_pref.h`, `theme_pref.h`, `pace_pref.h`, `button_pref.h`, `button_action.h`, `provisioning.h` (new `provisioning_take_reverted`)

- [ ] **Step 1: Add includes** to `main.cpp`:
  ```cpp
  #include "clock_pref.h"
  #include "theme_pref.h"
  #include "pace_pref.h"
  #include "button_pref.h"
  #include "button_action.h"
  ```

- [ ] **Step 2: Add pref loads to `setup()`** after `chime_pref_load()`:
  ```cpp
  clock_pref_load();
  theme_pref_load();     // before ui_init() — palette live when LVGL objects are styled
  ```
  And after `ui_init()`:
  ```cpp
  pace_pref_load();      // after ui_init() — calls ui_set_pace_marker() which needs bar objects
  button_pref_load();
  ```

- [ ] **Step 3: Replace hardcoded HID sends with `button_pref` decode** in `loop()`.

  Primary button press (currently `ble_keyboard_press(0x2C, 0)`):
  ```cpp
  ButtonBinding b = button_action_decode(button_pref_get(BTN_SLOT_PRIMARY));
  if (b.kind == BTN_KIND_HID) ble_keyboard_press(b.keycode, b.mods);
  ```
  Release: `ble_keyboard_release()` (unchanged — no binding needed).

  Secondary button press (currently `ble_keyboard_press(0x2B, 0x02)`):
  ```cpp
  ButtonBinding b = button_action_decode(button_pref_get(BTN_SLOT_SECONDARY));
  if (b.kind == BTN_KIND_HID) ble_keyboard_press(b.keycode, b.mods);
  ```

  PWR button HID event (new — alongside existing `ui_cycle_screen()`):
  ```cpp
  ButtonBinding b = button_action_decode(button_pref_get(BTN_SLOT_PWR));
  if (b.kind == BTN_KIND_HID) { ble_keyboard_press(b.keycode, b.mods); ble_keyboard_release(); }
  ```
  PWR always cycles screens regardless of k3; k3 only controls whether a HID event fires.

  `BTN_KIND_BRIGHTNESS` (preset 6) → no-op (brightness slider is the only control in this fork; a stored 6 is silently ignored).

- [ ] **Step 4: Add revert flag check** in `loop()`, after `wifi_poller_tick()`:
  ```cpp
  if (provisioning_take_reverted()) {
      ui_set_status(UI_STATUS_WARN,
          "Wi-Fi change failed - reverted to previous settings");
  }
  ```

- [ ] **Step 5: Build verify**

  Run: `pio run -d firmware -e waveshare_amoled_216 -e waveshare_amoled_18`
  Expected: SUCCESS.

- [ ] **Step 6: Commit**

  ```bash
  git add firmware/src/main.cpp
  git commit -m "feat: wire clock/theme/pace/button prefs and revert flag into main"
  ```

---

## Post-Implementation Hardware Checklist

These are manually verified after flashing; not automated:

- [ ] Connect to Wi-Fi; navigate to `http://clawdmeter.local/settings` — page loads.
- [ ] Clock: enable clock, set TZ to Europe/Berlin, verify displayed time matches phone's clock.
- [ ] Clock: disable clock, verify "Usage" title reappears immediately.
- [ ] Pace marker: enable, observe tick appears on session and weekly bars.
- [ ] Button remap: set Primary to Tab (`btn_k1=3`), verify Tab HID event reaches connected host.
- [ ] Theme: switch to Nord, screenshot; switch back to Dark, screenshot. Verify full repaint.
- [ ] Captive portal: press "Start Hotspot", verify portal still works; stop portal, verify settings page resumes at `clawdmeter.local/settings`.
- [ ] Credential update — happy path: submit correct new Wi-Fi credentials; device reboots; `clawdmeter.local` reachable again.
- [ ] Credential update — revert path: submit wrong password with safety on; device reboots; fails; reverts; status overlay says "Wi-Fi change failed - reverted to previous settings"; device reachable on old network.
- [ ] Safety disabled: submit wrong password with safety unchecked; device stays disconnected (no overlay).
- [ ] Token update: submit new token without changing SSID/pass; no reboot; next poll uses new token.
- [ ] Screenshot `out.png`: `./screenshot.sh out.png` — verify clock renders in title area.
