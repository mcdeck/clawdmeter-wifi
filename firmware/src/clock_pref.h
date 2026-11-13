#pragma once
#include <Arduino.h>

// Clock display preferences. NVS namespace "clawdmeter".
// Keys: clk_on (bool, default true), clk_fmt (uint8: 24 or 12, default 24),
//       clk_tz (string, POSIX tz rule, default "UTC0").

void clock_pref_load(void);         // read NVS → static vars (call before ui_init)
void clock_pref_apply_tz(void);     // setenv("TZ", ...) + tzset(); call after NTP sync
bool clock_pref_on(void);
int  clock_pref_fmt(void);          // returns 24 or 12
const char* clock_pref_tz(void);        // returns POSIX rule string
const char* clock_pref_tz_name(void);   // returns IANA name (e.g. "Europe/Berlin")
void clock_pref_set(bool on, int fmt, const char* posix_tz, const char* tz_name);  // persist + apply
