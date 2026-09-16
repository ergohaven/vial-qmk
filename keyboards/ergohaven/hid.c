#include "hid.h"
#include <string.h>
#include "via.h"
#include "raw_hid.h"
#include "ergohaven_rgb.h"
#ifdef EH_STANDBY_BACKGROUND_ENABLE
#    include "src/display/eh_background.h"
#endif
#ifdef EH_PICTOGRAM_ENABLE
#    include "src/display/eh_pictograms.h"
#endif
#ifdef EH_STARTUP_IMAGE_ENABLE
#    include "src/display/eh_startup_image.h"
#endif

static hid_data_t hid_data;

hid_data_t *get_hid_data(void) {
    return &hid_data;
}

static uint32_t hid_sync_time = 0;
static uint32_t time_sync_time, host_status_time;
static bool time_received, host_status_seen, host_online;
static bool volume_received;
static uint32_t volume_sync_time;

bool is_hid_time_active(void) {
    if (!time_received) return false;
    if (host_status_seen) return host_online && timer_elapsed32(host_status_time) < 5000;
    return timer_elapsed32(time_sync_time) < 61000;
}

bool is_hid_active(void) {
    return (hid_sync_time != 0) && timer_elapsed32(hid_sync_time) < 61 * 1000;
}

bool is_hid_volume_active(void) {
    if (!volume_received) return false;
    // Modern Entropy reports shutdown explicitly and sends a heartbeat. Other
    // traffic (clock/layout/media) must not keep an absent volume host alive.
    if (host_status_seen) return host_online && timer_elapsed32(host_status_time) < 5000;
    return timer_elapsed32(volume_sync_time) < 61000;
}

typedef enum {
    _TIME = 0xAA, // random value that does not conflict with VIA, must match companion app
    _VOLUME,
    _LAYOUT,
    _MEDIA_ARTIST,
    _MEDIA_TITLE,
    _DATE,

    _HOST_STATUS = 0xBA,

    _RELAY_FROM_DEVICE = 0xCC,
    _RELAY_TO_DEVICE,
} hid_data_type;

typedef enum {
    _POINTING = 10,
} relay_data_type;

void read_string(uint8_t *data, char *string_data) {
    uint8_t data_length = MIN(31, data[1]);
    memcpy(string_data, data + 2, data_length);
    string_data[data_length] = '\0';
}

bool process_raw_hid_data(uint8_t *data, uint8_t length) {
    if (length < 3) return false;
    uint8_t data_type = data[0];

    bool new_hid_data = false;

    switch (data_type) {
        case _TIME:
            // 0xff:0xff was used by older Entropy builds as a shutdown
            // sentinel. Never expose it (or any malformed packet) as a clock
            // value; the last valid time remains visible until normal timeout.
            if (data[1] < 24 && data[2] < 60) {
                time_received = true;
                time_sync_time = timer_read32();
                hid_data.hours        = data[1];
                hid_data.minutes      = data[2];
                hid_data.time_changed = true;
                new_hid_data          = true;
            }
            break;

        case _HOST_STATUS:
            if (data[1] <= 1) {
                host_status_seen = true;
                host_online = data[1] != 0;
                if (!host_online) volume_received = false;
                host_status_time = timer_read32();
                new_hid_data = true;
            }
            break;

        case _DATE: {
            if (length < 5) break;
            uint16_t year = data[3] | ((uint16_t)data[4] << 8);
            uint8_t month = data[2], day = data[1];
            static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            if (year < 2000 || year > 9999 || month < 1 || month > 12) break;
            uint8_t maximum = days[month-1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            if (!day || day > maximum) break;
            hid_data.year = year; hid_data.month = month; hid_data.day = day; hid_data.date_valid = true;
            new_hid_data = true;
            break;
        }
        case _VOLUME:
            volume_received        = true;
            volume_sync_time       = timer_read32();
            hid_data.volume         = data[1];
            hid_data.volume_changed = true;
            new_hid_data            = true;
            break;

        case _LAYOUT:
            hid_data.layout         = data[1];
            hid_data.layout_changed = true;
            new_hid_data            = true;
            break;

        case _MEDIA_ARTIST:
            read_string(data, hid_data.media_artist);
            hid_data.media_artist_changed = true;
            new_hid_data                  = true;
            break;

        case _MEDIA_TITLE:
            read_string(data, hid_data.media_title);
            hid_data.media_title_changed = true;
            new_hid_data                 = true;
            break;

        case _RELAY_TO_DEVICE:
            switch (data[1]) {
                case _POINTING:
                    set_pointing_mode_from_hid(data[2]);
                    break;
            }
            new_hid_data = true;

        default:
            break;
    }

    if (new_hid_data) {
        hid_sync_time        = timer_read32();
        hid_data.hid_changed = new_hid_data;
    }

    return new_hid_data;
}

void hid_send_pointing_mode(pointing_mode_t mode) {
    uint8_t data[32];
    memset(data, 0, 32);
    data[0] = _RELAY_FROM_DEVICE;
    data[1] = _POINTING;
    data[2] = mode;
    raw_hid_send(data, 32);
}

static bool process_via_custom_lighting(uint8_t *data, uint8_t length) {
#if defined(VIA_CUSTOM_LIGHTING_ENABLE)
    if (length < 4) {
        return false;
    }

    uint8_t *command_id = &data[0];
    uint8_t *channel_id = &data[1];
    uint8_t *value_id   = &data[2];
    uint8_t *value_data = &data[3];

    if (*channel_id != id_custom_channel) {
        *command_id = id_unhandled;
        return true;
    }

    switch (*command_id) {
        case id_lighting_get_value:
            if (*value_id == 1) {
                value_data[0] = get_led_rgb_brightness();
                return true;
            }
            if (*value_id >= 2 && *value_id < 2 + EH_RGB_LAYER_COUNT) {
                value_data[0] = get_layer_rgb_color(*value_id - 2);
                return true;
            }
            *command_id = id_unhandled;
            return true;

        case id_lighting_set_value:
            if (*value_id == 1) {
                set_led_rgb_brightness(value_data[0]);
                return true;
            }
            if (*value_id >= 2 && *value_id < 2 + EH_RGB_LAYER_COUNT) {
                set_layer_rgb_color(*value_id - 2, value_data[0]);
                return true;
            }
            *command_id = id_unhandled;
            return true;

        case id_lighting_save:
            return true;
    }
#endif

    return false;
}

#if defined(SPLIT_KEYBOARD) && (defined(OLED_ENABLE) || defined(EH_HAS_DISPLAY) || defined(EH_FORCE_SPLIT_HID_SYNC))
#    include "transactions.h"

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
#ifdef EH_STARTUP_IMAGE_ENABLE
    if (eh_startup_image_process_hid(data, length)) return;
#endif
#ifdef EH_STANDBY_BACKGROUND_ENABLE
    if (eh_background_process_hid(data, length)) return;
#endif
#ifdef EH_PICTOGRAM_ENABLE
    if (eh_pictograms_process_hid(data, length)) return;
#endif
    if (process_via_custom_lighting(data, length)) {
        return;
    }

    bool res = process_raw_hid_data(data, length);
    if (res && is_keyboard_master()) transaction_rpc_send(RPC_SYNC_HID, length, data);
    if (res) *((uint64_t *)data) = VIAL_HID_MAGIC;
}

void hid_sync(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    (void)out_buflen;
    (void)out_data;
    process_raw_hid_data((uint8_t *)in_data, in_buflen);
}

void keyboard_post_init_hid(void) {
    transaction_register_rpc(RPC_SYNC_HID, hid_sync);
}

#else

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
#ifdef EH_STARTUP_IMAGE_ENABLE
    if (eh_startup_image_process_hid(data, length)) return;
#endif
#ifdef EH_STANDBY_BACKGROUND_ENABLE
    if (eh_background_process_hid(data, length)) return;
#endif
#ifdef EH_PICTOGRAM_ENABLE
    if (eh_pictograms_process_hid(data, length)) return;
#endif
    if (process_via_custom_lighting(data, length)) {
        return;
    }

    bool res = process_raw_hid_data(data, length);
    if (res) *((uint64_t *)data) = VIAL_HID_MAGIC;
}

void keyboard_post_init_hid(void) {}

#endif
