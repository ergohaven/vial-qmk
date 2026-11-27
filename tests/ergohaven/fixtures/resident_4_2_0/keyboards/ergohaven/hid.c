#include "hid.h"
#include "sm62_trace.h"
#include <string.h>
#include "via.h"
#include "raw_hid.h"
#include "ergohaven_rgb.h"
#ifdef EH_SM62_SPLIT_RUNTIME_ENABLE
#    include <stddef.h>
#    include "src/eh_settings.h"
#    include "dynamic_keymap.h"
#    include "transactions.h"
#endif
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

__attribute__((weak)) bool sm62_diag_raw(uint8_t *data, uint8_t length) { (void)data; (void)length; return false; }
static hid_data_t hid_data;

#ifdef EH_APP_LAYOUT_ENABLE
// Boards without a layout display need no notification.
__attribute__((weak)) void hid_app_layout_changed_kb(void) {}

#    define EH_APP_LAYOUT_PROTOCOL_VERSION 7
#    define EH_APP_LAYOUT_STACK_SLOTS 4
#    define EH_APP_LAYOUT_STACK_NAME_BYTES 12
#    define EH_APP_LAYOUT_CONTROL_COUNT 15
#    define EH_APP_LAYOUT_KEY_COUNT 13
#    if defined(KEYBOARD_ergohaven_sm62)
#        undef EH_APP_LAYOUT_PROTOCOL_VERSION
#        undef EH_APP_LAYOUT_CONTROL_COUNT
#        undef EH_APP_LAYOUT_KEY_COUNT
#        define EH_APP_LAYOUT_PROTOCOL_VERSION 8
#        define EH_APP_LAYOUT_CONTROL_COUNT 62
#        define EH_APP_LAYOUT_KEY_COUNT 62
#        include "sm62/application_geometry.h"
#    endif
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
#    define EH_APP_LAYOUT_ENCODER_STACK 0xE9
#    define EH_APP_LAYOUT_DEACTIVATE 0xEA
#    define EH_APP_LAYOUT_EVENT_REQUEST 0xA5
#    define EH_APP_LAYOUT_EVENT_RESPONSE 0x5A
#    define EH_APP_LAYOUT_EVENT_QUEUE_LENGTH 8

typedef struct {
    bool     valid;
    bool     active;
    uint32_t chunks;
    uint16_t layer_name_chunks;
    uint16_t visual_chunks;
#    if defined(KEYBOARD_ergohaven_sm62)
    uint8_t key_segments[16];
    uint8_t visual_segments[16];
#    endif
    uint32_t revision;
    uint8_t  name_length;
    char     name[EH_APP_LAYOUT_NAME_BYTES + 1];
    uint8_t  layer_name_lengths[EH_APP_LAYOUT_LAYER_COUNT];
    char     layer_names[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_NAME_BYTES + 1];
    uint16_t keycodes[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
    uint8_t  visuals[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
    uint8_t  stack_chunks[EH_APP_LAYOUT_LAYER_COUNT];
    uint8_t  stack_counts[EH_APP_LAYOUT_LAYER_COUNT];
    uint16_t stack_keycodes[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_STACK_SLOTS][2];
    char     stack_names[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_STACK_SLOTS][EH_APP_LAYOUT_STACK_NAME_BYTES + 1];
} eh_app_layout_staging_t;

static eh_app_layout_staging_t app_layout_staging;
#if defined(KEYBOARD_ergohaven_sm62)
static uint32_t app_layout_staging_time;
#endif
static bool                    app_layout_active;
static uint32_t                app_layout_revision;
static uint32_t                app_layout_sync_time;
static char                    app_layout_name[EH_APP_LAYOUT_NAME_BYTES + 1];
static char                    app_layout_layer_names[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_NAME_BYTES + 1];
static uint16_t                app_layout_keycodes[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
static uint8_t                 app_layout_visuals[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
static uint8_t                 app_layout_stack_counts[EH_APP_LAYOUT_LAYER_COUNT];
static uint8_t                 app_layout_stack_selected[EH_APP_LAYOUT_LAYER_COUNT];
static uint16_t                app_layout_stack_keycodes[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_STACK_SLOTS][2];
static char                    app_layout_stack_names[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_STACK_SLOTS][EH_APP_LAYOUT_STACK_NAME_BYTES + 1];
static uint8_t                 app_layout_execution_states[EH_APP_LAYOUT_LAYER_COUNT][EH_APP_LAYOUT_CONTROL_COUNT];
static uint32_t                app_layout_visual_generation;
static uint32_t                app_layout_wake_epoch;
static bool                    app_layout_wake_epoch_seen;
static uint8_t                 app_layout_wake_identity[20];
static bool                    app_layout_wake_identity_seen;
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

static void app_layout_enqueue_event(uint32_t revision, uint8_t layer, uint8_t control) {
    if (app_layout_event_count >= EH_APP_LAYOUT_EVENT_QUEUE_LENGTH) return;
    app_layout_event_sequence++;
    if (app_layout_event_sequence == 0) app_layout_event_sequence = 1;
    uint8_t tail = (app_layout_event_head + app_layout_event_count) % EH_APP_LAYOUT_EVENT_QUEUE_LENGTH;
    app_layout_events[tail] = (eh_app_layout_event_t){
        .sequence = app_layout_event_sequence,
        .revision = revision,
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
    for (uint8_t layer = 0; layer < EH_APP_LAYOUT_LAYER_COUNT; layer++) {
        crc = app_layout_crc16_byte(crc, snapshot->stack_counts[layer]);
        for (uint8_t slot = 0; slot < EH_APP_LAYOUT_STACK_SLOTS; slot++) {
            for (uint8_t direction = 0; direction < 2; direction++) {
                uint16_t keycode = snapshot->stack_keycodes[layer][slot][direction];
                crc = app_layout_crc16_byte(crc, keycode);
                crc = app_layout_crc16_byte(crc, keycode >> 8);
            }
            uint8_t length = strlen(snapshot->stack_names[layer][slot]);
            crc = app_layout_crc16_byte(crc, length);
            for (uint8_t i = 0; i < length; i++) crc = app_layout_crc16_byte(crc, snapshot->stack_names[layer][slot][i]);
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
#if defined(KEYBOARD_ergohaven_sm62)
    app_layout_visual_generation++;
#endif
    app_layout_clear_events();
    return false;
}

#if defined(KEYBOARD_ergohaven_sm62)
#include "sm62/compact_usb.h"
#endif

void hid_app_layout_task(void) {
    app_layout_session_live();
#if defined(KEYBOARD_ergohaven_sm62)
    app_compact_expire();
    if (app_layout_staging.valid && timer_elapsed32(app_layout_staging_time) >= EH_APP_LAYOUT_TIMEOUT_MS) app_layout_staging.valid = false;
#endif
}

#ifdef SM62_RAW_HID_FRAME_GATE
/* Parser-owned RAM packets plus the exact event request are safe while
 * pixels are in flight. E6 only acknowledges the core0 event queue and replies
 * through VIA; core1 renders its leased snapshot, never this queue. Wrong
 * versions, sizes and E6 subcommands retain the ordinary FIFO path. */
bool hid_app_layout_packet_safe_during_refresh(const uint8_t *data, uint8_t length) {
    if (length==32 && data[0]==0xEF && data[1]==9)
        return data[2]==0 || data[2]==2 || data[2]==3 || data[2]==5 || data[2]==6;
    if (length != 32 || data[1] != EH_APP_LAYOUT_PROTOCOL_VERSION) return false;
    switch (data[0]) {
        case 0xEB:
            return data[2] == 0xA5 && (data[3] == 1 || data[3] == 2);
        case 0xEF:
            return data[2] == 0 || data[2] == 2; /* STORE must wait for flash/render quiescence. */
        case 0xEC:
        case 0xED:
        case 0xEE:
            return true;
        case EH_APP_LAYOUT_EVENT_POLL:
            return data[2] == EH_APP_LAYOUT_EVENT_REQUEST;
        case EH_APP_LAYOUT_BEGIN:
        case EH_APP_LAYOUT_KEYCODES:
        case EH_APP_LAYOUT_VISUALS:
        case EH_APP_LAYOUT_LAYER_NAME:
        case EH_APP_LAYOUT_ENCODER_STACK:
        case EH_APP_LAYOUT_COMMIT:
        case EH_APP_LAYOUT_KEEPALIVE:
        case EH_APP_LAYOUT_DEACTIVATE:
        case EH_APP_LAYOUT_EXECUTION_STATE:
            return true;
        default:
            return false;
    }
}
#endif

bool hid_app_layout_process_packet(uint8_t *data, uint8_t length) {
#if defined(KEYBOARD_ergohaven_sm62)
    if (length && app_compact_packet(data, length)) return true;
    if (!app_compact_replaying && length &&
        (data[0] == EH_APP_LAYOUT_BEGIN || data[0] == EH_APP_LAYOUT_DEACTIVATE ||
         data[0] == EH_APP_LAYOUT_KEYCODES || data[0] == EH_APP_LAYOUT_COMMIT ||
         data[0] == EH_APP_LAYOUT_VISUALS || data[0] == EH_APP_LAYOUT_LAYER_NAME || data[0] == EH_APP_LAYOUT_ENCODER_STACK)) app_compact_reset();
#endif
    if (length < 2 || data[1] != EH_APP_LAYOUT_PROTOCOL_VERSION) return false;
#if defined(KEYBOARD_ergohaven_sm62)
    if (app_layout_staging.valid && timer_elapsed32(app_layout_staging_time) >= EH_APP_LAYOUT_TIMEOUT_MS) app_layout_staging.valid = false;
#endif
    switch (data[0]) {
        case EH_APP_LAYOUT_DEACTIVATE:
            app_layout_release_held();
            layer_clear();
            app_layout_active = false;
            app_layout_staging.valid = false;
            app_layout_clear_events();
            app_layout_visual_generation++;
            return true;

        case EH_APP_LAYOUT_BEGIN:
#if defined(KEYBOARD_ergohaven_sm62)
            app_layout_staging.valid = false;
            app_layout_staging_time = timer_read32();
#endif
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
#    if defined(KEYBOARD_ergohaven_sm62)
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || start >= EH_APP_LAYOUT_CONTROL_COUNT || start % 13 != 0) return true;
            uint8_t expected = EH_APP_LAYOUT_CONTROL_COUNT - start;
            if (expected > 13) expected = 13;
            if (count != expected || length < (uint8_t)(5 + count * 2)) return true;
#    else
            uint8_t expected = start == 0 ? EH_APP_LAYOUT_KEY_COUNT : 2;
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || (start != 0 && start != EH_APP_LAYOUT_KEY_COUNT) || count != expected ||
                length < (uint8_t)(5 + count * 2))
                return true;
#    endif
            for (uint8_t index = 0; index < count; index++) {
                uint8_t offset = 5 + index * 2;
                app_layout_staging.keycodes[layer][start + index] = data[offset] | ((uint16_t)data[offset + 1] << 8);
            }
#    if defined(KEYBOARD_ergohaven_sm62)
            app_layout_staging.key_segments[layer] |= (uint8_t)1U << (start / 13);
            if (app_layout_staging.key_segments[layer] == 0x1F) app_layout_staging.chunks |= (uint32_t)3U << (layer * 2);
#    else
            uint8_t chunk = layer * 2 + (start == 0 ? 0 : 1);
            app_layout_staging.chunks |= (uint32_t)1U << chunk;
#    endif
            return true;
        }

        case EH_APP_LAYOUT_VISUALS: {
            if (!app_layout_staging.valid || length < 4) return true;
            uint8_t layer = data[2];
#    if defined(KEYBOARD_ergohaven_sm62)
            if (length < 5) return true;
            uint8_t start = data[3], count = data[4];
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || start >= EH_APP_LAYOUT_CONTROL_COUNT || start % 27 != 0) return true;
            uint8_t expected = EH_APP_LAYOUT_CONTROL_COUNT - start;
            if (expected > 27) expected = 27;
            if (count != expected || length < (uint8_t)(5 + count)) return true;
            memcpy(app_layout_staging.visuals[layer] + start, data + 5, count);
            app_layout_staging.visual_segments[layer] |= (uint8_t)1U << (start / 27);
            if (app_layout_staging.visual_segments[layer] == 7) app_layout_staging.visual_chunks |= (uint16_t)1U << layer;
#    else
            uint8_t count = data[3];
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || count != EH_APP_LAYOUT_CONTROL_COUNT ||
                length < (uint8_t)(4 + count))
                return true;
            memcpy(app_layout_staging.visuals[layer], data + 4, count);
            app_layout_staging.visual_chunks |= (uint16_t)1U << layer;
#    endif
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

        case EH_APP_LAYOUT_ENCODER_STACK: {
            if (!app_layout_staging.valid || length < 10) return true;
            uint8_t layer = data[2], slot = data[3], count = data[4], name_length = data[5];
            if (layer >= EH_APP_LAYOUT_LAYER_COUNT || slot >= EH_APP_LAYOUT_STACK_SLOTS ||
                (count != 0 && (count < 2 || count > EH_APP_LAYOUT_STACK_SLOTS)) ||
                name_length > EH_APP_LAYOUT_STACK_NAME_BYTES || length < (uint8_t)(10 + name_length) ||
                (app_layout_staging.stack_chunks[layer] && app_layout_staging.stack_counts[layer] != count))
                return true;
            app_layout_staging.stack_counts[layer] = count;
            app_layout_staging.stack_keycodes[layer][slot][0] = data[6] | ((uint16_t)data[7] << 8);
            app_layout_staging.stack_keycodes[layer][slot][1] = data[8] | ((uint16_t)data[9] << 8);
            memcpy(app_layout_staging.stack_names[layer][slot], data + 10, name_length);
            app_layout_staging.stack_names[layer][slot][name_length] = '\0';
            app_layout_staging.stack_chunks[layer] |= (uint8_t)1U << slot;
            return true;
        }

        case EH_APP_LAYOUT_COMMIT: {
            if (length < 9 || !app_layout_staging.valid || app_layout_staging.chunks != EH_APP_LAYOUT_ALL_CHUNKS ||
                app_layout_staging.layer_name_chunks != UINT16_MAX || app_layout_staging.visual_chunks != UINT16_MAX)
                return true;
            for (uint8_t layer = 0; layer < EH_APP_LAYOUT_LAYER_COUNT; layer++) {
                if (app_layout_staging.stack_chunks[layer] != 0x0F) return true;
            }
            uint32_t revision = app_layout_read_u32(data + 3);
            uint16_t crc      = data[7] | ((uint16_t)data[8] << 8);
            if ((data[2] != 0) != app_layout_staging.active || revision != app_layout_staging.revision ||
                crc != app_layout_crc16(&app_layout_staging)) {
                app_layout_staging.valid = false;
                return true;
            }
            // Validate the original wire CRC first. Refuse, rather than rewrite,
            // forbidden ingress so resident raw CRC/identity and SELECT agree.
            // Only an immutable record's nonpublishing integrity check is exempt.
            bool authorize_ingress = true;
#if defined(KEYBOARD_ergohaven_sm62)
            authorize_ingress = !(app_compact_validate_only && app_compact_record_integrity);
#endif
            for (uint8_t layer = 0; layer < EH_APP_LAYOUT_LAYER_COUNT; layer++) {
                for (uint8_t control = 0; control < EH_APP_LAYOUT_CONTROL_COUNT; control++) {
                    uint16_t keycode = app_layout_staging.keycodes[layer][control];
                    if (authorize_ingress && vial_keycode_firewall(keycode) != keycode) {
                        app_layout_staging.valid = false;
                        return true;
                    }
                }
                for (uint8_t slot = 0; slot < EH_APP_LAYOUT_STACK_SLOTS; slot++) {
                    for (uint8_t direction = 0; direction < 2; direction++) {
                        uint16_t keycode = app_layout_staging.stack_keycodes[layer][slot][direction];
                        if (authorize_ingress && vial_keycode_firewall(keycode) != keycode) {
                            app_layout_staging.valid = false;
                            return true;
                        }
                    }
                }
            }
#if defined(KEYBOARD_ergohaven_sm62)
            if (app_compact_validate_only && app_compact_replaying) {
                app_compact_validated = true;
                app_layout_staging.valid = false;
                return true;
            }
#endif
            bool layout_changed = app_layout_active != app_layout_staging.active ||
                                  memcmp(app_layout_keycodes, app_layout_staging.keycodes, sizeof(app_layout_keycodes)) != 0 ||
                                  memcmp(app_layout_stack_counts, app_layout_staging.stack_counts, sizeof(app_layout_stack_counts)) != 0 ||
                                  memcmp(app_layout_stack_keycodes, app_layout_staging.stack_keycodes, sizeof(app_layout_stack_keycodes)) != 0 ||
                                  memcmp(app_layout_stack_names, app_layout_staging.stack_names, sizeof(app_layout_stack_names)) != 0 ||
                                  memcmp(app_layout_name, app_layout_staging.name, sizeof(app_layout_name)) != 0 ||
                                  memcmp(app_layout_layer_names, app_layout_staging.layer_names, sizeof(app_layout_layer_names)) != 0;
            if (layout_changed) {
                app_layout_release_held();
                layer_clear();
            }
            memcpy(app_layout_keycodes, app_layout_staging.keycodes, sizeof(app_layout_keycodes));
            memcpy(app_layout_visuals, app_layout_staging.visuals, sizeof(app_layout_visuals));
            memcpy(app_layout_stack_counts, app_layout_staging.stack_counts, sizeof(app_layout_stack_counts));
            memcpy(app_layout_stack_keycodes, app_layout_staging.stack_keycodes, sizeof(app_layout_stack_keycodes));
            memcpy(app_layout_stack_names, app_layout_staging.stack_names, sizeof(app_layout_stack_names));
            if (layout_changed) memset(app_layout_stack_selected, 0, sizeof(app_layout_stack_selected));
            memcpy(app_layout_name, app_layout_staging.name, sizeof(app_layout_name));
            memcpy(app_layout_layer_names, app_layout_staging.layer_names, sizeof(app_layout_layer_names));
            app_layout_active        = app_layout_staging.active;
            app_layout_revision      = app_layout_staging.revision;
            app_layout_sync_time     = timer_read32();
            app_layout_staging.valid = false;
            app_layout_clear_events();
            memset(app_layout_execution_states, 0, sizeof(app_layout_execution_states));
            app_layout_visual_generation++;
            // v2 is a stable semantic-ID digest (not map content). Retain it
            // across deactivate, expiry and transport replacement. Unknown or
            // legacy snapshots do not erase the last known semantic identity.
            if (length >= 31 && data[9] == 0x57 && data[10] == 2) {
                bool changed = !app_layout_wake_identity_seen || memcmp(app_layout_wake_identity, data + 11, 20) != 0;
                memcpy(app_layout_wake_identity, data + 11, 20);
                app_layout_wake_identity_seen = true;
                if (changed) hid_app_layout_changed_kb();
            } else if (!app_layout_wake_identity_seen && length >= 15 && data[9] == 0x57 && data[10] == 1) {
                // Backward compatibility only: v1 has worker-local epochs and
                // cannot certify semantic transitions across worker restarts.
                uint32_t epoch = app_layout_read_u32(data + 11);
                bool changed = !app_layout_wake_epoch_seen || epoch != app_layout_wake_epoch;
                app_layout_wake_epoch_seen = true;
                app_layout_wake_epoch = epoch;
                if (changed) hid_app_layout_changed_kb();
            }
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
    if (EH_APP_LAYOUT_CONTROL_COUNT == 15 && control >= 13 && app_layout_stack_counts[layer] >= 2) {
        uint16_t selected = app_layout_stack_keycodes[layer][app_layout_stack_selected[layer]][control == 14 ? 1 : 0];
        *keycode = (selected == EH_APP_LAYOUT_UNSET_KEYCODE || selected == KC_TRNS) ? KC_NO : selected;
        return true;
    }
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
    if (app_layout_stack_counts[layer] >= 2) {
        uint8_t selected = app_layout_stack_selected[layer];
        snprintf(name, size, "<%s>", app_layout_stack_names[layer][selected]);
        return true;
    }
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
    if (EH_APP_LAYOUT_CONTROL_COUNT == 15 && !encoder && control == 12 && app_layout_stack_counts[layer] >= 2) {
        if (pressed) {
            app_layout_stack_selected[layer] = (app_layout_stack_selected[layer] + 1) % app_layout_stack_counts[layer];
            app_layout_visual_generation++;
        }
        return true;
    }
    if (pressed) app_layout_enqueue_event(app_layout_revision, layer, control);
    uint16_t keycode = app_layout_keycode_for_control(layer, control);
    if (encoder && app_layout_stack_counts[layer] >= 2) {
        keycode = app_layout_stack_keycodes[layer][app_layout_stack_selected[layer]][control == 14 ? 1 : 0];
        if (keycode == EH_APP_LAYOUT_UNSET_KEYCODE || keycode == KC_TRNS) keycode = KC_NO;
    }
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

static bool app_layout_control_for_position(uint8_t row, uint8_t col, uint8_t *control) {
#    if defined(KEYBOARD_ergohaven_sm62)
    for (uint8_t i = 0; i < EH_APP_LAYOUT_KEY_COUNT; ++i) {
        if (sm62_application_positions[i][0] == row && sm62_application_positions[i][1] == col) {
            *control = i;
            return true;
        }
    }
    return false;
#    else
    if (row == 0 && col == 2) { *control = 12; return true; }
    if (row < 1 || row > 4 || col > 2) return false;
    *control = (row - 1) * 3 + col;
    return true;
#    endif
}

bool hid_app_layout_process_keyevent(uint8_t row, uint8_t col, bool pressed) {
    uint8_t control;
    return app_layout_control_for_position(row, col, &control) && app_layout_dispatch_control(control, pressed, false);
}

#if defined(KEYBOARD_ergohaven_sm62)
/* Combo's delayed replay bypasses pre_process_record_kb. The record already
 * owns its press-time keycode; only publish receipt here, then let the normal
 * QMK processors handle both edges (including Key Override and Tap Dance).
 * Do not populate app_layout_held: that direct-injection ledger is Macropad's,
 * and releasing it on a layout transition would bypass those processors. */
void hid_app_layout_capture_identity(uint32_t *revision, uint8_t *layer) {
    *revision = app_layout_revision;
    *layer = app_layout_current_layer();
}

bool hid_app_layout_process_cached_keyevent(uint8_t row, uint8_t col, bool pressed, uint16_t keycode, uint32_t revision, uint8_t layer) {
    uint8_t control;
    if (pressed && keycode != 0 && app_layout_control_for_position(row, col, &control)) {
        // A buffered press still belongs to its original snapshot. Publish
        // before downstream handlers, which can synchronously change layers.
        app_layout_enqueue_event(revision, layer, control);
    }
    return false; // Never consume the QMK record or inject a second key event.
}
#endif

bool hid_app_layout_get_combo_keycode(uint8_t row, uint8_t col, uint16_t *keycode) {
    if (keycode == NULL) return false;
    uint8_t control;
    if (!app_layout_control_for_position(row, col, &control)) return false;
    if (!app_layout_session_live()) return false;
    uint8_t layer = app_layout_current_layer();
    if (EH_APP_LAYOUT_CONTROL_COUNT == 15 && control == 12 && app_layout_stack_counts[layer] >= 2) {
        *keycode = KC_NO;
        return true;
    }
    return hid_app_layout_get_keycode(layer, control, keycode);
}

bool hid_app_layout_process_encoder_event(uint8_t index, bool clockwise, bool pressed) {
    if (EH_APP_LAYOUT_CONTROL_COUNT != 15 || index != 0) return false;
    return app_layout_dispatch_control(clockwise ? 14 : 13, pressed, true);
}
#ifdef EH_SM62_SPLIT_RUNTIME_ENABLE
#    include "sm62/application_split.inc"
#endif
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
#if defined(KEYBOARD_ergohaven_sm62)
/* Layout has its own freshness: time/media traffic is not language sync.
 * A separate received flag also accepts a genuine report at timer tick zero. */
static uint32_t hid_layout_sync_time;
static bool hid_layout_received;

bool is_hid_layout_valid(void) {
    return hid_layout_received && timer_elapsed32(hid_layout_sync_time) < 15U * 1000U;
}

static bool     sm62_timeout_cleared = true;
#if defined(SM62_DEFER_HID_RELAY)
static void sm62_hid_relay_task(void);
#endif
#endif


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

#if defined(KEYBOARD_ergohaven_sm62)
#    ifdef PROTOCOL_CHIBIOS
#        include "ch.h"
#        define HID_STATE_LOCK() chSysLock()
#        define HID_STATE_UNLOCK() chSysUnlock()
#    else
#        define HID_STATE_LOCK() ((void)0)
#        define HID_STATE_UNLOCK() ((void)0)
#    endif
void hid_housekeeping_task(void) {
#if defined(SM62_DEFER_HID_RELAY)
    sm62_hid_relay_task();
#endif
    /* Check and expire atomically with respect to the serial receiver. The
     * relay above may block; never include transport in this critical section. */
    HID_STATE_LOCK();
    if (is_hid_active()) {
        sm62_timeout_cleared = false;
        HID_STATE_UNLOCK();
        return;
    }

    /* A lost relay must behave like releasing every key.  Otherwise layer 4
     * can remain zoomed forever after Entropy/tester disappears mid-press. */
    if (!sm62_timeout_cleared && hid_data.sm62_pressed_mask) {
        hid_data.sm62_pressed_mask = 0;
        hid_data.sm62_key_event_sequence++;
        hid_data.hid_changed = true;
    }
    sm62_timeout_cleared = true;
    HID_STATE_UNLOCK();
}
#endif


typedef enum {
    _TIME = 0xAA, // random value that does not conflict with VIA, must match companion app
    _VOLUME,
    _LAYOUT,
    _MEDIA_ARTIST,
    _MEDIA_TITLE,
    _DATE = 0xAF,
    _HOST_STATUS = 0xBA,
    _SM62_KEY_EVENT = 0xB0,
    _MEDIA_PROGRESS = 0xB1,
    _SM62_MEDIA_TITLE_CHUNK = 0xBB, /* offset, total UTF-8 bytes, up to 29 bytes */

    _RELAY_FROM_DEVICE = 0xCC,
    _RELAY_TO_DEVICE,
} hid_data_type;

typedef enum {
    _POINTING = 10,
} relay_data_type;

#if defined(KEYBOARD_ergohaven_sm62)
/* SM62 title assembly is atomic: never expose a partial UTF-8 sequence or
 * replace the displayed title until all bytes arrive. Offset zero starts a
 * replacement. Exact duplicate chunks are harmless after a transport retry. */
static char sm62_title_staging[256];
static uint8_t sm62_title_total, sm62_title_received;
#endif

void read_string(uint8_t *data, uint8_t length, char *string_data) {
    /* Both legacy text destinations hold 31 bytes plus NUL. The source is
     * only the logical report, which can be shorter than its backing array. */
    uint8_t data_length = length >= 2 ? MIN(MIN(31, data[1]), length - 2U) : 0;
    if (data_length) memcpy(string_data, data + 2, data_length);
    string_data[data_length] = '\0';
}

bool process_raw_hid_data(uint8_t *data, uint8_t length) {
#if !defined(KEYBOARD_ergohaven_sm62)
    if (length < 3) return false;
#else
    /* Accepted SM62 reports include two-byte volume/layout packets. */
    if (length < 2) return false;
#endif
    uint8_t data_type = data[0];

    bool new_hid_data = false;

    switch (data_type) {
#if defined(KEYBOARD_ergohaven_sm62)
        case _TIME:
            if (length < 3) break;
            hid_data.hours        = data[1];
            hid_data.minutes      = data[2];
            hid_data.time_changed = true;
            new_hid_data          = true;
            break;

#else
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

#endif
        case _HOST_STATUS:
            if (data[1] <= 1) {
                host_status_seen = true;
                host_online = data[1] != 0;
                if (!host_online) volume_received = false;
                host_status_time = timer_read32();
                new_hid_data = true;
            }
            break;

#if defined(KEYBOARD_ergohaven_sm62)
        case _DATE:
            if (length >= 5 && data[1] >= 1 && data[1] <= 31 &&
                data[2] >= 1 && data[2] <= 12) {
                hid_data.date_day = data[1];
                hid_data.date_month = data[2];
                hid_data.date_year = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
                hid_data.date_changed = true;
                new_hid_data = true;
            }
            break;

#else
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
#endif
        case _VOLUME:
            volume_received        = true;
            volume_sync_time       = timer_read32();
            hid_data.volume         = data[1];
            hid_data.volume_changed = true;
            new_hid_data            = true;
            break;

#if defined(KEYBOARD_ergohaven_sm62)
        case _LAYOUT:
            // Explicit companion shutdown. Other invalid values remain ignored.
            if (length >= 2 && data[1] == 255U) {
                hid_layout_received = false;
                hid_data.layout_changed = true;
                new_hid_data = true;
            } else if (length >= 2 && data[1] <= 1U) {
                hid_data.layout         = data[1];
                hid_data.layout_changed = true;
                hid_layout_sync_time    = timer_read32();
                hid_layout_received     = true;
                new_hid_data            = true;
            }
            break;

#else
        case _LAYOUT:
            hid_data.layout         = data[1];
            hid_data.layout_changed = true;
            new_hid_data            = true;
            break;

#endif
        case _MEDIA_ARTIST:
            read_string(data, length, hid_data.media_artist);
            hid_data.media_artist_changed = true;
            new_hid_data                  = true;
            break;

        case _MEDIA_TITLE:
#if defined(KEYBOARD_ergohaven_sm62)
            sm62_title_total = sm62_title_received = 0;
            {
                uint8_t bytes = MIN(MIN(30, data[1]), length - 2);
                memcpy(hid_data.media_title, data + 2, bytes);
                hid_data.media_title[bytes] = '\0';
            }
#else
            read_string(data, length, hid_data.media_title);
#endif
            hid_data.media_title_changed = true;
            new_hid_data                 = true;
            break;

#if defined(KEYBOARD_ergohaven_sm62)
        case _SM62_MEDIA_TITLE_CHUNK: {
            if (length < 3) break;
            uint8_t offset = data[1], total = data[2];
            if (!total || offset >= total) break;
            uint8_t bytes = MIN(29, total - offset);
            if (length < 3 + bytes) break;
            if (!offset) {
                sm62_title_total = total;
                sm62_title_received = 0;
            }
            if (total != sm62_title_total) break;
            if (offset < sm62_title_received) {
                /* A retry of a previously accepted chunk must not abort the
                 * following chunks, nor cause a partial title publication. */
                if (offset + bytes <= sm62_title_received &&
                    memcmp(sm62_title_staging + offset, data + 3, bytes) == 0) new_hid_data = true;
                break;
            }
            if (offset != sm62_title_received) break;
            memcpy(sm62_title_staging + offset, data + 3, bytes);
            sm62_title_received += bytes;
            if (sm62_title_received == total) {
                sm62_title_staging[total] = '\0';
                memcpy(hid_data.media_title, sm62_title_staging, total + 1U);
                hid_data.media_title_changed = true;
            }
            new_hid_data = true;
            break;
        }
#endif

#if defined(KEYBOARD_ergohaven_sm62)
        /* Companion -> M4CR0Pad v3, 32-byte Raw HID report:
         * [0] 0xB0, [1] SM62 row (0..4), [2] column (0..6),
         * [3] pressed (0/1), remaining bytes reserved and zero.
         * A 35-bit held-key mask preserves chords while row/col identify the
         * most recent press used as the zoom-screen centre. */
        case _SM62_KEY_EVENT:
            if (length >= 4 && data[1] < 5 && data[2] < 7) {
                uint8_t  bit_index = data[1] * 7U + data[2];
                uint64_t bit       = UINT64_C(1) << bit_index;
                if (data[3]) {
                    hid_data.sm62_pressed_mask |= bit;
                    hid_data.sm62_last_row = data[1];
                    hid_data.sm62_last_col = data[2];
                } else {
                    hid_data.sm62_pressed_mask &= ~bit;
                }
                hid_data.sm62_key_event_sequence++;
                new_hid_data = true;
            }
            break;

        /* Host -> SM62: B1, position seconds LE32, duration seconds LE32.
         * A zero duration explicitly clears the timeline. */
        case _MEDIA_PROGRESS:
            if (length >= 9) {
                hid_data.media_position_seconds = (uint32_t)data[1] | ((uint32_t)data[2] << 8) |
                                                  ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);
                hid_data.media_duration_seconds = (uint32_t)data[5] | ((uint32_t)data[6] << 8) |
                                                  ((uint32_t)data[7] << 16) | ((uint32_t)data[8] << 24);
                hid_data.media_progress_valid = hid_data.media_duration_seconds != 0;
                hid_data.media_progress_time = timer_read32();
                new_hid_data = true;
            }
            break;


#endif
        case _RELAY_TO_DEVICE:
            if (length < 3) break;
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
#if defined(KEYBOARD_ergohaven_sm62)
        sm62_timeout_cleared = false;
#endif
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
#    if defined(SM62_DEFER_HID_RELAY)
#        include "split_util.h"
#        include "keyboards/ergohaven/sm62/sm62_settings.h"
#    endif

#    if defined(SM62_DEFER_HID_RELAY)
/* A synchronous split RPC inside the USB Raw HID callback holds the host
 * report open while the other half may be flushing an entire display frame.
 * Retain the latest state per data type and deliver it from housekeeping.
 * Key transitions are retained separately per physical key, so a transient
 * split stall cannot leave a released key held on the other half. */
#        define SM62_RELAY_TYPES 8
static uint8_t sm62_relay_data[SM62_RELAY_TYPES][32];
static uint8_t sm62_relay_length[SM62_RELAY_TYPES];
static uint8_t sm62_relay_pending;
static uint8_t sm62_relay_cursor;
/* Cache a complete title, not just the latest chunk: housekeeping can be
 * stalled for the duration of an entire host transfer. Slot 4 remains the
 * title slot for mode gating and replay. */
static char sm62_relay_title[256];
static uint8_t sm62_relay_title_total, sm62_relay_title_offset;
static uint64_t sm62_key_pending;
static uint64_t sm62_key_pressed;
static uint32_t sm62_relay_last_attempt;
static bool sm62_relay_failed;
static uint8_t sm62_relay_remote_mode = UINT8_MAX;

static uint8_t sm62_remote_display_mode(void) {
    return sm62_remote_display_settings()->mode;
}

static bool sm62_relay_needed(uint8_t mode, uint8_t slot) {
    if (slot == 2 || slot == 7) return true; // layout and device relay
    if (mode == 0) return slot == 0 || slot == 5; // clock: time and date
    if (mode == 1) return slot == 3 || slot == 4 || slot == 6; // player
    return false; // ship and keymap use no other host data
}

static int8_t sm62_relay_slot(uint8_t type) {
    if (type >= _TIME && type <= _DATE) return type - _TIME;
    if (type == _MEDIA_PROGRESS) return 6;
    if (type == _RELAY_TO_DEVICE) return 7;
    return -1;
}

static void sm62_hid_relay_queue(const uint8_t *data, uint8_t length) {
    if (!length) return;
    if (length > sizeof(sm62_relay_data[0])) length = sizeof(sm62_relay_data[0]);
    if (data[0] == _SM62_KEY_EVENT && length >= 4 && data[1] < 5 && data[2] < 7) {
        uint64_t bit = UINT64_C(1) << (data[1] * 7U + data[2]);
        if (data[3]) sm62_key_pressed |= bit;
        else sm62_key_pressed &= ~bit;
        sm62_key_pending |= bit;
        return;
    }
    if (data[0] == _SM62_MEDIA_TITLE_CHUNK) {
        /* Only a completed, accepted last chunk may replace the relay's
         * immutable snapshot. Intermediate chunks are never coalesced. */
        if (length < 3 || !data[2] || sm62_title_received != data[2] ||
            data[1] + MIN(29, data[2] - data[1]) != data[2]) return;
        sm62_relay_title_total = data[2];
        sm62_relay_title_offset = 0;
        memcpy(sm62_relay_title, hid_data.media_title, data[2] + 1U);
        sm62_relay_data[4][0] = _SM62_MEDIA_TITLE_CHUNK;
        sm62_relay_length[4] = 3;
        sm62_relay_pending |= 1U << 4;
        return;
    }
    int8_t slot = sm62_relay_slot(data[0]);
    if (slot < 0) return;
    memcpy(sm62_relay_data[slot], data, length);
    sm62_relay_length[slot] = length;
    sm62_relay_pending |= (uint8_t)(1U << slot);
}

static void sm62_hid_relay_task(void) {
    if (!is_keyboard_master()) return;
    /* Never turn an expired queued layout into a fresh slave synchronization,
     * including cached replay after a remote mode change. */
    if (!is_hid_layout_valid() && !(sm62_relay_length[2] >= 2 && sm62_relay_data[2][1] == 255U)) {
        sm62_relay_pending &= (uint8_t)~(1U << 2);
        sm62_relay_length[2] = 0;
    }
    uint8_t remote_mode = sm62_remote_display_mode();
    if (sm62_relay_remote_mode != remote_mode) {
        /* A mode can make previously suppressed data relevant again. Replay
         * the newest cached values, never the stale per-frame history. */
        for (uint8_t slot = 0; slot < SM62_RELAY_TYPES; ++slot) {
            /* Layout is always forwarded, independent of mode. Replaying an
             * already delivered value would falsely renew slave freshness. */
            if (slot == 2) continue;
            if (sm62_relay_length[slot] && sm62_relay_needed(remote_mode, slot)) {
                sm62_relay_pending |= (uint8_t)(1U << slot);
            }
        }
        sm62_relay_title_offset = 0;
        sm62_relay_remote_mode = remote_mode;
    }
    if (!sm62_key_pending && !sm62_relay_pending) return;
    if (!is_transport_connected()) return;
    uint32_t interval = sm62_relay_failed ? 50U : 10U;
    if (sm62_relay_last_attempt && timer_elapsed32(sm62_relay_last_attempt) < interval) return;
    sm62_relay_last_attempt = timer_read32();

    if (sm62_key_pending) {
        uint8_t index = (uint8_t)__builtin_ctzll(sm62_key_pending);
        uint64_t bit = UINT64_C(1) << index;
        uint8_t packet[32] = {_SM62_KEY_EVENT, index / 7U, index % 7U,
                              (sm62_key_pressed & bit) != 0};
        sm62_relay_failed = !transaction_rpc_send(RPC_SYNC_HID, sizeof(packet), packet);
        if (!sm62_relay_failed) sm62_key_pending &= ~bit;
        return;
    }

    for (uint8_t offset = 0; offset < SM62_RELAY_TYPES; ++offset) {
        uint8_t slot = (sm62_relay_cursor + offset) % SM62_RELAY_TYPES;
        uint8_t bit = (uint8_t)(1U << slot);
        if (!(sm62_relay_pending & bit)) continue;
        /* Do not block the USB master on synchronous split RPCs for data
         * the remote view cannot display. In particular, returning from
         * player to clock must not keep forwarding media frames. */
        if (!sm62_relay_needed(remote_mode, slot)) continue;
        if (slot == 4 && sm62_relay_data[slot][0] == _SM62_MEDIA_TITLE_CHUNK) {
            uint8_t bytes = MIN(29, sm62_relay_title_total - sm62_relay_title_offset);
            uint8_t packet[32] = {_SM62_MEDIA_TITLE_CHUNK, sm62_relay_title_offset, sm62_relay_title_total};
            memcpy(packet + 3, sm62_relay_title + sm62_relay_title_offset, bytes);
            sm62_relay_failed = !transaction_rpc_send(RPC_SYNC_HID, sizeof(packet), packet);
            if (!sm62_relay_failed) {
                sm62_relay_title_offset += bytes;
                if (sm62_relay_title_offset == sm62_relay_title_total) sm62_relay_pending &= ~bit;
                sm62_relay_cursor = (slot + 1U) % SM62_RELAY_TYPES;
            }
            return;
        }
        sm62_relay_failed = !transaction_rpc_send(RPC_SYNC_HID,
                                                  sm62_relay_length[slot],
                                                  sm62_relay_data[slot]);
        if (!sm62_relay_failed) {
            sm62_relay_pending &= ~bit;
            sm62_relay_cursor = (slot + 1U) % SM62_RELAY_TYPES;
        }
        return;
    }
}
#    endif

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
#ifdef EH_APP_LAYOUT_ENABLE
    bool app_layout_event_poll = length > 0 && (data[0] == EH_APP_LAYOUT_EVENT_POLL
#if defined(KEYBOARD_ergohaven_sm62)
        || data[0] == 0xEB || data[0] == 0xEF
#endif
    );
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
#if defined(KEYBOARD_ergohaven_sm62)
    if (sm62_diag_raw(data, length)) return;
#endif
    if (process_via_custom_lighting(data, length)) {
        return;
    }

    bool res = process_raw_hid_data(data, length);
    if (res && is_keyboard_master()) {
#    if defined(SM62_DEFER_HID_RELAY)
        sm62_hid_relay_queue(data, length);
#    else
        transaction_rpc_send(RPC_SYNC_HID, length, data);
#    endif
    }
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
    bool app_layout_event_poll = length > 0 && (data[0] == EH_APP_LAYOUT_EVENT_POLL
#if defined(KEYBOARD_ergohaven_sm62)
        || data[0] == 0xEB || data[0] == 0xEF
#endif
    );
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
#if defined(KEYBOARD_ergohaven_sm62)
    if (sm62_diag_raw(data, length)) return;
#endif
    if (process_via_custom_lighting(data, length)) {
        return;
    }

    bool res = process_raw_hid_data(data, length);
    if (res) *((uint64_t *)data) = VIAL_HID_MAGIC;
}

void keyboard_post_init_hid(void) {}

#endif
