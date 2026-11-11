#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "hook_event.h"

// Signals: turns incoming hook events into "what just happened" for the
// Clawd director. Owns the fast clock (hook events, activity level, session
// count). Pure: no LVGL, no Arduino — the _at functions take an explicit
// timestamp so they can run in native unit tests.
//
// Call order per event: settings_server receives POST /event →
// writes g_hook_event.pending = true → loop() calls signals_ingest().
//
// Backward compatibility: if no hook events ever arrive, signals_hooks_seen()
// stays false and every getter returns its "unknown" value. The director
// falls back to the slow clock (usage polling) in that case.

struct SignalsUpdate {
    bool     event_fired;      // a hook event the device hasn't reacted to yet
    char     event;            // 'p' prompt, 's' stop, 'b' back; valid when event_fired
    uint32_t event_seq;        // es that fired
    bool     sessions_changed; // n changed (including first sight)
};

// Event dedup: the device reacts once per distinct es. The first es seen
// after boot sets the baseline (it happened before we were listening) and
// does not fire. A later restart of the hook process that resets its counter
// still fires, because the seq value changed.

void          signals_clear(void);  // reset to boot state; also for tests
SignalsUpdate signals_ingest_at(const ClawdHookEvent* ev, uint32_t now_ms);
SignalsUpdate signals_ingest(const ClawdHookEvent* ev);  // millis() wrapper

// True once any fast-clock field (a, e/es, n) has arrived since boot.
bool signals_hooks_seen(void);

int  signals_activity_at(uint32_t now_ms);  // 0–3; -1 unknown or expired (>5 min)
int  signals_activity(void);                // millis() wrapper
int  signals_sessions(void);                // -1 unknown
char signals_last_event(void);              // '\0' none since boot
