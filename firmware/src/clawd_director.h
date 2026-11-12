#pragma once
#include <stdint.h>
#include "data.h"
#include "signals.h"

// The director: one Clawd, on the meters, driven by the situation.
//
//   meters  home and default; every path ends here. The corner Clawd sits in
//           his seat and does small acts (mascot.h). Phase 2: he also performs
//           here, full size in front of them (performer.h): a treat after a busy
//           stretch, or a calm dip during the screensaver.
//   face    Phase 2: the #198 face resting on the glass (face.h): the screensaver,
//           and the peek when you tap the meters.
//
// Beats (design system §09): notice, show, tap → meters, screensaver + wake,
// attention, fresh-window joy, long-turn wave, and the tap-on-the-meters
// peek. The lurk trip and idle acts are the corner's own secondary action.
// Minimal mode collapses all of it to a still Clawd.
//
// Fork Phase 1: B_HOME, B_CORNER, B_ATTENTION, B_PEEK active.
// B_SHOW, B_SCREENSAVER, B_FACE_HOME, B_RUN_HOME stub to B_HOME.

enum DirectorRoom { ROOM_METERS, ROOM_FACE };

void director_init(void);          // after ui_init()
void director_tick(void);

// Per parsed payload.
void director_on_payload(const UsageData* d, const SignalsUpdate* sig);
// Called when the 5-hour usage window resets (fires "jumping happy" joy).
void director_on_reset(void);
void director_alert(const char* message);   // attention hook fired
void director_alert_clear(void);            // the session moved on
bool director_take_dismissed(void);         // an alert ended on the device since the last call

// Any touch press at screen x (wired internally to LVGL).
void director_tap(int x);
// No-op in this fork (BLE pairing mode not applicable).
static inline void director_set_pairing_touch(bool on) { (void)on; }
// A physical button press: you're here.
bool director_button(bool main_button);
// Apply a liveliness level (clawd_policy.h) now.
void director_set_liveliness(int level);

// How long Clawd's attention animation runs, in seconds (clawd_policy.h; 0 = off).
void director_set_attn_anim(int seconds);

// Print every beat step with a timestamp.
void director_set_trace(bool on);

DirectorRoom director_room(void);
const char*  director_beat_name(void);

// QA hook for the `clawd <beat>` serial command.
bool director_debug(const char* beat);
