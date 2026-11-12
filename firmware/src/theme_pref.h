#pragma once
#include <Arduino.h>

// Runtime theme selection preference. NVS namespace "clawdmeter".
// Key: theme_id (string, default "dark").

void        theme_pref_load(void);                      // read NVS + apply palette (call before ui_init)
void        theme_pref_set_by_name(const char* id);    // persist + apply + repaint
const char* theme_pref_current_id(void);               // returns stored id string
