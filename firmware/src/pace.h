#pragma once

// Returns the fraction of the reset window that has elapsed, 0–100.
// reset_mins: minutes remaining until the window resets.
// window_mins: total window length in minutes.
float pace_elapsed_pct(int reset_mins, int window_mins);

// Returns the pace level for bar fill coloring:
//   0 = at or under pace (green)
//   1 = ahead of pace (amber)
//   2 = at limit (red, bar_pct >= 80)
int pace_level(float bar_pct, float elapsed_pct);
