#pragma once
#include "clawd_policy.h"
#include <stdbool.h>

// Per-feature on/off prefs, persisted to NVS. Each defaults to true (follow
// liveliness). feature_pref_apply() ANDs them into an already-scaled tuning
// struct, so liveliness still wins when it disables a feature at level 1.
void feature_pref_load(void);
void feature_pref_apply(ClawdTuning* t);

bool feature_pref_shows(void);
bool feature_pref_screensaver(void);
bool feature_pref_idle_acts(void);
bool feature_pref_reactions(void);

void feature_pref_set_shows(bool on);
void feature_pref_set_screensaver(bool on);
void feature_pref_set_idle_acts(bool on);
void feature_pref_set_reactions(bool on);
