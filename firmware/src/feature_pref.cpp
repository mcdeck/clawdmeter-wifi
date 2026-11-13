#include "feature_pref.h"
#include <Preferences.h>
#include <Arduino.h>

static bool g_shows     = true;
static bool g_screensaver = true;
static bool g_idle_acts = true;
static bool g_reactions = true;

void feature_pref_load(void) {
    Preferences prefs;
    prefs.begin("clawdmeter", true);
    g_shows       = prefs.getBool("feat_shows", true);
    g_screensaver = prefs.getBool("feat_scrn",  true);
    g_idle_acts   = prefs.getBool("feat_idle",  true);
    g_reactions   = prefs.getBool("feat_react", true);
    prefs.end();
    Serial.printf("feature_pref: shows=%d scrn=%d idle=%d react=%d\n",
                  g_shows, g_screensaver, g_idle_acts, g_reactions);
}

void feature_pref_apply(ClawdTuning* t) {
    t->shows       &= g_shows;
    t->screensaver &= g_screensaver;
    t->idle_acts   &= g_idle_acts;
    t->reactions   &= g_reactions;
}

bool feature_pref_shows(void)       { return g_shows; }
bool feature_pref_screensaver(void) { return g_screensaver; }
bool feature_pref_idle_acts(void)   { return g_idle_acts; }
bool feature_pref_reactions(void)   { return g_reactions; }

static void save_bool(const char* key, bool v) {
    Preferences prefs;
    prefs.begin("clawdmeter", false);
    prefs.putBool(key, v);
    prefs.end();
}

void feature_pref_set_shows(bool on) {
    g_shows = on;
    save_bool("feat_shows", on);
}

void feature_pref_set_screensaver(bool on) {
    g_screensaver = on;
    save_bool("feat_scrn", on);
}

void feature_pref_set_idle_acts(bool on) {
    g_idle_acts = on;
    save_bool("feat_idle", on);
}

void feature_pref_set_reactions(bool on) {
    g_reactions = on;
    save_bool("feat_react", on);
}
