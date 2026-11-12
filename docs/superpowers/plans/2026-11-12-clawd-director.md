# Clawd Director Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the upstream Clawd director subsystem Phase 1 so Clawd idles in the bottom-right corner of the usage screen and visibly reacts to Claude Code hook events.

**Architecture:** A `clawd_anim.{h,cpp}` wrapper converts the fork's 20×20 `splash_anim_def_t` data into the upstream `ClawdAnim` API; `mascot.{h,cpp}` renders the corner character via LVGL; `clawd_director.{h,cpp}` drives the state machine (B_HOME / B_CORNER / B_ATTENTION / B_PEEK); `main.cpp` wires hook events → `director_on_payload`; the settings server gains a Clawd section.

**Tech Stack:** C++17, LVGL 9, Arduino ESP32 (pioarduino), PlatformIO, NVS via Preferences

**Spec:** `docs/superpowers/specs/2026-11-11-clawd-director-design.md`

## Global Constraints

- Both `waveshare_amoled_216` and `waveshare_amoled_18` envs must compile after every task.
- `splash_animations.h` is included by exactly **one** translation unit: `clawd_anim.cpp`. Any other TU that now includes it (`splash.cpp`) must be migrated first.
- No new PlatformIO `lib_deps`.
- No `#ifdef BOARD_*` in any shared file.
- Phase 2 files (`performer.{h,cpp}`, `face.{h,cpp}`) are out of scope; stub their guards.
- Corner mascot cell size: **`cell = 3`** (gives 60×60 px mascot on 480-wide board). This is a fork adaptation — upstream computes cell differently for a wider art stage.
- The fork's `theme.h` has no `theme_clawd_art()` / `theme_hex()`. Phase 1 uses raw palettes; `clawd_anim_recolor()` is a no-op stub.

## Fork Data-Format Incompatibilities

These affect Tasks 1 and 4 and are the most likely source of build failures:

| Issue | Fork reality | Upstream expectation | Resolution |
|-------|-------------|---------------------|------------|
| Palette size | `SPLASH_PALETTE_SIZE 10` | `CLAWD_PALETTE_SIZE 16` | Define `CLAWD_PALETTE_SIZE 10` |
| Frames field type | `const uint8_t (*frames)[400]` (2D) | `const uint8_t*` (flat) | `a.frames = &splash_anims[i].frames[0][0]` |
| Missing struct fields | `splash_anim_def_t` has no `w`, `h`, `ox`, `oy`, `loop_start`, `loop_end` | `ClawdAnim` has all | Fill in `build()`: `w=20, h=20, ox=0, oy=0, loop_start=0, loop_end=frame_count-1` |
| No `theme_palette.h` | Fork has `theme.h` only (LVGL macros, no `TH_CLAWD`) | `theme_clawd_art()`, `theme_hex()` | Drop recolor machinery; `clawd_anim_recolor()` is empty |

## Review Focus

1. **`clawd_frame(a, f)` indexing** — after the 2D→flat cast, `&a->frames[f * a->w * a->h]` must equal `&splash_anims[i].frames[f][0]`; a one-off or wrong `w` silently corrupts palette indices for every frame.
2. **mascot z-order** — the mascot LVGL object must be created as a child of the SCREEN_USAGE container *after* the usage bar widgets so it renders on top; a wrong parent or creation order produces an invisible or under-painted mascot.
3. **port 80 on reconnect** — `settings_server_start()` is called on Wi-Fi connect; a reconnect cycle must not double-register routes or leave the server in a stopped state; the existing `s_routes_done` guard handles this only if `settings_server_stop()` does not clear it.
4. **`director_on_payload` called before `director_init`** — if a poll arrives before `ui_init()` completes, `director_on_payload` must tolerate a null/uninitialized mascot; add a guard or init early.
5. **`loop_end = frame_count - 1` for single-frame animations** — if any animation has `frame_count = 1`, `loop_end = 0 = loop_start = 0`; mascot must handle a zero-length loop without spinning.

---

### Task 1: Animation API Foundation

Migrate `splash.cpp` away from directly including `splash_animations.h`, then create `clawd_anim.{h,cpp}` as the sole includer with the fork-adapted `ClawdAnim` wrapper.

**Files:**
- Modify: `firmware/src/splash.cpp`
- Create: `firmware/src/clawd_anim.h`
- Create: `firmware/src/clawd_anim.cpp`

**Interfaces:**
- Produces:
  - `int clawd_anim_count(void)` — number of animations
  - `const ClawdAnim* clawd_anim_at(int index)` — NULL when out of range
  - `const ClawdAnim* clawd_anim(const char* name)` — NULL when not found
  - `int clawd_anim_index(const char* name)` — -1 when not found
  - `void clawd_anim_recolor(void)` — no-op stub in Phase 1
  - `static inline const uint8_t* clawd_frame(const ClawdAnim* a, uint16_t frame)` — `&a->frames[(uint32_t)frame * a->w * a->h]`
  - `ClawdAnim` struct: `name`, `w`, `h`, `ox`, `oy`, `frame_count`, `loop_start`, `loop_end`, `palette` (`const uint16_t*`), `frames` (`const uint8_t*`), `holds` (`const uint16_t*`)
  - `CLAWD_PALETTE_SIZE 10`, `CLAWD_ART_W 20`, `CLAWD_ART_H 20`

- [ ] **Step 1: Write `clawd_anim.h`**

  Define constants and struct, keeping the upstream's field layout but with fork values:
  ```cpp
  #define CLAWD_PALETTE_SIZE 10
  #define CLAWD_ART_W        20
  #define CLAWD_ART_H        20
  
  struct ClawdAnim {
      const char*     name;
      uint8_t         w, h;
      uint8_t         ox, oy;
      uint16_t        frame_count;
      uint16_t        loop_start, loop_end;
      const uint16_t* palette;
      const uint8_t*  frames;
      const uint16_t* holds;
  };
  ```
  Include `clawd_gait_cells` / `clawd_hold` inlines verbatim from upstream (harmless if unused). Add all four API declarations. Add `clawd_frame` inline.

- [ ] **Step 2: Write `clawd_anim.cpp`**

  Include `clawd_anim.h` and `splash_animations.h` (the ONLY TU to do so). Compile-time gate:
  ```cpp
  static_assert(SPLASH_PALETTE_SIZE == CLAWD_PALETTE_SIZE, "palette size drifted");
  ```

  `build()` fills `ClawdAnim` from `splash_anim_def_t` with these fork adaptations:
  ```cpp
  a.w = 20; a.h = 20;
  a.ox = 0; a.oy = 0;
  a.loop_start = 0;
  a.loop_end = (s.frame_count > 0) ? s.frame_count - 1 : 0;
  a.frames = &splash_anims[i].frames[0][0];   // 2D → flat
  a.palette = s.palette;                       // raw, no recolor
  ```
  `clawd_anim_recolor()` is an empty function body. No `theme_palette.h` include.

- [ ] **Step 3: Migrate `splash.cpp`**

  Replace the `#include "splash_animations.h"` line with `#include "clawd_anim.h"`.
  Apply the following substitutions throughout the file:

  | Old | New |
  |-----|-----|
  | `SPLASH_ANIM_COUNT` | `clawd_anim_count()` |
  | `splash_anims[i].name` | `clawd_anim_at(i)->name` |
  | `splash_anims[i].category` | `clawd_anim_at(i)->name` (category is not in `ClawdAnim`; use name until a category field is added) |
  | `splash_anims[i].frame_count` | `clawd_anim_at(i)->frame_count` |
  | `splash_anims[i].palette[c]` | `clawd_anim_at(i)->palette[c]` |
  | `mini_anim->frames[mini_frame]` | `clawd_frame(mini_anim, mini_frame)` |
  | `mini_anim->palette[c]` | `mini_anim->palette[c]` (unchanged) |
  | `mini_anim->holds[f]` | `mini_anim->holds[f]` (unchanged) |
  | `const splash_anim_def_t*` | `const ClawdAnim*` |

  Note: `GRID = 20` in `splash.cpp` stays hardcoded — it matches `a->w = 20`, but keeping the constant avoids a dependency on the anim struct during drawing.

  Note: if `splash.cpp` uses `splash_anims[i].category` for the GROUP_NAMES lookup, the category field is not in `ClawdAnim`. Either add `category` to `ClawdAnim`/`build()` from `s.category`, or inline the lookup to use `clawd_anim_at(i)->name` for now. The cleaner fix is to add `const char* category` to `ClawdAnim` and fill it in `build()`.

- [ ] **Step 4: Verify build — both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | grep -E "error:|warning:|RAM:|Flash:"
  pio run -d firmware -e waveshare_amoled_18 2>&1 | grep -E "error:|warning:|RAM:|Flash:"
  ```
  Expected: zero errors; `splash_animations.h` appears in only one `.o` (check with `nm` if uncertain).

- [ ] **Step 5: Commit**

  ```
  git add firmware/src/clawd_anim.h firmware/src/clawd_anim.cpp firmware/src/splash.cpp
  git commit -m "feat: animation API wrapper (clawd_anim); migrate splash.cpp"
  ```

---

### Task 2: Pure Logic + Stubs

Port `clawd_policy.{h,cpp}` verbatim and add the two stubs required by later tasks.

**Files:**
- Create: `firmware/src/clawd_policy.h`
- Create: `firmware/src/clawd_policy.cpp`
- Create: `firmware/src/clawd_still.h`
- Create: `firmware/src/minimal_mode.h`

**Interfaces:**
- Consumes: nothing (pure C++, no LVGL, no Arduino)
- Produces:
  - `ClawdTuning clawd_tuning_for_liveliness(int liveliness)` — struct with scale factors
  - `struct ShowGate`, `struct QuietInput`, `struct TurnTracker`, `struct ClawdRng`
  - `int corner_seat_feet(int feet_y, int cell, const ClawdAnim* tallest)` — returns adjusted feet_y to clear tallest animation
  - `bool minimal_mode_active(void)` — always `false`
  - `clawd_still_dsc` — `lv_image_dsc_t` (may be a stub declaration if the actual 10 KB array is not ported)

- [ ] **Step 1: Copy `clawd_policy.h` and `clawd_policy.cpp` verbatim from upstream**

  Source: `../Clawdmeter/firmware/src/clawd_policy.h` and `.cpp`.
  Remove any `#include "theme_palette.h"` if present — policy is pure math and must not depend on themes. If policy uses `TH_*` values, replace with hardcoded RGB565 constants matching `THEME_ACCENT 0xd97757`.

- [ ] **Step 2: Create `minimal_mode.h`**

  ```cpp
  #pragma once
  inline bool minimal_mode_active(void) { return false; }
  ```

- [ ] **Step 3: Create `clawd_still.h` stub**

  The still image is used only when `minimal_mode_active()` returns `true`, which never happens. Stub it rather than copy the 10 KB array:
  ```cpp
  #pragma once
  #include <lvgl.h>
  // Stub — minimal mode is never active in this fork.
  extern const lv_image_dsc_t clawd_still_dsc;
  ```
  If `clawd_director.cpp` actually references `clawd_still_dsc` at link time (not just in a `#include`), copy the real array from `../Clawdmeter/firmware/src/clawd_still.h`.

- [ ] **Step 4: Verify build — both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | grep -E "error:|warning:"
  ```
  Expected: zero errors.

- [ ] **Step 5: Commit**

  ```
  git add firmware/src/clawd_policy.h firmware/src/clawd_policy.cpp \
           firmware/src/minimal_mode.h firmware/src/clawd_still.h
  git commit -m "feat: clawd_policy pure logic + minimal_mode/clawd_still stubs"
  ```

---

### Task 3: Preferences

Adapt `clawd_prefs.{h,cpp}` from upstream, keeping only `liveliness` and `attn_anim`.

**Files:**
- Create: `firmware/src/clawd_prefs.h`
- Create: `firmware/src/clawd_prefs.cpp`

**Interfaces:**
- Consumes: `clawd_policy.h` (for `ClawdTuning`), NVS via `Preferences`
- Produces:
  - `void clawd_prefs_init(void)` — loads from NVS, applies defaults
  - `int liveliness_get(void)` — 1–5, default 3
  - `void liveliness_set(int v)` — clamps 1–5, saves to NVS key `"liveliness"`
  - `int clawd_attn_anim(void)` — seconds: 0 / 10 / 30 / 120, default 120
  - `void attn_anim_set(int v)` — saves to NVS key `"attn_anim"`
  - `ClawdTuning clawd_tuning(void)` — `clawd_tuning_for_liveliness(liveliness_get())`

- [ ] **Step 1: Copy `clawd_prefs.h` and `.cpp` from upstream**

  Source: `../Clawdmeter/firmware/src/clawd_prefs.h` and `.cpp`.

- [ ] **Step 2: Strip upstream-only responsibilities**

  Remove from `clawd_prefs`:
  - `pace_marker` (not in fork)
  - `button_action` (not in fork)
  - `palette` / `theme` (no theme system in fork)

  Keep only `liveliness` and `attn_anim`. Remove any `#include "theme_palette.h"`. Remove any call to `clawd_anim_recolor()` or `theme_prefs_load()`.

  NVS namespace: `"clawdmeter"` (same as `provisioning.cpp`). NVS keys: `"liveliness"` (int32, default 3), `"attn_anim"` (int32, default 120).

- [ ] **Step 3: Verify build — both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | grep -E "error:|warning:"
  ```

- [ ] **Step 4: Commit**

  ```
  git add firmware/src/clawd_prefs.h firmware/src/clawd_prefs.cpp
  git commit -m "feat: clawd_prefs — liveliness + attn_anim NVS persistence"
  ```

---

### Task 4: Corner Character

Port `mascot.{h,cpp}` from upstream; adapt for `cell = 3` and no `theme_palette.h`.

**Files:**
- Create: `firmware/src/mascot.h`
- Create: `firmware/src/mascot.cpp`

**Interfaces:**
- Consumes: `clawd_anim.h`, `clawd_policy.h`, LVGL
- Produces:
  - `void mascot_create(lv_obj_t* parent, int slot_x, int feet_y, int cell)` — allocates LVGL image at the corner position
  - `void mascot_tick(void)` — called every loop, advances frame timers
  - `void mascot_autonomy(bool on)` — enables/disables idle acts
  - `void mascot_act(const char* anim_name, int times, bool snappy)` — plays a named animation
  - `void mascot_attention(bool on)` — corner wave+point attention mode
  - `bool mascot_busy(void)` — true while performing an act
  - `void mascot_repaint(void)` — no-op (no live theming in Phase 1)

- [ ] **Step 1: Copy `mascot.h` and `mascot.cpp` verbatim from upstream**

  Source: `../Clawdmeter/firmware/src/mascot.h` and `.cpp`.

- [ ] **Step 2: Remove theme dependency**

  Remove `#include "theme_palette.h"` from `mascot.cpp`. Replace `mascot_repaint()` body with an empty implementation. If mascot.cpp calls `clawd_anim_recolor()`, keep the call (it's a no-op stub).

- [ ] **Step 3: Remove performer dependency**

  `mascot_attention(true)` in upstream may try to call a `performer` walk-on when PSRAM is available. Guard the performer path with `#ifdef BOARD_HAS_PERFORMER` or simply remove the full-size walk-on branch, keeping only the corner wave+point path that runs when `performer_available()` returns false. Since there is no `performer.h` in the fork, do not include it.

- [ ] **Step 4: Verify build — both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | grep -E "error:|warning:"
  ```
  Expected: zero errors. No `performer.h` or `theme_palette.h` references.

- [ ] **Step 5: Commit**

  ```
  git add firmware/src/mascot.h firmware/src/mascot.cpp
  git commit -m "feat: corner mascot sprite engine (Phase 1 — no performer)"
  ```

---

### Task 5: Director State Machine

Adapt `clawd_director.{h,cpp}` from upstream, implementing B_HOME / B_CORNER / B_ATTENTION / B_PEEK; stubbing B_SHOW / B_SCREENSAVER / B_FACE_HOME / B_RUN_HOME to B_HOME.

**Files:**
- Create: `firmware/src/clawd_director.h`
- Create: `firmware/src/clawd_director.cpp`

**Interfaces:**
- Consumes: `mascot.h`, `clawd_policy.h`, `clawd_prefs.h`, `clawd_anim.h`, `minimal_mode.h`, `signals.h`, LVGL
- Produces:
  - `void director_init(void)` — creates the mascot at corner position (see §Mascot Position below)
  - `void director_tick(void)` — called every loop
  - `void director_on_payload(const UsageData* d, const SignalsUpdate* su)` — drives beat transitions
  - `void director_alert(const char* msg)` — triggers B_ATTENTION
  - `void director_alert_clear(void)` — dismisses B_ATTENTION
  - `bool director_button(bool main_btn)` — returns true if the director consumed the press
  - `void director_tap(int x)` — routes a tap on the usage screen
  - `void director_debug(const char* cmd)` — serial debug commands

- [ ] **Step 1: Copy `clawd_director.h` and `clawd_director.cpp` verbatim from upstream**

  Source: `../Clawdmeter/firmware/src/clawd_director.h` and `.cpp`.

- [ ] **Step 2: Adapt `director_init()` — mascot position**

  The upstream computes `cell` from the art stage width. For the fork's 20×20 animations use `cell = 3` (60×60 px mascot on 480-wide board).

  Position in `director_init()`:
  ```cpp
  const BoardCaps& bc = board_caps();
  int cell = 3;
  int feet_y = bc.height - 2 * cell;
  int slot_x = (bc.width / cell) - 20 - 2;  // 20 cells wide, 2-cell right margin
  feet_y = corner_seat_feet(feet_y, cell, clawd_anim_at(0));  // clear tallest anim
  lv_obj_t* parent = ui_mascot_parent();
  mascot_create(parent, slot_x, feet_y, cell);
  ```

- [ ] **Step 3: Remove upstream-only signal ingestion**

  Upstream's `director_on_payload(UsageData* d)` calls `signals_ingest(d)` itself. The fork's `director_on_payload(const UsageData* d, const SignalsUpdate* su)` receives a pre-computed `SignalsUpdate*` from `main.cpp` (signals_ingest is called there). Update the signature and remove any internal `signals_ingest` call.

- [ ] **Step 4: Stub Phase 2 beats**

  Replace `B_SHOW`, `B_SCREENSAVER`, `B_FACE_HOME`, `B_RUN_HOME` with immediate transitions to `B_HOME`. Add `// Phase 2` comment on each stub.

  Remove `director_set_pairing_touch` (BLE pairing mode not applicable). If other call sites reference it, add a no-op inline in the header.

- [ ] **Step 5: Adapt `settled()`**

  Without face/performer, `settled()` is:
  ```cpp
  static bool settled(void) {
      return !mascot_busy() && (int32_t)(millis() - wait_until) >= 0;
  }
  ```
  Remove any face/performer checks.

- [ ] **Step 6: Remove theme/face dependencies**

  Remove `#include "theme_palette.h"`, `#include "performer.h"`, `#include "face.h"` if present. Replace `performer_available()` guard with `false` inline. Replace `face_available()` guard with `false` inline.

- [ ] **Step 7: Wire `ui_meters_live()` call**

  `clawd_policy` / director may call `ui_meters_live()` to check if the usage screen is live. This function does not yet exist in `ui.cpp` (see Task 6) — add a forward declaration `bool ui_meters_live(void);` in `clawd_director.cpp` to compile before Task 6 lands it.

- [ ] **Step 8: Verify build — both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | grep -E "error:|warning:"
  pio run -d firmware -e waveshare_amoled_18 2>&1 | grep -E "error:|warning:"
  ```
  Expected: zero errors.

- [ ] **Step 9: Commit**

  ```
  git add firmware/src/clawd_director.h firmware/src/clawd_director.cpp
  git commit -m "feat: director state machine Phase 1 (HOME/CORNER/ATTENTION/PEEK)"
  ```

---

### Task 6: UI Plumbing + main.cpp Wiring

Add `ui_mascot_parent()`, `ui_meters_live()`, tap routing, `usage_data_valid` tracking in `ui.cpp`; wire `director_init/tick/on_payload/button/serial` in `main.cpp`.

**Files:**
- Modify: `firmware/src/ui.h`
- Modify: `firmware/src/ui.cpp`
- Modify: `firmware/src/main.cpp`

**Interfaces:**
- Consumes: `clawd_director.h`, `signals.h`, `hook_event.h`, `clawd_prefs.h`
- Produces:
  - `lv_obj_t* ui_mascot_parent(void)` — returns the SCREEN_USAGE container object (or NULL)
  - `bool ui_meters_live(void)` — true when SCREEN_USAGE is active and `usage_data_valid`

- [ ] **Step 1: Add `usage_data_valid` tracking to `ui.cpp`**

  Add `static bool s_usage_data_valid = false;` file-scope variable in `ui.cpp`. In `ui_update(const UsageData& d)`, set `s_usage_data_valid = d.valid && !d.stale;` (or equivalent based on the existing stale-data logic). Remove the flag when switching away from SCREEN_USAGE or in `ui_update()` when data becomes invalid.

- [ ] **Step 2: Add `ui_mascot_parent()` to `ui.cpp`**

  Returns the LVGL object for SCREEN_USAGE. The object must already exist by the time `director_init()` calls `ui_mascot_parent()`. Add `lv_obj_t* ui_mascot_parent(void);` to `ui.h`. Implement in `ui.cpp`:
  ```cpp
  lv_obj_t* ui_mascot_parent(void) {
      return s_usage_screen;  // the SCREEN_USAGE root container
  }
  ```
  Identify the actual variable name for the usage screen root by reading `ui.cpp`.

- [ ] **Step 3: Add `ui_meters_live()` to `ui.cpp`**

  ```cpp
  bool ui_meters_live(void) {
      return ui_get_current_screen() == SCREEN_USAGE && s_usage_data_valid;
  }
  ```
  Add declaration to `ui.h`.

- [ ] **Step 4: Wire tap routing in `ui.cpp`**

  In the existing `LV_EVENT_CLICKED` handler on the usage screen container, add:
  ```cpp
  if (ui_get_current_screen() == SCREEN_USAGE) {
      lv_point_t pos;
      lv_indev_get_point(lv_indev_active(), &pos);
      director_tap(pos.x);
  }
  ```
  Include `clawd_director.h` at the top of `ui.cpp`.

- [ ] **Step 5: Modify `main.cpp` — init and tick**

  After `ui_init()`, add:
  ```cpp
  clawd_prefs_init();
  director_init();
  ```

  In `loop()`, after `settings_server_tick()`, add:
  ```cpp
  director_tick();
  ```

- [ ] **Step 6: Modify `main.cpp` — `director_on_payload` wiring**

  Replace the current bare `signals_ingest` call in the hook-event drain block with a full `director_on_payload` call:
  ```cpp
  if (g_hook_event.pending) {
      SignalsUpdate su = signals_ingest(&g_hook_event);
      g_hook_event.pending = false;
      if (su.event_fired && su.event == 'p' && clawd_attn_anim() > 0)
          director_alert("Claude is thinking\xe2\x80\xa6");
      director_on_payload(&g_last_usage, &su);
  }
  ```
  Also in the `wifi_poller_has_new_data()` block, after updating `g_last_usage`, add:
  ```cpp
  SignalsUpdate su_empty = {};
  director_on_payload(&g_last_usage, &su_empty);
  ```
  Add `static UsageData g_last_usage = {};` file-scope in `main.cpp` and populate it from the poller result. (If `main.cpp` already has such a variable, use that one.)

- [ ] **Step 7: Modify `main.cpp` — PWR button routing**

  In the existing PWR press handler (`pwr_pressed()` from `power_hal`), add:
  ```cpp
  if (director_button(true)) {
      // director consumed the press (dismissed alert)
  } else {
      // existing screen-cycle action
      ui_slide_to(next_screen);
  }
  ```

- [ ] **Step 8: Add `clawd` serial command to `main.cpp`**

  In `provisioning_handle_cmd()` or the main serial handler, add a `clawd` prefix dispatch:
  ```cpp
  if (strncmp(cmd, "clawd", 5) == 0)
      director_debug(cmd + 5);  // passes " home", " trace on", etc.
  ```

- [ ] **Step 9: Verify build — both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | grep -E "error:|warning:"
  pio run -d firmware -e waveshare_amoled_18 2>&1 | grep -E "error:|warning:"
  ```

- [ ] **Step 10: Commit**

  ```
  git add firmware/src/ui.h firmware/src/ui.cpp firmware/src/main.cpp
  git commit -m "feat: wire director to UI, main loop, and PWR button"
  ```

---

### Task 7: Settings Web UI + Visual Verification

Add the Clawd section to the settings HTML page, wire `POST /settings/clawd`, then do a screenshot-based visual QA pass on each director beat.

**Files:**
- Modify: `firmware/src/settings_server.cpp`

- [ ] **Step 1: Add Clawd section to `SETTINGS_HTML`**

  Below the Security section, add:
  ```html
  <section>
    <h2>Clawd</h2>
    <form method="post" action="/settings/clawd">
      <label>Liveliness
        <select name="liveliness">
          <option value="1">1 — No-nonsense</option>
          <option value="2">2 — Mostly quiet</option>
          <option value="3" selected>3 — Some nonsense (default)</option>
          <option value="4">4 — Lively</option>
          <option value="5">5 — All-nonsense</option>
        </select>
      </label>
      <label>Attention animation
        <select name="attn_anim">
          <option value="120" selected>120 s (default)</option>
          <option value="30">30 s</option>
          <option value="10">10 s</option>
          <option value="0">Off</option>
        </select>
      </label>
      <button type="submit">Save Clawd settings</button>
    </form>
  </section>
  ```
  The `selected` attributes must reflect the live NVS values on page load. Update the `GET /` handler to inject `liveliness_get()` and `clawd_attn_anim()` into the HTML string (replace `selected` with a computed string, or use a small helper that wraps printf into the response).

- [ ] **Step 2: Add `POST /settings/clawd` route**

  In `settings_server.cpp`, add the route registration (inside the `s_routes_done` guard):
  ```cpp
  s_server.on("/settings/clawd", HTTP_POST, []() {
      if (!check_web_auth()) return;
      int lv  = s_server.arg("liveliness").toInt();
      int attn = s_server.arg("attn_anim").toInt();
      liveliness_set(lv);
      attn_anim_set(attn);
      s_server.sendHeader("Location", "/?saved=1");
      s_server.send(303);
  });
  ```
  Add `#include "clawd_prefs.h"` to `settings_server.cpp`.

- [ ] **Step 3: Verify build — both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | grep -E "error:|warning:|RAM:|Flash:"
  pio run -d firmware -e waveshare_amoled_18 2>&1 | grep -E "error:|warning:|RAM:|Flash:"
  ```

- [ ] **Step 4: Screenshot QA — B_HOME (idle)**

  Set default boot screen to `SCREEN_USAGE` in `main.cpp`, flash `waveshare_amoled_216`, capture:
  ```bash
  ./screenshot.sh qa_home.png
  ```
  Read `qa_home.png` with the Read tool. Verify: Clawd visible in bottom-right corner, usage bars visible, no Z-order issues.

- [ ] **Step 5: Screenshot QA — B_ATTENTION**

  Via serial: `clawd attention`. Capture:
  ```bash
  ./screenshot.sh qa_attention.png
  ```
  Verify: Clawd animation playing (different frame from B_HOME), still in corner.

- [ ] **Step 6: Screenshot QA — AMOLED-1.8**

  Flash `waveshare_amoled_18`, capture same two screenshots. Verify correct positioning on 368×448 panel (slot_x and feet_y recalculate correctly from `board_caps()`).

- [ ] **Step 7: Revert boot screen**

  Revert `main.cpp` to boot with `SCREEN_SPLASH`.

- [ ] **Step 8: Final build both envs**

  ```bash
  pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5
  pio run -d firmware -e waveshare_amoled_18 2>&1 | tail -5
  ```

- [ ] **Step 9: Commit**

  ```
  git add firmware/src/settings_server.cpp firmware/src/main.cpp
  git commit -m "feat: Clawd settings in web UI; director Phase 1 complete"
  ```
