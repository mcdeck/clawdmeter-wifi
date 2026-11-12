#pragma once
#include <Arduino.h>

void provisioning_init(void);
void provisioning_handle_cmd(const char* cmd);

bool     provisioning_has_wifi(void);
bool     provisioning_has_token(void);
String   provisioning_get_ssid(void);
String   provisioning_get_pass(void);
String   provisioning_get_token(void);

// Write ssid + pass (and optionally token) in a single NVS transaction.
// Pass nullptr for token to leave the stored token unchanged.
void provisioning_save_wifi(const char* ssid, const char* pass, const char* token);

String provisioning_get_webpass(void);
String provisioning_get_event_token(void);
void   provisioning_save_webpass(const char* pass);
void   provisioning_save_event_token(const char* token);

// Pending Wi-Fi credential trial — 30s safety revert.
// save_pending writes to temp NVS slots; get_trial_creds returns them (or
// falls back to current) and starts the 30s timer; promote makes them
// permanent; revert discards them and restores the previous credentials.
void provisioning_save_pending_wifi(const char* ssid, const char* pass);
bool provisioning_has_pending_wifi(void);
void provisioning_get_trial_creds(String& ssid_out, String& pass_out);
void provisioning_promote_pending(void);
void provisioning_revert_pending(void);
bool provisioning_trial_timed_out(void);  // true when >30s since get_trial_creds() was called
bool provisioning_take_reverted(void);    // true once after a revert; clears flag
bool provisioning_no_revert(void);
void provisioning_set_no_revert(bool v);
