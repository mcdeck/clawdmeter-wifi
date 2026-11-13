#include "clawd_director.h"
#include "clawd_policy.h"
#include "clawd_anim.h"
#include "mascot.h"
#include "face.h"
#include "performer.h"
#include "ui.h"
#include "idle.h"
#include "minimal_mode.h"
#include "usage_rate.h"
#include "hal/board_caps.h"
#include <Arduino.h>
#include <lvgl.h>
#include <string.h>
#include <stdlib.h>

// ─── Beats ───────────────────────────────────────────────────────────────────
// Phase 1 active beats: B_HOME, B_CORNER, B_ATTENTION, B_PEEK.
// Phase 2 beats stub to B_HOME immediately: B_SHOW, B_SCREENSAVER,
// B_FACE_HOME, B_RUN_HOME.
enum Beat {
    B_HOME,          // meters, corner Clawd idling on his own
    B_CORNER,        // a commanded corner act (notice, wave, joy) or walk home
    B_SHOW,          // Phase 2: notice → walks off → performs on the meters → home
    B_SCREENSAVER,   // Phase 2: walk off → face rests, dips into calm performances
    B_FACE_HOME,     // Phase 2: face drops into the meters, he hurries to his seat
    B_RUN_HOME,      // Phase 2: the performance stops, he runs off, the corner walks home
    B_ATTENTION,     // the big Clawd fetches you
    B_PEEK,          // a tap on the meters: hop, duck out, he walks back (Phase 2: face)
};
static const char* const BEAT_NAMES[] = {
    "home", "corner", "show", "screensaver", "face-home", "run-home", "attention", "peek",
};

static Beat     beat = B_HOME;
static uint8_t  step = 0;
static uint32_t wait_until = 0;
static bool     notice_wake = false;  // Phase 2: B_FACE_HOME blink-first flag

// Attention
static bool     attention_on = false;
static bool     attention_dismissed = false;  // ended here; the app hasn't heard
static uint32_t attention_since = 0;
static bool     attn_anim_done = false;       // Clawd's part of the alert is over; it stays up
static AttnFrom attn_from = ATTN_FROM_HOME;   // what it interrupted
static uint32_t attn_mark = 0;                // last_engagement right after the alert
static bool     attn_engaged = false;         // you showed up since the alert (sticky)
static bool     resume_screensaver = false;   // decided when attention ends

// A notice / wave waiting for him to hurry home from a lurk trip.
static const char* react_anim = NULL;
static uint8_t     react_times = 0;
static uint32_t    react_at = 0;
static bool        trace = false;

// Situation
static ShowGate    show_gate;
static TurnTracker turn;
static uint32_t    last_engagement = 0;
static bool        window_not_started = false;
static bool        have_pct = false;
static float       last_pct = 0;
static bool        joy_pending = false;
static uint32_t    joy_at = 0;
static bool        minimal = false;
static ClawdRng    rng;

// A tap on the meters (the aquarium)
static int         peek_x = 0;
static uint32_t    peek_up_at = 0;
static lv_indev_t* touch_indev = NULL;

static uint32_t now_ms(void) { return millis(); }

static void go(Beat b) {
    if (b != beat) Serial.printf("[%lu] director: %s -> %s\n", (unsigned long)now_ms(),
                                 BEAT_NAMES[beat], BEAT_NAMES[b]);
    beat = b;
    step = 0;
    wait_until = now_ms();
    mascot_autonomy(b == B_HOME && !minimal && clawd_tuning().idle_acts);
}

void director_set_trace(bool on) { trace = on; }

static void next_step(uint32_t pause_ms = 0) {
    step++;
    if (trace)
        Serial.printf("[%lu] director:   %s step %u\n", (unsigned long)now_ms(), BEAT_NAMES[beat], step);
    wait_until = now_ms() + pause_ms;
}

static bool settled(void) {
    return !mascot_busy() && !face_moving() && !performer_busy()
           && (int32_t)(millis() - wait_until) >= 0;
}

static void engage(void) { last_engagement = now_ms(); }

// Back on the meters after a transition: attention takes over if it's
// waiting; an alert that ended mid-transition puts the screensaver back
// (Phase 2); otherwise he walks back to his seat from the left.
[[maybe_unused]] static void arrive_meters(void) {
    if (attention_on) { go(B_ATTENTION); return; }
    if (resume_screensaver) {
        resume_screensaver = false;
        // Phase 2: screensaver resume
    }
    mascot_return(-1, 150);
    go(B_CORNER);
}

// A corner reaction (noticing, the long-turn wave). If he's out on a lurk
// trip he cuts it short and hurries back, then reacts.
static void react(const char* anim, uint8_t times) {
    if (!clawd_tuning().reactions) return;
    if (beat != B_HOME && beat != B_CORNER) return;
    if (mascot_act(anim, times)) { react_anim = NULL; go(B_CORNER); return; }
    if (!mascot_hurry_home()) return;
    react_anim = anim;
    react_times = times;
    react_at = now_ms();
    if (beat != B_CORNER) go(B_CORNER);
}

// "Show me the meters": the fastest no-cut path from wherever we are.
static void to_meters(bool noticed) {
    (void)noticed;
    if (beat == B_FACE_HOME || beat == B_RUN_HOME) return;   // already on the way
    // Phase 2: face_set_resting(false), face_visible() → B_FACE_HOME, performing → B_RUN_HOME
    if (beat == B_SHOW || beat == B_SCREENSAVER) {
        if (attention_on) { go(B_ATTENTION); return; }
        mascot_return(-1, 150);
        go(B_CORNER);
    }
}

// ─── Attention ───────────────────────────────────────────────────────────────
static bool attn_anim_running(uint32_t now) {
    return !attn_anim_done &&
           (uint32_t)(now - attention_since) < clawd_attn_anim_ms(clawd_attn_anim(), clawd_tuning().alert_timeout_ms);
}

static void end_attention(AttnEnd how) {
    if (!attention_on) return;
    attention_on = false;
    if (attention_end_tells_host(how)) attention_dismissed = true;
    const bool engaged = attn_engaged || (int32_t)(last_engagement - attn_mark) > 0;
    resume_screensaver = attention_resumes_screensaver(attn_from, how, engaged);
    ui_set_alert_text(NULL);
    mascot_attention(false, !resume_screensaver);
    Serial.printf("[%lu] director: attention over (%s)%s\n", (unsigned long)now_ms(),
                  how == ATTN_END_TAP ? "tap" : how == ATTN_END_CLEAR ? "cleared" : "timeout",
                  resume_screensaver ? ", back to the screensaver" : "");
    if (beat == B_ATTENTION && step == 0) next_step();
}

void director_alert(const char* message) {
    if (!attention_on) {
        // Remember what it interrupts, so it can return there.
        const bool leaving_for_screensaver = beat == B_ATTENTION && resume_screensaver;
        attn_from = (beat == B_SCREENSAVER || leaving_for_screensaver) ? ATTN_FROM_SCREENSAVER
                  : (beat == B_SHOW)                                   ? ATTN_FROM_SHOW
                                                                       : ATTN_FROM_HOME;
        attn_engaged = false;
        resume_screensaver = false;
        react_anim = NULL;                    // the big Clawd outranks a corner reaction
    } else if ((int32_t)(last_engagement - attn_mark) > 0) {
        attn_engaged = true;
    }
    engage();
    attn_mark = last_engagement;
    attention_since = now_ms();
    attn_anim_done = false;
    ui_set_alert_text(message);
    if (attention_on) return;
    attention_on = true;
    if (beat == B_ATTENTION) { step = 0; return; }
    // Phase 2: face_visible() → B_FACE_HOME, performing → B_RUN_HOME
    // Attention never waits: interrupt whatever is on.
    go(B_ATTENTION);
}

void director_alert_clear(void) { end_attention(ATTN_END_CLEAR); }

bool director_take_dismissed(void) {
    const bool d = attention_dismissed;
    attention_dismissed = false;
    return d;
}

// ─── Inputs ──────────────────────────────────────────────────────────────────
#define PEEK_HOP_PCT    250
#define PEEK_LEAVE_PCT  500

// A tap while he's on the meters: brings the face up (Phase 2).
// In Phase 1 he hops (if seated) and ducks out, then returns after a beat.
static void tap_home(int x) {
    TapInput in;
    in.on_meters = beat == B_HOME || beat == B_CORNER;
    in.minimal = minimal;
    in.corner = mascot_free() ? CORNER_FREE : mascot_out() ? CORNER_OUT : CORNER_BUSY;
    TapReact r = clawd_tap_react(&in);
    if (r == TAP_PEEK_HOP && !mascot_act("jumping", 1, true, PEEK_HOP_PCT)) r = TAP_PEEK;
    if (r == TAP_PEEK) mascot_leave(PEEK_LEAVE_PCT);
    if (r != TAP_NONE) face_rise_toward(x);
    if (r != TAP_NONE) {
        react_anim = NULL;
        peek_x = x;
        peek_up_at = now_ms();
        go(B_PEEK);
    }
    Serial.printf("[%lu] director: tap at x %d -> %s\n", (unsigned long)now_ms(), x,
                  r == TAP_PEEK_HOP ? "peek (hop)" : r == TAP_PEEK ? "peek (heads out)" : "absorbed");
}

void director_tap(int x) {
    engage();
    if (attention_on) { end_attention(ATTN_END_TAP); return; }
    resume_screensaver = false;
    if (minimal) return;
    if (beat == B_HOME || beat == B_CORNER) {
        tap_home(x);
        return;
    }
    if (beat == B_PEEK) {
        if (face_visible()) { face_drop(); face_set_resting(false); }
        return;
    }
    to_meters(false);
}

bool director_button(bool main_button) {
    engage();
    // Fork: all boards have touch; has_touch is not in BoardCaps. Adapt:
    // button_count==1 boards (1.8) have only PWR — same role as BOOT here.
    if (main_button && attention_on && board_caps().button_count == 1) {
        end_attention(ATTN_END_TAP);
        return true;
    }
    resume_screensaver = false;
    if (beat == B_SCREENSAVER) to_meters(false);
    return false;
}

void director_set_attn_anim(int seconds) {
    clawd_set_attn_anim(seconds);
}

void director_on_reset(void) {
    if (!minimal && clawd_tuning().reactions) {
        if (beat == B_HOME && mascot_act("jumping happy", 2)) go(B_CORNER);
        else { joy_pending = true; joy_at = now_ms(); if (beat == B_HOME) mascot_hurry_home(); }
    }
}

void director_set_liveliness(int level) {
    clawd_set_liveliness(level);
    const ClawdTuning& t = clawd_tuning();
    if (!t.reactions) { react_anim = NULL; joy_pending = false; }
    if (!t.screensaver) resume_screensaver = false;
    if ((beat == B_SHOW && !t.shows) || (beat == B_SCREENSAVER && !t.screensaver)) to_meters(false);
    mascot_autonomy(beat == B_HOME && !minimal && t.idle_acts);
}

static void indev_cb(lv_event_t* e) {
    (void)e;
    if (ui_get_current_screen() != SCREEN_USAGE) return;
    lv_point_t p = { 0, 0 };
    if (touch_indev) lv_indev_get_point(touch_indev, &p);
    director_tap(p.x);
}

void director_on_payload(const UsageData* d, const SignalsUpdate* sig) {
    if (!d->valid || !d->ok) return;
    // Plans without a 5h session window (and not Enterprise) have no window to start.
    window_not_started = !d->enterprise && !d->has_session && d->session_reset_mins < 0;

    // The slow clock moving counts as you being around.
    if (have_pct && d->session_pct > last_pct + 0.01f) {
        engage();
        // Phase 2: screensaver wake-up here
    }
    have_pct = true;
    last_pct = d->session_pct;

    // sessions_changed fires on any CC-session-count change (opening/closing a
    // terminal); joy_pending is now set via director_on_reset() on the real
    // 5-hour usage-window refill. Remove the sessions_changed joy trigger so
    // "jumping happy" doesn't fire every time you open a new terminal.

    if (sig->event_fired) {
        engage();
        const bool wave = turn_on_event(&turn, sig->event, now_ms(), &clawd_tuning());
        if (minimal) return;
        if (sig->event == 'p' || sig->event == 'b') {
            resume_screensaver = false;
            // Phase 2: screensaver wake-on-prompt (to_meters(true))
            react("pointing", 1);
        } else if (sig->event == 's' && wave) {
            react("waving", 1);
        }
    }
}

// ─── Minimal mode ────────────────────────────────────────────────────────────
static void apply_minimal(bool on) {
    minimal = on;
    react_anim = NULL;
    resume_screensaver = false;
    // Phase 2: face_hide_now(), performer_place_offstage()
    mascot_set_minimal(on);
    beat = attention_on ? B_ATTENTION : B_HOME;
    step = 0;
    mascot_autonomy(!on && !attention_on);
    if (attention_on && attn_anim_running(now_ms())) mascot_attention(true);
    Serial.printf("director: minimal %s\n", on ? "on" : "off");
}

// ─── Init ────────────────────────────────────────────────────────────────────
void director_init(void) {
    show_gate_clear(&show_gate);
    turn_clear(&turn);
    clawd_rng_seed(&rng, now_ms() ^ 0xD1EC7u);
    last_engagement = now_ms();
    apply_minimal(minimal_mode_active());
    go(B_HOME);

    const BoardCaps& bc = board_caps();
    int cell = 3;
    int feet_y = bc.height - 2 * cell;
    int slot_x = bc.width - (20 + 2) * cell;   // right edge minus 22 cells (pixels)
    const ClawdAnim* tallest = clawd_anim_at(0);
    const int tallest_px = tallest ? tallest->h * cell : 20 * cell;
    feet_y = corner_seat_feet(feet_y, tallest_px, bc.height - 2, 5);
    lv_obj_t* parent = ui_mascot_parent();
    lv_obj_t* result = mascot_create(parent, slot_x, feet_y, cell);
    if (!result) Serial.println("[director] WARN: mascot_create returned NULL — sprite won't render");
    face_init();
    performer_init(parent, bc.height * 7 / 8);

    // Taps on the usage screen are routed via global_click_cb (LV_EVENT_CLICKED)
    // in ui.cpp, which calls director_tap(). No indev-level hook needed here.
    touch_indev = lv_indev_get_next(NULL);  // kept for lv_indev_get_point in indev_cb (unused path)
}

DirectorRoom director_room(void) { return ROOM_METERS; }  // Phase 2: face_visible() check

const char* director_beat_name(void) { return BEAT_NAMES[beat]; }

// ─── Beat scripts ────────────────────────────────────────────────────────────
static void run_corner(uint32_t now) {
    if (react_anim) {
        if (!reaction_still_due(react_at, now, &clawd_tuning())) {
            react_anim = NULL;
        } else {
            if (!mascot_seated() && !mascot_acting()) return;
            if (!mascot_act(react_anim, react_times)) return;
            react_anim = NULL;
            return;
        }
    }
    if (settled()) go(B_HOME);
}

static void run_home(uint32_t now) {
    if (minimal) return;
    if (idle_is_asleep()) return;
    if (joy_pending) {
        if ((uint32_t)(now - joy_at) > clawd_tuning().joy_pending_ms) joy_pending = false;
        else if (mascot_act("jumping happy", 2)) { joy_pending = false; go(B_CORNER); return; }
    }
    const bool live = ui_meters_live();
    QuietInput q = { now, last_engagement, signals_hooks_seen(), window_not_started, live };
    // Phase 2: screensaver
    (void)q;
    // if (screensaver_ready(&q, &clawd_tuning())) { go(B_SCREENSAVER); return; }
    // Phase 2: show
    // if (live && signals_hooks_seen() && show_gate_ready(&show_gate, now, &clawd_tuning()))
    //     go(B_SHOW);
}

static void run_attention(uint32_t now) {
    if (step == 0) {
        if (attn_anim_running(now)) {
            mascot_attention(true);
        } else {
            if (!attn_anim_done) {
                attn_anim_done = true;
                mascot_attention(false, true);
                mascot_hurry_home();
            }
            if (mascot_away()) mascot_return(-1, 150);
        }
        if ((uint32_t)(now - attention_since) >= clawd_tuning().alert_timeout_ms)
            end_attention(ATTN_END_TIMEOUT);
        return;
    }
    // Over: wait for the big one to leave and the small one to sit down.
    if (mascot_in_attention() || mascot_busy()) return;
    if (resume_screensaver) {
        resume_screensaver = false;
        // Phase 2: screensaver resume
    }
    if (mascot_away()) {
        mascot_return(-1, 150);
        go(B_CORNER);
        return;
    }
    go(B_HOME);
}

// A tap on the meters: face rises, mascot ducks out, curious hold, face drops, mascot home.
static void run_peek(uint32_t now) {
    switch (step) {
    case 0:  // hop (if any) landed; mascot leaving
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

void director_tick(void) {
    const uint32_t now = now_ms();
    const int activity = signals_activity();
    show_gate_update(&show_gate, now, activity, &clawd_tuning());
    if (activity > 0) last_engagement = now;
    face_tick();
    performer_tick();

    switch (beat) {
    case B_HOME:        run_home(now); break;
    case B_CORNER:      run_corner(now); break;
    case B_SHOW:        go(B_HOME); break;        // Phase 2
    case B_SCREENSAVER: go(B_HOME); break;        // Phase 2
    case B_FACE_HOME:   go(B_HOME); break;        // Phase 2
    case B_RUN_HOME:    go(B_HOME); break;        // Phase 2
    case B_ATTENTION:   run_attention(now); break;
    case B_PEEK:        run_peek(now); break;
    }
}

// ─── QA ──────────────────────────────────────────────────────────────────────
bool director_debug(const char* name) {
    if (strcmp(name, "home") == 0) {
        engage();
        if (attention_on) { end_attention(ATTN_END_TAP); return true; }
        resume_screensaver = false;
        if (!minimal) to_meters(false);
        return true;
    }
    if (strcmp(name, "attention") == 0)   { director_alert("Clawdmeter test alert"); return true; }
    if (minimal) return true;
    if (strcmp(name, "joy") == 0) {
        joy_pending = true;
        joy_at = now_ms();
        return true;
    }
    if (strncmp(name, "peek", 4) == 0) {
        if (beat != B_HOME && beat != B_CORNER) return false;
        const int x = name[4] == ' ' ? atoi(name + 5) : lv_display_get_horizontal_resolution(NULL) * 3 / 4;
        engage();
        resume_screensaver = false;
        tap_home(x);
        return beat == B_PEEK;
    }
    if (strcmp(name, "lurk") == 0) {
        if (beat != B_HOME) return false;
        return mascot_lurk();
    }
    if (strcmp(name, "liveliness") == 0) {
        Serial.printf("director: liveliness %d\n", clawd_liveliness());
        return true;
    }
    if (strcmp(name, "trace") == 0) {
        trace = !trace;
        Serial.printf("director: trace %s\n", trace ? "on" : "off");
        return true;
    }
    const bool show = strcmp(name, "show") == 0;
    if (!show && strcmp(name, "screensaver") != 0) return false;
    if (beat != B_HOME) return false;
    go(B_HOME);  // Phase 2: B_SHOW / B_SCREENSAVER
    return true;
}
