#include "pace.h"

float pace_elapsed_pct(int reset_mins, int window_mins) {
    if (window_mins <= 0) return 0.0f;
    float elapsed = (float)(window_mins - reset_mins) / (float)window_mins * 100.0f;
    if (elapsed < 0.0f) return 0.0f;
    if (elapsed > 100.0f) return 100.0f;
    return elapsed;
}

int pace_level(float bar_pct, float elapsed_pct) {
    if (bar_pct >= 80.0f) return 2;
    if (bar_pct > elapsed_pct + 5.0f) return 1;
    return 0;
}
