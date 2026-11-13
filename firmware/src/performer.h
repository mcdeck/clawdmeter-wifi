#pragma once
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

// Full-screen PSRAM performer for the director's show and screensaver calm-dip
// beats. Allocates a planar RGB565A8 buffer in PSRAM at performer_init(); if
// PSRAM is absent, performer_available() returns false and all API calls are
// no-ops.

void performer_init(lv_obj_t* parent, int feet_y);
void performer_tick(void);
bool performer_available(void);

enum PerformerSide { PERFORMER_LEFT = -1, PERFORMER_RIGHT = +1 };

void performer_place_offstage(int side);
void performer_walk_home(uint16_t speed_pct);
void performer_walk_off(int side, uint16_t speed_pct);
// Walk off with speed ramping linearly from from_pct to to_pct over ramp_ms.
void performer_run_off(int side, uint16_t from_pct, uint16_t to_pct, uint32_t ramp_ms);
void performer_perform(int anim_index, uint32_t loop_ms);
void performer_perform_group(int group, uint32_t loop_ms, bool calm);
// Freeze on the current frame and go idle (scene interrupted).
void performer_release(void);
bool performer_busy(void);
bool performer_visible(void);
void performer_repaint(void); // no-op: fork has no live theme recoloring
