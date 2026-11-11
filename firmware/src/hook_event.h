#pragma once
#include <stdint.h>
#include <stdbool.h>

// A Claude Code hook event pushed to the device via POST /event.
// Field names match the upstream BLE fast-clock payload (a, e/es, n) so
// signals.cpp can be ported verbatim later.
struct ClawdHookEvent {
    char     type;      // 'p' prompt, 's' stop, 'b' back after gap, '\0' none
    uint32_t seq;       // event sequence number (es)
    int      sessions;  // active Claude Code session count (n), -1 = unknown
    int      activity;  // 0–3 activity level (a), -1 = unknown
    bool     pending;   // true when a new event has arrived but not yet consumed
};

// Written by settings_server on the main Arduino task; consumed by loop().
// No cross-task race: both sides run on Core 1.
extern ClawdHookEvent g_hook_event;
