#include "pace_pref.h"
#include "ui.h"
#include <Preferences.h>

static bool s_on = false;

void pace_pref_load(void) {
    Preferences p;
    p.begin("clawdmeter", true);
    s_on = p.getBool("pace_on", false);
    p.end();
}

bool pace_pref_get(void) { return s_on; }

void pace_pref_set(bool on) {
    s_on = on;
    Preferences p;
    p.begin("clawdmeter", false);
    p.putBool("pace_on", s_on);
    p.end();
    ui_set_pace_marker(s_on);
}
