#include "eh_settings.h"
#include "eh_ruen.h"
#include "eh_pointing.h"
#include <eeconfig.h>
#include <debug.h>
#include <qmk_settings.h>

char layer_names[DYNAMIC_KEYMAP_LAYER_COUNT][LAYER_LABEL_SIZE];

__attribute__((weak)) const char *default_layer_label(uint8_t layer) {
    static const char *PROGMEM default_layer_labels[] = {
        "BASE", "LOWER", "RAISE", "ADJST", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVN", "TWLVE", "THRTN", "FRTN", "FIFTN",
    };
    return default_layer_labels[layer];
}

void kb_settings_layer_labels_reset(void) {
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i) {
        eeconfig_update_kb_datablock(default_layer_label(i), KB_SETTINGS_LAYER_LABELS_OFFSET + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
    }
    layer_name_updated = true;
}

void kb_settings_layer_labels_init(void) {
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i)
        eeconfig_read_kb_datablock(layer_names[i], KB_SETTINGS_LAYER_LABELS_OFFSET + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
}

void kb_settings_reset(void) {
    kb_settings_ruen_reset();
    kb_settings_layer_labels_reset();
    kb_settings_pointing_reset();
    kb_settings_init();
}

void eeconfig_init_kb(void) {
    kb_settings_reset();
    eeconfig_init_user();
}

void kb_settings_init(void) {
    kb_settings_ruen_init();
    kb_settings_layer_labels_init();
    kb_settings_pointing_init();
}

#define DECLARE_SETTING_NOTIFY(id, _get, _set, _notify) {.qsid = id, .get = _get, .set = _set, .notify = _notify}
#define DECLARE_SETTING(id, _get, _set) DECLARE_SETTING_NOTIFY(id, _get, _set, NULL)

static int ruen_toggle_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t mode = get_ruen_toggle_mode();
    if (maxsz < sizeof(mode)) return -1;
    memcpy(setting, &mode, sizeof(mode));
    return 0;
}

static int ruen_toggle_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t mode;
    if (maxsz < sizeof(mode)) return -1;
    memcpy(&mode, setting, sizeof(mode));
    set_ruen_toggle_mode(mode);
    return 0;
}

static int ruen_macos_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    bool mac = get_ruen_mac_layout();
    if (maxsz < sizeof(mac)) return -1;
    memcpy(setting, &mac, sizeof(mac));
    return 0;
}

static int ruen_macos_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    bool mac;
    if (maxsz < sizeof(mac)) return -1;
    memcpy(&mac, setting, sizeof(mac));
    set_ruen_mac_layout(mac);
    return 0;
}

// should match with json/settings_ruen.json
// and UNICODE_SELECTED_MODES in config.h
static int unicode_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t unicode_mode = get_unicode_input_mode();
    uint8_t mode         = 0;
    switch (unicode_mode) {
        case UNICODE_MODE_MACOS:
            mode = 0;
            break;
        case UNICODE_MODE_LINUX:
            mode = 1;
            break;
        case UNICODE_MODE_WINDOWS:
            mode = 2;
            break;
        case UNICODE_MODE_WINCOMPOSE:
            mode = 3;
            break;
        default:
            return -1;
    }
    if (maxsz < sizeof(mode)) return -1;
    memcpy(setting, &mode, sizeof(mode));
    return 0;
}

static int unicode_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t mode = 0;
    if (maxsz < sizeof(mode)) return -1;
    memcpy(&mode, setting, sizeof(mode));
    switch (mode) {
        case 0:
            set_unicode_input_mode(UNICODE_MODE_MACOS);
            break;
        case 1:
            set_unicode_input_mode(UNICODE_MODE_LINUX);
            break;
        case 2:
            set_unicode_input_mode(UNICODE_MODE_WINDOWS);
            break;
        case 3:
            set_unicode_input_mode(UNICODE_MODE_WINCOMPOSE);
            break;
        default:
            return -1;
    }
    return 0;
}

const char *layer_name(uint8_t layer) {
    if (layer >= 0 && layer <= 15)
        return layer_names[layer];
    else
        return "UNDEF";
}

static int layer_name_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    int layer = proto->qsid - 200;
    if (layer < 0 || layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return -1;
    strcpy(setting, layer_names[layer]);
    return 0;
}

bool layer_name_updated = false;

static int layer_name_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    int layer = proto->qsid - 200;
    if (layer < 0 || layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return -1;
    dprintf("layer_name_set %d %s\n", layer, (const char *)setting);
    snprintf(layer_names[layer], sizeof(layer_names[layer]), (const char *)setting);
    eeconfig_update_kb_datablock(layer_names[layer], KB_SETTINGS_LAYER_LABELS_OFFSET + LAYER_LABEL_SIZE * layer, LAYER_LABEL_SIZE);
    layer_name_updated = true;
    return 0;
}

static int pointing_cpi_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint16_t cpi = get_cpi();
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(setting, &cpi, sizeof(cpi));
    return 0;
}

static int pointing_cpi_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint16_t cpi;
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(&cpi, setting, sizeof(cpi));
    dprintf("pointing_cpi_set %d\n", cpi);
    set_cpi(cpi);
    return 0;
}

static int pointing_sens_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t sens;

    pointing_mode_t pointing_mode = proto->qsid - 120;
    switch (pointing_mode) {
        case POINTING_MODE_SNIPER:
            sens = get_sniper_sens();
            break;
        case POINTING_MODE_SCROLL:
            sens = get_scroll_sens();
            break;
        case POINTING_MODE_TEXT ... POINTING_MODE_USR3:
            sens = get_text_sens();
            break;
        default:
            return -1;
    }
    if (maxsz < sizeof(sens)) return -1;
    memcpy(setting, &sens, sizeof(sens));
    return 0;
}

static int pointing_sens_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t sens;
    if (maxsz < sizeof(sens)) return -1;
    memcpy(&sens, setting, sizeof(sens));

    pointing_mode_t pointing_mode = proto->qsid - 120;
    switch (pointing_mode) {
        case POINTING_MODE_SNIPER:
            set_sniper_sens(sens);
            break;
        case POINTING_MODE_SCROLL:
            set_scroll_sens(sens);
            break;
        case POINTING_MODE_TEXT ... POINTING_MODE_USR3:
            set_text_sens(sens);
            break;
        default:
            return -1;
    }
    return 0;
}

static int pointing_bits_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t bits = (get_sticky_mode() << 2) | (get_acceleration() << 1) | (get_invert_scroll());
    if (maxsz < sizeof(bits)) return -1;
    memcpy(setting, &bits, sizeof(bits));
    return 0;
}

static int pointing_bits_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t bits;
    if (maxsz < sizeof(bits)) return -1;
    memcpy(&bits, setting, sizeof(bits));
    dprintf("pointing_bits_set %d\n", bits);
    set_invert_scroll(bits & 0x01);
    set_acceleration(bits & 0x02);
    set_sticky_mode(bits & 0x04);
    return 0;
}

qmk_settings_proto_t kb_protos[KB_SETTINGS_NPROTOS] PROGMEM = {
    // clang-format off
    DECLARE_SETTING(100, ruen_toggle_get, ruen_toggle_set),
    DECLARE_SETTING(101, ruen_macos_get, ruen_macos_set),
    DECLARE_SETTING(102, unicode_get, unicode_set),
    DECLARE_SETTING(120, pointing_cpi_get, pointing_cpi_set),
    DECLARE_SETTING(121, pointing_sens_get, pointing_sens_set),
    DECLARE_SETTING(122, pointing_sens_get, pointing_sens_set),
    DECLARE_SETTING(123, pointing_sens_get, pointing_sens_set),
    DECLARE_SETTING(124, pointing_bits_get, pointing_bits_set),
    DECLARE_SETTING(200, layer_name_get, layer_name_set),
    DECLARE_SETTING(201, layer_name_get, layer_name_set),
    DECLARE_SETTING(202, layer_name_get, layer_name_set),
    DECLARE_SETTING(203, layer_name_get, layer_name_set),
    DECLARE_SETTING(204, layer_name_get, layer_name_set),
    DECLARE_SETTING(205, layer_name_get, layer_name_set),
    DECLARE_SETTING(206, layer_name_get, layer_name_set),
    DECLARE_SETTING(207, layer_name_get, layer_name_set),
    DECLARE_SETTING(208, layer_name_get, layer_name_set),
    DECLARE_SETTING(209, layer_name_get, layer_name_set),
    DECLARE_SETTING(210, layer_name_get, layer_name_set),
    DECLARE_SETTING(211, layer_name_get, layer_name_set),
    DECLARE_SETTING(212, layer_name_get, layer_name_set),
    DECLARE_SETTING(213, layer_name_get, layer_name_set),
    DECLARE_SETTING(214, layer_name_get, layer_name_set),
    DECLARE_SETTING(215, layer_name_get, layer_name_set),
    // clang-format on
};
