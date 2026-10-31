#include "jdprofile.h"

// SCRATCH compile-test target. Unique ID so check-fw-id.js is happy; never shipped.
FIRMWARE_IDENTIFIER(0x3e11ab03, "PY32F030 button compile test");

void app_init_services() {
    // PA0, active low (switch to GND, internal pull-up), no backlight.
    button_init(PA_0, 0, NO_PIN);
}
