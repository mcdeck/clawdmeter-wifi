# Clawd Director — Design Spec

**Date:** 2026-11-11  
**Branch:** wifi-native

---

## Summary

Port the upstream Clawd director subsystem to the Wi-Fi fork so the device
reacts to Claude Code activity in real time: Clawd idles in his corner seat,
reacts to prompts and stops with animation, and the user can tune how lively
he is.

The fork already has `splash_animations.h` (all the animation data) and the
just-landed `signals.h/.cpp` + `hook_event.h` (the HTTP event receiver that
feeds hook events from Claude Code). The director is the missing piece between
those inputs and an on-screen reaction.

---

## Phase 1 (this spec) vs Phase 2 (deferred)

### Phase 1 — Corner + Attention

Implement the **B_HOME**, **B_CORNER**, **B_ATTENTION**, and simplified
**B_PEEK** beats. This covers the main ask: Clawd idles in the corner and
visibly reacts when you start a Claude Code prompt.

Attention in Phase 1 uses the **corner animation** (small Clawd waves and
points from his seat); the full-size walk-on requires Phase 2.

| File | Phase | Note |
|------|-------|------|
| `clawd_anim.{h,cpp}` | 1 | Thin wrapper over `splash_animations.h` — port verbatim |
| `clawd_policy.{h,cpp}` | 1 | Pure math — port verbatim |
| `clawd_prefs.{h,cpp}` | 1 | NVS persistence — adapt (see §Adaptations) |
| `clawd_still.h` | 1 | Static RGB565A8 pose — copy verbatim |
| `mascot.{h,cpp}` | 1 | Corner character — port verbatim |
| `clawd_director.{h,cpp}` | 1 | State machine — adapt (skip face/show/screensaver beats) |

### Phase 2 — Performer + Face (deferred)

| File | Beats unlocked |
|------|---------------|
| `performer.{h,cpp}` | B_SHOW (treat after busy stretch), full-size attention walk-on |
| `face.{h,cpp}`, `face_motion.{h,cpp}` | B_SCREENSAVER, full B_PEEK (curious face at the glass) |

Phase 2 depends on Phase 1 being stable and needs its own spec.

---

## Architecture and Data Flow

```
Claude Code hooks  ──POST /event──►  g_hook_event  ──loop()──►  signals_ingest()
                                                                       │
wifi_poller (60s)  ──new data──►  UsageData                           │
                                       │                               │
                                       └──────► director_on_payload(d, sig)
                                                        │
                                              clawd_policy (gates, timing)
                                                        │
                                      ┌─────────────────┴────────────────────┐
                                   mascot                            (phase 2)
                              (corner sprite,                     performer + face
                               clawd_anim API)
```

`director_on_payload()` takes both `UsageData*` and `SignalsUpdate*`. In the
fork, main.cpp calls it in two places:

1. **On new poll data**: `SignalsUpdate su = {}; director_on_payload(&usage, &su);`
2. **On hook event**: `SignalsUpdate su = signals_ingest(&g_hook_event); director_on_payload(&usage, &su);`

Tap events are routed from `ui.cpp`'s gesture/touch handler to
`director_tap(x)`. Button presses call `director_button()`.

---

## Files

### New (Phase 1)

**`clawd_anim.{h,cpp}`** — Port verbatim from upstream. `clawd_anim.cpp` is the
single translation unit that includes `splash_animations.h`; all other modules
access the art through the `clawd_anim_at()` / `clawd_anim()` / `clawd_anim_index()`
API. The fork's `splash_animations.h` is already present and compatible —
verify `SPLASH_PALETTE_SIZE == CLAWD_PALETTE_SIZE` (both 16) at compile time.

**`clawd_still.h`** — Copy verbatim. 10 KB, RGB565A8 static Clawd-Still pose
used as a no-animation fallback in minimal mode.

**`clawd_policy.{h,cpp}`** — Port verbatim. Pure C++ (no LVGL, no Arduino) so
it compiles in native unit tests too. Contains: `ClawdTuning` struct, liveliness
1–5 scaling, `ShowGate`, `QuietInput`, `TurnTracker`, peek geometry helpers,
resting face behaviour helpers, `ClawdRng` (xorshift32).

**`mascot.{h,cpp}`** — Port verbatim. The corner character: `mascot_create()`
allocates the LVGL image object at `(slot_x, feet_y)` in cells, `mascot_tick()`
advances the animation each loop, `mascot_autonomy(bool)` enables idle acts.
Corner sprite data comes through `clawd_anim_at()`; the full-size sprite path
(`mascot_attention` with PSRAM walk-on) requires Phase 2 performer — the
fallback path (wave + point from seat) already handles this gracefully in the
existing code.

**`clawd_prefs.{h,cpp}`** — Adapted from upstream (see §Adaptations).
Loads and applies `liveliness` and `attn_anim` from NVS on boot.

**`clawd_director.{h,cpp}`** — Adapted from upstream (see §Adaptations).
Beats implemented in Phase 1: `B_HOME`, `B_CORNER`, `B_ATTENTION`,
`B_PEEK` (simplified — hop only, no face). Beats stubbed to `B_HOME`:
`B_SHOW`, `B_SCREENSAVER`, `B_FACE_HOME`, `B_RUN_HOME`.

### Modified

**`main.cpp`**
- Call `director_init()` after `ui_init()`
- Call `director_tick()` in the main loop
- Call `director_on_payload(&usage, &su)` from both the new-data block and the
  hook-event block (replacing the current bare `signals_ingest` call in the
  hook-event block)
- Route `director_button(true)` on PWR press (PRIMARY and SECONDARY buttons
  already go to BLE; PWR's screen-cycle action stays as-is for now — see
  §Adaptation note on buttons)

**`ui.h/.cpp`**
- Add `ui_mascot_parent()` — returns the LVGL container the mascot is drawn
  into (see §Mascot position)
- Add `ui_meters_live()` — returns true when the usage screen is showing valid
  non-stale data (used by screensaver gate in `clawd_policy`)
- Wire tap events on SCREEN_USAGE to `director_tap(x)` from the existing
  `screen_gesture_cb` or the touch callback (see §Tap routing)

**`settings_server.cpp`**
- Add Liveliness section to the settings HTML (slider or select 1–5)
- Add Attention animation section (select: 120 s / 30 s / 10 s / off)
- Add `POST /settings/clawd` handler that calls `liveliness_set()` and
  `attn_anim_set()` from `clawd_prefs.h`

---

## Director Beats (Phase 1)

### B_HOME — corner, Clawd idling on his own

Default state. `mascot_autonomy(true)` is set here when
`clawd_tuning().idle_acts` is true (driven by liveliness). Clawd sits in his
seat and occasionally does idle acts (small waves, stretches) on his own.
Transitions out on:
- Prompt hook event ('p') and liveliness > 1 → B_ATTENTION (or B_CORNER
  react-notice if attention is off)
- Stop hook event after a long turn ('s') → B_CORNER (wave)
- Usage reset → B_CORNER (joy animation)
- Show gate ready → stub to B_HOME for Phase 1 (B_SHOW deferred)
- Tap on meters → B_PEEK

### B_CORNER — a commanded corner animation

Steps: wait for mascot_free(), issue `mascot_act(anim, times)`, wait for
mascot_free(), return to B_HOME. Used for: notice-on-prompt, long-turn wave,
usage-reset joy, hurry-home after a reaction queued during a lurk trip.

### B_ATTENTION — the big Clawd fetches you

Triggered by `director_alert(message)` from main.cpp when a 'p' hook event
fires and attention animation is enabled (`clawd_attn_anim() > 0`).

Phase 1 (no performer): `mascot_attention(true)` — small Clawd walks to the
seat if needed, then waves and points from his seat until `mascot_attention(false)`.
Dismissed by: tap, `director_alert_clear()`, or `alert_timeout_ms` timeout.

Phase 2 upgrade: add performer walk-on path when `performer_available()`.

The alert message is displayed via the existing `ui_set_status()` channel.

### B_PEEK — tap on the meters

Phase 1 (no face): `mascot_act("hopup", 1, true)` (snappy, startled hop).
After the act completes, return to B_HOME. Tap is ignored while already in
B_PEEK (debounced by beat).

Phase 2 upgrade: add face-up path when `face_available()`.

---

## Mascot Position

The corner mascot lives on the **usage screen** (SCREEN_USAGE).

```
cell = min(board_caps().width, board_caps().height) / 20

// Feet line: near the bottom of the screen, above the bottom margin
feet_y = board_caps().height - 2 * cell

// X slot: bottom-right corner, offset inward by ~4 cells from the right edge
slot_x = board_caps().width / cell - 4
```

`corner_seat_feet()` from `clawd_policy.h` refines `feet_y` so the tallest
animation clears the usage bars above it. The director calls this during
`director_init()`.

`ui_mascot_parent()` returns `lv_screen_active()` while SCREEN_USAGE is
visible (the mascot is a direct child of the usage screen object so it
z-orders above the bars). The director calls `mascot_create(parent, slot_x,
feet_y, cell)` during `director_init()`.

---

## Tap Routing

The existing `screen_gesture_cb` in `ui.cpp` handles swipes. Single taps on
SCREEN_USAGE should additionally call `director_tap(x)`. Add to the
`LV_EVENT_CLICKED` path on the usage screen container:

```cpp
if (ui_get_current_screen() == SCREEN_USAGE) {
    lv_point_t pos;
    lv_indev_get_point(lv_indev_active(), &pos);
    director_tap(pos.x);
}
```

Taps on other screens reach the director only as `director_button(false)` (a
"you are here" engagement signal), not as `director_tap()`.

---

## Settings Web UI Additions

New **Clawd** section in the settings page, below the Security section:

**Liveliness** — `<select>` with options:
- 1 — No-nonsense (sits still, no acts or reactions)
- 2 — Mostly quiet
- 3 — Some nonsense (default)
- 4 — Lively
- 5 — All-nonsense

**Attention animation** — `<select>` with options:
- 120 s (default)
- 30 s
- 10 s
- Off

Form submits `POST /settings/clawd` with `liveliness` and `attn_anim` fields.
Handler calls `liveliness_set(v)` and `attn_anim_set(v)` and redirects with
`?saved=1`.

---

## NVS Keys (clawd_prefs)

| Key | Type | Default | Function |
|-----|------|---------|----------|
| `liveliness` | int32 | 3 | `liveliness_set()` / `liveliness_get()` |
| `attn_anim` | int32 | 120 | `attn_anim_set()` / `attn_anim_get()` |

The upstream `clawd_prefs` also stores `pace_marker`, `button_actions`, and
`palette` (`th`). In the fork, button actions and palette are handled
separately (see §Adaptations) and pace_marker is not yet implemented — omit
all three from `clawd_prefs` in Phase 1.

---

## Adaptations from Upstream

### Signals interface
Upstream `clawd_director.cpp` calls `signals_ingest(d)` with a `UsageData*`
that includes fast-clock fields (`a`, `e`, `es`, `n`, `rp`). The fork's
`UsageData` does not have these fields; instead `signals_ingest()` takes
a `ClawdHookEvent*`. The director receives a `SignalsUpdate*` from main.cpp
and does not call `signals_ingest` directly — that call stays in main.cpp.

### No `director_alert` from BLE
Upstream receives alert messages via BLE payload field. In the fork, an alert
fires from main.cpp whenever a 'p' hook event arrives and
`clawd_attn_anim() > 0`:
```cpp
if (su.event_fired && su.event == 'p' && clawd_attn_anim() > 0)
    director_alert("Claude is thinking…");
```

### No `director_set_pairing_touch`
The fork has no pairing touch mode (BLE pairing is unaffected by the usage
screen). Remove from the adapted header; leave a no-op stub if other call
sites reference it.

### Buttons
Upstream `director_button(main_button)` distinguishes the "main" button (the
only button on boards without a touch screen) from secondary buttons. In the
fork:
- PRIMARY (Space / PTT) → passes `director_button(false)` as engagement only
- PWR → `director_button(true)` before the screen-cycle action; if the
  director returns `true` (it consumed the press to dismiss an alert),
  skip the cycle

### `clawd_prefs` stripped
Phase 1 `clawd_prefs` persists only `liveliness` and `attn_anim`. Drop the
`pace_marker`, `button_action`, `theme` (palette) responsibilities. When
those features land (settings web UI spec), their NVS load moves to their
own modules.

### `ui_meters_live()`
Not present in the fork. Implement as:
```cpp
bool ui_meters_live(void) {
    return ui_get_current_screen() == SCREEN_USAGE && usage_data_valid;
}
```
where `usage_data_valid` is a file-scoped flag in `ui.cpp` set by
`ui_update()` from `usage.valid`.

### `minimal_mode.h` stub
Upstream has a `minimal_mode.h` controlling a low-resource display path.
The fork does not implement minimal mode; stub it as:
```cpp
inline bool minimal_mode_active(void) { return false; }
```
in a local `minimal_mode.h` so `clawd_director.cpp` compiles unchanged.

---

## Serial Debug Commands (Phase 1)

Add a `clawd <sub>` serial command handled in `main.cpp`:

| Command | Effect |
|---------|--------|
| `clawd home` | Force B_HOME |
| `clawd attention` | Force B_ATTENTION |
| `clawd corner` | Force B_CORNER (wave) |
| `clawd peek` | Force B_PEEK at centre-x |
| `clawd liveliness <1-5>` | Set liveliness immediately |
| `clawd trace on\|off` | Toggle director beat logging |

Wraps `director_debug()` from upstream, which already handles these names.

---

## Build Notes

- No new PlatformIO `lib_deps` required — all dependencies (LVGL, Arduino,
  `splash_animations.h`) are already present.
- **Critical prerequisite:** `splash.cpp` currently includes `splash_animations.h`
  directly. With `clawd_anim.cpp` also including it, `splash_anims[]` would be
  compiled into two translation units → ~800 KB flash waste and linker warnings.
  **Before implementing `clawd_anim.cpp`, migrate `splash.cpp` to use the
  `clawd_anim` API:**

  | `splash.cpp` today | Replace with |
  |--------------------|--------------|
  | `#include "splash_animations.h"` | `#include "clawd_anim.h"` |
  | `SPLASH_ANIM_COUNT` | `clawd_anim_count()` |
  | `splash_anims[i].name` | `clawd_anim_at(i)->name` |
  | `splash_anims[i].w / h / ox / oy / frame_count / loop_start / loop_end` | same fields on `clawd_anim_at(i)` |
  | `splash_anims[i].palette[c]` | `clawd_anim_at(i)->palette[c]` (now live / theme-aware) |
  | `splash_anims[i].frames[f*w*h+j]` | `clawd_frame(clawd_anim_at(i), f)[j]` |
  | `splash_anims[i].holds[f]` | `clawd_anim_at(i)->holds[f]` |
  | `const splash_anim_def_t*` pointers | `const ClawdAnim*` |

  Side effect: splash gains live palette support — TH_CLAWD theme changes
  now affect the splash screen colour without a rebuild.

- `clawd_still.h` is a 10 KB array in flash (same pattern as `bell_pcm.h`).
- Build both envs (`waveshare_amoled_216`, `waveshare_amoled_18`) before
  considering complete.

---

## Implementation Order

1. `clawd_anim.{h,cpp}` — verify `splash_animations.h` single-include
2. `clawd_still.h` — copy
3. `clawd_policy.{h,cpp}` — pure, compiles immediately
4. `minimal_mode.h` — stub
5. `clawd_prefs.{h,cpp}` — depends on policy
6. `mascot.{h,cpp}` — depends on anim + policy + LVGL
7. `clawd_director.{h,cpp}` — depends on all above
8. `ui.cpp` changes — `ui_mascot_parent()`, `ui_meters_live()`, tap routing
9. `main.cpp` changes — init/tick/on_payload wiring, serial command
10. `settings_server.cpp` — Clawd section in settings HTML
11. Build both envs, `screenshot` each beat for visual QA
