#include "jdprofile.h"

// Bring-up firmware for the PY32F030K28U6 dev board. Not a product: the unique ID
// keeps it from being mistaken for a shipping module in the Jacdac dashboard.
FIRMWARE_IDENTIFIER(0x3e11ab04, "PY32F030 dev board button (bring-up)");

void app_init_services() {
    // Both onboard keys: active low (to GND, internal pull-up), no backlight.
    // Two button services, so the dashboard shows which key is which.
    button_init(KEY_A, 0, NO_PIN);
    button_init(KEY_B, 0, NO_PIN);
}
