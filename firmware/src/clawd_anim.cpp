// The only translation unit that includes the generated animation data.
#include "clawd_anim.h"
#include "splash_animations.h"
#include <string.h>

static_assert(SPLASH_PALETTE_SIZE == CLAWD_PALETTE_SIZE, "palette size drifted");

static ClawdAnim table[SPLASH_ANIM_COUNT > 0 ? SPLASH_ANIM_COUNT : 1];
static bool built = false;

// Fork: no live-palette recoloring. Palette pointers point straight into the
// const data in flash; clawd_anim_recolor() is intentionally empty.
static void build(void) {
    if (built) return;
    for (int i = 0; i < SPLASH_ANIM_COUNT; i++) {
        const splash_anim_def_t& s = splash_anims[i];
        ClawdAnim& a = table[i];
        a.name        = s.name;
        a.category    = s.category;
        a.w           = 20;
        a.h           = 20;
        a.ox          = 0;
        a.oy          = 0;
        a.frame_count = s.frame_count;
        a.loop_start  = 0;
        a.loop_end    = (s.frame_count > 0) ? (uint16_t)(s.frame_count - 1) : 0;
        a.palette     = s.palette;                    // raw const pointer, no recolor
        a.frames      = &splash_anims[i].frames[0][0]; // 2D → flat cast
        a.holds       = s.holds;
    }
    built = true;
}

void clawd_anim_recolor(void) {
    // No-op in Phase 1: the fork has no live theme palette for the art.
}

int clawd_anim_count(void) { return SPLASH_ANIM_COUNT; }

const ClawdAnim* clawd_anim_at(int index) {
    if (index < 0 || index >= SPLASH_ANIM_COUNT) return NULL;
    build();
    return &table[index];
}

int clawd_anim_index(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < SPLASH_ANIM_COUNT; i++)
        if (strcmp(splash_anims[i].name, name) == 0) return i;
    return -1;
}

const ClawdAnim* clawd_anim(const char* name) {
    return clawd_anim_at(clawd_anim_index(name));
}
