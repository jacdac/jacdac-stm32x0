#include "jdprofile.h"

// Bring-up firmware for the PY32F030K28U6 dev board. Not a product: the unique ID
// keeps it from being mistaken for a shipping module in the Jacdac dashboard.
FIRMWARE_IDENTIFIER(0x3e11ab08, "PY32F030 dev board slider (bring-up)");

void app_init_services() {
    potentiometer_init(NO_PIN, ADC_PIN, POT_EN);
}
