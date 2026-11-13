#include "performer.h"
#include "clawd_anim.h"
#include "clawd_policy.h"
#include "splash_geometry.h"
#include "hal/board_caps.h"
#include "perf.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

static const char* const GROUPS[4][4] = {
    { "idle breathe",    "idle look around", "work think",     "dance sway"       },
    { "work coding",     "dance sway",       "idle look around","dance bounce"    },
    { "dance bounce",    "work coding",      "dance sway",     "idle look around" },
    { "dance bounce dj", "dance sway dj",    "dance djmix",    "work coding"      },
};
static const char* const CALM[2] = { "idle breathe", "work think" };
static const int GROUPS_SIZE   = 4;
static const int CALM_SIZE     = 2;

// ─── State ───────────────────────────────────────────────────────────────────
enum PState { P_OFF, P_STAND, P_WALK, P_SCENE };

static lv_obj_t*      img    = NULL;
static uint8_t*       buf    = NULL;
static lv_image_dsc_t dsc;
static const ClawdAnim* walk_anim = NULL;

static int W = 480, H = 480;
static int p_cell = 8;      // min(W,H)/60
static int slot_x = 0;      // home x (left edge of centred art)
static int p_feet_y = 0;    // feet anchor y
static int art_w = 0, art_h = 0; // in px

static PState           state       = P_OFF;
static const ClawdAnim* p_anim      = NULL;
static uint16_t         p_frame     = 0;
static uint32_t         frame_start = 0;
static bool             from_loop   = false;
static int              p_x         = 0;
static int              walk_target = 0;
static int              walk_dir    = +1;
static uint16_t         walk_speed  = 100;
static bool             walk_exit   = false;  // hide + P_OFF when walk reaches target

// run_off ramping
static bool     ramp_active  = false;
static uint16_t ramp_from    = 100;
static uint16_t ramp_to      = 100;
static uint32_t ramp_ms      = 0;
static uint32_t ramp_started = 0;

// scene
static uint32_t scene_loop_ms   = 0;
static uint32_t scene_loop_until = 0;
static bool     scene_looping   = false;

// no-repeat state
static const char* last_show_anim = NULL;
static const char* last_calm_anim = NULL;
static ClawdRng    p_rng;

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool available = false;

bool performer_available(void) { return available; }

static int offstage_left(void) {
    return offstage_left_x(W, H, false, p_feet_y, art_w);
}
static int offstage_right(void) {
    return offstage_right_x(W, H, false, p_feet_y);
}

static void render_frame(void) {
    if (!p_anim || !buf || !img) return;
    perf_begin(PERF_PERFORMER_WALK); // best effort; perf is a no-op stub
    const int w = p_anim->w * p_cell;
    const int h = p_anim->h * p_cell;
    uint16_t* color = (uint16_t*)buf;
    uint8_t*  alpha = buf + (size_t)w * h * 2;
    const uint8_t* src = clawd_frame(p_anim, p_frame);
    for (int gy = 0; gy < p_anim->h; gy++) {
        for (int gx = 0; gx < p_anim->w; gx++) {
            uint8_t  code = src[gy * p_anim->w + gx];
            uint16_t col  = (code && code < CLAWD_PALETTE_SIZE) ? p_anim->palette[code] : 0;
            uint8_t  al   = code ? 255 : 0;
            for (int dy = 0; dy < p_cell; dy++) {
                uint16_t* cp = &color[(gy * p_cell + dy) * w + gx * p_cell];
                uint8_t*  ap = &alpha[(gy * p_cell + dy) * w + gx * p_cell];
                for (int dx = 0; dx < p_cell; dx++) { cp[dx] = col; ap[dx] = al; }
            }
        }
    }
    dsc.header.w      = w;
    dsc.header.h      = h;
    dsc.header.cf     = LV_COLOR_FORMAT_RGB565A8;
    dsc.header.stride = w * 2;
    dsc.data          = buf;
    dsc.data_size     = (size_t)w * h * 3;
    lv_image_set_src(img, &dsc);
    lv_obj_set_pos(img, p_x, p_feet_y - h);
    lv_obj_invalidate(img);
    perf_end(PERF_PERFORMER_WALK);
}

static void show_obj(bool on) {
    if (!img) return;
    if (on) lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void performer_init(lv_obj_t* parent, int feet_y) {
    const BoardCaps& bc = board_caps();
    W = bc.width; H = bc.height;
    p_feet_y = feet_y;
    p_cell = (W < H ? W : H) / SPLASH_GRID;
    if (p_cell < 1) p_cell = 1;

    walk_anim = clawd_anim("walking");
    if (!walk_anim) return;

    art_w = walk_anim->w * p_cell;
    art_h = walk_anim->h * p_cell;
    slot_x = (W - art_w) / 2 - walk_anim->ox * p_cell;

    const size_t dim = (size_t)p_cell * SPLASH_GRID;
    buf = (uint8_t*)heap_caps_malloc(dim * dim * 3, MALLOC_CAP_SPIRAM);
    if (!buf) return;
    available = true;

    img = lv_image_create(parent);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);

    clawd_rng_seed(&p_rng, millis() ^ 0xC1A3);
    state = P_OFF;
    p_anim = walk_anim;
    p_frame = 0;
    p_x = offstage_left();
}

// ─── API ─────────────────────────────────────────────────────────────────────
void performer_place_offstage(int side) {
    if (!available) return;
    p_x = (side == PERFORMER_LEFT) ? offstage_left() : offstage_right();
    state = P_STAND;
    p_anim = walk_anim;
    p_frame = 0;
    from_loop = false;
    show_obj(true);
    render_frame();
}

static void start_walk(int target, uint16_t spd, bool exit_on_arrive = false) {
    walk_target = target;
    walk_dir = (target > p_x) ? +1 : -1;
    walk_speed = spd ? spd : 100;
    walk_exit = exit_on_arrive;
    ramp_active = false;
    p_anim = walk_anim;
    p_frame = walk_anim->loop_start;
    from_loop = false;
    frame_start = millis();
    state = P_WALK;
    perf_begin(PERF_PERFORMER_WALK);
    render_frame();
}

void performer_walk_home(uint16_t speed_pct) {
    if (!available) return;
    start_walk(slot_x, speed_pct);
}

void performer_walk_off(int side, uint16_t speed_pct) {
    if (!available) return;
    const int target = (side == PERFORMER_LEFT) ? offstage_left() : offstage_right();
    start_walk(target, speed_pct, true);
}

void performer_run_off(int side, uint16_t from_pct, uint16_t to_pct, uint32_t ramp_duration_ms) {
    if (!available) return;
    const int target = (side == PERFORMER_LEFT) ? offstage_left() : offstage_right();
    ramp_from    = from_pct;
    ramp_to      = to_pct;
    ramp_ms      = ramp_duration_ms;
    ramp_started = millis();
    ramp_active  = true;
    start_walk(target, from_pct, true);
}

void performer_perform(int anim_index, uint32_t loop_ms) {
    if (!available) return;
    const ClawdAnim* a = clawd_anim_at(anim_index);
    if (!a) return;
    p_anim = a;
    p_frame = 0;
    from_loop = false;
    frame_start = millis();
    scene_loop_ms    = loop_ms;
    scene_loop_until = loop_ms ? millis() + loop_ms : 0;
    scene_looping    = loop_ms > 0;
    state = P_SCENE;
    perf_begin(PERF_PERFORMER_SCENE);
    render_frame();
}

void performer_perform_group(int group, uint32_t loop_ms, bool calm) {
    if (!available) return;
    if (group < 0) group = 0;
    if (group > 3) group = 3;

    const char** pool = calm ? (const char**)CALM : (const char**)GROUPS[group];
    int pool_size = calm ? CALM_SIZE : GROUPS_SIZE;
    const char*& last = calm ? last_calm_anim : last_show_anim;

    int pick = clawd_pick_no_repeat(pool, pool_size, last, &p_rng);
    if (pick < 0) pick = 0;
    last = pool[pick];

    int idx = clawd_anim_index(last);
    if (idx < 0) {
        // fallback to first anim in pool
        idx = clawd_anim_index(pool[0]);
        last = pool[0];
    }
    performer_perform(idx, loop_ms);
}

void performer_release(void) {
    if (!available) return;
    // Freeze current frame, go idle
    p_frame = 0;
    from_loop = false;
    state = P_STAND;
    perf_end(PERF_PERFORMER_SCENE);
}

bool performer_busy(void) {
    return available && state != P_OFF && state != P_STAND;
}

bool performer_visible(void) {
    return available && state != P_OFF;
}

void performer_repaint(void) {
    // No-op: no live theme recoloring in fork.
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
void performer_tick(void) {
    if (!available || !img || state == P_OFF || state == P_STAND) return;
    if (!p_anim) return;

    const uint32_t now = millis();

    if (state == P_WALK) {
        // Apply speed ramp if active
        if (ramp_active) {
            uint32_t elapsed = now - ramp_started;
            if (elapsed >= ramp_ms) {
                walk_speed = ramp_to;
            } else {
                walk_speed = (uint16_t)(ramp_from + (int32_t)(ramp_to - ramp_from) * (int32_t)elapsed / (int32_t)ramp_ms);
            }
        }

        if ((int32_t)(now - frame_start) < (int32_t)clawd_hold(p_anim, p_frame, walk_speed)) return;
        frame_start = now;

        const bool was_loop = from_loop;
        uint16_t next = p_frame + 1;
        if (next > p_anim->loop_end) next = p_anim->loop_start;

        p_frame = next;
        from_loop = (p_frame >= p_anim->loop_start && p_frame <= p_anim->loop_end);

        if (from_loop) {
            const int step = clawd_gait_cells(GAIT_WALK, p_frame, was_loop) * p_cell;
            p_x += walk_dir * step;
            const bool reached = (walk_dir > 0) ? (p_x >= walk_target) : (p_x <= walk_target);
            if (reached) {
                p_x = walk_target;
                ramp_active = false;
                perf_end(PERF_PERFORMER_WALK);
                if (walk_exit) {
                    show_obj(false);
                    state = P_OFF;
                } else {
                    state = P_STAND;
                    render_frame();
                }
                return;
            }
        }
        render_frame();
        return;
    }

    if (state == P_SCENE) {
        if ((int32_t)(now - frame_start) < (int32_t)clawd_hold(p_anim, p_frame, 100)) return;
        frame_start = now;

        uint16_t next = p_frame + 1;

        if (scene_looping) {
            if (next > p_anim->loop_end) next = p_anim->loop_start;
            if ((int32_t)(now - scene_loop_until) >= 0) {
                // Loop time expired: play through the remaining frames as outro
                scene_looping = false;
                next = p_anim->loop_end + 1;
                if (next >= p_anim->frame_count) {
                    perf_end(PERF_PERFORMER_SCENE);
                    state = P_STAND;
                    return;
                }
            }
        }

        if (next >= p_anim->frame_count) {
            perf_end(PERF_PERFORMER_SCENE);
            state = P_STAND;
            return;
        }

        p_frame = next;
        render_frame();
    }
}
