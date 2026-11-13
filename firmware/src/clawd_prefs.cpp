#include "clawd_prefs.h"
#include "feature_pref.h"
#include <Preferences.h>
#include <Arduino.h>

// NVS namespace shared with provisioning.cpp.
static const char* const NVS_NS = "clawdmeter";

static void save_i32(const char* key, int32_t v) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putInt(key, v);
    prefs.end();
}

void clawd_prefs_init(void) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    const int lively = prefs.getInt("liveliness", CLAWD_LIVELINESS_DEFAULT);
    const int attn   = prefs.getInt("attn_anim",  CLAWD_ATTN_ANIM_DEFAULT_S);
    prefs.end();
    clawd_set_liveliness(clawd_liveliness_clamp(lively));
    clawd_set_attn_anim(clawd_attn_anim_clamp(attn));
    feature_pref_load();
    feature_pref_apply(&clawd_tuning());
    Serial.printf("clawd_prefs: liveliness %d, attn_anim %d s\n",
                  clawd_liveliness(), clawd_attn_anim());
}

int liveliness_get(void) { return clawd_liveliness(); }

void liveliness_set(int v) {
    v = clawd_liveliness_clamp(v);
    if (v == clawd_liveliness()) return;
    save_i32("liveliness", v);
    clawd_set_liveliness(v);
    Serial.printf("clawd_prefs: liveliness %d\n", v);
}

void attn_anim_set(int v) {
    v = clawd_attn_anim_clamp(v);
    if (v == clawd_attn_anim()) return;
    save_i32("attn_anim", v);
    clawd_set_attn_anim(v);
    Serial.printf("clawd_prefs: attn_anim %d s\n", v);
}
