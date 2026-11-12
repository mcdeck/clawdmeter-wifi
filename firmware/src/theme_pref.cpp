#include "theme_pref.h"
#include "theme_palette.h"
#include "theme_presets.h"
#include "ui.h"
#include <Preferences.h>

static char s_id[32] = "dark";

static bool apply_by_id(const char* id) {
    for (int i = 0; i < theme_preset_count; i++) {
        if (strcmp(theme_presets[i].id, id) == 0) {
            theme_palette_set(theme_presets[i].colors);
            return true;
        }
    }
    return false;
}

void theme_pref_load(void) {
    Preferences p;
    p.begin("clawdmeter", true);
    String id = p.getString("theme_id", "dark");
    p.end();
    strlcpy(s_id, id.c_str(), sizeof(s_id));
    // If the stored id is unknown, fall back to dark without touching NVS.
    if (!apply_by_id(s_id)) {
        strlcpy(s_id, "dark", sizeof(s_id));
        apply_by_id("dark");
    }
}

void theme_pref_set_by_name(const char* id) {
    if (!id || !id[0]) return;
    if (!apply_by_id(id)) return;  // unknown id — keep current palette, don't write NVS
    strlcpy(s_id, id, sizeof(s_id));
    Preferences p;
    p.begin("clawdmeter", false);
    p.putString("theme_id", s_id);
    p.end();
    ui_repaint_theme();
}

const char* theme_pref_current_id(void) { return s_id; }
