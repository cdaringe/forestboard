#include <Arduino.h>
#include <PeripheralPins.h>

#if defined(ERGOBOARD_KEYBOARD_MODE) && defined(USB_OTG_FS)

// STM32duino's generic F411/F446 USB table also configures PA8 (SOF), PA9
// (VBUS), and PA10 (ID). This board uses device-mode USB with VBUS sensing and
// external SOF disabled, while PA8 and PA10 are keyboard columns. Override the
// framework's weak table so USB owns only the physical D-/D+ pins.
const PinMap PinMap_USB_OTG_FS[] = {
    {PA_11, USB_OTG_FS,
        STM_PIN_DATA(STM_MODE_AF_PP, LL_GPIO_PULL_UP, GPIO_AF10_OTG_FS)},
    {PA_12, USB_OTG_FS,
        STM_PIN_DATA(STM_MODE_AF_PP, LL_GPIO_PULL_UP, GPIO_AF10_OTG_FS)},
    {NC, NP, 0},
};

#endif
