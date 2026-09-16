#include "eh_background.h"

#include <stddef.h>
#include <string.h>

#include <lvgl.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"
#include "via.h"

#define EH_BACKGROUND_FLASH_OFFSET 0x100000u
#define EH_BACKGROUND_SECOND_REGION_OFFSET 0x100000u
#define EH_BACKGROUND_LOGICAL_SECOND_OFFSET \
    (FLASH_PAGE_SIZE + EH_BACKGROUND_FIRST_FRAME_COUNT * EH_BACKGROUND_ANIMATION_FRAME_SIZE)
#define EH_BACKGROUND_SPEED_FLASH_OFFSET 0x0FB000u
#define EH_BACKGROUND_SETTINGS_FLASH_OFFSET 0x0FC000u
#define EH_BACKGROUND_MAGIC 0x47424845u
#define EH_BACKGROUND_SPEED_MAGIC 0x44505345u
#define EH_BACKGROUND_FORMAT_VERSION 4u

#define EH_BACKGROUND_CMD_QUERY 0xB0
#define EH_BACKGROUND_CMD_BEGIN 0xB1
#define EH_BACKGROUND_CMD_DATA 0xB2
#define EH_BACKGROUND_CMD_COMMIT 0xB3
#define EH_BACKGROUND_CMD_CLEAR 0xB4
#define EH_BACKGROUND_CMD_DATA_STREAM 0xB5
#define EH_BACKGROUND_CMD_SESSION 0xB6
#define EH_BACKGROUND_CMD_SPEED 0xB7
#define EH_BACKGROUND_CMD_BIND 0xB8
#ifdef EH_FAST_UPLOAD_ENABLE
static uint8_t fast_cookie[8];
#endif
#define EH_BACKGROUND_SESSION_TIMEOUT_MS 60000u

enum {
    EH_BACKGROUND_STATUS_OK = 0,
    EH_BACKGROUND_STATUS_BAD_COMMAND = 1,
    EH_BACKGROUND_STATUS_BAD_SIZE = 2,
    EH_BACKGROUND_STATUS_BAD_SEQUENCE = 3,
    EH_BACKGROUND_STATUS_FLASH_ERROR = 4,
    EH_BACKGROUND_STATUS_BAD_CRC = 5,
    EH_BACKGROUND_STATUS_BAD_FORMAT = 6,
    EH_BACKGROUND_STATUS_NOT_UPLOADING = 7,
    EH_BACKGROUND_STATUS_BAD_VALUE = 8,
    EH_BACKGROUND_STATUS_BUSY = 9,
};

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t kind;
    uint8_t frame_count;
    uint8_t reserved0;
    uint16_t width;
    uint16_t height;
    uint32_t frame_size;
    uint32_t total_size;
    uint32_t data_crc32;
    uint32_t header_crc32;
    uint16_t frame_delays_ms[EH_BACKGROUND_FORMAT_MAX_FRAMES];
    uint8_t reserved[116];
} eh_background_header_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t speed_percent;
    uint16_t speed_percent_inverse;
} eh_background_speed_record_t;

_Static_assert(sizeof(eh_background_header_t) == FLASH_PAGE_SIZE, "background header must fill one flash page");
_Static_assert(EH_BACKGROUND_ANIMATION_FRAME_SIZE % FLASH_PAGE_SIZE == 0, "animation frames must be page-aligned");
_Static_assert(EH_BACKGROUND_LOGICAL_SECOND_OFFSET <= EH_BACKGROUND_SPEED_FLASH_OFFSET,
               "first animation region overlaps speed storage");
_Static_assert(EH_BACKGROUND_SPEED_FLASH_OFFSET + FLASH_SECTOR_SIZE <= EH_BACKGROUND_SETTINGS_FLASH_OFFSET,
               "speed storage overlaps settings");
_Static_assert(EH_BACKGROUND_FLASH_OFFSET + EH_BACKGROUND_SETTINGS_FLASH_OFFSET == WEAR_LEVELING_RP2040_FLASH_BASE,
               "background gap must preserve wear-leveling storage");
// Keep the on-flash/wire header unchanged across 2 MiB and 4 MiB boards.
_Static_assert(EH_BACKGROUND_MAX_FRAMES <= EH_BACKGROUND_FORMAT_MAX_FRAMES, "frame limit exceeds header");
#define EH_BACKGROUND_PHYSICAL_END \
    (EH_BACKGROUND_FLASH_OFFSET + (EH_BACKGROUND_MAX_PACKAGE_SIZE <= EH_BACKGROUND_LOGICAL_SECOND_OFFSET \
        ? EH_BACKGROUND_MAX_PACKAGE_SIZE \
        : EH_BACKGROUND_SECOND_REGION_OFFSET + EH_BACKGROUND_MAX_PACKAGE_SIZE - EH_BACKGROUND_LOGICAL_SECOND_OFFSET))
_Static_assert(EH_BACKGROUND_PHYSICAL_END <= PICO_FLASH_SIZE_BYTES, "background area exceeds physical flash");
_Static_assert(EH_BACKGROUND_FLASH_OFFSET + EH_BACKGROUND_SPEED_FLASH_OFFSET + FLASH_SECTOR_SIZE <= PICO_FLASH_SIZE_BYTES,
               "speed storage exceeds physical flash");


static const eh_background_header_t *const stored_header = (const eh_background_header_t *)(XIP_BASE + EH_BACKGROUND_FLASH_OFFSET);
static const eh_background_speed_record_t *const stored_speed =
    (const eh_background_speed_record_t *)(XIP_BASE + EH_BACKGROUND_FLASH_OFFSET + EH_BACKGROUND_SPEED_FLASH_OFFSET);
static bool valid;
static uint32_t generation;
static bool animation_paused;
static uint32_t animation_paused_at;
static bool config_read_active;
static uint32_t config_read_at;
static uint16_t animation_speed_percent = EH_BACKGROUND_SPEED_DEFAULT_PERCENT;

static bool upload_active;
static uint32_t upload_total;
static uint32_t upload_received;
static uint32_t upload_expected_crc;
static uint32_t upload_crc;
static uint16_t upload_next_sequence;
static uint32_t upload_page_offset;
static uint16_t upload_page_used;
static uint8_t upload_header[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t upload_page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t speed_page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t length) {
    while (length--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc;
}

static uint32_t header_crc(const uint8_t *header) {
    uint32_t crc = crc32_update(0xFFFFFFFFu, header, offsetof(eh_background_header_t, header_crc32));
    const uint8_t zero[4] = {0};
    crc = crc32_update(crc, zero, sizeof(zero));
    crc = crc32_update(crc, header + offsetof(eh_background_header_t, header_crc32) + sizeof(uint32_t),
                       FLASH_PAGE_SIZE - offsetof(eh_background_header_t, header_crc32) - sizeof(uint32_t));
    return crc ^ 0xFFFFFFFFu;
}

static bool header_is_valid(const eh_background_header_t *header) {
    if (header->magic != EH_BACKGROUND_MAGIC || header->version != EH_BACKGROUND_FORMAT_VERSION) return false;
    if (header->kind != EH_BACKGROUND_KIND_IMAGE && header->kind != EH_BACKGROUND_KIND_ANIMATION) return false;
    if (header->frame_count == 0 || header->frame_count > EH_BACKGROUND_MAX_FRAMES) return false;
    if (header->kind == EH_BACKGROUND_KIND_IMAGE && header->frame_count != 1) return false;
    uint32_t expected_frame_size = header->kind == EH_BACKGROUND_KIND_IMAGE ? EH_BACKGROUND_IMAGE_FRAME_SIZE : EH_BACKGROUND_ANIMATION_FRAME_SIZE;
    if (header->width != EH_BACKGROUND_WIDTH || header->height != EH_BACKGROUND_HEIGHT || header->frame_size != expected_frame_size) return false;
    uint32_t expected_size = FLASH_PAGE_SIZE + (uint32_t)header->frame_count * expected_frame_size;
    if (header->total_size != expected_size || expected_size > EH_BACKGROUND_MAX_PACKAGE_SIZE) return false;
    return header_crc((const uint8_t *)header) == header->header_crc32;
}

static void flash_erase(uint32_t relative_offset) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(EH_BACKGROUND_FLASH_OFFSET + relative_offset, FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
}

static void flash_program(uint32_t relative_offset, const uint8_t *page) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(EH_BACKGROUND_FLASH_OFFSET + relative_offset, page, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);
}

static uint32_t logical_to_physical_offset(uint32_t logical_offset) {
    if (logical_offset < EH_BACKGROUND_LOGICAL_SECOND_OFFSET) return logical_offset;
    return EH_BACKGROUND_SECOND_REGION_OFFSET + logical_offset - EH_BACKGROUND_LOGICAL_SECOND_OFFSET;
}

static bool speed_is_valid(uint16_t speed_percent) {
    return speed_percent >= EH_BACKGROUND_SPEED_MIN_PERCENT && speed_percent <= EH_BACKGROUND_SPEED_MAX_PERCENT;
}

static bool speed_record_is_valid(const eh_background_speed_record_t *record) {
    return record->magic == EH_BACKGROUND_SPEED_MAGIC && speed_is_valid(record->speed_percent) &&
           record->speed_percent_inverse == (uint16_t)~record->speed_percent;
}

static void persist_animation_speed(uint16_t speed_percent) {
    memset(speed_page, 0xFF, sizeof(speed_page));
    eh_background_speed_record_t *record = (eh_background_speed_record_t *)speed_page;
    record->magic = EH_BACKGROUND_SPEED_MAGIC;
    record->speed_percent = speed_percent;
    record->speed_percent_inverse = (uint16_t)~speed_percent;
    flash_erase(EH_BACKGROUND_SPEED_FLASH_OFFSET);
    flash_program(EH_BACKGROUND_SPEED_FLASH_OFFSET, speed_page);
    animation_speed_percent = speed_percent;
}

static bool animation_decoder_source(const void *source) {
    if (lv_img_src_get_type(source) != LV_IMG_SRC_VARIABLE) return false;
    const lv_img_dsc_t *image = source;
    return image->header.cf == LV_IMG_CF_RAW_ALPHA && image->header.w == EH_BACKGROUND_WIDTH &&
           image->header.h == EH_BACKGROUND_HEIGHT && image->data_size == EH_BACKGROUND_ANIMATION_FRAME_SIZE;
}

static lv_res_t animation_decoder_info(lv_img_decoder_t *decoder, const void *source, lv_img_header_t *header) {
    LV_UNUSED(decoder);
    if (!animation_decoder_source(source)) return LV_RES_INV;
    *header = ((const lv_img_dsc_t *)source)->header;
    return LV_RES_OK;
}

static lv_res_t animation_decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *descriptor) {
    LV_UNUSED(decoder);
    if (!animation_decoder_source(descriptor->src)) return LV_RES_INV;
    descriptor->img_data = NULL;
    descriptor->user_data = (void *)((const lv_img_dsc_t *)descriptor->src)->data;
    return LV_RES_OK;
}

static lv_res_t animation_decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *descriptor, lv_coord_t x,
                                            lv_coord_t y, lv_coord_t length, uint8_t *buffer) {
    LV_UNUSED(decoder);
    const uint8_t *frame = descriptor->user_data;
    if (frame == NULL || x < 0 || y < 0 || length < 0 || x + length > EH_BACKGROUND_WIDTH || y >= EH_BACKGROUND_HEIGHT) {
        return LV_RES_INV;
    }
    for (lv_coord_t offset = 0; offset < length; offset++) {
        uint32_t pixel = (uint32_t)y * EH_BACKGROUND_WIDTH + (uint32_t)(x + offset);
        const uint8_t *packed = frame + EH_BACKGROUND_ANIMATION_PALETTE_SIZE + (pixel / 4u) * 3u;
        uint8_t palette_index;
        switch (pixel & 3u) {
            case 0:
                palette_index = packed[0] >> 2;
                break;
            case 1:
                palette_index = (uint8_t)(((packed[0] & 0x03u) << 4) | (packed[1] >> 4));
                break;
            case 2:
                palette_index = (uint8_t)(((packed[1] & 0x0Fu) << 2) | (packed[2] >> 6));
                break;
            default:
                palette_index = packed[2] & 0x3Fu;
                break;
        }
        const uint8_t *palette = frame + (uint32_t)palette_index * 4u;
        lv_color_t color = lv_color_make(palette[2], palette[1], palette[0]);
        memcpy(buffer, &color, sizeof(color));
        buffer += sizeof(color);
        *buffer++ = palette[3];
    }
    return LV_RES_OK;
}

static void register_animation_decoder(void) {
    lv_img_decoder_t *decoder = lv_img_decoder_create();
    if (decoder == NULL) return;
    lv_img_decoder_set_info_cb(decoder, animation_decoder_info);
    lv_img_decoder_set_open_cb(decoder, animation_decoder_open);
    lv_img_decoder_set_read_line_cb(decoder, animation_decoder_read_line);
}

static void invalidate_background(void) {
    valid = false;
    generation++;
}

void eh_background_init(void) {
    valid = header_is_valid(stored_header);
    animation_speed_percent = speed_record_is_valid(stored_speed) ? stored_speed->speed_percent : EH_BACKGROUND_SPEED_DEFAULT_PERCENT;
    register_animation_decoder();
}

bool eh_background_is_valid(void) {
    return valid;
}

uint8_t eh_background_kind(void) {
    return valid ? stored_header->kind : EH_BACKGROUND_KIND_NONE;
}

uint8_t eh_background_frame_count(void) {
    return valid ? stored_header->frame_count : 0;
}

const uint8_t *eh_background_frame_data(uint8_t frame) {
    if (!valid || frame >= stored_header->frame_count) return NULL;
    uint32_t logical_offset = FLASH_PAGE_SIZE + (uint32_t)frame * stored_header->frame_size;
    uint32_t physical_offset = stored_header->kind == EH_BACKGROUND_KIND_ANIMATION
                                   ? logical_to_physical_offset(logical_offset)
                                   : logical_offset;
    return (const uint8_t *)stored_header + physical_offset;
}

uint16_t eh_background_frame_delay(uint8_t frame) {
    if (!valid || frame >= stored_header->frame_count) return 0;
    return stored_header->frame_delays_ms[frame];
}

uint16_t eh_background_speed_percent(void) {
    return animation_speed_percent;
}

uint32_t eh_background_generation(void) {
    return generation;
}

void eh_background_note_config_read(uint8_t command, uint8_t subcommand) {
    // Only bulk configuration reads pause the backdrop. Live matrix (02/03)
    // and unlock-status (FE/05) polling run every20ms in Vial and must never
    // renew this pause. Host clock/media traffic also leaves it untouched.
    bool bulk_read = command == 0x04 || command == 0x0E || command == 0x12 ||
        (command == 0xFE && (subcommand == 0x02 || subcommand == 0x03 ||
         subcommand == 0x09 || subcommand == 0x0A || subcommand == 0x0D));
    if (bulk_read) {
        config_read_active = true;
        config_read_at = timer_read32();
    }
}

bool eh_background_animation_paused(void) {
    if (animation_paused && timer_elapsed32(animation_paused_at) >= EH_BACKGROUND_SESSION_TIMEOUT_MS) {
        animation_paused = false;
    }
    if (config_read_active && timer_elapsed32(config_read_at) >= 250u) config_read_active = false;
    return animation_paused || config_read_active;
}

static void start_upload(uint32_t total, uint32_t expected_crc) {
    invalidate_background();
    upload_active = true;
    upload_total = total;
    upload_received = 0;
    upload_expected_crc = expected_crc;
    upload_crc = 0xFFFFFFFFu;
    upload_next_sequence = 0;
    upload_page_offset = FLASH_PAGE_SIZE;
    upload_page_used = 0;
    memset(upload_header, 0xFF, sizeof(upload_header));
    memset(upload_page, 0xFF, sizeof(upload_page));
    flash_erase(0);
}

static void program_upload_page(void) {
    uint32_t physical_offset = logical_to_physical_offset(upload_page_offset);
    if (physical_offset != 0 && (physical_offset % FLASH_SECTOR_SIZE) == 0) flash_erase(physical_offset);
    flash_program(physical_offset, upload_page);
    upload_page_offset += FLASH_PAGE_SIZE;
    upload_page_used = 0;
    memset(upload_page, 0xFF, sizeof(upload_page));
}

static void consume_upload_bytes(const uint8_t *bytes, uint8_t length) {
    upload_crc = crc32_update(upload_crc, bytes, length);
    while (length > 0) {
        if (upload_received < FLASH_PAGE_SIZE) {
            uint16_t offset = upload_received;
            uint16_t amount = MIN((uint16_t)length, (uint16_t)(FLASH_PAGE_SIZE - offset));
            memcpy(upload_header + offset, bytes, amount);
            upload_received += amount;
            bytes += amount;
            length -= amount;
            continue;
        }
        uint16_t amount = MIN((uint16_t)length, (uint16_t)(FLASH_PAGE_SIZE - upload_page_used));
        memcpy(upload_page + upload_page_used, bytes, amount);
        upload_page_used += amount;
        upload_received += amount;
        bytes += amount;
        length -= amount;
        if (upload_page_used == FLASH_PAGE_SIZE) program_upload_page();
    }
}

bool eh_background_process_hid(uint8_t *data, uint8_t length) {
    #ifdef EH_FAST_UPLOAD_ENABLE
    if (length != 32 && length != 64) return false;
#else
    if (length != 32) return false;
#endif
    if (data[0] < EH_BACKGROUND_CMD_QUERY || data[0] > EH_BACKGROUND_CMD_BIND) return false;
    uint8_t command = data[0];
    uint8_t status = EH_BACKGROUND_STATUS_OK;

    switch (command) {
        case EH_BACKGROUND_CMD_QUERY:
            memset(data, 0, length);
            data[0] = command;
            data[1] = EH_BACKGROUND_STATUS_OK;
            data[2] = EH_BACKGROUND_FORMAT_VERSION;
            data[3] = eh_background_kind();
            data[4] = eh_background_frame_count();
            write_u32(data + 5, EH_BACKGROUND_MAX_PACKAGE_SIZE);
            write_u32(data + 9, valid ? stored_header->total_size : 0);
            data[13] = EH_BACKGROUND_MAX_FRAMES;
            write_u16(data + 14, animation_speed_percent);
            write_u32(data + 16, valid ? stored_header->header_crc32 : 0);
#ifdef EH_FAST_UPLOAD_ENABLE
            data[20] = 64;
#endif
            return true;

#ifdef EH_FAST_UPLOAD_ENABLE
        case EH_BACKGROUND_CMD_BIND:
            if (length == 32) memcpy(fast_cookie, data + 2, sizeof(fast_cookie));
            memset(data, 0, length);
            data[0] = command;
            memcpy(data + 2, fast_cookie, sizeof(fast_cookie));
            return true;
#endif
        case EH_BACKGROUND_CMD_BEGIN: {
            uint32_t total = read_u32(data + 1);
            uint32_t expected_crc = read_u32(data + 5);
            if (total < FLASH_PAGE_SIZE + EH_BACKGROUND_ANIMATION_FRAME_SIZE || total > EH_BACKGROUND_MAX_PACKAGE_SIZE) {
                status = EH_BACKGROUND_STATUS_BAD_SIZE;
            } else {
                start_upload(total, expected_crc);
            }
            break;
        }

        case EH_BACKGROUND_CMD_DATA:
        case EH_BACKGROUND_CMD_DATA_STREAM: {
            if (!upload_active) {
                status = EH_BACKGROUND_STATUS_NOT_UPLOADING;
                break;
            }
            uint16_t sequence = read_u16(data + 1);
            if (sequence != upload_next_sequence) {
                status = EH_BACKGROUND_STATUS_BAD_SEQUENCE;
                break;
            }
            uint32_t remaining = upload_total - upload_received;
            uint8_t payload = MIN((uint32_t)(length - 3), remaining);
            consume_upload_bytes(data + 3, payload);
            upload_next_sequence++;
            break;
        }

        case EH_BACKGROUND_CMD_COMMIT: {
            if (!upload_active) {
                status = EH_BACKGROUND_STATUS_NOT_UPLOADING;
                break;
            }
            if (upload_received != upload_total) {
                status = EH_BACKGROUND_STATUS_BAD_SIZE;
                break;
            }
            uint32_t actual_crc = upload_crc ^ 0xFFFFFFFFu;
            if (actual_crc != upload_expected_crc) {
                status = EH_BACKGROUND_STATUS_BAD_CRC;
                upload_active = false;
                break;
            }
            eh_background_header_t *header = (eh_background_header_t *)upload_header;
            if (!header_is_valid(header) || header->total_size != upload_total) {
                status = EH_BACKGROUND_STATUS_BAD_FORMAT;
                upload_active = false;
                break;
            }
            if (upload_page_used > 0) program_upload_page();
            flash_program(0, upload_header);
            upload_active = false;
            valid = header_is_valid(stored_header);
            generation++;
            if (!valid) status = EH_BACKGROUND_STATUS_FLASH_ERROR;
            break;
        }

        case EH_BACKGROUND_CMD_CLEAR:
            upload_active = false;
            invalidate_background();
            flash_erase(0);
            break;

        case EH_BACKGROUND_CMD_SESSION:
            animation_paused = data[1] != 0;
            animation_paused_at = timer_read32();
            break;

        case EH_BACKGROUND_CMD_SPEED: {
            uint16_t speed_percent = read_u16(data + 1);
            if (!speed_is_valid(speed_percent)) {
                status = EH_BACKGROUND_STATUS_BAD_VALUE;
            } else if (upload_active) {
                status = EH_BACKGROUND_STATUS_BUSY;
            } else if (speed_percent != animation_speed_percent) {
                persist_animation_speed(speed_percent);
            }
            break;
        }

        default:
            status = EH_BACKGROUND_STATUS_BAD_COMMAND;
            break;
    }

    memset(data, 0, length);
    data[0] = command;
    data[1] = status;
    write_u16(data + 2, upload_next_sequence);
    write_u32(data + 4, upload_received);
    if (command == EH_BACKGROUND_CMD_DATA_STREAM) *((uint64_t *)data) = VIAL_HID_MAGIC;
    return true;
}
