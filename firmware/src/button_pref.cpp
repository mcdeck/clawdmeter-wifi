#include "button_pref.h"
#include <Preferences.h>

static int32_t s_slots[3] = { 1, 2, 0 };  // PRIMARY=Space, SECONDARY=Shift+Tab, PWR=None

static const char* slot_key(ButtonSlot slot) {
    switch (slot) {
    case BTN_SLOT_PRIMARY:   return "btn_k1";
    case BTN_SLOT_SECONDARY: return "btn_k2";
    case BTN_SLOT_PWR:       return "btn_k3";
    default:                 return "btn_k1";
    }
}

void button_pref_load(void) {
    Preferences p;
    p.begin("clawdmeter", true);
    s_slots[BTN_SLOT_PRIMARY]   = p.getInt("btn_k1", 1);
    s_slots[BTN_SLOT_SECONDARY] = p.getInt("btn_k2", 2);
    s_slots[BTN_SLOT_PWR]       = p.getInt("btn_k3", 0);
    p.end();
    // Sanitize: ignore invalid stored values
    for (int i = 0; i < 3; i++) {
        if (!button_action_valid(s_slots[i])) s_slots[i] = 0;
    }
}

int32_t button_pref_get(ButtonSlot slot) {
    if (slot < 0 || slot >= 3) return 0;
    return s_slots[slot];
}

void button_pref_set(ButtonSlot slot, int32_t v) {
    if (slot < 0 || slot >= 3) return;
    if (!button_action_valid(v)) return;
    s_slots[slot] = v;
    Preferences p;
    p.begin("clawdmeter", false);
    p.putInt(slot_key(slot), v);
    p.end();
}
