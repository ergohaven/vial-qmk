#include QMK_KEYBOARD_H

#include <string.h>

#include "drivers/sensors/azoteq_iqs5xx.h"
#include "drivers/sensors/pmw3610.h"
#include "gpio.h"
#include "dynamic_keymap.h"
#include "eeconfig.h"
#include "quantum/split_common/transactions.h"
#include "quantum/split_common/split_util.h"
#include "src/eh_pointing.h"
#include "ergohaven_rgb.h"
#include "via.h"
#include "pointing_device_internal.h"

static_assert(sizeof(kb_settings_split_pointing_t) <= RPC_M2S_BUFFER_SIZE, "Split pointing settings exceed the RPC buffer");

#ifndef AZOTEQ_IQS5XX_ADDRESS
#define AZOTEQ_IQS5XX_ADDRESS (0x74 << 1)
#endif

#ifndef ARRAY_SIZE
#    define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef PHENOM_MODULE_PROBE_INTERVAL_MS
#    define PHENOM_MODULE_PROBE_INTERVAL_MS 250
#endif

#ifndef PHENOM_MODULE_PROBE_STREAK_REQUIRED
#    define PHENOM_MODULE_PROBE_STREAK_REQUIRED 2
#endif

typedef enum {
    PHENOM_MODULE_AUTO = 0,
    PHENOM_MODULE_NONE,
    PHENOM_MODULE_TRACKBALL,
    PHENOM_MODULE_TOUCHPAD,
} phenom_module_t;

typedef union {
    uint32_t raw;
    struct {
        bool    hide_left_encoder : 1;
        bool    hide_right_encoder : 1;
        uint8_t left_ball_orientation : 2;
        uint8_t right_ball_orientation : 2;
        uint8_t left_touch_orientation : 2;
        uint8_t right_touch_orientation : 2;
        uint8_t left_ball_dpi : 4;
        uint8_t right_ball_dpi : 4;
        uint8_t left_touch_dpi : 4;
        uint8_t right_touch_dpi : 4;
        uint8_t _reserved : 5;
        bool    schema_v2 : 1;
    } __attribute__((packed));
} phenom_via_config_t;

static const uint16_t phenom_trackball_cpi_table[] = {200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000, 3200};
static const uint16_t phenom_touchpad_cpi_table[]  = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};

#define PHENOM_LAYOUT_SCHEMA_V2 (1u << 31)
#define PHENOM_LEFT_BALL_AXIS_SHIFT 2
#define PHENOM_RIGHT_BALL_AXIS_SHIFT 4
#define PHENOM_LEFT_TOUCH_AXIS_SHIFT 6
#define PHENOM_RIGHT_TOUCH_AXIS_SHIFT 8
#define PHENOM_LEFT_BALL_DPI_SHIFT 10
#define PHENOM_RIGHT_BALL_DPI_SHIFT 14
#define PHENOM_LEFT_TOUCH_DPI_SHIFT 18
#define PHENOM_RIGHT_TOUCH_DPI_SHIFT 22

static uint32_t phenom_upgrade_raw(uint32_t raw) {
    if (raw & PHENOM_LAYOUT_SCHEMA_V2) {
        return raw;
    }

    if (raw == 0x03324480u || raw == 0x03324540u) {
        return VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT;
    }

    uint32_t upgraded = raw & 0x3u;
    uint32_t left_orientation  = (raw >> 6) & 0x3u;
    uint32_t right_orientation = (raw >> 8) & 0x3u;
    uint32_t trackball_dpi     = (raw >> 10) & 0xFu;
    uint32_t touchpad_dpi      = (raw >> 14) & 0x7u;

    upgraded |= left_orientation << PHENOM_LEFT_BALL_AXIS_SHIFT;
    upgraded |= right_orientation << PHENOM_RIGHT_BALL_AXIS_SHIFT;
    upgraded |= left_orientation << PHENOM_LEFT_TOUCH_AXIS_SHIFT;
    upgraded |= right_orientation << PHENOM_RIGHT_TOUCH_AXIS_SHIFT;
    upgraded |= trackball_dpi << PHENOM_LEFT_BALL_DPI_SHIFT;
    upgraded |= trackball_dpi << PHENOM_RIGHT_BALL_DPI_SHIFT;
    upgraded |= touchpad_dpi << PHENOM_LEFT_TOUCH_DPI_SHIFT;
    upgraded |= touchpad_dpi << PHENOM_RIGHT_TOUCH_DPI_SHIFT;
    upgraded |= PHENOM_LAYOUT_SCHEMA_V2;
    return upgraded;
}

static uint8_t phenom_clamp_index(uint8_t index, uint8_t max) {
    return index < max ? index : (max - 1);
}

// phenom-axis-indep-v0.0.5, read side-specific fields from raw, not C bitfields.
static phenom_via_config_t          phenom_via_config           = {.raw = VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT};
static uint32_t                   phenom_synced_raw           = VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT;
static uint32_t                   phenom_applied_raw          = UINT32_MAX;
static kb_settings_split_pointing_t phenom_synced_devices;
static bool                         phenom_synced_devices_valid = false;
static kb_settings_split_pointing_t phenom_applied_devices;
static bool                       phenom_applied_devices_valid = false;
static kb_settings_led_colors_t   phenom_synced_led_colors;

static split_pointing_device_id_t phenom_get_local_device_id(phenom_module_t module) {
    bool left = is_keyboard_left();

    switch (module) {
        case PHENOM_MODULE_TRACKBALL:
            return left ? SPLIT_POINTING_DEVICE_LEFT_BALL : SPLIT_POINTING_DEVICE_RIGHT_BALL;
        case PHENOM_MODULE_TOUCHPAD:
        default:
            return left ? SPLIT_POINTING_DEVICE_LEFT_TOUCH : SPLIT_POINTING_DEVICE_RIGHT_TOUCH;
    }
}

static bool          phenom_touchpad_available    = false;
static bool          phenom_touchpad_initialized  = false;
static bool          phenom_trackball_available   = false;
static bool          phenom_trackball_initialized = false;
static phenom_module_t phenom_detected_module       = PHENOM_MODULE_NONE;
static uint32_t      phenom_last_probe_time       = 0;
static phenom_module_t phenom_last_probed_module    = PHENOM_MODULE_NONE;
static uint8_t       phenom_probe_streak          = 0;

static orientation_t phenom_get_local_orientation(void) {
    return get_split_pointing_device_orientation(phenom_get_local_device_id(phenom_detected_module));
}

static phenom_module_t phenom_get_active_module(void) {
    return phenom_detected_module;
}

static report_mouse_t phenom_rotate_report(report_mouse_t report, orientation_t orientation) {
    int8_t tmp;
    switch (orientation) {
        case ROT_0:
            break;
        case ROT_90:
            tmp      = report.x;
            report.x = -report.y;
            report.y = tmp;
            break;
        case ROT_180:
            report.x = -report.x;
            report.y = -report.y;
            break;
        case ROT_270:
            tmp      = report.x;
            report.x = report.y;
            report.y = -tmp;
            break;
    }
    return report;
}

static report_mouse_t phenom_rotate_trackball_report(report_mouse_t report, orientation_t orientation) {
    report = phenom_rotate_report(report, orientation);
    report.y = -report.y;
    return report;
}

static phenom_module_t phenom_pick_module(bool touchpad_available, bool trackball_available) {
    if (phenom_detected_module == PHENOM_MODULE_TOUCHPAD && touchpad_available) {
        return PHENOM_MODULE_TOUCHPAD;
    }
    if (phenom_detected_module == PHENOM_MODULE_TRACKBALL && trackball_available) {
        return PHENOM_MODULE_TRACKBALL;
    }
    if (touchpad_available) {
        return PHENOM_MODULE_TOUCHPAD;
    }
    if (trackball_available) {
        return PHENOM_MODULE_TRACKBALL;
    }
    return PHENOM_MODULE_NONE;
}

static bool phenom_probe_touchpad_present(void) {
    pd_dprintf("phenom_probe_touchpad_present: start (I2C address=0x%02X)\n", AZOTEQ_IQS5XX_ADDRESS);
    i2c_init();
    i2c_ping_address(AZOTEQ_IQS5XX_ADDRESS, 1);
    wait_ms(1);
    uint16_t product = azoteq_iqs5xx_get_product();
    bool ok = product != AZOTEQ_IQS5XX_UNKNOWN;
    pd_dprintf("phenom_probe_touchpad_present: product=%u ok=%d\n", product, ok);
    return ok;
}

static bool phenom_probe_trackball_present(void) {
    gpio_set_pin_output(sclk_pin);
    gpio_set_pin_output(sdio_pin);
    if (cs_pin != NO_PIN) {
        gpio_set_pin_output(cs_pin);
        gpio_write_pin_high(cs_pin);
    }
    wait_us(10);

    uint8_t product_id = pmw3610_read(0, PMW3610_REG_PRODUCT_ID);
    bool    ok         = product_id == PMW3610_PRODUCT_ID;
    pd_dprintf("phenom_probe_trackball_present: product_id=0x%02X ok=%d\n", product_id, ok);
    return ok;
}

static bool phenom_detect_touchpad(void) {
    pd_dprintf("phenom_detect_touchpad: start\n");
    azoteq_iqs5xx_init();
    wait_ms(50);
    uint16_t product = azoteq_iqs5xx_get_product();
    bool     ok      = product != AZOTEQ_IQS5XX_UNKNOWN;
    pd_dprintf("phenom_detect_touchpad: product=%u ok=%d\n", product, ok);
    return ok;
}

static bool phenom_detect_trackball(void) {
    bool ok = pmw3610_init(0);
    if (ok) {
        pd_dprintf("phenom right trackball detected\n");
    } else {
        pd_dprintf("phenom right trackball not detected\n");
    }
    return ok;
}

static void phenom_commit_module_probe(bool touchpad_available, bool trackball_available, phenom_module_t detected_module) {
    bool module_changed = phenom_detected_module != detected_module;
    bool state_changed  = phenom_touchpad_available != touchpad_available || phenom_trackball_available != trackball_available;

    if (!module_changed && !state_changed) {
        return;
    }

    pd_dprintf("phenom_commit_module_probe: module %u -> %u, touch=%d, trackball=%d\n", phenom_detected_module, detected_module, touchpad_available, trackball_available);

    phenom_touchpad_available  = touchpad_available;
    phenom_trackball_available = trackball_available;
    if (!phenom_touchpad_available) {
        phenom_touchpad_initialized = false;
    }
    if (!phenom_trackball_available) {
        phenom_trackball_initialized = false;
    }
    phenom_detected_module        = detected_module;
    phenom_applied_raw            = UINT32_MAX;
    phenom_applied_devices_valid  = false;
}

static void phenom_detect_modules(bool force) {
    if (!force && timer_elapsed32(phenom_last_probe_time) < PHENOM_MODULE_PROBE_INTERVAL_MS) {
        return;
    }

    phenom_last_probe_time = timer_read32();

    bool          touchpad_available  = phenom_probe_touchpad_present();
    bool          trackball_available = phenom_probe_trackball_present();
    phenom_module_t detected_module     = phenom_pick_module(touchpad_available, trackball_available);

    pd_dprintf("phenom_detect_modules: force=%d probe module=%u touch=%d trackball=%d\n", force, detected_module, touchpad_available, trackball_available);

    if (force || detected_module == phenom_detected_module) {
        phenom_last_probed_module = detected_module;
        phenom_probe_streak       = 0;
        phenom_commit_module_probe(touchpad_available, trackball_available, detected_module);
        return;
    }

    if (detected_module != phenom_last_probed_module) {
        phenom_last_probed_module = detected_module;
        phenom_probe_streak       = 1;
        pd_dprintf("phenom_detect_modules: new candidate module=%u\n", detected_module);
        return;
    }

    if (phenom_probe_streak < UINT8_MAX) {
        phenom_probe_streak++;
    }
    pd_dprintf("phenom_detect_modules: candidate module=%u streak=%u\n", detected_module, phenom_probe_streak);

    if (phenom_probe_streak >= PHENOM_MODULE_PROBE_STREAK_REQUIRED) {
        phenom_commit_module_probe(touchpad_available, trackball_available, detected_module);
        phenom_probe_streak = 0;
    }
}

static void phenom_ensure_selected_module_ready(void) {
    phenom_detect_modules(false);

    switch (phenom_get_active_module()) {
        case PHENOM_MODULE_TRACKBALL:
            if (phenom_trackball_available && !phenom_trackball_initialized) {
                phenom_trackball_available   = phenom_detect_trackball();
                phenom_trackball_initialized = phenom_trackball_available;
                if (!phenom_trackball_available) {
                    phenom_detect_modules(true);
                }
            }
            break;
        case PHENOM_MODULE_TOUCHPAD:
            if (phenom_touchpad_available && !phenom_touchpad_initialized) {
                phenom_touchpad_available   = phenom_detect_touchpad();
                phenom_touchpad_initialized = phenom_touchpad_available;
                if (!phenom_touchpad_available) {
                    phenom_detect_modules(true);
                }
            }
            break;
        default:
            break;
    }
}

static void phenom_apply_device_config(void) {
    kb_settings_split_pointing_t devices = get_split_pointing_settings();
    if (phenom_applied_raw == phenom_via_config.raw && phenom_applied_devices_valid && memcmp(&phenom_applied_devices, &devices, sizeof(devices)) == 0) {
        return;
    }

    dprintf("phenom_apply_device_config raw=0x%08lX\n", (unsigned long)phenom_via_config.raw);
    dprintf("  left_ball_axis=%u right_ball_axis=%u\n", get_split_pointing_device_orientation(SPLIT_POINTING_DEVICE_LEFT_BALL), get_split_pointing_device_orientation(SPLIT_POINTING_DEVICE_RIGHT_BALL));
    dprintf("  left_touch_axis=%u right_touch_axis=%u\n", get_split_pointing_device_orientation(SPLIT_POINTING_DEVICE_LEFT_TOUCH), get_split_pointing_device_orientation(SPLIT_POINTING_DEVICE_RIGHT_TOUCH));
    dprintf("  left_ball_dpi=%u right_ball_dpi=%u\n", get_split_pointing_device_dpi_index(SPLIT_POINTING_DEVICE_LEFT_BALL), get_split_pointing_device_dpi_index(SPLIT_POINTING_DEVICE_RIGHT_BALL));
    dprintf("  left_touch_dpi=%u right_touch_dpi=%u\n", get_split_pointing_device_dpi_index(SPLIT_POINTING_DEVICE_LEFT_TOUCH), get_split_pointing_device_dpi_index(SPLIT_POINTING_DEVICE_RIGHT_TOUCH));

    if (phenom_trackball_initialized) {
        uint8_t idx = phenom_clamp_index(get_split_pointing_device_dpi_index(phenom_get_local_device_id(PHENOM_MODULE_TRACKBALL)), ARRAY_SIZE(phenom_trackball_cpi_table));
        uint16_t cpi = phenom_trackball_cpi_table[idx];
        dprintf("  trackball CPI=%u (idx=%u)\n", cpi, idx);
        pmw3610_set_cpi(0, cpi);
    }
    if (phenom_touchpad_initialized) {
        uint8_t idx = phenom_clamp_index(get_split_pointing_device_dpi_index(phenom_get_local_device_id(PHENOM_MODULE_TOUCHPAD)), ARRAY_SIZE(phenom_touchpad_cpi_table));
        uint16_t cpi = phenom_touchpad_cpi_table[idx];
        dprintf("  touchpad CPI=%u (idx=%u)\n", cpi, idx);
        split_pointing_side_t side = is_keyboard_left() ? SPLIT_POINTING_SIDE_LEFT : SPLIT_POINTING_SIDE_RIGHT;
        azoteq_iqs5xx_set_gesture_config(get_split_pointing_side_touch_gestures(side), false);
        azoteq_iqs5xx_set_cpi(cpi);
    }

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    set_auto_mouse_layer(get_split_pointing_auto_mouse_layer());
    set_auto_mouse_enable(get_split_pointing_auto_mouse_enable());
    if (!get_split_pointing_auto_mouse_enable()) {
        auto_mouse_layer_off();
    }
#endif

    phenom_applied_raw           = phenom_via_config.raw;
    phenom_applied_devices       = devices;
    phenom_applied_devices_valid = true;
}

uint32_t via_set_layout_options_normalize_kb(uint32_t value) {
    if ((phenom_via_config.raw & PHENOM_LAYOUT_SCHEMA_V2) && !(value & PHENOM_LAYOUT_SCHEMA_V2)) {
        value = (phenom_via_config.raw & ~0x3u) | (value & 0x3u);
    }
    return value;
}

void via_set_layout_options_kb(uint32_t value) {
    value = phenom_upgrade_raw(value);
    dprintf("via_set_layout_options_kb raw=0x%08lX\n", (unsigned long)value);
    dprintf("  left_ball_axis=%lu, right_ball_axis=%lu\n", (unsigned long)((value >> PHENOM_LEFT_BALL_AXIS_SHIFT) & 3), (unsigned long)((value >> PHENOM_RIGHT_BALL_AXIS_SHIFT) & 3));
    dprintf("  left_touch_axis=%lu, right_touch_axis=%lu\n", (unsigned long)((value >> PHENOM_LEFT_TOUCH_AXIS_SHIFT) & 3), (unsigned long)((value >> PHENOM_RIGHT_TOUCH_AXIS_SHIFT) & 3));
    dprintf("  left_ball_dpi=%lu, right_ball_dpi=%lu\n", (unsigned long)((value >> PHENOM_LEFT_BALL_DPI_SHIFT) & 15), (unsigned long)((value >> PHENOM_RIGHT_BALL_DPI_SHIFT) & 15));
    dprintf("  left_touch_dpi=%lu, right_touch_dpi=%lu\n", (unsigned long)((value >> PHENOM_LEFT_TOUCH_DPI_SHIFT) & 15), (unsigned long)((value >> PHENOM_RIGHT_TOUCH_DPI_SHIFT) & 15));
    phenom_via_config.raw = value;
    phenom_synced_raw     = value;
    phenom_apply_device_config();
}

static void phenom_sync_config_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(uint32_t) && in_data != NULL) {
        uint32_t value = 0;
        memcpy(&value, in_data, sizeof(value));
        value = phenom_upgrade_raw(value);
        if (via_get_layout_options() != value) {
            via_set_layout_options(value);
        } else {
            via_set_layout_options_kb(value);
        }
    }
}

static void phenom_sync_split_pointing_settings_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(kb_settings_split_pointing_t) && in_data != NULL) {
        kb_settings_split_pointing_t value;
        memcpy(&value, in_data, sizeof(value));
        set_split_pointing_settings(value);
        phenom_applied_devices_valid = false;
    }
}

static void phenom_sync_led_colors_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(kb_settings_led_colors_t) && in_data != NULL) {
        kb_settings_led_colors_t value;
        memcpy(&value, in_data, sizeof(value));
        set_settings_led_colors(value);
    }
}

void keyboard_post_init_user(void) {
#ifdef CONSOLE_ENABLE
    debug_enable = true;
    dprintf("keyboard_post_init_user: phenom both halves debug\n");
#endif
    transaction_register_rpc(RPC_PHENOM_CONFIG, phenom_sync_config_rpc);
    transaction_register_rpc(RPC_PHENOM_SPLIT_POINTING_SETTINGS, phenom_sync_split_pointing_settings_rpc);
    transaction_register_rpc(RPC_PHENOM_LED_COLORS, phenom_sync_led_colors_rpc);
    phenom_applied_raw = UINT32_MAX; // force re-apply after early kb_settings_pointing_init may have set CPI=0
    phenom_applied_devices_valid = false;

    uint32_t raw = phenom_upgrade_raw(via_get_layout_options());
    if (raw != via_get_layout_options()) {
        via_set_layout_options(raw);
    } else {
        via_set_layout_options_kb(raw);
    }

    phenom_synced_raw = phenom_via_config.raw;
    phenom_synced_devices = get_split_pointing_settings();
    phenom_synced_led_colors = get_settings_led_colors();
    dprintf("final raw=0x%08lX\n", (unsigned long)phenom_synced_raw);
}

void housekeeping_task_user(void) {
    static uint32_t last_sync = 0;

    if (!is_keyboard_master()) {
        return;
    }
    if (!is_transport_connected()) {
        phenom_synced_devices_valid = false;
        return;
    }
    if (timer_elapsed32(last_sync) < 100) {
        return;
    }

    last_sync = timer_read32();
    if (phenom_synced_raw != phenom_via_config.raw) {
        phenom_synced_raw = phenom_via_config.raw;
        transaction_rpc_send(RPC_PHENOM_CONFIG, sizeof(phenom_synced_raw), &phenom_synced_raw);
    }

    kb_settings_split_pointing_t devices = get_split_pointing_settings();
    if (!phenom_synced_devices_valid || memcmp(&phenom_synced_devices, &devices, sizeof(devices)) != 0) {
        if (transaction_rpc_send(RPC_PHENOM_SPLIT_POINTING_SETTINGS, sizeof(devices), &devices)) {
            phenom_synced_devices = devices;
            phenom_synced_devices_valid = true;
        } else {
            // The peer may have applied the settings before the RPC failed.
            phenom_synced_devices_valid = false;
        }
    }

    kb_settings_led_colors_t led_colors = get_settings_led_colors();
    if (memcmp(&phenom_synced_led_colors, &led_colors, sizeof(led_colors)) != 0) {
        phenom_synced_led_colors = led_colors;
        transaction_rpc_send(RPC_PHENOM_LED_COLORS, sizeof(phenom_synced_led_colors), &phenom_synced_led_colors);
    }
}

void pointing_device_driver_init(void) {
    pd_dprintf("pointing_device_driver_init: entry, is_keyboard_left()=%d, is_keyboard_master()=%d\n", is_keyboard_left(), is_keyboard_master());
    pd_dprintf("pointing_device_driver_init: both halves\n");
    phenom_detect_modules(true);
    pd_dprintf("  detected_module=%u, touchpad_avail=%d, trackball_avail=%d\n", (unsigned int)phenom_detected_module, phenom_touchpad_available, phenom_trackball_available);
    phenom_ensure_selected_module_ready();
    phenom_apply_device_config();
    pointing_device_set_cpi(pointing_device_driver_get_cpi());
    pd_dprintf("pointing_device_driver_init: done\n");
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    static uint32_t last_log = 0;
    if (timer_elapsed32(last_log) > 1000) {
        pd_dprintf("pointing_device_driver_get_report: is_left=%d, active_module=%u, touchpad_avail=%d, trackball_avail=%d\n",
                is_keyboard_left(), (unsigned int)phenom_get_active_module(), phenom_touchpad_available, phenom_trackball_available);
        last_log = timer_read32();
    }

    phenom_ensure_selected_module_ready();
    phenom_apply_device_config();

    switch (phenom_get_active_module()) {
        case PHENOM_MODULE_TRACKBALL:
            if (phenom_trackball_initialized) {
                mouse_report = pmw3610_get_report(mouse_report);
            }
            break;
        case PHENOM_MODULE_TOUCHPAD:
            if (phenom_touchpad_initialized) {
                mouse_report = azoteq_iqs5xx_get_report(mouse_report);
            }
            break;
        default:
            break;
    }

    if (phenom_get_active_module() == PHENOM_MODULE_TRACKBALL) {
        return phenom_rotate_trackball_report(mouse_report, phenom_get_local_orientation());
    }
    return phenom_rotate_report(mouse_report, phenom_get_local_orientation());
}

static bool phenom_report_has_motion(report_mouse_t report) {
    return abs(report.x) >= 1 || abs(report.y) >= 1 || abs(report.h) >= 1 || abs(report.v) >= 1 || report.buttons;
}

static report_mouse_t phenom_apply_side_mode(report_mouse_t mrpt, split_pointing_side_t side) {
    static int32_t accumulated_h[SPLIT_POINTING_SIDE_COUNT] = {0};
    static int32_t accumulated_v[SPLIT_POINTING_SIDE_COUNT] = {0};

    if (get_split_pointing_side_acceleration(side)) {
        int x = mrpt.x;
        int y = mrpt.y;
        mrpt.x = (mouse_xy_report_t)(x > 0 ? x * x / 16 + x : -x * x / 16 + x);
        mrpt.y = (mouse_xy_report_t)(y > 0 ? y * y / 16 + y : -y * y / 16 + y);
    }

    pointing_mode_t pmode   = get_split_pointing_side_mode(side);
    int32_t         divisor = get_split_pointing_side_sens(side, pmode);

    if (pmode == POINTING_MODE_NORMAL || divisor <= 0) {
        return mrpt;
    }

    accumulated_h[side] += mrpt.x;
    accumulated_v[side] += mrpt.y;

    int shift_x = accumulated_h[side] / divisor;
    int shift_y = accumulated_v[side] / divisor;

    accumulated_h[side] -= shift_x * divisor;
    accumulated_v[side] -= shift_y * divisor;

    mrpt.x = 0;
    mrpt.y = 0;

    if (shift_x == 0 && shift_y == 0) {
        return mrpt;
    }

    switch (pmode) {
        case POINTING_MODE_SNIPER:
            mrpt.x = shift_x;
            mrpt.y = shift_y;
            break;

        case POINTING_MODE_SCROLL:
            if (abs(shift_x) > abs(shift_y)) {
                mrpt.h              = shift_x;
                accumulated_v[side] = 0;
            } else if (abs(shift_x) < abs(shift_y)) {
                mrpt.v              = -shift_y;
                accumulated_h[side] = 0;
            }
            if (get_split_pointing_side_invert_scroll_h(side)) {
                mrpt.h = -mrpt.h;
            }
            if (get_split_pointing_side_invert_scroll(side)) {
                mrpt.v = -mrpt.v;
            }
            break;

        case POINTING_MODE_TEXT:
        case POINTING_MODE_USR1:
        case POINTING_MODE_USR2:
        case POINTING_MODE_USR3: {
#ifdef EH_TRACKBALL_TEXT_DIR_REMAP
            static uint16_t kc_up[SPLIT_POINTING_SIDE_COUNT]    = {KC_UP, KC_UP};
            static uint16_t kc_down[SPLIT_POINTING_SIDE_COUNT]  = {KC_DOWN, KC_DOWN};
            static uint16_t kc_left[SPLIT_POINTING_SIDE_COUNT]  = {KC_LEFT, KC_LEFT};
            static uint16_t kc_right[SPLIT_POINTING_SIDE_COUNT] = {KC_RIGHT, KC_RIGHT};
#ifdef EH_HPD_LAYERS
            uint8_t layer   = get_current_layer();
            kc_up[side]     = dynamic_keymap_get_keycode(layer, 0, 0);
            kc_down[side]   = dynamic_keymap_get_keycode(layer, 0, 1);
            kc_left[side]   = dynamic_keymap_get_keycode(layer, 0, 2);
            kc_right[side]  = dynamic_keymap_get_keycode(layer, 0, 3);
#endif

            if (kc_up[side] != kc_down[side] || kc_up[side] != kc_left[side] || kc_up[side] != kc_right[side] || kc_up[side] != KC_NO) {
                if (abs(shift_x) > abs(shift_y)) {
                    shift_y             = 0;
                    accumulated_v[side] = 0;
                } else if (abs(shift_x) < abs(shift_y)) {
                    shift_x             = 0;
                    accumulated_h[side] = 0;
                }

                // phenom-text-dir-default-v0.0.14: Phenom's natural Text mode vertical direction is opposite
                // to the shared fallback; Invert text flips it back when enabled.
                if (!get_split_pointing_side_invert_text(side)) {
                    shift_y = -shift_y;
                }
                if (get_split_pointing_side_invert_text_h(side)) {
                    shift_x = -shift_x;
                }

                for (; shift_x > 0; shift_x--) tap_code16(kc_right[side]);
                for (; shift_x < 0; shift_x++) tap_code16(kc_left[side]);
                for (; shift_y > 0; shift_y--) tap_code16(kc_up[side]);
                for (; shift_y < 0; shift_y++) tap_code16(kc_down[side]);
            }
#endif
            break;
        }

        default:
            break;
    }

    return mrpt;
}

report_mouse_t pointing_device_task_combined_kb(report_mouse_t left_report, report_mouse_t right_report) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool auto_layer_active = false;
    if (get_split_pointing_auto_mouse_enable()) {
        if (get_split_pointing_auto_mouse_mode_enabled(get_split_pointing_side_mode(SPLIT_POINTING_SIDE_LEFT)) && phenom_report_has_motion(left_report)) {
            auto_layer_active = true;
        }
        if (get_split_pointing_auto_mouse_mode_enabled(get_split_pointing_side_mode(SPLIT_POINTING_SIDE_RIGHT)) && phenom_report_has_motion(right_report)) {
            auto_layer_active = true;
        }
    }
    set_pointing_auto_mouse_override(true, auto_layer_active);
#endif

    left_report  = phenom_apply_side_mode(left_report, SPLIT_POINTING_SIDE_LEFT);
    right_report = phenom_apply_side_mode(right_report, SPLIT_POINTING_SIDE_RIGHT);
    return pointing_device_combine_reports(left_report, right_report);
}

uint16_t pointing_device_driver_get_cpi(void) {
    switch (phenom_get_active_module()) {
        case PHENOM_MODULE_TRACKBALL:
            return phenom_trackball_cpi_table[phenom_clamp_index(get_split_pointing_device_dpi_index(phenom_get_local_device_id(PHENOM_MODULE_TRACKBALL)), ARRAY_SIZE(phenom_trackball_cpi_table))];
        case PHENOM_MODULE_TOUCHPAD:
            return phenom_touchpad_cpi_table[phenom_clamp_index(get_split_pointing_device_dpi_index(phenom_get_local_device_id(PHENOM_MODULE_TOUCHPAD)), ARRAY_SIZE(phenom_touchpad_cpi_table))];
        default:
            return 0;
    }
}

void pointing_device_driver_set_cpi(uint16_t cpi) {
    dprintf("pointing_device_driver_set_cpi: cpi=%u, active_module=%u\n", cpi, (unsigned int)phenom_get_active_module());
    switch (phenom_get_active_module()) {
        case PHENOM_MODULE_TRACKBALL:
            if (phenom_trackball_initialized) {
                dprintf("  setting trackball CPI\n");
                pmw3610_set_cpi(0, cpi);
            } else {
                dprintf("  trackball not initialized\n");
            }
            break;
        case PHENOM_MODULE_TOUCHPAD:
            if (phenom_touchpad_initialized) {
                dprintf("  setting touchpad CPI\n");
                azoteq_iqs5xx_set_cpi(cpi);
            } else {
                dprintf("  touchpad not initialized\n");
            }
            break;
        default:
            dprintf("  no active module\n");
            break;
    }
}
