#pragma once
#include <stdint.h>
#include <stdbool.h>

// Button slot indices.
enum ButtonSlot {
    BTN_SLOT_PRIMARY   = 0,
    BTN_SLOT_SECONDARY = 1,
    BTN_SLOT_PWR       = 2,
};

// ButtonBinding kind values.
#define BTN_KIND_NONE       0  // no action
#define BTN_KIND_HID        1  // send HID keyboard event (keycode + mods)
#define BTN_KIND_BRIGHTNESS 2  // brightness cycle (no-op in this fork)

// Decoded button binding.
struct ButtonBinding {
    int     kind;
    uint8_t keycode;
    uint8_t mods;
};

// Preset indices (0–6). Indices beyond 6 are invalid.
//   0 = None
//   1 = Space (HID 0x2C, mods 0x00) — voice PTT
//   2 = Shift+Tab (HID 0x2B, mods 0x02) — mode toggle
//   3 = Tab (HID 0x2B, mods 0x00)
//   4 = Enter (HID 0x28, mods 0x00)
//   5 = Escape (HID 0x29, mods 0x00)
//   6 = Brightness (BTN_KIND_BRIGHTNESS — no-op; not offered in web UI)

bool          button_action_valid(int32_t v);
ButtonBinding button_action_decode(int32_t v);
