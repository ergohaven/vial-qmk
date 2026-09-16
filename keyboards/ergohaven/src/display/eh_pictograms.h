#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EH_PICTOGRAM_WIDTH 35
#define EH_PICTOGRAM_HEIGHT 35
#define EH_PICTOGRAM_BYTES ((EH_PICTOGRAM_WIDTH * EH_PICTOGRAM_HEIGHT + 7) / 8)
#define EH_PICTOGRAM_MACRO_SLOTS 256
#define EH_PICTOGRAM_TAP_DANCE_SLOTS 256

#ifdef EH_PICTOGRAM_ENABLE
void eh_pictograms_init(void);
uint8_t eh_pictogram_stored_width(void);
const uint8_t *eh_pictogram_for_keycode(uint16_t keycode);
uint32_t eh_pictogram_color_for_keycode(uint16_t keycode);
uint32_t eh_pictograms_generation(void);
bool eh_pictograms_process_hid(uint8_t *data, uint8_t length);
#else
static inline void eh_pictograms_init(void) {}
static inline uint8_t eh_pictogram_stored_width(void) { return EH_PICTOGRAM_WIDTH; }
static inline const uint8_t *eh_pictogram_for_keycode(uint16_t keycode) {
    (void)keycode;
    return (const uint8_t *)0;
}
static inline uint32_t eh_pictogram_color_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}
static inline uint32_t eh_pictograms_generation(void) {
    return 0;
}
#endif
