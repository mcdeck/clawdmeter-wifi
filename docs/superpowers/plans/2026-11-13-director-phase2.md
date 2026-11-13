# Director Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the upstream `field`, `face_motion`, `face`, and `performer` subsystems into the fork, then activate the four stubbed beats (B_SHOW, B_SCREENSAVER, B_FACE_HOME, B_RUN_HOME) in `clawd_director.cpp`.

**Architecture:** `field` and `face_motion` are direct copies (pure LVGL utility / pure math). `face` and `performer` are ported with two adaptations each: `th_col()` for the theme macro, and `perf.h` calls replaced by a stub. The director's beat scripts are added in three separate tasks (peek, show, screensaver/returns) so each is independently buildable and testable.

**Tech Stack:** C++17, Arduino/ESP-IDF, LVGL 9, PlatformIO, ESP32-S3 (PSRAM boards), ESP32-C6 (no-PSRAM board).

**Spec:** `docs/superpowers/specs/2026-11-13-director-phase2-design.md`

## Global Constraints

- No `#ifdef BOARD_*` in shared code — board differences via `board_caps()`.
- No new `#include` in shared `.h` files that would pull in LVGL outside existing pattern.
- `face_init()` and `performer_init()` called from `director_init()`, after `ui_init()` has run.
- `performer_available()` must return false when PSRAM is absent; all show/calm paths must degrade to corner acts.
- Builds must pass for both `waveshare_amoled_216` and `waveshare_amoled_18` envs.
- Build must also pass for `waveshare_amoled_216_c6` (no PSRAM, `-DHEAP_DEBUG`).

## Review Focus

- **Screensaver gate fires too early:** `screensaver_ready()` checks `hooks_seen || window_not_started`; if neither is true the gate never fires. Verify the check is preserved exactly from `clawd_policy.h`.
- **to_meters() mid-screensaver:** a prompt, tap, or button during the screensaver should go to B_FACE_HOME (face then drops, corner hurries home), not B_CORNER directly — the face must come down first. Verify the `face_visible()` guard in `to_meters()`.
- **Calm dip after attention interrupt:** attention can interrupt the screensaver. When `resume_screensaver` is set and attention ends, the screensaver must restart from the face already up (step 2), not from step 0 (which walks off again). The `arrive_meters()` Phase 2 path must check `resume_screensaver` and go to B_SCREENSAVER step 2.
- **Performer not available on C6:** every path that calls `performer_*` must first check `performer_available()`; a missing check compiles fine on S3 but crashes on C6 as the objects are NULL.
- **TH_COUNT increase in theme_presets.h:** `ThemePreset.colors` is sized by `TH_COUNT`. Adding two slots without updating every preset initializer is a compile error or undefined behaviour; each of the 11 existing presets must add `0xE8622A, 0x1A1A1A` at the end.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `firmware/src/perf.h` | Create | Empty stub for perf_begin/perf_end |
| `firmware/src/field.h` | Create | Efficient flat-colour overlay shape (direct copy) |
| `firmware/src/field.cpp` | Create | field implementation (direct copy) |
| `firmware/src/face_motion.h` | Create | Face rise/drop math types and API (direct copy) |
| `firmware/src/face_motion.cpp` | Create | Face motion pose calculations (direct copy) |
| `firmware/src/theme_palette.h` | Modify | Add TH_CLAWD=9, TH_EYE=10, TH_COUNT=11 |
| `firmware/src/theme_palette.cpp` | Modify | Extend default palette array to 11 entries |
| `firmware/src/theme_presets.h` | Modify | Add two color values to every preset |
| `firmware/src/face.h` | Create | Face API (identical to upstream) |
| `firmware/src/face.cpp` | Create | LVGL-primitive face with rise/drop motion |
| `firmware/src/performer.h` | Create | Performer API (identical to upstream) |
| `firmware/src/performer.cpp` | Create | Full-screen PSRAM performer with claudepix scenes |
| `firmware/src/clawd_director.cpp` | Modify | face_init/performer_init, settled(), beat scripts |

---

### Task 1: perf.h stub

**Files:**
- Create: `firmware/src/perf.h`

**Interfaces:**
- Produces: `perf_begin(w)`, `perf_end(w)` as no-op macros; `PerfWindow` enum used by face.cpp and performer.cpp.

- [ ] **Step 1:** Create `firmware/src/perf.h` with this exact content:

```cpp
#pragma once
// Stub: the fork has no perf profiler. face.cpp and performer.cpp include this.
enum PerfWindow { PERF_FACE_RISE, PERF_FACE_SETTLE, PERF_FACE_DROP,
                  PERF_PERFORMER_WALK, PERF_PERFORMER_SCENE };
#define perf_begin(w) ((void)(w))
#define perf_end(w)   ((void)(w))
```

- [ ] **Step 2:** Confirm file exists: `ls firmware/src/perf.h`

- [ ] **Step 3:** Commit: `git add firmware/src/perf.h && git commit -m "feat: add perf.h stub for face/performer port"`

---

### Task 2: field.h + field.cpp

**Files:**
- Create: `firmware/src/field.h`
- Create: `firmware/src/field.cpp`

**Interfaces:**
- Produces: `Field`, `FieldShape`, `field_init()`, `field_set_shape()`, `field_set()`, `field_clear()`, `field_shape_init()`, `field_invalidate_change()`, `field_show()`, `field_hide()`, `field_to_front()`, `field_quietly()`. Consumed by face.cpp and performer.cpp.

- [ ] **Step 1:** Create `firmware/src/field.h` — copy the exact content from the spec fetch (reproduced here for the implementer):

```cpp
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <lvgl.h>

#define FIELD_EXTRAS 2

struct FieldShape {
    lv_area_t body;
    lv_area_t extra[FIELD_EXTRAS];
};

struct Field {
    lv_obj_t*  obj;
    FieldShape shape;
    lv_color_t color;
};

void field_init(Field* f, lv_obj_t* parent, int32_t w, int32_t h, lv_color_t color);
void field_set(Field* f, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void field_set_shape(Field* f, const FieldShape* s);
void field_invalidate_change(lv_obj_t* obj, const lv_area_t* o, const lv_area_t* n);
void field_shape_init(FieldShape* s);
void field_clear(Field* f);
void field_show(Field* f);
void field_hide(Field* f);
void field_to_front(Field* f);
void field_quietly(lv_obj_t* any, void (*fn)(void* ctx), void* ctx);
```

- [ ] **Step 2:** Create `firmware/src/field.cpp` — copy verbatim from the upstream source retrieved in the design session (the content is in the spec fetch; it implements the draw_cb/invalidation logic described in the header comment).

- [ ] **Step 3:** Build to verify: `pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5`

- [ ] **Step 4:** Commit: `git add firmware/src/field.h firmware/src/field.cpp && git commit -m "feat: add field.h/field.cpp efficient flat-colour overlay"`

---

### Task 3: face_motion.h + face_motion.cpp

**Files:**
- Create: `firmware/src/face_motion.h`
- Create: `firmware/src/face_motion.cpp`

**Interfaces:**
- Produces: `FaceMove` (enum: FACE_MOVE_RISE, FACE_MOVE_DROP), `FaceGeom` (w, h, mind, cell, start_y), `FacePose` (y, margin, eye_dy, arm_dy, stretch), `face_motion_duration(FaceMove)→uint32_t`, `face_motion_rest_ms()→uint32_t`, `face_motion_pose(FaceMove, uint32_t t_ms, const FaceGeom*)→FacePose`, `face_arm_w(const FaceGeom*)→int`, `face_arm_h(const FaceGeom*)→int`. Consumed by face.cpp.

- [ ] **Step 1:** Create `firmware/src/face_motion.h` — copy verbatim from the upstream source retrieved in the design session. It defines the structs and function signatures above with no LVGL or Arduino includes.

- [ ] **Step 2:** Create `firmware/src/face_motion.cpp` — copy verbatim from the upstream source. Key constants: RISE_UP_MS=500, RISE_END_MS=860, DROP_ANT_MS=150, DROP_END_MS=510; `face_motion_pose()` sets `p.margin=0`, `p.arm_dy=0`, `p.stretch=0` (the fork already implements the simplified version — no squash/stretch, no narrowing).

- [ ] **Step 3:** Build: `pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5`

- [ ] **Step 4:** Commit: `git add firmware/src/face_motion.h firmware/src/face_motion.cpp && git commit -m "feat: add face_motion.h/cpp rise/drop curve math"`

---

### Task 4: theme palette — TH_CLAWD and TH_EYE

**Files:**
- Modify: `firmware/src/theme_palette.h`
- Modify: `firmware/src/theme_palette.cpp`
- Modify: `firmware/src/theme_presets.h`

**Interfaces:**
- Produces: `TH_CLAWD=9` (Clawd orange), `TH_EYE=10` (dark), `TH_COUNT=11`. Consumed by face.cpp.

- [ ] **Step 1:** In `theme_palette.h`, change the enum:
  - `TH_BAR_BG = 8,` stays
  - Add `TH_CLAWD  = 9,` and `TH_EYE    = 10,` after it
  - Change `TH_COUNT  = 9,` to `TH_COUNT  = 11,`

- [ ] **Step 2:** In `theme_palette.cpp`, extend `s_palette[TH_COUNT]` — add two entries after `0x2a2a28`:
  - `0xE8622A,   // TH_CLAWD — Clawd orange`
  - `0x1A1A1A,   // TH_EYE  — near-black`

- [ ] **Step 3:** In `theme_presets.h`, add `0xE8622A, 0x1A1A1A` at the end of **every** preset's colors array (there are 11 presets). Update the comment at the top from `// colors order: TH_BG, … TH_BAR_BG` to append `, TH_CLAWD, TH_EYE`.

- [ ] **Step 4:** Build both envs: `pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5 && pio run -d firmware -e waveshare_amoled_18 2>&1 | tail -5`

- [ ] **Step 5:** Commit: `git add firmware/src/theme_palette.h firmware/src/theme_palette.cpp firmware/src/theme_presets.h && git commit -m "feat: add TH_CLAWD and TH_EYE theme slots for face"`

---

### Task 5: face.h + face.cpp

**Files:**
- Create: `firmware/src/face.h`
- Create: `firmware/src/face.cpp`

**Interfaces:**
- Consumes: `field.h` (Field, FieldShape), `face_motion.h` (FaceGeom, FacePose, FaceMove), `perf.h` (stub), `clawd_policy.h` (face_plan_next, face_eye_offsets, peek_glance_px, face_glance_px, clawd_rng_seed, clawd_rng_range, clawd_tuning), `theme.h` (th_col, TH_CLAWD, TH_EYE), `hal/board_caps.h`.
- Produces: `face_init()`, `face_tick()`, `face_rise()`, `face_rise_toward(int x)`, `face_drop()`, `face_hide_now()`, `face_visible()→bool`, `face_moving()→bool`, `face_busy()→bool`, `face_blink(bool twice)`, `face_set_resting(bool on)`, `face_apply_theme()`, `face_look_at_x(int x, uint32_t hold_ms)`.

- [ ] **Step 1:** Create `firmware/src/face.h` with the API as listed in Interfaces above (matches upstream exactly).

- [ ] **Step 2:** Create `firmware/src/face.cpp` — port from upstream with **two adaptations only**:
  1. `theme_color(TH_CLAWD)` → `th_col(TH_CLAWD)`, `theme_color(TH_EYE)` → `th_col(TH_EYE)` (the macro name in this fork is `th_col`).
  2. All `perf_begin(…)` and `perf_end(…)` calls compile to no-ops via `perf.h` — no other changes needed.
  
  The upstream source was retrieved verbatim in the design session; apply the two substitutions and copy the rest unchanged. Key geometry constants (all authored at 480px, scaled by `S(v) = v * mind / 480`): eye centres at ±80px from screen centre, eye width 44px, eye height 120px, blink=180ms, glance=280ms.

- [ ] **Step 3:** Build: `pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5`

- [ ] **Step 4:** Build the C6 (no PSRAM) env to confirm face compiles there too: `pio run -d firmware -e waveshare_amoled_216_c6 2>&1 | tail -5`

- [ ] **Step 5:** Commit: `git add firmware/src/face.h firmware/src/face.cpp && git commit -m "feat: add face.h/cpp LVGL-primitive face with rise/drop motion"`

---

### Task 6: performer.h + performer.cpp

**Files:**
- Create: `firmware/src/performer.h`
- Create: `firmware/src/performer.cpp`

**Interfaces:**
- Consumes: `field.h`, `perf.h`, `clawd_anim.h`, `clawd_policy.h`, `splash_geometry.h`, `hal/board_caps.h`, `usage_rate.h`.
- Produces:
  - `performer_init(lv_obj_t* parent, int feet_y) → void`
  - `performer_tick() → void`
  - `performer_available() → bool` — false when PSRAM absent
  - `enum PerformerSide { PERFORMER_LEFT = -1, PERFORMER_RIGHT = +1 }`
  - `performer_place_offstage(int side) → void`
  - `performer_walk_home(uint16_t speed_pct) → void`
  - `performer_walk_off(int side, uint16_t speed_pct) → void`
  - `performer_run_off(int side, uint16_t from_pct, uint16_t to_pct, uint32_t ramp_ms) → void`
  - `performer_perform(int anim_index, uint32_t loop_ms) → void`
  - `performer_perform_group(int group, uint32_t loop_ms, bool calm) → void`
  - `performer_release() → void`
  - `performer_busy() → bool`
  - `performer_visible() → bool`
  - `performer_repaint() → void`

**performer.cpp implementation notes:**

Geometry (compute in `performer_init`):
```
cell  = min(W,H) / 60          // 8 on 480, 6 on 368; min 1
stage = cell * 60               // full display width on 480
art_w = walk_anim->w * cell     // 20*8=160 at 480
slot_x= (W - art_w) / 2 - walk_anim->ox * cell   // centred
feet_y= feet_y parameter (bc.height * 7 / 8 from director)
```

Buffer (in `performer_init`):
```cpp
const size_t dim = (size_t)cell * 60;
big_buf = (uint8_t*)heap_caps_malloc(dim * dim * 3, MALLOC_CAP_SPIRAM);
// if NULL: performer_available() returns false, all API calls are no-ops
```

Render: use the same `render()` helper pattern as `mascot.cpp` (planar RGB565A8 into `big_buf`, widget anchored at feet).

States enum: `P_OFF`, `P_STAND`, `P_WALK`, `P_SCENE`. Walking uses `clawd_gait_cells(GAIT_WALK, frame, from_loop) * cell` per frame. Scene plays from `loop_start` to `loop_end` for `loop_ms`, then plays remaining frames once (the outro), then goes to P_STAND. `performer_busy()` = not P_OFF and not P_STAND.

Off-stage positions (use `offstage_left_x` / `offstage_right_x` from `splash_geometry.h`, same as mascot.cpp).

Scene groups (fork adaptation — indexed 0–3, `calm=true` picks from CALM set ignoring group):

```cpp
static const char* const GROUPS[4][4] = {
    { "idle breathe",   "idle look around", "work think",    "dance sway"       },
    { "work coding",    "dance sway",       "idle look around","dance bounce"   },
    { "dance bounce",   "work coding",      "dance sway",    "idle look around" },
    { "dance bounce dj","dance sway dj",    "dance djmix",   "work coding"      },
};
static const char* const CALM[2] = { "idle breathe", "work think" };
```

`performer_perform_group(group, loop_ms, calm)`: pick from `calm ? CALM : GROUPS[group]` using `clawd_pick_no_repeat()`, then call `performer_perform(anim_index, loop_ms)`. Track last show anim and last calm anim separately.

`performer_run_off(side, from_pct, to_pct, ramp_ms)`: walk off at speed ramping linearly from `from_pct` to `to_pct` over `ramp_ms`. Interpolate speed each frame by `from_pct + (to_pct - from_pct) * elapsed / ramp_ms`, clamp to `to_pct` after `ramp_ms`.

- [ ] **Step 1:** Create `firmware/src/performer.h` with the API signatures listed in Interfaces above.

- [ ] **Step 2:** Create `firmware/src/performer.cpp` per the implementation notes above. `performer_repaint()` is a no-op (no live theme recoloring in fork, same as `mascot_repaint`).

- [ ] **Step 3:** Build all three envs:
  ```
  pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5
  pio run -d firmware -e waveshare_amoled_18 2>&1 | tail -5
  pio run -d firmware -e waveshare_amoled_216_c6 2>&1 | tail -5
  ```

- [ ] **Step 4:** Commit: `git add firmware/src/performer.h firmware/src/performer.cpp && git commit -m "feat: add performer.h/cpp full-screen PSRAM performer"`

---

### Task 7: Director init wiring

**Files:**
- Modify: `firmware/src/clawd_director.cpp`

**Interfaces:**
- Consumes: `face.h` (face_init, face_tick, face_moving, face_visible, face_hide_now), `performer.h` (performer_init, performer_tick, performer_busy, performer_place_offstage).
- Produces: `director_init()` calling face_init + performer_init; `director_tick()` calling face_tick + performer_tick; `settled()` including face and performer checks.

- [ ] **Step 1:** Add `#include "face.h"` and `#include "performer.h"` near the top of `clawd_director.cpp` (after the existing includes).

- [ ] **Step 2:** In `director_init()`, after the `mascot_create()` call, add:
  ```cpp
  face_init();
  performer_init(parent, bc.height * 7 / 8);
  ```
  (`parent` is the `ui_mascot_parent()` local already in scope; `bc` is the `board_caps()` reference already there.)

- [ ] **Step 3:** In `director_tick()`, add `face_tick();` and `performer_tick();` calls at the top of the function (after `show_gate_update`).

- [ ] **Step 4:** Update `settled()` to:
  ```cpp
  static bool settled(void) {
      return !mascot_busy() && !face_moving() && !performer_busy()
             && (int32_t)(millis() - wait_until) >= 0;
  }
  ```

- [ ] **Step 5:** Build and confirm existing beats still work: `pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5`. Flash and test `clawd attention` via serial — should still work.

- [ ] **Step 6:** Commit: `git add firmware/src/clawd_director.cpp && git commit -m "feat: wire face_init/performer_init into director, update settled()"`

---

### Task 8: Peek beat — face rise on tap

**Files:**
- Modify: `firmware/src/clawd_director.cpp`

**Interfaces:**
- Consumes: `face_rise_toward(int x)`, `face_drop()`, `face_set_resting(bool)`, `face_busy()`, `face_visible()`, `peek_hold()` (from clawd_policy.h).

- [ ] **Step 1:** In `tap_home()`, add `face_rise_toward(x);` immediately after `mascot_leave(PEEK_LEAVE_PCT)` and after the `if (r == TAP_PEEK)` check — whenever `r != TAP_NONE` and B_PEEK is entered, the face should start rising.

  Current code:
  ```cpp
  if (r == TAP_PEEK_HOP && !mascot_act("jumping", 1, true, PEEK_HOP_PCT)) r = TAP_PEEK;
  if (r == TAP_PEEK) mascot_leave(PEEK_LEAVE_PCT);
  if (r != TAP_NONE) {
      react_anim = NULL;
      peek_x = x;
      peek_up_at = now_ms();
      go(B_PEEK);
  }
  ```
  
  After `if (r == TAP_PEEK) mascot_leave(PEEK_LEAVE_PCT);` and before the `if (r != TAP_NONE)` block, add:
  ```cpp
  if (r != TAP_NONE) face_rise_toward(x);
  ```

- [ ] **Step 2:** Rewrite `run_peek()` to implement the full peek beat:

  ```cpp
  static void run_peek(uint32_t now) {
      switch (step) {
      case 0:   // hop (if any) landed; mascot leaving
          if (mascot_away()) {
              face_set_resting(true);
              next_step();
          }
          break;
      case 1: { // curious: face is up and resting
          PeekHold h = peek_hold(peek_up_at, now, &clawd_tuning());
          if (h == PEEK_HOLD_DONE) {
              face_drop();
              face_set_resting(false);
              next_step();
          }
          break;
      }
      default:  // face dropped; walk home
          if (!face_visible()) {
              mascot_return(-1, 150);
              go(B_CORNER);
          }
          break;
      }
  }
  ```

- [ ] **Step 3:** In `director_tap()`, update the B_PEEK case — a second tap while peek face is up should drop it:
  ```cpp
  if (beat == B_PEEK) {
      if (face_visible()) { face_drop(); face_set_resting(false); }
      return;
  }
  ```

- [ ] **Step 4:** Build: `pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5`

- [ ] **Step 5:** Flash and verify via serial: `clawd peek` → mascot hops and ducks out → face rises. Second `clawd peek` → face drops immediately.

- [ ] **Step 6:** `screenshot.sh out_peek.png` to confirm face is visible on screen.

- [ ] **Step 7:** Commit: `git add firmware/src/clawd_director.cpp && git commit -m "feat: activate peek beat — face rises on tap, curious hold, drops on return"`

---

### Task 9: Show beat

**Files:**
- Modify: `firmware/src/clawd_director.cpp`

**Interfaces:**
- Consumes: `performer_available()`, `performer_place_offstage()`, `performer_walk_home()`, `performer_perform_group()`, `performer_walk_off()`, `performer_busy()`, `show_gate_mark_shown()` (clawd_policy.h), `clawd_show_group()` (clawd_policy.h), `usage_rate_group()`.

- [ ] **Step 1:** Add a `run_show()` function. All `performer_*` calls are wrapped by `performer_available()` guard. The step machine:

  ```cpp
  static void run_show(uint32_t now) {
      (void)now;
      if (!performer_available()) {
          // No PSRAM: dance in the seat instead
          if (mascot_act("dancing", 2)) go(B_CORNER);
          return;
      }
      switch (step) {
      case 0:   // point first
          if (mascot_act("pointing", 1)) next_step();
          break;
      case 1:   // settled after point; walk off, stage performer
          if (!settled()) return;
          mascot_leave(150);
          performer_place_offstage(PERFORMER_RIGHT);
          next_step();
          break;
      case 2:   // wait for mascot to leave
          if (!mascot_away()) return;
          performer_walk_home(150);
          next_step();
          break;
      case 3:   // wait for performer to reach centre
          if (performer_busy()) return;
          show_gate_mark_shown(&show_gate, now_ms());
          performer_perform_group(clawd_show_group(usage_rate_group()),
                                  clawd_tuning().show_scene_ms, false);
          next_step();
          break;
      case 4:   // wait for scene loop
          if (performer_busy()) return;
          mascot_act("waving", 1);
          next_step();
          break;
      case 5:   // settled after wave
          if (!settled()) return;
          performer_walk_off(PERFORMER_LEFT, 150);
          next_step();
          break;
      default:  // performer gone; corner walks home
          if (performer_busy()) return;
          mascot_return(-1, 150);
          go(B_CORNER);
          break;
      }
  }
  ```

- [ ] **Step 2:** In `director_tick()`, replace `case B_SHOW: go(B_HOME); break;` with `case B_SHOW: run_show(now); break;`.

- [ ] **Step 3:** In `run_home()`, uncomment the show gate:
  ```cpp
  if (live && signals_hooks_seen() && show_gate_ready(&show_gate, now, &clawd_tuning()))
      { go(B_SHOW); return; }
  ```

- [ ] **Step 4:** Build all three envs.

- [ ] **Step 5:** Flash and test: `clawd show` via serial → mascot points, walks off, performer walks on, plays scene, walks off, mascot returns.

- [ ] **Step 6:** `screenshot.sh out_show.png` during the scene to confirm performer is visible.

- [ ] **Step 7:** Commit: `git add firmware/src/clawd_director.cpp && git commit -m "feat: activate show beat — full-size performer after heavy usage stretch"`

---

### Task 10: Screensaver, face-home, and run-home beats

**Files:**
- Modify: `firmware/src/clawd_director.cpp`

**Interfaces:**
- Consumes: `face_rise()`, `face_drop()`, `face_set_resting()`, `face_moving()`, `face_visible()`, `face_blink()`, `performer_place_offstage()`, `performer_walk_home()`, `performer_perform_group()`, `performer_walk_off()`, `performer_busy()`, `performer_release()`, `performer_run_off()`, `screensaver_ready()` (clawd_policy.h), `clawd_jitter()`.

- [ ] **Step 1:** Add `static uint32_t calm_at = 0;` to the director's static variables (near the other `static uint32_t` declarations at the top of `clawd_director.cpp`).

- [ ] **Step 2:** Add `run_screensaver()`:

  ```cpp
  static void run_screensaver(uint32_t now) {
      switch (step) {
      case 0:
          mascot_leave(100);
          face_rise();
          next_step();
          break;
      case 1:   // wait: mascot away and face settled
          if (!mascot_away() || face_moving()) return;
          face_set_resting(true);
          calm_at = now + clawd_jitter(
              (clawd_tuning().calm_min_ms + clawd_tuning().calm_max_ms) / 2, &rng);
          next_step();
          break;
      case 2:   // resting; wait for calm dip
          if (!performer_available()) return;   // no PSRAM: stay resting indefinitely
          if ((int32_t)(now - calm_at) < 0) return;
          face_set_resting(false);
          performer_place_offstage(PERFORMER_LEFT);
          next_step();
          break;
      case 3:
          if (!settled()) return;
          performer_walk_home(100);
          next_step();
          break;
      case 4:
          if (performer_busy()) return;
          performer_perform_group(0, clawd_tuning().calm_scene_ms, true);
          next_step();
          break;
      case 5:
          if (performer_busy()) return;
          performer_walk_off(PERFORMER_LEFT, 100);
          next_step();
          break;
      default:  // performer gone; back to resting
          if (performer_busy()) return;
          face_set_resting(true);
          calm_at = now + clawd_jitter(
              (clawd_tuning().calm_min_ms + clawd_tuning().calm_max_ms) / 2, &rng);
          step = 2;   // back to resting, don't re-leave
          break;
      }
  }
  ```

- [ ] **Step 3:** Add `run_face_home()` (face was up — screensaver or peek — now coming down and walking home):

  ```cpp
  static void run_face_home(uint32_t now) {
      (void)now;
      switch (step) {
      case 0:
          face_blink(false);
          next_step();
          break;
      case 1:
          if (face_busy()) return;
          face_drop();
          face_set_resting(false);
          next_step();
          break;
      default:
          if (face_visible()) return;
          mascot_hurry_home();
          go(B_CORNER);
          break;
      }
  }
  ```

- [ ] **Step 4:** Add `run_run_home()` (performer on stage — interrupted mid-show):

  ```cpp
  static void run_run_home(uint32_t now) {
      switch (step) {
      case 0:
          performer_release();
          next_step(200);
          break;
      case 1:
          if ((int32_t)(now - wait_until) < 0) return;
          performer_run_off(PERFORMER_LEFT, 150, 300, 400);
          mascot_return(-1, 150);
          next_step();
          break;
      default:
          if (performer_busy() || mascot_busy()) return;
          go(B_HOME);
          break;
      }
  }
  ```

- [ ] **Step 5:** In `director_tick()`, replace the three Phase 2 stubs:
  ```cpp
  case B_SCREENSAVER: run_screensaver(now); break;
  case B_FACE_HOME:   run_face_home(now);   break;
  case B_RUN_HOME:    run_run_home(now);    break;
  ```

- [ ] **Step 6:** In `run_home()`, uncomment the screensaver gate:
  ```cpp
  if (screensaver_ready(&q, &clawd_tuning())) { go(B_SCREENSAVER); return; }
  ```

- [ ] **Step 7:** Update `to_meters()` — replace the Phase 2 comment block:
  ```cpp
  static void to_meters(bool noticed) {
      (void)noticed;
      if (beat == B_FACE_HOME || beat == B_RUN_HOME) return;
      if (face_visible()) { face_set_resting(false); go(B_FACE_HOME); return; }
      if (beat == B_SHOW) { go(B_RUN_HOME); return; }
      if (beat == B_SCREENSAVER) {
          if (attention_on) { go(B_ATTENTION); return; }
          face_set_resting(false);
          go(B_FACE_HOME);
          return;
      }
  }
  ```

- [ ] **Step 8:** Update `arrive_meters()` (called after attention ends) — replace the Phase 2 screensaver resume comment:
  ```cpp
  [[maybe_unused]] static void arrive_meters(void) {
      if (attention_on) { go(B_ATTENTION); return; }
      if (resume_screensaver) {
          resume_screensaver = false;
          face_rise();
          calm_at = now_ms() + clawd_jitter(
              (clawd_tuning().calm_min_ms + clawd_tuning().calm_max_ms) / 2, &rng);
          go(B_SCREENSAVER);
          step = 2;   // already off-stage and face up; skip the leave/rise steps
          return;
      }
      mascot_return(-1, 150);
      go(B_CORNER);
  }
  ```

- [ ] **Step 9:** In `director_button()`, activate the screensaver wake — replace `if (beat == B_SCREENSAVER) to_meters(false);` (this line is already there; verify it now calls `to_meters()` which routes through `B_FACE_HOME`).

- [ ] **Step 10:** Build all three envs: `pio run -d firmware -e waveshare_amoled_216 2>&1 | tail -5 && pio run -d firmware -e waveshare_amoled_18 2>&1 | tail -5 && pio run -d firmware -e waveshare_amoled_216_c6 2>&1 | tail -5`

- [ ] **Step 11:** Flash and test via serial:
  - `clawd screensaver` → mascot walks off, face rises, face rests with blinks
  - `clawd home` → face drops (blink first), mascot hurries back
  - `clawd show` then `clawd home` mid-show → performer runs off, mascot returns

- [ ] **Step 12:** `screenshot.sh out_screensaver.png` to confirm face is on screen.

- [ ] **Step 13:** Commit: `git add firmware/src/clawd_director.cpp && git commit -m "feat: activate screensaver, face-home, and run-home beats — Phase 2 complete"`
