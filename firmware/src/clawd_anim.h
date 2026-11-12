#pragma once
#include <stdint.h>

// Read-only view of the Clawd animations (splash_animations.h).
//
// splash_animations.h defines large `static const` frame data, so it may
// only be included by ONE translation unit (clawd_anim.cpp); every other
// module (mascot, director) reaches the art through this view.
//
// Fork adaptation: 20x20 pixel-art grids, 10-colour palettes, no ox/oy crop.
// All upstream ClawdAnim fields are present so later tasks (mascot, director)
// that copy from upstream compile against this header without change.

#define CLAWD_PALETTE_SIZE 10
#define CLAWD_ART_W        20
#define CLAWD_ART_H        20

struct ClawdAnim {
    const char*     name;
    uint8_t         w, h;              // frame size, cells (always 20x20 in fork)
    uint8_t         ox, oy;            // crop origin — always 0,0 in fork
    uint16_t        frame_count;
    uint16_t        loop_start, loop_end;
    const char*     category;          // animation category string from source data
    const uint16_t* palette;           // RGB565, CLAWD_PALETTE_SIZE entries
    const uint8_t*  frames;            // frame_count x (w*h) palette indices, flat
    const uint16_t* holds;             // ms per frame
};

int              clawd_anim_count(void);
const ClawdAnim* clawd_anim_at(int index);          // NULL when out of range
const ClawdAnim* clawd_anim(const char* name);      // NULL when not found
int              clawd_anim_index(const char* name); // -1 when not found
// No-op in Phase 1 (no live theme palette recoloring in the fork).
void             clawd_anim_recolor(void);

static inline const uint8_t* clawd_frame(const ClawdAnim* a, uint16_t frame) {
    return &a->frames[(uint32_t)frame * a->w * a->h];
}

// ─── Walk gait ───────────────────────────────────────────────────────────────
// Kept for API compatibility with upstream; not used in the fork's splash.
enum ClawdGait { GAIT_NONE, GAIT_WALK, GAIT_CRAB };

static inline int clawd_gait_cells(ClawdGait g, uint16_t frame, bool from_loop) {
    if (g == GAIT_CRAB) {
        if (frame == 1) return from_loop ? 1 : 0;
        return (frame == 4 || frame == 5 || frame == 8) ? 1 : 0;
    }
    if (g == GAIT_WALK) {
        if (frame < 2 || frame > 6) return 0;
        if (frame == 2 && !from_loop) return 0;
        return (frame == 6) ? 2 : 1;
    }
    return 0;
}

// Frame hold scaled by a speed in percent (100 = authored timing, 200 = 2x).
static inline uint32_t clawd_hold(const ClawdAnim* a, uint16_t frame, uint16_t speed_pct) {
    if (speed_pct == 0) speed_pct = 100;
    return (uint32_t)a->holds[frame] * 100u / speed_pct;
}
