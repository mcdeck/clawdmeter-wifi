#include "button_action.h"

bool button_action_valid(int32_t v) {
    return v >= 0 && v <= 6;
}

ButtonBinding button_action_decode(int32_t v) {
    ButtonBinding b = { BTN_KIND_NONE, 0, 0 };
    switch (v) {
        case 0:  break;                                        // None
        case 1:  b = { BTN_KIND_HID, 0x2C, 0x00 }; break;    // Space
        case 2:  b = { BTN_KIND_HID, 0x2B, 0x02 }; break;    // Shift+Tab
        case 3:  b = { BTN_KIND_HID, 0x2B, 0x00 }; break;    // Tab
        case 4:  b = { BTN_KIND_HID, 0x28, 0x00 }; break;    // Enter
        case 5:  b = { BTN_KIND_HID, 0x29, 0x00 }; break;    // Escape
        case 6:  b = { BTN_KIND_BRIGHTNESS, 0, 0 }; break;    // Brightness (no-op)
        default: break;
    }
    return b;
}
