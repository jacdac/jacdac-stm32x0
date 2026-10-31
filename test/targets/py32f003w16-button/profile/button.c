#include "jdprofile.h"

// Button on PY32F003W16S6TU. Not a product: the unique ID and name
// keep it from being mistaken for a shipping module.
FIRMWARE_IDENTIFIER(0x3e11ab05, "PY32F003 SOP-16 button");

void app_init_services() {
    // PA0, active low (switch to GND, internal pull-up), no backlight.
    button_init(PA_0, 0, NO_PIN);
}
