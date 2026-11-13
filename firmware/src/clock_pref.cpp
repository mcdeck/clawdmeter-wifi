#include "clock_pref.h"
#include "ui.h"
#include <Preferences.h>
#include <time.h>

static bool s_on  = true;
static int  s_fmt = 24;
static char s_tz[64]      = "UTC0";
static char s_tz_name[64] = "";

void clock_pref_load(void) {
    Preferences p;
    p.begin("clawdmeter", true);
    s_on  = p.getBool("clk_on", true);
    s_fmt = p.getUChar("clk_fmt", 24) == 12 ? 12 : 24;
    String tz = p.getString("clk_tz", "UTC0");
    strlcpy(s_tz, tz.c_str(), sizeof(s_tz));
    String tz_name = p.getString("clk_tz_name", "");
    strlcpy(s_tz_name, tz_name.c_str(), sizeof(s_tz_name));
    p.end();
}

void clock_pref_apply_tz(void) {
    setenv("TZ", s_tz, 1);
    tzset();
}

bool clock_pref_on(void)              { return s_on; }
int  clock_pref_fmt(void)             { return s_fmt; }
const char* clock_pref_tz(void)       { return s_tz; }
const char* clock_pref_tz_name(void)  { return s_tz_name; }

void clock_pref_set(bool on, int fmt, const char* posix_tz, const char* tz_name) {
    s_on  = on;
    s_fmt = (fmt == 12) ? 12 : 24;
    if (posix_tz && posix_tz[0]) strlcpy(s_tz, posix_tz, sizeof(s_tz));
    if (tz_name) strlcpy(s_tz_name, tz_name, sizeof(s_tz_name));
    Preferences p;
    p.begin("clawdmeter", false);
    p.putBool("clk_on",  s_on);
    p.putUChar("clk_fmt", (uint8_t)s_fmt);
    p.putString("clk_tz", s_tz);
    p.putString("clk_tz_name", s_tz_name);
    p.end();
    clock_pref_apply_tz();
    ui_clock_invalidate();
}
