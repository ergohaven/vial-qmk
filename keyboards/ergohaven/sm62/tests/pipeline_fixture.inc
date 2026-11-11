static key_override_t test_override = {.trigger=KC_A,.trigger_mods=2,.layers=~0u,.replacement=KC_B,.options=ko_options_default};
static uint8_t key_override_count(void) { return 1; }
static const key_override_t *key_override_get(uint8_t i) { (void)i; return &test_override; }
static tap_dance_action_t dance;
static uint8_t tap_dance_count(void) { return 1; }
static tap_dance_action_t *tap_dance_get(uint8_t i) { assert(i==0); return &dance; }
static const uint16_t chord[] = {KC_A, KC_C, COMBO_END};
static combo_t test_combo = COMBO(chord, KC_D);
static uint16_t combo_count(void) { return 1; }
static combo_t *combo_get(uint16_t i) { assert(i==0); return &test_combo; }
