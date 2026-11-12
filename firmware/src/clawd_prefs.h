#pragma once

// Device preferences kept in NVS so they apply from boot, before the app
// connects. This fork keeps only the director-relevant fields:
//   liveliness   "liveliness"  int32  1 No-nonsense .. 5 All-nonsense
//   attn_anim    "attn_anim"   int32  seconds Clawd points for an alert; 0 = off
//
// pace_marker, button_action, and palette are not in Phase 1 of the fork.
//
// clawd_attn_anim() and clawd_tuning() are re-exported via clawd_policy.h:
//   int            clawd_attn_anim(void)   — 0/10/30/120, default 120
//   ClawdTuning&   clawd_tuning(void)      — effective tuning at current liveliness

#include "clawd_policy.h"

void clawd_prefs_init(void);   // load from NVS, apply defaults; call after board_init()
int  liveliness_get(void);     // 1–5, default 3
void liveliness_set(int v);    // clamp 1–5, save NVS key "liveliness", apply immediately
void attn_anim_set(int v);     // clamp, save NVS key "attn_anim", apply immediately
