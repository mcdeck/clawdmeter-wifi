#pragma once
#include <Arduino.h>

// Pace marker preference. NVS namespace "clawdmeter".
// Key: pace_on (bool, default false).

void pace_pref_load(void);      // read NVS → static var (call before ui_init)
bool pace_pref_get(void);
void pace_pref_set(bool on);    // persist + call ui_set_pace_marker(on)
