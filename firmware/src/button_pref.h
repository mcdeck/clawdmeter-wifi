#pragma once
#include <Arduino.h>
#include "button_action.h"

// Button preset preferences. NVS namespace "clawdmeter".
// Keys: btn_k1 (int32, default 1=Space), btn_k2 (int32, default 2=Shift+Tab),
//       btn_k3 (int32, default 0=None).

void    button_pref_load(void);                              // read NVS (call before ui_init)
int32_t button_pref_get(ButtonSlot slot);
void    button_pref_set(ButtonSlot slot, int32_t v);        // validates then persists
