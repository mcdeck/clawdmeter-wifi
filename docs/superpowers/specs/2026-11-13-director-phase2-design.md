# Director Phase 2: face, performer, full beat scripts

**Date:** 2026-11-13  
**Branch:** wifi-native

## Goal

Activate the four stubbed beats in `clawd_director.cpp` (B_SHOW, B_SCREENSAVER, B_FACE_HOME, B_RUN_HOME) by porting the upstream `face`, `face_motion`, `field`, and `performer` subsystems into the fork, adapting them for the fork's claudepix animation library and theme system.

Success: a tap on the meters brings the face up; idle time brings the screensaver; heavy sustained usage earns a full-size performer show; all four Phase 2 beats run end-to-end on hardware.

---

## New files

### `field.h` / `field.cpp` — direct copy

An efficient flat-colour overlay. The face is a large orange rectangle that moves every frame; without this, LVGL would invalidate its full old+new bounding box each frame (nearly the whole screen). Instead, `field` is a full-screen object that never moves: it draws a shape in `LV_EVENT_DRAW_MAIN`, and changing the shape invalidates only the changed strips (old XOR new). This cuts the face rise from ~154 K px/frame to ~11 K.

Port: copy verbatim from upstream. No fork-specific changes.

### `face_motion.h` / `face_motion.cpp` — direct copy

Pure math (no LVGL, no Arduino): given `(FaceMove, elapsed_ms, FaceGeom)` returns a `FacePose`. Used by `face.cpp`'s LVGL animation callback so the motion is frame-rate-independent and unit-testable on the host.

**Rise** (860 ms total): ease-out to 44 px past rest in 500 ms, then an underdamped spring settle (period 300 ms, decay 110 ms) back to y = 0.  
**Drop** (510 ms): 60 ms crouch, 90 ms anticipation hop, 360 ms ease-in fall past the bezel.  
Eyes lag the body by 40 ms (clamped to 40 px).

The upstream already simplified away squash/stretch and arm narrowing — `FacePose.margin`, `arm_dy`, `stretch` are defined in the struct but set to 0. Port verbatim.

### `perf.h` — new stub

The upstream's performance-profiling header. `face.cpp` and `performer.cpp` call `perf_begin(window)` / `perf_end(window)` to measure redraw cost. The fork has no perf profiler. Create a 6-line header:

```cpp
#pragma once
enum PerfWindow { PERF_FACE_RISE, PERF_FACE_SETTLE, PERF_FACE_DROP };
#define perf_begin(w) ((void)(w))
#define perf_end(w)   ((void)(w))
```

### `face.h` — identical API to upstream

```cpp
void face_init(void);
void face_tick(void);
void face_rise(void);
void face_rise_toward(int x);
void face_drop(void);
void face_hide_now(void);
bool face_visible(void);
bool face_moving(void);
bool face_busy(void);
void face_blink(bool twice);
void face_set_resting(bool on);
void face_apply_theme(void);
void face_look_at_x(int x, uint32_t hold_ms);
```

### `face.cpp` — ported from upstream, two adaptations

**What it does:** `lv_layer_top()` holds a full-screen `Field` (orange). Two dark eye rectangles sit as LVGL children. Blinks are the closed squint shape (> <); glances move the eyes with a short `lv_anim`. The face rises/drops via an `lv_anim` whose exec callback calls `face_motion_pose()` and `apply_pose()`. Resting: irregular blinks and glances from `face_plan_next()` (already in `clawd_policy.h`).

**Adaptation 1 — theme macro:**  
Upstream: `theme_color(TH_CLAWD)` / `theme_color(TH_EYE)`  
Fork: `th_col(TH_CLAWD)` / `th_col(TH_EYE)` (same resolved type, different macro name)

**Adaptation 2 — perf calls removed:**  
Replace all `perf_begin(…)` / `perf_end(…)` with the stub from `perf.h` (included once).

No other changes.

### `performer.h` — identical API to upstream

```cpp
void performer_init(lv_obj_t* parent, int feet_y);
void performer_tick(void);
bool performer_available(void);
enum PerformerSide { PERFORMER_LEFT = -1, PERFORMER_RIGHT = +1 };
void performer_place_offstage(int side);
void performer_walk_home(uint16_t speed_pct);
void performer_walk_off(int side, uint16_t speed_pct);
void performer_run_off(int side, uint16_t from_pct, uint16_t to_pct, uint32_t ramp_ms);
void performer_perform(int anim_index, uint32_t loop_ms);
void performer_perform_group(int group, uint32_t loop_ms, bool calm);
void performer_release(void);
bool performer_busy(void);
bool performer_visible(void);
void performer_repaint(void);
```

### `performer.cpp` — ported from upstream, two adaptations

**What it does:** Allocates a full-screen planar RGB565A8 buffer in PSRAM (~690 KB on a 480-panel). Renders the current claudepix frame into it at `cell = min(W,H)/60` px per cell (8 px on a 480 panel → 160×160 px art on a 480×480 stage). Walking uses `clawd_gait_cells()` exactly as in `mascot.cpp`. Without PSRAM, `performer_available()` returns false and the director falls back to corner acts.

**Adaptation 1 — scene groups:**  
Upstream uses upstream-only animation names (laptop, skateboard, etc.). Fork maps to claudepix:

| Group | Use | Scenes |
|-------|-----|--------|
| 0 (Idle) | — | "idle breathe", "idle look around", "work think", "dance sway" |
| 1 (Normal) | — | "work coding", "dance sway", "idle look around", "dance bounce" |
| 2 (Active show) | earned show | "dance bounce", "work coding", "dance sway", "idle look around" |
| 3 (Heavy show) | earned show | "dance bounce dj", "dance sway dj", "dance djmix", "work coding" |
| Calm | screensaver dip | "idle breathe", "work think" |

The director always calls `clawd_show_group(base_group)` which floors at group 2, so groups 0 and 1 are only reachable via liveliness level 5 or QA.

**Adaptation 2 — perf calls removed** (same as face.cpp).

---

## Modified files

### `theme_palette.h` / `theme_palette.cpp`

Add two new slots after `TH_BAR_BG`:

```cpp
TH_CLAWD  = 9,   // Clawd orange — face body colour
TH_EYE    = 10,  // near-black  — face eye colour
TH_COUNT  = 11,
```

Default hex values (dark theme):
- `TH_CLAWD`: `0xE8622A` (terracotta orange, matches the upstream design)
- `TH_EYE`:   `0x1A1A1A` (near-black)

`theme_palette.cpp`: extend the default array from 9 to 11 entries.

**Impact:** Any code reading `TH_COUNT` to size a fixed array (e.g. `theme_format_hex`, `theme_parse_hex`, `theme_palette_set`) will automatically handle 11 entries because they all use `TH_COUNT`. NVS-stored theme strings are 54 hex chars (9×6); they become 66 chars (11×6). Existing stored strings are shorter — a migration is needed: on read, if the stored length is 54, append the two new defaults. This is handled in `theme_pref.cpp`.

### `clawd_director.cpp`

**`director_init()`**: add `face_init()` and `performer_init(ui_mascot_parent(), bc.height * 7 / 8)` calls, using the same `bc = board_caps()` reference already computed there for the mascot position.

**`settled()`**: add `&& !face_moving() && !performer_busy()`.

**`director_tick()`**: add `face_tick()` and `performer_tick()` calls. Uncomment the screensaver gate in `run_home()`. Uncomment the show gate in `run_home()`. Activate all four Phase 2 beat cases.

**`to_meters()`**: add `face_visible() → B_FACE_HOME`, `performing → B_RUN_HOME` paths (Phase 2 comment blocks).

**`tap_home()`**: call `face_rise_toward(x)` when transitioning to B_PEEK.

**`run_peek()`**: in step 1 (mascot gone), call `face_rise_toward(peek_x)` and `face_set_resting(true)`. In the timeout step, check `peek_hold()` → if PEEK_HOLD_DONE call `face_drop()` and walk home; if PEEK_HOLD_KEEP stay up. A second tap while peek is up: `face_drop()` immediately.

**`run_show()`** — new, implements the show beat:
1. Step 0: `mascot_act("pointing", 1)` → go step 1
2. Step 1: wait settled; `mascot_leave(150)` → performer_place_offstage(RIGHT) → step 2
3. Step 2: wait mascot away; `performer_walk_home(150)` → step 3
4. Step 3: wait performer home; `show_gate_mark_shown()`; `performer_perform_group(group, show_scene_ms, false)` → step 4
5. Step 4: wait loop done; `mascot_act("waving", 1)` → step 5 (without performer: skip to step 6)
6. Step 5: wait settled; `performer_walk_off(LEFT, 150)` → step 6
7. Step 6: wait performer away; `mascot_return(-1, 150)` → B_CORNER
   - Without performer: `mascot_act("dancing", 2)` in seat + same return

**`run_screensaver()`** — new:
1. Step 0: `mascot_leave(100)` → `face_rise()` → step 1
2. Step 1: wait mascot away and face settled; `face_set_resting(true)` → step 2
3. Step 2: idle resting. Check calm-dip timer (`calm_min_ms`..`calm_max_ms`). On dip:
   - `performer_place_offstage(LEFT)` → `face_set_resting(false)` → step 3
4. Step 3: wait settled; `performer_walk_home(100)` → step 4  
5. Step 4: wait performer home; `performer_perform_group(0, calm_scene_ms, true)` → step 5
6. Step 5: wait loop done; `performer_walk_off(LEFT, 100)` → step 6
7. Step 6: wait performer away; `face_set_resting(true)` → step 2 (back to resting)

**`run_face_home()`** — new:
1. Step 0: `face_blink(false)` → `face_drop()` → step 1
2. Step 1: wait `!face_visible()`; `mascot_hurry_home()` → B_CORNER

**`run_run_home()`** — new:
1. Step 0: `performer_release()` → wait 200 ms → step 1
2. Step 1: `performer_run_off(LEFT, 150, 300, 400)` → mascot walks home off-screen → step 2
3. Step 2: wait performer away; `mascot_return(-1, 150)` → B_CORNER

### `main.cpp`

No separate `face_init()` / `performer_init()` calls needed — `director_init()` now calls them internally (the director owns both subsystems).

---

## Memory budget

| Subsystem | RAM | Notes |
|-----------|-----|-------|
| Performer buffer | ~690 KB PSRAM | Only allocated when PSRAM present |
| Mascot big_buf | ~250 KB PSRAM | Already allocated (attention/lurk) |
| Face (LVGL objects) | ~2 KB internal | ~20 LVGL objects, no pixel buffer |
| field.obj | 0 (LVGL obj) | Reuses existing LVGL heap |

Without PSRAM: `performer_available()` = false, shows fall back to corner acts.

---

## Constraints

- No `#ifdef BOARD_*` in shared code — board differences come via `board_caps()`.
- The face lives on `lv_layer_top()`: it always renders above the meter UI.
- `face_init()` must be called after `ui_init()` (needs a valid display).
- Screensaver requires `clawd_tuning().screensaver` to be true (liveliness ≥ 2).
- Shows require `clawd_tuning().shows` and hooks seen or the window not started.
- `TH_COUNT` increase means theme NVS strings are 12 chars longer — migration required.

---

## Testing

- `clawd show` serial command → exercises run_show()
- `clawd screensaver` serial command → exercises run_screensaver() + calm dip
- `clawd peek [x]` serial command → exercises face rise/peek/drop
- `clawd home` → exercises run_face_home() / run_run_home() interrupts
- `screenshot.sh out.png` on each beat step to visually verify
- Build both `waveshare_amoled_216` and `waveshare_amoled_18` envs
