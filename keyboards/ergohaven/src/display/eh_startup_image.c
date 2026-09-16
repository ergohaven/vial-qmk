#include "eh_startup_image.h"

#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"
#include "via.h"

#define EH_STARTUP_IMAGE_FLASH_OFFSET 0x000C0000u
#define EH_STARTUP_IMAGE_FLASH_SIZE 0x00020000u
#define EH_STARTUP_IMAGE_MAGIC 0x49534845u
#define EH_STARTUP_IMAGE_FORMAT_VERSION 1u
#define EH_STARTUP_IMAGE_HEADER_SIZE FLASH_PAGE_SIZE
#define EH_STARTUP_IMAGE_PACKAGE_SIZE (EH_STARTUP_IMAGE_HEADER_SIZE + EH_STARTUP_IMAGE_DATA_SIZE)

#define EH_STARTUP_IMAGE_CMD_QUERY 0xD0
#define EH_STARTUP_IMAGE_CMD_BEGIN 0xD1
#define EH_STARTUP_IMAGE_CMD_DATA 0xD2
#define EH_STARTUP_IMAGE_CMD_COMMIT 0xD3
#define EH_STARTUP_IMAGE_CMD_CLEAR 0xD4
#define EH_STARTUP_IMAGE_CMD_DATA_STREAM 0xD5
#define EH_STARTUP_IMAGE_CMD_PREVIEW 0xD6
#define EH_STARTUP_IMAGE_PREVIEW_WIDTH 60u
#define EH_STARTUP_IMAGE_PREVIEW_HEIGHT 70u
#define EH_STARTUP_IMAGE_PREVIEW_SIZE (EH_STARTUP_IMAGE_PREVIEW_WIDTH * EH_STARTUP_IMAGE_PREVIEW_HEIGHT)

enum {
    EH_STARTUP_IMAGE_STATUS_OK = 0,
    EH_STARTUP_IMAGE_STATUS_BAD_COMMAND = 1,
    EH_STARTUP_IMAGE_STATUS_BAD_SIZE = 2,
    EH_STARTUP_IMAGE_STATUS_BAD_SEQUENCE = 3,
    EH_STARTUP_IMAGE_STATUS_FLASH_ERROR = 4,
    EH_STARTUP_IMAGE_STATUS_BAD_CRC = 5,
    EH_STARTUP_IMAGE_STATUS_BAD_FORMAT = 6,
    EH_STARTUP_IMAGE_STATUS_NOT_UPLOADING = 7,
};

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t reserved0[3];
    uint16_t width;
    uint16_t height;
    uint32_t data_size;
    uint32_t total_size;
    uint32_t data_crc32;
    uint32_t header_crc32;
    uint8_t reserved[228];
} eh_startup_image_header_t;

_Static_assert(sizeof(eh_startup_image_header_t) == EH_STARTUP_IMAGE_HEADER_SIZE,
               "startup image header must fill one flash page");
_Static_assert(EH_STARTUP_IMAGE_PACKAGE_SIZE <= EH_STARTUP_IMAGE_FLASH_SIZE,
               "startup image exceeds reserved flash");
_Static_assert(EH_STARTUP_IMAGE_FLASH_OFFSET >= 0x000A0000u,
               "startup image overlaps firmware image");
_Static_assert(EH_STARTUP_IMAGE_FLASH_OFFSET + EH_STARTUP_IMAGE_FLASH_SIZE <= 0x000E0000u,
               "startup image overlaps pictograms");

static const eh_startup_image_header_t *const stored_header =
    (const eh_startup_image_header_t *)(XIP_BASE + EH_STARTUP_IMAGE_FLASH_OFFSET);
static const uint8_t *const stored_data =
    (const uint8_t *)(XIP_BASE + EH_STARTUP_IMAGE_FLASH_OFFSET + EH_STARTUP_IMAGE_HEADER_SIZE);

static bool valid;
static bool upload_active;
static uint32_t upload_received;
static uint32_t upload_expected_crc;
static uint32_t upload_crc;
static uint16_t upload_next_sequence;
static uint32_t upload_page_offset;
static uint16_t upload_page_used;
static uint8_t upload_header[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t upload_page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

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

static uint32_t crc32(const uint8_t *data, uint32_t length) {
    return crc32_update(0xFFFFFFFFu, data, length) ^ 0xFFFFFFFFu;
}

static uint32_t header_crc(const uint8_t *header) {
    uint32_t crc = crc32_update(0xFFFFFFFFu, header, offsetof(eh_startup_image_header_t, header_crc32));
    const uint8_t zero[4] = {0};
    crc = crc32_update(crc, zero, sizeof(zero));
    crc = crc32_update(crc, header + offsetof(eh_startup_image_header_t, header_crc32) + sizeof(uint32_t),
                       EH_STARTUP_IMAGE_HEADER_SIZE - offsetof(eh_startup_image_header_t, header_crc32) - sizeof(uint32_t));
    return crc ^ 0xFFFFFFFFu;
}

static bool header_is_valid(const eh_startup_image_header_t *header) {
    return header->magic == EH_STARTUP_IMAGE_MAGIC && header->version == EH_STARTUP_IMAGE_FORMAT_VERSION &&
           header->width == EH_STARTUP_IMAGE_WIDTH && header->height == EH_STARTUP_IMAGE_HEIGHT &&
           header->data_size == EH_STARTUP_IMAGE_DATA_SIZE && header->total_size == EH_STARTUP_IMAGE_PACKAGE_SIZE &&
           header_crc((const uint8_t *)header) == header->header_crc32;
}

static void flash_erase(uint32_t relative_offset) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(EH_STARTUP_IMAGE_FLASH_OFFSET + relative_offset, FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
}

static void flash_program(uint32_t relative_offset, const uint8_t *page) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(EH_STARTUP_IMAGE_FLASH_OFFSET + relative_offset, page, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);
}

void eh_startup_image_init(void) {
    valid = header_is_valid(stored_header) && crc32(stored_data, EH_STARTUP_IMAGE_DATA_SIZE) == stored_header->data_crc32;
}

bool eh_startup_image_is_valid(void) {
    return valid;
}

const uint8_t *eh_startup_image_data(void) {
    return valid ? stored_data : NULL;
}

static void start_upload(uint32_t expected_crc) {
    valid = false;
    upload_active = true;
    upload_received = 0;
    upload_expected_crc = expected_crc;
    upload_crc = 0xFFFFFFFFu;
    upload_next_sequence = 0;
    upload_page_offset = EH_STARTUP_IMAGE_HEADER_SIZE;
    upload_page_used = 0;
    memset(upload_header, 0xFF, sizeof(upload_header));
    memset(upload_page, 0xFF, sizeof(upload_page));
    flash_erase(0);
}

static void program_upload_page(void) {
    if (upload_page_offset != 0 && (upload_page_offset % FLASH_SECTOR_SIZE) == 0) flash_erase(upload_page_offset);
    flash_program(upload_page_offset, upload_page);
    upload_page_offset += FLASH_PAGE_SIZE;
    upload_page_used = 0;
    memset(upload_page, 0xFF, sizeof(upload_page));
}

static void consume_upload_bytes(const uint8_t *bytes, uint8_t length) {
    upload_crc = crc32_update(upload_crc, bytes, length);
    while (length > 0) {
        if (upload_received < EH_STARTUP_IMAGE_HEADER_SIZE) {
            uint16_t offset = upload_received;
            uint16_t amount = MIN((uint16_t)length, (uint16_t)(EH_STARTUP_IMAGE_HEADER_SIZE - offset));
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

bool eh_startup_image_process_hid(uint8_t *data, uint8_t length) {
    if (length != 32 || data[0] < EH_STARTUP_IMAGE_CMD_QUERY || data[0] > EH_STARTUP_IMAGE_CMD_PREVIEW) return false;
    uint8_t command = data[0];
    uint8_t status = EH_STARTUP_IMAGE_STATUS_OK;

    switch (command) {
        case EH_STARTUP_IMAGE_CMD_QUERY:
            memset(data, 0, length);
            data[0] = command;
            data[1] = EH_STARTUP_IMAGE_STATUS_OK;
            data[2] = EH_STARTUP_IMAGE_FORMAT_VERSION;
            data[3] = valid ? 1 : 0;
            write_u32(data + 4, EH_STARTUP_IMAGE_PACKAGE_SIZE);
            write_u32(data + 8, valid ? EH_STARTUP_IMAGE_PACKAGE_SIZE : 0);
            return true;

        case EH_STARTUP_IMAGE_CMD_PREVIEW: {
            uint32_t offset = read_u32(data + 1);
            if (!valid || offset >= EH_STARTUP_IMAGE_PREVIEW_SIZE) {
                memset(data, 0, length);
                data[0] = command;
                data[1] = valid ? EH_STARTUP_IMAGE_STATUS_BAD_SIZE : EH_STARTUP_IMAGE_STATUS_BAD_FORMAT;
                return true;
            }
            memset(data, 0, length);
            data[0] = command;
            data[1] = EH_STARTUP_IMAGE_STATUS_OK;
            uint8_t count = MIN((uint32_t)(length - 2), EH_STARTUP_IMAGE_PREVIEW_SIZE - offset);
            for (uint8_t i = 0; i < count; ++i) {
                uint32_t preview_pixel = offset + i;
                uint32_t preview_x = preview_pixel % EH_STARTUP_IMAGE_PREVIEW_WIDTH;
                uint32_t preview_y = preview_pixel / EH_STARTUP_IMAGE_PREVIEW_WIDTH;
                uint32_t source_x = preview_x * 4u + 2u;
                uint32_t source_y = preview_y * 4u + 2u;
                data[2 + i] = stored_data[EH_STARTUP_IMAGE_PALETTE_SIZE + source_y * EH_STARTUP_IMAGE_WIDTH + source_x];
            }
            return true;
        }

        case EH_STARTUP_IMAGE_CMD_BEGIN:
            if (read_u32(data + 1) != EH_STARTUP_IMAGE_PACKAGE_SIZE) {
                status = EH_STARTUP_IMAGE_STATUS_BAD_SIZE;
            } else {
                start_upload(read_u32(data + 5));
            }
            break;

        case EH_STARTUP_IMAGE_CMD_DATA:
        case EH_STARTUP_IMAGE_CMD_DATA_STREAM: {
            if (!upload_active) {
                status = EH_STARTUP_IMAGE_STATUS_NOT_UPLOADING;
                break;
            }
            uint16_t sequence = read_u16(data + 1);
            if (sequence != upload_next_sequence) {
                status = EH_STARTUP_IMAGE_STATUS_BAD_SEQUENCE;
                break;
            }
            uint32_t remaining = EH_STARTUP_IMAGE_PACKAGE_SIZE - upload_received;
            uint8_t payload = MIN((uint32_t)29, remaining);
            consume_upload_bytes(data + 3, payload);
            upload_next_sequence++;
            break;
        }

        case EH_STARTUP_IMAGE_CMD_COMMIT: {
            if (!upload_active) {
                status = EH_STARTUP_IMAGE_STATUS_NOT_UPLOADING;
                break;
            }
            if (upload_received != EH_STARTUP_IMAGE_PACKAGE_SIZE) {
                status = EH_STARTUP_IMAGE_STATUS_BAD_SIZE;
                break;
            }
            if ((upload_crc ^ 0xFFFFFFFFu) != upload_expected_crc) {
                status = EH_STARTUP_IMAGE_STATUS_BAD_CRC;
                upload_active = false;
                break;
            }
            eh_startup_image_header_t *header = (eh_startup_image_header_t *)upload_header;
            if (!header_is_valid(header)) {
                status = EH_STARTUP_IMAGE_STATUS_BAD_FORMAT;
                upload_active = false;
                break;
            }
            if (upload_page_used > 0) program_upload_page();
            flash_program(0, upload_header);
            upload_active = false;
            valid = header_is_valid(stored_header) && crc32(stored_data, EH_STARTUP_IMAGE_DATA_SIZE) == stored_header->data_crc32;
            if (!valid) status = EH_STARTUP_IMAGE_STATUS_FLASH_ERROR;
            break;
        }

        case EH_STARTUP_IMAGE_CMD_CLEAR:
            upload_active = false;
            valid = false;
            flash_erase(0);
            break;

        default:
            status = EH_STARTUP_IMAGE_STATUS_BAD_COMMAND;
            break;
    }

    memset(data, 0, length);
    data[0] = command == EH_STARTUP_IMAGE_CMD_DATA_STREAM ? EH_STARTUP_IMAGE_CMD_DATA : command;
    data[1] = status;
    if (command == EH_STARTUP_IMAGE_CMD_DATA || command == EH_STARTUP_IMAGE_CMD_DATA_STREAM) write_u16(data + 2, upload_next_sequence);
    if (command == EH_STARTUP_IMAGE_CMD_DATA_STREAM) *((uint64_t *)data) = VIAL_HID_MAGIC;
    return true;
}
