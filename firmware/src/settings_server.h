#pragma once

// Persistent settings web server on port 80, active while in station mode.
// Provides clawdmeter.local/settings and POST /event for Claude Code hooks.
//
// Lifecycle:
//   settings_server_init()  — register routes once at boot (call from setup())
//   settings_server_start() — begin WebServer + mDNS after WiFi connects
//   settings_server_stop()  — stop WebServer + mDNS on disconnect or before AP mode
//   settings_server_tick()  — handleClient(); call every loop() iteration

void settings_server_init(void);
void settings_server_start(void);
void settings_server_stop(void);
void settings_server_tick(void);
