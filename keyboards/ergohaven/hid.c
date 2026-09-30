#include "hid.h"
#include <string.h>
#include "via.h"
#include "raw_hid.h"
#include "ergohaven_rgb.h"
#ifdef EH_APP_LAYOUT_ENABLE
#    include "vial.h"
#endif
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

#ifdef EH_APP_LAYOUT_ENABLE
#    define EH_APP_LAYOUT_PROTOCOL_VERSION 6
#    define EH_APP_LAYOUT_CONTROL_COUNT 15
#    define EH_APP_LAYOUT_KEY_COUNT 13
#    define EH_APP_LAYOUT_LAYER_COUNT 16
#    define EH_APP_LAYOUT_NAME_BYTES 22
#    define EH_APP_LAYOUT_CHUNK_COUNT (EH_APP_LAYOUT_LAYER_COUNT * 2)
#    define EH_APP_LAYOUT_ALL_CHUNKS 0xFFFFFFFFUL
#    define EH_APP_LAYOUT_UNSET_KEYCODE 0xFFFF
#    define EH_APP_LAYOUT_TIMEOUT_MS 2500
// Display transfers use 0xC0..0xD6. Keep runtime layouts isolated.
#    define EH_APP_LAYOUT_BEGIN 0xE1
#    define EH_APP_LAYOUT_KEYCODES 0xE2
#    define EH_APP_LAYOUT_COMMIT 0xE3
#    define EH_APP_LAYOUT_LAYER_NAME 0xE4
#    define EH_APP_LAYOUT_KEEPALIVE 0xE5
#    define EH_APP_LAYOUT_EVENT_POLL 0xE6
#    define EH_APP_LAYOUT_VISUALS 0xE7
#    define EH_APP_LAYOUT_EXECUTION_STATE 0xE8
#    define EH_APP_LAYOUT_EVENT_REQUEST 0xA5
#    define EH_APP_LAYOUT_EVENT_RESPONSE 0x5A
#    define EH_APP_LAYOUT_EVENT_QUEUE_LENGTH 8

typedef struct {
    bool     valid;
    bool     active;
    uint32_t chunks;
    uint16_t layer_name_chunks;
    uint16_t visual_chunks;
    uint32_t revision;
    uint8_t  name_length;
    char     name[EH_APP_LAYOUT_NAME_BYTES + 1];
    uint8_t  layer_name_lengths[EH_APP_LAYOUT_LAYER_COUNT];
    char     layer_names[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_NAME_BYTES + 1];
    uint16_t keycodes[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
    uint8_t  visuals[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
} eh_app_layout_staging_t;

static eh_app_layout_staging_t app_layout_staging;
static bool                    app_layout_active;
static uint32_t                app_layout_revision;
static uint32_t                app_layout_sync_time;
static char                    app_layout_name[EH_APP_LAYOUT_NAME_BYTES + 1];
static char                    app_layout_layer_names[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_NAME_BYTES + 1];
static uint16_t                app_layout_keycodes[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
static uint8_t                 app_layout_visuals[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
static uint8_t                 app_layout_execution_states[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
static uint32_t                app_layout_visual_generation;
static uint16_t                app_layout_held[EH_APP_LAYOUT_KEY_COUNT];

typedef struct {
    uint16_t sequence;
    uint32_t revision;
    uint8_t  layer;
    uint8_t  control;
} eh_app_layout_event_t;

static eh_app_layout_event_t app_layout_events[EH_APP_LAYOUT_EVENT_QUEUE_LENGTH];
static uint8_t               app_layout_event_head;
static uint8_t               app_layout_event_count;
static uint16_t              app_layout_event_sequence;

static uint32_t app_layout_read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void app_layout_clear_events(void) {
    app_layout_event_head  = 0;
    app_layout_event_count = 0;
}

static uint8_t app_layout_current_layer(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    return layer < EH_APP_LAYOUT_LAYER_COUNT ? layer : 0;
}

static void app_layout_enqueue_event(uint8_t layer, uint8_t control) {
    if (app_layout_event_count >= EH_APP_LAYOUT_EVENT_QUEUE_LENGTH) return;
    app_layout_event_sequence++;
    if (app_layout_event_sequence == 0) app_layout_event_sequence = 1;
    uint8_t tail = (app_layout_event_head + app_layout_event_count) % EH_APP_LAYOUT_EVENT_QUEUE_LENGTH;
    app_layout_events[tail] = (eh_app_layout_event_t){
        .sequence = app_layout_event_sequence,
        .revision = app_layout_revision,
        .layer    = layer,
        .control  = control,
    };
    app_layout_event_count++;
}

static void app_layout_poll_event(uint8_t *data, uint8_t length) {
    uint16_t acknowledged = data[3] | ((uint16_t)data[4] << 8);
    if (app_layout_event_count > 0 && app_layout_events[app_layout_event_head].sequence == acknowledged) {
        app_layout_event_head = (app_layout_event_head + 1) % EH_APP_LAYOUT_EVENT_QUEUE_LENGTH;
        app_layout_event_count--;
    }
    memset(data, 0, length);
    data[0] = EH_APP_LAYOUT_EVENT_POLL;
    data[1] = EH_APP_LAYOUT_PROTOCOL_VERSION;
    data[2] = EH_APP_LAYOUT_EVENT_RESPONSE;
    if (app_layout_event_count == 0) return;
    const eh_app_layout_event_t *event = &app_layout_events[app_layout_event_head];
    data[3]  = 1;
    data[4]  = event->sequence;
    data[5]  = event->sequence >> 8;
    data[6]  = event->layer;
    data[7]  = event->control;
    data[8]  = event->revision;
    data[9]  = event->revision >> 8;
    data[10] = event->revision >> 16;
    data[11] = event->revision >> 24;
}

static uint16_t app_layout_crc16_byte(uint16_t crc, uint8_t value) {
    crc ^= (uint16_t)value << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint16_t app_layout_crc16(const eh_app_layout_staging_t *snapshot) {
    uint16_t crc = 0xFFFF;
    for (uint8_t layer = 0; layer < EH_APP_LAYOUT_LAYER_COUNT; layer++) {
        for (uint8_t index = 0; index < EH_APP_LAYOUT_CONTROL_COUNT; index++) {
            crc = app_layout_crc16_byte(crc, snapshot->keycodes[layer][index]);
            crc = app_layout_crc16_byte(crc, snapshot->keycodes[layer][index] >> 8);
        }
    }
    for (uint8_t layer = 0; layer < EH_APP_LAYOUT_LAYER_COUNT; layer++) {
        for (uint8_t index = 0; index < EH_APP_LAYOUT_CONTROL_COUNT; index++) {
            crc = app_layout_crc16_byte(crc, snapshot->visuals[layer][index]);
        }
    }
    crc = app_layout_crc16_byte(crc, snapshot->name_length);
    for (uint8_t index = 0; index < snapshot->name_length; index++) {
        crc = app_layout_crc16_byte(crc, snapshot->name[index]);
    }
    for (uint8_t layer = 0; layer < EH_APP_LAYOUT_LAYER_COUNT; layer++) {
        crc = app_layout_crc16_byte(crc, snapshot->layer_name_lengths[layer]);
        for (uint8_t index = 0; index < snapshot->layer_name_lengths[layer]; index++) {
            crc = app_layout_crc16_byte(crc, snapshot->layer_names[layer][index]);
        }
    }
    return crc;
}

static void app_layout_release_held(void) {
    for (uint8_t control = 0; control < EH_APP_LAYOUT_KEY_COUNT; control++) {
        if (app_layout_held[control] == KC_NO) continue;
        vial_keycode_up(app_layout_held[control]);
        app_layout_held[control] = KC_NO;
    }
}

static bool app_layout_session_live(void) {
    if (!app_layout_active) return false;
    if (timer_elapsed32(app_layout_sync_time) < EH_APP_LAYOUT_TIMEOUT_MS) return true;
    app_layout_release_held();
    layer_clear();
    app_layout_active = false;
    app_layout_clear_events();
    return false;
}

void hid_app_layout_task(void) {
    app_layout_session_live();
}

bool hid_app_layout_process_packet(uint8_t *data, uint8_t length) {
    if (length < 2 || data[1] != EH_APP_LAYOUT_PROTOCOL_VERSION) return false;
    switch (data[0]) {
        case EH_APP_LAYOUT_BEGIN:
            if (length < 10 || data[7] != EH_APP_LAYOUT_CONTROL_COUNT || data[8] != EH_APP_LAYOUT_LAYER_COUNT || data[2] > 1 ||
                data[9] > EH_APP_LAYOUT_NAME_BYTES || length < (uint8_t)(10 + data[9]))
                return true;
            memset(&app_layout_staging, 0, sizeof(app_layout_staging));
            app_layout_staging.valid    = true;
            app_layout_staging.active   = data[2] != 0;
            app_layout_staging.revision = app_layout_read_u32(data + 3);
            app_layout_staging.name_length = data[9];
            memcpy(app_layout_staging.name, data + 10, app_layout_staging.name_length);
            app_layout_staging.name[app_layout_staging.name_length] = '\0';
            return true;

        case EH_APP_LAYOUT_KEYCODES: {
            if (!app_layout_staging.valid || length < 5) return true;
            uint8_t layer    = data[2];
            uint8_t start    = data[3];
            uint8_t count    = data[4];
            uint8_t expected = start == 0 ? EH_APP_LAYOUT_KEY_COUNT : 2;
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || (start != 0 && start != EH_APP_LAYOUT_KEY_COUNT) || count != expected ||
                length < (uint8_t)(5 + count * 2))
                return true;
            for (uint8_t index = 0; index < count; index++) {
                uint8_t offset = 5 + index * 2;
                app_layout_staging.keycodes[layer][start + index] = data[offset] | ((uint16_t)data[offset + 1] << 8);
            }
            uint8_t chunk = layer * 2 + (start == 0 ? 0 : 1);
            app_layout_staging.chunks |= (uint32_t)1U << chunk;
            return true;
        }

        case EH_APP_LAYOUT_VISUALS: {
            if (!app_layout_staging.valid || length < 4) return true;
            uint8_t layer = data[2];
            uint8_t count = data[3];
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || count != EH_APP_LAYOUT_CONTROL_COUNT ||
                length < (uint8_t)(4 + count))
                return true;
            memcpy(app_layout_staging.visuals[layer], data + 4, count);
            app_layout_staging.visual_chunks |= (uint16_t)1U << layer;
            return true;
        }

        case EH_APP_LAYOUT_LAYER_NAME: {
            if (!app_layout_staging.valid || length < 4) return true;
            uint8_t layer       = data[2];
            uint8_t name_length = data[3];
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || name_length > EH_APP_LAYOUT_NAME_BYTES ||
                length < (uint8_t)(4 + name_length))
                return true;
            app_layout_staging.layer_name_lengths[layer] = name_length;
            memcpy(app_layout_staging.layer_names[layer], data + 4, name_length);
            app_layout_staging.layer_names[layer][name_length] = '\0';
            app_layout_staging.layer_name_chunks |= (uint16_t)1U << layer;
            return true;
        }

        case EH_APP_LAYOUT_COMMIT: {
            if (length < 9 || !app_layout_staging.valid || app_layout_staging.chunks != EH_APP_LAYOUT_ALL_CHUNKS ||
                app_layout_staging.layer_name_chunks != UINT16_MAX || app_layout_staging.visual_chunks != UINT16_MAX)
                return true;
            uint32_t revision = app_layout_read_u32(data + 3);
            uint16_t crc      = data[7] | ((uint16_t)data[8] << 8);
            if ((data[2] != 0) != app_layout_staging.active || revision != app_layout_staging.revision ||
                crc != app_layout_crc16(&app_layout_staging)) {
                app_layout_staging.valid = false;
                return true;
            }
            bool layout_changed = app_layout_active != app_layout_staging.active ||
                                  memcmp(app_layout_keycodes, app_layout_staging.keycodes, sizeof(app_layout_keycodes)) != 0 ||
                                  memcmp(app_layout_visuals, app_layout_staging.visuals, sizeof(app_layout_visuals)) != 0 ||
                                  memcmp(app_layout_name, app_layout_staging.name, sizeof(app_layout_name)) != 0 ||
                                  memcmp(app_layout_layer_names, app_layout_staging.layer_names, sizeof(app_layout_layer_names)) != 0;
            if (layout_changed) {
                app_layout_release_held();
                layer_clear();
            }
            memcpy(app_layout_keycodes, app_layout_staging.keycodes, sizeof(app_layout_keycodes));
            memcpy(app_layout_visuals, app_layout_staging.visuals, sizeof(app_layout_visuals));
            memcpy(app_layout_name, app_layout_staging.name, sizeof(app_layout_name));
            memcpy(app_layout_layer_names, app_layout_staging.layer_names, sizeof(app_layout_layer_names));
            app_layout_active        = app_layout_staging.active;
            app_layout_revision      = app_layout_staging.revision;
            app_layout_sync_time     = timer_read32();
            app_layout_staging.valid = false;
            app_layout_clear_events();
            memset(app_layout_execution_states, 0, sizeof(app_layout_execution_states));
            app_layout_visual_generation++;
            return true;
        }

        case EH_APP_LAYOUT_KEEPALIVE:
            if (length >= 7 && app_layout_active && data[2] != 0 && app_layout_read_u32(data + 3) == app_layout_revision) {
                app_layout_sync_time = timer_read32();
            }
            return true;

        case EH_APP_LAYOUT_EVENT_POLL:
            if (length >= 5 && data[2] == EH_APP_LAYOUT_EVENT_REQUEST) app_layout_poll_event(data, length);
            return true;

        case EH_APP_LAYOUT_EXECUTION_STATE: {
            if (length < 9 || !app_layout_active || app_layout_read_u32(data + 2) != app_layout_revision) return true;
            uint8_t layer   = data[6];
            uint8_t control = data[7];
            uint8_t state   = data[8];
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || control >= EH_APP_LAYOUT_CONTROL_COUNT || state > 3) return true;
            if (app_layout_execution_states[layer][control] != state) {
                app_layout_execution_states[layer][control] = state;
                app_layout_visual_generation++;
            }
            return true;
        }
    }
    return false;
}

static uint16_t app_layout_keycode_for_control(uint8_t layer, uint8_t control) {
    for (int8_t candidate = (int8_t)layer; candidate >= 0; candidate--) {
        uint16_t keycode = app_layout_keycodes[candidate][control];
        if (keycode != KC_TRNS) return keycode;
    }
    return KC_NO;
}

bool hid_app_layout_get_keycode(uint8_t layer, uint8_t control, uint16_t *keycode) {
    if (keycode == NULL || control >= EH_APP_LAYOUT_CONTROL_COUNT || !app_layout_session_live()) return false;
    if (layer >= EH_APP_LAYOUT_LAYER_COUNT) layer = 0;
    for (int8_t candidate = (int8_t)layer; candidate >= 0; candidate--) {
        uint16_t candidate_keycode = app_layout_keycodes[candidate][control];
        if (candidate_keycode == KC_TRNS) continue;
        if (candidate_keycode == EH_APP_LAYOUT_UNSET_KEYCODE) return false;
        *keycode = candidate_keycode;
        return true;
    }
    *keycode = KC_NO;
    return true;
}

bool hid_app_layout_get_visual(uint8_t layer, uint8_t control, uint8_t *visual, uint8_t *state) {
    if (visual == NULL || state == NULL || control >= EH_APP_LAYOUT_CONTROL_COUNT || !app_layout_session_live()) return false;
    if (layer >= EH_APP_LAYOUT_LAYER_COUNT) layer = 0;
    for (int8_t candidate = (int8_t)layer; candidate >= 0; candidate--) {
        uint16_t keycode = app_layout_keycodes[candidate][control];
        if (keycode == KC_TRNS) continue;
        uint8_t candidate_visual = app_layout_visuals[candidate][control];
        if (candidate_visual == 0) return false;
        *visual = candidate_visual;
        *state  = app_layout_execution_states[candidate][control];
        return true;
    }
    return false;
}

uint32_t hid_app_layout_visual_generation(void) {
    return app_layout_visual_generation;
}

bool hid_app_layout_get_name(uint8_t layer, char *name, uint8_t size) {
    if (name == NULL || size == 0 || !app_layout_session_live()) return false;
    if (layer >= EH_APP_LAYOUT_LAYER_COUNT) layer = 0;
    const char *source = app_layout_layer_names[layer][0] != '\0' ? app_layout_layer_names[layer] : app_layout_name;
    if (source[0] == '\0') return false;
    snprintf(name, size, "%s", source);
    return true;
}

static bool app_layout_dispatch_control(uint8_t control, bool pressed, bool encoder) {
    if (control >= EH_APP_LAYOUT_CONTROL_COUNT) return false;
    if (!encoder && control < EH_APP_LAYOUT_KEY_COUNT && !pressed && app_layout_held[control] != KC_NO) {
        vial_keycode_up(app_layout_held[control]);
        app_layout_held[control] = KC_NO;
        return true;
    }
    if (!app_layout_session_live()) return false;
    uint8_t layer = app_layout_current_layer();
    if (pressed) app_layout_enqueue_event(layer, control);
    uint16_t keycode = app_layout_keycode_for_control(layer, control);
    if (keycode == EH_APP_LAYOUT_UNSET_KEYCODE) return false;
    if (encoder) {
        if (pressed && keycode != KC_NO) vial_keycode_tap(keycode);
        return true;
    }
    if (!pressed) return true;
    if (keycode != KC_NO) {
        vial_keycode_down(keycode);
        app_layout_held[control] = keycode;
    }
    return true;
}

bool hid_app_layout_process_keyevent(uint8_t row, uint8_t col, bool pressed) {
    if (row == 0 && col == 2) return app_layout_dispatch_control(12, pressed, false);
    if (row < 1 || row > 4 || col > 2) return false;
    return app_layout_dispatch_control((row - 1) * 3 + col, pressed, false);
}

bool hid_app_layout_process_encoder_event(uint8_t index, bool clockwise, bool pressed) {
    if (index != 0) return false;
    return app_layout_dispatch_control(clockwise ? 14 : 13, pressed, true);
}
#endif

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
#ifdef EH_APP_LAYOUT_ENABLE
    bool app_layout_event_poll = length > 0 && data[0] == EH_APP_LAYOUT_EVENT_POLL;
    if (hid_app_layout_process_packet(data, length)) {
        if (!app_layout_event_poll) *((uint64_t *)data) = VIAL_HID_MAGIC;
        return;
    }
#endif
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
#ifdef EH_APP_LAYOUT_ENABLE
    bool app_layout_event_poll = length > 0 && data[0] == EH_APP_LAYOUT_EVENT_POLL;
    if (hid_app_layout_process_packet(data, length)) {
        if (!app_layout_event_poll) *((uint64_t *)data) = VIAL_HID_MAGIC;
        return;
    }
#endif
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
