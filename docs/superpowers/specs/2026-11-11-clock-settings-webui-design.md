# Clock, Themes, Pace Marker, Button Config, Credential Update & Settings Web UI

**Date:** 2026-11-11  
**Branch:** wifi-native  
**Author:** Oliver van Porten

---

## Context

The wifi fork polls the Anthropic API directly from the ESP32 — no desktop app, no daemon. The
upstream (Clawdmeter) delivers device settings (clock, pace marker, button mapping, theme, etc.)
via the Rust desktop app over BLE. This fork has no equivalent mechanism: today's configuration
story is a one-time captive portal (SSID/pass/token) and a few serial commands.

The firmware already has NTP sync (`configTime` runs after every WiFi connect), so the clock
feature is simpler here than upstream: no epoch handshake needed. The design adds the clock,
ports two other pure upstream features (pace marker, button actions), adds runtime theme
selection, and builds a lightweight settings web UI accessible from any browser on the LAN.
That same web UI also covers credential updates (Wi-Fi SSID/password and Anthropic token), with
a safety-revert mechanism for Wi-Fi changes so a typo cannot brick the device.

---

## Goals

1. Replace the "Usage" title with the current local time (opt-in, TZ-aware, 12h/24h).
2. Add a pace marker to the usage bars (opt-in; pure port from upstream).
3. Make button actions configurable (presets; pure port from upstream).
4. Add runtime theme selection from upstream's 11 preset palettes.
5. Allow updating Wi-Fi credentials and Anthropic token from the settings web UI.
   - Wi-Fi changes: device reboots and tries new credentials; reverts to previous if they fail
     within 30 s, then flags the rollback on-screen.
   - The revert safety can be disabled per-save (for deliberate network moves).
   - Serial commands always work regardless of web UI state.
6. Surface all device settings — new and existing (brightness, chime) — via a web page at
   `http://clawdmeter.local/settings` while the device is connected to WiFi.
7. No new dependencies beyond what the ESP32 Arduino core already ships.
8. No restart required when changing device preferences. Wi-Fi credential changes reboot.

---

## Out of scope

- Custom keyboard shortcut entry in the web UI (presets only for buttons).
- Liveliness / attention animation (require the Clawd director subsystem).
- Custom color picker in the web UI (preset themes only; 11 presets cover the common cases).
- Upstream's `acct:"week"` path changes (no-5h-limit accounts) — already handled in the fork.

---

## Architecture

### Shared WebServer (port 80)

The captive portal and the settings server cannot both own a `WebServer` on port 80 at the same
time. They are mutually exclusive modes (the device is never simultaneously in AP and station
mode), so they share one `WebServer` instance owned by `settings_server`. The captive portal
module is refactored internally but its external API (`captive_portal_start/stop/tick/is_active`)
is unchanged.

`settings_server` exports one extra call that the captive portal uses:

```cpp
void settings_server_begin_portal(void);  // stop settings routes, start portal routes, s_server.begin()
void settings_server_tick(void);          // s_server.handleClient(); called from captive_portal_tick()
```

**Lifecycle:**
```
setup()                  → settings_server_init()        (register settings routes; don't start yet)
wifi connected           → settings_server_start()       (s_server.begin() + mDNS)
captive_portal_start()   → settings_server_stop()        (s_server.stop() + mDNS end)
                           settings_server_begin_portal() (re-register portal routes, s_server.begin())
                           s_dns.start(...)
captive_portal_stop()    → s_dns.stop(); s_server.stop()
                           settings_server_start()        (settings routes, s_server.begin(), mDNS)
```

`captive_portal_tick()` calls `settings_server_tick()` (instead of `s_server.handleClient()`
directly) so that `captive_portal.cpp` has no direct dependency on the WebServer instance.

The settings server runs only in station mode. During the AP-mode captive portal the WebServer
serves the provisioning form, identical to today.

### mDNS

`ESPmDNS` (part of the ESP32 Arduino core, no extra `lib_deps`) advertises:
- hostname `clawdmeter` → resolves to the device's station IP
- service `_http._tcp` port 80

`http://clawdmeter.local/` redirects to `/settings`. The raw IP also works.

### NVS namespaces

All settings use the existing `"clawdmeter"` namespace in `Preferences`. New keys:

| Key | Type | Default | Meaning |
|-----|------|---------|---------|
| `clk_on` | bool | true | Clock displayed on Usage screen |
| `clk_fmt` | uint8 | 24 | 12 or 24 (hour format) |
| `clk_tz` | string | `"UTC0"` | POSIX TZ string |
| `pace_on` | bool | false | Pace marker on usage bars |
| `btn_k1` | int32 | 1 (Space) | PRIMARY button action |
| `btn_k2` | int32 | 2 (Shift+Tab) | SECONDARY button action |
| `btn_k3` | int32 | 0 (None) | PWR button HID action (screen-cycle is always the device behavior) |
| `theme` | string | `""` (Dark) | 66 hex chars — 11 × `rrggbb` palette (same wire format as upstream's `th`) |

| `ssid_new` | string | `""` | Pending Wi-Fi SSID (cleared after trial) |
| `pass_new` | string | `""` | Pending Wi-Fi password (cleared after trial) |
| `no_revert` | bool | false | Skip rollback when true (user opted out of safety) |
| `reverted` | bool | false | Set true when a pending trial fails and reverts; cleared after display |

Existing keys `ssid`, `pass`, `token`, `brt_lvl`, and `chime` are unchanged.
Serial commands (`ssid`, `pass`, `token`) write directly to the current slots and bypass the
pending/trial flow entirely — they are the emergency recovery path and always work.

---

## New files

### `firmware/src/tz_list.h`

Compile-time `const struct { const char* name; const char* posix; }` array of ~50 named
timezones grouped by region (Americas, Europe, Africa, Asia, Pacific). Each entry maps a
friendly name to its POSIX TZ string, e.g.:

```cpp
{"Europe/Berlin",   "CET-1CEST,M3.5.0,M10.5.0/3"},
{"America/New_York","EST5EDT,M3.2.0,M11.1.0"},
{"Asia/Tokyo",      "JST-9"},
```

The list is a header-only array; no .cpp needed. Used by `clock_pref.cpp` and the settings
server's HTML generator.

### `firmware/src/clock_pref.{h,cpp}`

```cpp
void clock_pref_load(void);        // load from NVS; call before ui_init()
void clock_pref_apply_tz(void);    // setenv("TZ",…) + tzset(); call after configTime()
bool clock_pref_on(void);
int  clock_pref_fmt(void);         // 12 or 24
const char* clock_pref_tz(void);   // POSIX string
void clock_pref_set(bool on, int fmt, const char* posix_tz);  // save + apply
```

`clock_pref_apply_tz()` is called by `wifi_poller.cpp` after every `configTime()` call (NTP sync
fires once per connect), so the TZ is always in effect before the first tick.

### `firmware/src/pace_pref.{h,cpp}`

```cpp
void pace_pref_load(void);         // load from NVS; calls ui_set_pace_marker()
bool pace_pref_get(void);
void pace_pref_set(bool on);       // save + call ui_set_pace_marker()
```

Thin wrapper; the actual pace rendering lives in `ui.cpp`.

### `firmware/src/button_pref.{h,cpp}`

```cpp
void    button_pref_load(void);                       // load from NVS
int32_t button_pref_get(ButtonSlot slot);             // current value for slot
void    button_pref_set(ButtonSlot slot, int32_t v);  // save; validated with button_action_valid()
```

`ButtonSlot` and `button_action_valid/decode` come from the ported `button_action.h`.

### `firmware/src/button_action.{h,cpp}`

Verbatim port from upstream. Pure logic (no Arduino types). Covers presets, keyboard chords,
consumer keys. The fork uses only presets (0–6) from the web UI; the full codec is kept so the
NVS format stays upstream-compatible.

### `firmware/src/pace.{h,cpp}`

Verbatim port from upstream. Pure math. `pace_elapsed_pct()` and `pace_level()`.

### `firmware/src/theme_palette.{h,cpp}`

Verbatim port from upstream. Pure (no Arduino, no LVGL). Owns the live palette array and
the `theme_parse_hex` / `theme_format_hex` / `theme_palette_set` / `theme_hex` /
`theme_rgb565` functions.

The fork's `theme.h` currently has hardcoded `lv_color_hex(0x…)` macros. These are **replaced**
with dynamic wrappers:

```cpp
// theme.h (after)
#include "theme_palette.h"
static inline lv_color_t th_col(ThemeColor c) { return lv_color_hex(theme_hex(c)); }
#define THEME_BG      th_col(TH_BG)
#define THEME_PANEL   th_col(TH_PANEL)
// … etc.
```

This change is backward-compatible: `ui.cpp` already uses `COL_BG` etc. via those macros; it
recompiles cleanly. The runtime cost is one integer lookup per style call, negligible.

### `firmware/src/theme_pref.{h,cpp}`

```cpp
void    theme_pref_load(void);           // load from NVS; calls theme_palette_set(); before ui_init()
void    theme_pref_set_by_name(const char* preset_id);  // look up palette, save + apply
const char* theme_pref_current_id(void); // the stored preset id ("dark" if unset)
```

### `firmware/src/theme_presets.h`

Compile-time `const struct { const char* id; const char* name; uint32_t colors[TH_COUNT]; }`
array — the 11 presets from upstream's `themes.rs` translated verbatim:

| id | Name |
|----|------|
| `dark` | Dark (firmware default) |
| `light` | Light |
| `catppuccin_mocha` | Catppuccin Mocha |
| `dracula` | Dracula |
| `gruvbox` | Gruvbox |
| `monokai` | Monokai |
| `nord` | Nord |
| `one_dark` | One Dark |
| `solarized_dark` | Solarized Dark |
| `solarized_light` | Solarized Light |
| `tokyo_night` | Tokyo Night |

### `firmware/src/settings_server.{h,cpp}`

```cpp
void settings_server_init(void);   // register routes; call in setup()
void settings_server_start(void);  // begin WebServer + mDNS; call when WiFi connects
void settings_server_stop(void);   // end WebServer + mDNS; call before captive portal starts
bool settings_server_running(void);
```

The module owns the `WebServer` on port 80 and the `ESPmDNS` instance. Routes:

| Route | Handler |
|-------|---------|
| `GET /` | redirect to `/settings` |
| `GET /settings` | serve settings HTML page |
| `POST /save-settings` | read fields, call pref setters, respond with confirmation |
| Catch-all (AP mode) | provisioning form (registered by captive_portal.cpp at start) |

The `captive_portal_start()` calls `settings_server_stop()` then directly calls
`s_server.begin()` with the portal routes. The `captive_portal_stop()` calls `s_server.stop()`
then `settings_server_start()`.

Additional routes handled by `settings_server`:

| Route | Handler |
|-------|---------|
| `POST /save-connection` | Update token (immediate) and/or Wi-Fi creds (pending + reboot) |

#### Settings HTML page

A single self-contained HTML page (inline in `settings_server.cpp`, styled to match the
captive portal aesthetic: `#1a1a1a` background, `#e8e0d4` text, `#d97757` accent). Five
sections with form fields:

1. **Clock**
   - Checkbox: "Show clock on Usage screen" (`clk_on`)
   - Radio: "24-hour" / "12-hour" (`clk_fmt`)
   - `<select>` dropdown: timezone (`clk_tz`), options generated from `tz_list.h`, grouped with
     `<optgroup label="...">` by region. The currently stored POSIX string selects the matching
     option.

2. **Pace marker**
   - Checkbox: "Show pace marker on usage bars" (`pace_on`)
   - One-line description: "Marks where usage would be if spent evenly; green = on pace,
     amber = ahead, red = at limit."

3. **Buttons**
   - Three `<select>` dropdowns: "Primary (left)", "Secondary (right)", "PWR" (`btn_k1/k2/k3`).
   - Options: None / Space (voice PTT) / Shift+Tab (mode toggle) / Tab / Enter / Escape.
   - Note: `BTN_ACT_BRIGHTNESS` (preset 6) is excluded. The fork uses a continuous brightness
     slider (no presets to cycle); that upstream preset has no meaningful equivalent here.
   - PWR always cycles screens as a device behavior regardless of k3; k3 only controls the HID
     event that fires alongside it (default: none).

4. **Theme**
   - `<select>` dropdown of the 11 preset names. The currently stored preset id is pre-selected.
   - Each `<option>` includes a small inline color swatch (three squares: bg, panel, accent)
     rendered as a short inline SVG data URI so the user can preview at a glance without
     opening the page on the device.

5. **Brightness**
   - `<input type="range" min="10" max="100">` (`brt_lvl`)

6. **Chime**
   - Checkbox: "Play session-reset chime" (`chime`)

7. **Connection** *(separate form, separate POST — triggers a reboot)*
   - **Wi-Fi network** — SSID text field (pre-filled with current SSID)
   - **Wi-Fi password** — password field (empty; placeholder "leave blank to keep current")
   - **Anthropic token** — password field (pre-filled with first 20 chars + "…"; placeholder
     "leave blank to keep current")
   - **Safety revert** — checkbox: "Revert to previous Wi-Fi settings if new connection fails
     within 30 s" (checked by default; unchecked maps to `no_revert = true`)
   - Save button: "Save & Reboot" (or "Save" for token-only, no reboot if SSID/pass unchanged)
   - If only the token changed: apply immediately (no reboot), confirm inline.
   - If SSID or password changed: write to pending NVS slots, respond with a "Rebooting…" page,
     call `ESP.restart()` after a 500 ms delay so the HTTP response reaches the browser.

Sections 1–6 share one "Save" button and POST to `/save-settings` (apply live, no reboot).
Section 7 POSTs to `/save-connection` (may reboot).

---

## Modified files

### `firmware/src/provisioning.{h,cpp}`

New API additions (existing API unchanged):

```cpp
// Pending Wi-Fi credentials (written by /save-connection, tried on next boot).
void provisioning_save_pending_wifi(const char* ssid, const char* pass);
bool provisioning_has_pending_wifi(void);  // pending slots non-empty

// Called by wifi_poller when it starts connecting: if pending is present,
// returns the pending creds and arms the trial timer. Otherwise returns current.
void provisioning_get_trial_creds(String& ssid_out, String& pass_out);

// Called by wifi_poller on successful connect: promote pending → current, clear pending.
void provisioning_promote_pending(void);

// Called by wifi_poller on trial timeout: restore current creds, set reverted = true, clear pending.
void provisioning_revert_pending(void);

// True once after a revert, then clears the flag. Called by main.cpp to show the status overlay.
bool provisioning_take_reverted(void);

// no_revert flag (written by /save-connection, read in provisioning_save_pending_wifi).
// When true, save-connection writes directly to current slots (bypasses pending entirely).
bool provisioning_no_revert(void);
void provisioning_set_no_revert(bool v);
```

**Trial timer:** `provisioning_get_trial_creds()` records `millis()` and the trial timeout
(30 s, `PENDING_TRIAL_MS`). `wifi_poller` calls `provisioning_trial_timed_out()` from its
`WIFI_ST_CONNECTING` branch when the trial has been active longer than the timeout.

**Serial path unchanged:** `provisioning_handle_cmd("ssid …")` / `pass` / `token` write
directly to `ssid`/`pass`/`token` NVS keys and in-memory, with no involvement of pending slots.
This is explicitly the emergency recovery path and must stay unconditional.

### `firmware/src/captive_portal.{h,cpp}`

- Extract the `WebServer s_server` global to `settings_server.cpp`.
- `captive_portal_start()`: calls `settings_server_stop()`, re-registers portal routes on the
  shared server, then calls `s_server.begin()`.
- `captive_portal_stop()`: calls `s_server.stop()`, then `settings_server_start()`.
- `captive_portal_tick()`: still calls `s_server.handleClient()` and `s_dns.processNextRequest()`.
- External API, portal HTML, and save handler are unchanged.

### `firmware/src/wifi_poller.cpp`

**Pending credential trial:**

`wifi_poller_init()` calls `provisioning_get_trial_creds()` to obtain the SSID/pass to try
(pending if present, current otherwise). This replaces the existing direct call to
`provisioning_get_ssid/pass`.

In `wifi_poller_tick()`, in the `WIFI_ST_CONNECTING` branch, add a trial-timeout check:
```cpp
if (provisioning_has_pending_wifi() && provisioning_trial_timed_out()) {
    WiFi.disconnect();
    provisioning_revert_pending();   // clears pending, sets reverted flag
    begin_connect();                 // reconnect with current (reverted) creds
    return;
}
```

In the `WIFI_ST_CONNECTING → WIFI_ST_CONNECTED` transition:
```cpp
provisioning_promote_pending();  // no-op if no pending; promotes if trial succeeded
configTime(0, 0, "pool.ntp.org");
clock_pref_apply_tz();
settings_server_start();
```

`settings_server_start()` is idempotent (no-op if already running).

### `firmware/src/ui.{h,cpp}`

**Clock rendering:**

Add static vars `clock_last_min` (int, -1) in `ui.cpp`. In `ui_tick_anim()`, when
`clock_pref_on()` is true and `time(nullptr) > 1000000000L` (NTP synced):

```cpp
time_t now = time(nullptr);
struct tm t;
localtime_r(&now, &t);
if (t.tm_min != clock_last_min) {
    clock_last_min = t.tm_min;
    char buf[16];
    if (clock_pref_fmt() == 12) {
        int h = t.tm_hour % 12; if (!h) h = 12;
        snprintf(buf, sizeof(buf), "%d:%02d %s", h, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
    } else {
        snprintf(buf, sizeof(buf), "%d:%02d", t.tm_hour, t.tm_min);
    }
    lv_label_set_text(lbl_title, buf);
}
```

When `clock_pref_on()` is false, or NTP not synced, `lbl_title` shows "Usage". On a settings
change, `clock_last_min` is reset to -1 so the title updates immediately.

Add `ui_clock_invalidate()` (called by `clock_pref_set()`) to trigger the immediate redraw.

**Pace marker:**

Add `ui_set_pace_marker(bool)` to `ui.h`. In `ui.cpp`:
- Two thin vertical bar objects (`lv_obj_t* tick_session, tick_weekly`), 2 px wide, full bar
  height, hidden by default. Created inside `build_usage_screen()`.
- `ui_set_pace_marker(bool on)` sets `pace_marker` static bool and forces a full `ui_update()`
  redraw with the last-known usage data (store a copy: `static UsageData s_last_data`).
- In `ui_update()`, when `pace_marker` is true: position each tick at
  `pace_elapsed_pct(reset_mins, window_mins)` and color the bar fill via `pace_level()`.

**Theme repaint:**

`theme_pref_set_by_name()` calls `theme_palette_set()` to update the live palette array, then
calls `ui_repaint_theme()`. That function re-applies `lv_obj_set_style_bg_color()` and all
other LVGL style properties that use `COL_*` on every persistent object (all panels, bars,
labels, icons). It then calls `lv_obj_invalidate(lv_screen_active())` to flush. The repaint
function is extracted from the existing `build_*_screen()` helpers into a separate
`repaint_*_screen()` path that only touches style properties, not layout or structure.

The `theme.h` macro change (`THEME_BG` etc. → dynamic `th_col(TH_*)` wrappers) means every
`lv_obj_set_style_bg_color(obj, THEME_BG, 0)` call already resolves from the live palette at
call time. `ui_repaint_theme()` therefore just re-calls all those style setters and invalidates.

**`ui_settings_refresh()`** — extended to also reflect `pace_pref_get()` and
`button_pref_get()` (the settings screen doesn't show buttons, but calling `ui_settings_refresh`
after a web save keeps the on-device brightness slider and chime switch in sync with NVS).

### `firmware/src/main.cpp`

**`setup()`** additions (after `chime_pref_load()`):
```cpp
clock_pref_load();
theme_pref_load();     // before ui_init(): applies the saved palette so the UI is built in it
// … ui_init() …
pace_pref_load();      // calls ui_set_pace_marker() — must be after ui_init()
button_pref_load();
settings_server_init();
```

Order constraints:
- `theme_pref_load()` before `ui_init()` — the palette must be live when LVGL objects are styled.
- `pace_pref_load()` after `ui_init()` — calls `ui_set_pace_marker()` which needs the bar objects.

**Button handling** in `loop()`: replace hardcoded `ble_keyboard_press(0x2C, 0)` etc. with calls
through `button_action_decode(button_pref_get(BTN_SLOT_PRIMARY))`. The decode returns a
`ButtonBinding`; the handler sends the appropriate HID report.

PWR always cycles screens (`ui_cycle_screen()` / `splash_next()`) as a device behavior; k3
controls the HID event that fires on a PWR press. `BTN_ACT_BRIGHTNESS` (preset 6) is not
offered in the web UI because the fork removed brightness presets in favor of a continuous
slider. The decode of a stored value of 6 therefore maps to `BTN_KIND_BRIGHTNESS` which
the button handler treats as a no-op (brightness cycle is unavailable).

**Revert flag check** in `loop()` (after `wifi_poller_tick()`):
```cpp
if (provisioning_take_reverted()) {
    // Pending Wi-Fi credentials failed — show a persistent status overlay.
    ui_set_status(UI_STATUS_WARN,
        "Wi-Fi change failed - reverted to previous settings");
}
```
`ui_set_status` with `UI_STATUS_WARN` pins the message until a successful poll clears it
(existing behavior — the wifi-status overlay logic in main.cpp already does this).

---

## Interaction between web UI and on-device settings

When the web UI saves brightness or chime, it calls the same setters as the on-device settings
screen. `ui_settings_refresh()` is called after a web save so the slider/switch on the settings
screen visually reflects the new value. No restart needed.

---

## Error handling

- NTP not synced (`time(nullptr) < 1000000000L`): clock rendering skipped; "Usage" title shown.
- Unknown TZ POSIX string: `setenv`/`tzset` silently falls back to UTC; the UI still shows time.
- `/save-settings` with an out-of-range brightness (< 10 or > 100): clamped to [10, 100].
- `/save-settings` with an invalid button action integer: ignored (keeps previous value).
- `/save-settings` with an unknown theme preset id: ignored (keeps previous palette).
- Stored `theme` NVS string with wrong length or non-hex chars: `theme_pref_load()` falls back
  to Dark silently.
- `/save-connection` with blank SSID and blank password: treat as token-only update (no reboot).
- `/save-connection` with blank token: keep existing token.
- `/save-connection` with blank password: keep existing password (only SSID changed — still
  triggers pending/reboot flow, since the SSID changed).
- Revert: if device has never successfully connected with current creds (e.g. pending was the
  first-ever credentials and failed), `provisioning_revert_pending()` leaves `ssid`/`pass` as
  empty strings; the existing `WIFI_POLL_NO_CREDS` path and auto-portal logic handle this
  naturally (the device falls into the captive-portal flow as it would on a fresh flash).

---

## Testing

- Build both envs (`pio run -d firmware -e waveshare_amoled_216 -e waveshare_amoled_18`).
- Hardware: connect to WiFi, navigate to `http://clawdmeter.local/settings` from a phone browser,
  change each setting, verify live update on device.
- Clock: set TZ to a region 2+ hours from UTC, verify displayed time matches the phone.
- Pace marker: enable, observe bar fill and tick position change.
- Button remap: set Primary to Tab, verify Tab HID report reaches the host.
- Existing captive portal: press "Start Hotspot", verify portal still works, verify settings
  server resumes after stopping the portal.
- Theme: switch to Nord, verify the display repaints live (background, bars, text all change).
  Switch back to Dark. Screenshot both.
- Credential update — happy path: submit correct new Wi-Fi credentials, device reboots,
  connects, `clawdmeter.local` is reachable again, settings page shows new SSID.
- Credential update — revert path: submit deliberately wrong password with safety on, device
  reboots, fails to connect within 30 s, reverts to previous network, status overlay shows
  "Wi-Fi change failed — reverted to previous settings", device is reachable again.
- Safety disabled: submit wrong password with "revert" unchecked, device reboots and stays
  disconnected (no overlay — the device simply couldn't connect; existing wifi-fail handling).
- Token update: submit new token without changing SSID/pass, no reboot, next poll uses new token.
- Serial: with the device connected, run `ssid newsid` over serial, verify it takes effect
  (direct NVS write, no pending, existing reconnect path).
- Screenshot command: take a screenshot with clock on and off to verify title render.
