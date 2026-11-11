#include "signals.h"
#include <string.h>
#include <Arduino.h>  // millis()

#define ACTIVITY_EXPIRE_MS 300000u  // 5 minutes with no update → activity unknown

static bool     s_hooks_seen   = false;
static int      s_activity     = -1;
static uint32_t s_activity_ms  = 0;
static int      s_sessions     = -1;
static char     s_last_event   = '\0';
static uint32_t s_last_seq     = 0;
static bool     s_seq_baseline = true;  // first seq after boot = baseline, no fire

void signals_clear(void) {
    s_hooks_seen   = false;
    s_activity     = -1;
    s_activity_ms  = 0;
    s_sessions     = -1;
    s_last_event   = '\0';
    s_last_seq     = 0;
    s_seq_baseline = true;
}

SignalsUpdate signals_ingest_at(const ClawdHookEvent* ev, uint32_t now_ms) {
    SignalsUpdate u = {};
    if (!ev || !ev->pending) return u;

    // Activity level (expires independently)
    if (ev->activity >= 0) {
        s_activity    = ev->activity;
        s_activity_ms = now_ms;
        s_hooks_seen  = true;
    }

    // Session count
    if (ev->sessions >= 0 && ev->sessions != s_sessions) {
        u.sessions_changed = true;
        s_sessions         = ev->sessions;
        s_hooks_seen       = true;
    }

    // Hook event with sequence dedup
    if (ev->type != '\0') {
        s_hooks_seen = true;
        if (s_seq_baseline) {
            // First event after boot: set baseline, do not fire
            s_last_seq     = ev->seq;
            s_last_event   = ev->type;
            s_seq_baseline = false;
        } else if (ev->seq != s_last_seq) {
            u.event_fired = true;
            u.event       = ev->type;
            u.event_seq   = ev->seq;
            s_last_seq    = ev->seq;
            s_last_event  = ev->type;
        }
    }

    return u;
}

SignalsUpdate signals_ingest(const ClawdHookEvent* ev) {
    return signals_ingest_at(ev, millis());
}

bool signals_hooks_seen(void) { return s_hooks_seen; }

int signals_activity_at(uint32_t now_ms) {
    if (s_activity < 0) return -1;
    if (now_ms - s_activity_ms > ACTIVITY_EXPIRE_MS) return -1;
    return s_activity;
}

int  signals_activity(void)   { return signals_activity_at(millis()); }
int  signals_sessions(void)   { return s_sessions; }
char signals_last_event(void) { return s_last_event; }
