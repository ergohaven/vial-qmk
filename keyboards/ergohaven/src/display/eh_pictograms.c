#include "eh_pictograms.h"

#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"
#include "quantum.h"
#include "via.h"

#define EH_PICTOGRAM_FLASH_OFFSET 0x000E0000u
#define EH_PICTOGRAM_FLASH_SIZE 0x00020000u
#define EH_PICTOGRAM_MAGIC 0x49504845u
#define EH_PICTOGRAM_FORMAT_VERSION 4u
#define EH_PICTOGRAM_HEADER_SIZE FLASH_PAGE_SIZE
#define EH_PICTOGRAM_VALID_BYTES 32u
#define EH_PICTOGRAM_RECORD_METADATA_SIZE 4u
#define EH_PICTOGRAM_RECORD_SIZE (EH_PICTOGRAM_RECORD_METADATA_SIZE + EH_PICTOGRAM_BYTES)
#define EH_PICTOGRAM_PAYLOAD_SIZE \
    ((EH_PICTOGRAM_MACRO_SLOTS + EH_PICTOGRAM_TAP_DANCE_SLOTS) * EH_PICTOGRAM_RECORD_SIZE)
#define EH_PICTOGRAM_PACKAGE_SIZE (EH_PICTOGRAM_HEADER_SIZE + EH_PICTOGRAM_PAYLOAD_SIZE)

#define EH_PICTOGRAM_CMD_QUERY 0xC0
#define EH_PICTOGRAM_CMD_READ 0xC1
#define EH_PICTOGRAM_CMD_BEGIN 0xC2
#define EH_PICTOGRAM_CMD_DATA 0xC3
#define EH_PICTOGRAM_CMD_COMMIT 0xC4
#define EH_PICTOGRAM_CMD_CLEAR 0xC5
#define EH_PICTOGRAM_CMD_DATA_STREAM 0xC6
#define EH_PICTOGRAM_CMD_SLOT_BEGIN 0xC7
#define EH_PICTOGRAM_CMD_SLOT_DATA 0xC8
#define EH_PICTOGRAM_CMD_SLOT_COMMIT 0xC9
#define EH_PICTOGRAM_CMD_VALID_READ 0xCA
#define EH_PICTOGRAM_CMD_SLOT_READ 0xCB

enum {
    EH_PICTOGRAM_STATUS_OK = 0,
    EH_PICTOGRAM_STATUS_BAD_COMMAND = 1,
    EH_PICTOGRAM_STATUS_BAD_SIZE = 2,
    EH_PICTOGRAM_STATUS_BAD_SEQUENCE = 3,
    EH_PICTOGRAM_STATUS_FLASH_ERROR = 4,
    EH_PICTOGRAM_STATUS_BAD_CRC = 5,
    EH_PICTOGRAM_STATUS_BAD_FORMAT = 6,
    EH_PICTOGRAM_STATUS_NOT_UPLOADING = 7,
    EH_PICTOGRAM_STATUS_BAD_OFFSET = 8,
};

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t width;
    uint8_t height;
    uint8_t bytes_per_icon;
    uint32_t total_size;
    uint32_t data_crc32;
    uint32_t header_crc32;
    uint8_t macro_valid[EH_PICTOGRAM_VALID_BYTES];
    uint8_t tap_dance_valid[EH_PICTOGRAM_VALID_BYTES];
    uint8_t reserved[172];
} eh_pictogram_header_t;

_Static_assert(sizeof(eh_pictogram_header_t) == EH_PICTOGRAM_HEADER_SIZE, "pictogram header must fill one flash page");
_Static_assert(EH_PICTOGRAM_PACKAGE_SIZE <= EH_PICTOGRAM_FLASH_SIZE, "pictogram package exceeds reserved flash");
_Static_assert(EH_PICTOGRAM_FLASH_OFFSET >= 0x000A0000u, "pictograms overlap firmware image");
_Static_assert(EH_PICTOGRAM_FLASH_OFFSET + EH_PICTOGRAM_FLASH_SIZE <= 0x00100000u,
               "pictograms overlap standby background");

static const eh_pictogram_header_t *const stored_header =
    (const eh_pictogram_header_t *)(XIP_BASE + EH_PICTOGRAM_FLASH_OFFSET);
static const uint8_t *const stored_payload =
    (const uint8_t *)(XIP_BASE + EH_PICTOGRAM_FLASH_OFFSET + EH_PICTOGRAM_HEADER_SIZE);

static bool valid;
static uint32_t generation;
static bool upload_active;
static uint32_t upload_received;
static uint32_t upload_expected_crc;
static uint32_t upload_crc;
static uint16_t upload_next_sequence;
static uint32_t upload_page_offset;
static uint16_t upload_page_used;
static uint8_t upload_header[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t upload_page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static bool slot_upload_active;
static uint8_t slot_upload_kind;
static uint16_t slot_upload_slot;
static bool slot_upload_present;
static uint16_t slot_upload_received;
static uint16_t slot_upload_next_sequence;
static uint32_t slot_upload_expected_crc;
static uint8_t slot_upload_record[EH_PICTOGRAM_RECORD_SIZE];
static uint8_t slot_sector_buffer[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));

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
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return crc;
}

static uint32_t crc32(const uint8_t *data, uint32_t length) {
    return crc32_update(0xFFFFFFFFu, data, length) ^ 0xFFFFFFFFu;
}

static uint32_t header_crc(const uint8_t *header) {
    uint8_t copy[EH_PICTOGRAM_HEADER_SIZE];
    memcpy(copy, header, sizeof(copy));
    memset(copy + offsetof(eh_pictogram_header_t, header_crc32), 0, sizeof(uint32_t));
    return crc32(copy, sizeof(copy));
}

static bool header_is_valid(const eh_pictogram_header_t *header) {
    bool current = header->version == 4 && header->width == 35 && header->height == 35 &&
        header->bytes_per_icon == EH_PICTOGRAM_BYTES && header->total_size == EH_PICTOGRAM_PACKAGE_SIZE;
    bool legacy = header->version == 3 && header->width == 32 && header->height == 32 &&
        header->bytes_per_icon == 128 && header->total_size == 256 + 512 * 132;
    return header->magic == EH_PICTOGRAM_MAGIC && (current || legacy) &&
        header_crc((const uint8_t *)header) == header->header_crc32;
}
static bool storage_is_valid(void) {
    return header_is_valid(stored_header) && crc32(stored_payload, stored_header->total_size - EH_PICTOGRAM_HEADER_SIZE) == stored_header->data_crc32;
}
static uint16_t stored_record_size(void) { return stored_header->bytes_per_icon + EH_PICTOGRAM_RECORD_METADATA_SIZE; }
uint8_t eh_pictogram_stored_width(void) { return valid ? stored_header->width : EH_PICTOGRAM_WIDTH; }

static void flash_erase(uint32_t relative_offset) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(EH_PICTOGRAM_FLASH_OFFSET + relative_offset, FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
}

static void flash_program(uint32_t relative_offset, const uint8_t *page) {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(EH_PICTOGRAM_FLASH_OFFSET + relative_offset, page, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);
}

static bool flash_program_sector(uint32_t relative_offset, const uint8_t *sector) {
    flash_erase(relative_offset);
    for (uint32_t page = 0; page < FLASH_SECTOR_SIZE; page += FLASH_PAGE_SIZE) {
        flash_program(relative_offset + page, sector + page);
    }
    /* Verify intended bytes, not a CRC computed from possibly failed writes. */
    return memcmp((const void *)(XIP_BASE + EH_PICTOGRAM_FLASH_OFFSET + relative_offset), sector, FLASH_SECTOR_SIZE) == 0;
}

/* Slot updates retain the v3/v4 package and wire layout. The unused tail of
 * this reservation holds an undo transaction: one descriptor sector and up
 * to three old sectors (header plus a record that straddles two sectors).
 * Backups are verified before publishing the descriptor, and no live sector
 * is touched before that descriptor is verified. Boot rolls back an armed
 * transaction; recovery is idempotent even if power fails during recovery.
 */
#define SLOT_JOURNAL_OFFSET 0x0001C000u
#define SLOT_JOURNAL_MAGIC 0x55494845u
#define SLOT_JOURNAL_SECTORS 3u
typedef struct {
    uint32_t magic, count;
    uint32_t offsets[SLOT_JOURNAL_SECTORS];
    uint32_t checksums[SLOT_JOURNAL_SECTORS];
    uint8_t reserved[220];
    uint32_t checksum;
} slot_journal_t;
_Static_assert(sizeof(slot_journal_t) == FLASH_PAGE_SIZE, "slot journal page size");
_Static_assert(((EH_PICTOGRAM_PACKAGE_SIZE + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE <= SLOT_JOURNAL_OFFSET, "slot journal overlaps assets");
_Static_assert(SLOT_JOURNAL_OFFSET + (1 + SLOT_JOURNAL_SECTORS) * FLASH_SECTOR_SIZE <= EH_PICTOGRAM_FLASH_SIZE, "slot journal exceeds reservation");
_Static_assert(EH_PICTOGRAM_FLASH_OFFSET + EH_PICTOGRAM_FLASH_SIZE <= PICO_FLASH_SIZE_BYTES, "pictograms exceed chip");

static const uint8_t *flash_data(uint32_t offset) {
    return (const uint8_t *)(XIP_BASE + EH_PICTOGRAM_FLASH_OFFSET + offset);
}

static bool slot_journal_armed(const slot_journal_t *journal) {
    if (journal->magic != SLOT_JOURNAL_MAGIC || journal->count < 1 || journal->count > SLOT_JOURNAL_SECTORS ||
        journal->checksum != crc32((const uint8_t *)journal, offsetof(slot_journal_t, checksum))) return false;
    for (uint32_t i = 0; i < journal->count; ++i) {
        if (journal->offsets[i] % FLASH_SECTOR_SIZE || journal->offsets[i] >= EH_PICTOGRAM_PACKAGE_SIZE) return false;
    }
    return true;
}

static bool slot_journal_recover(void) {
    const slot_journal_t *journal = (const slot_journal_t *)flash_data(SLOT_JOURNAL_OFFSET);
    if (!slot_journal_armed(journal)) {
        /* A torn prepare/disarm is harmless when the live package is valid.
         * Otherwise only an erased descriptor means no pending transaction;
         * corrupt evidence must not authorize destructive empty initialization.
         */
        const uint8_t *descriptor = (const uint8_t *)journal;
        for (uint32_t i = 0; i < sizeof(*journal); ++i) {
            if (descriptor[i] != 0xFF) return storage_is_valid();
        }
        return true;
    }
    /* Verify every backup before restoring any sector. */
    for (uint32_t i = 0; i < journal->count; ++i) {
        if (crc32(flash_data(SLOT_JOURNAL_OFFSET + (i + 1) * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE) != journal->checksums[i]) return false;
    }
    for (uint32_t i = 0; i < journal->count; ++i) {
        memcpy(slot_sector_buffer, flash_data(SLOT_JOURNAL_OFFSET + (i + 1) * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
        if (!flash_program_sector(journal->offsets[i], slot_sector_buffer)) return false;
    }
    flash_erase(SLOT_JOURNAL_OFFSET);
    return true;
}

static bool slot_journal_begin(uint32_t record_offset) {
    slot_journal_t journal;
    memset(&journal, 0xFF, sizeof(journal));
    journal.magic = SLOT_JOURNAL_MAGIC;
    journal.count = 1;
    journal.offsets[0] = 0;
    uint32_t first = record_offset & ~(FLASH_SECTOR_SIZE - 1u);
    uint32_t last = (record_offset + EH_PICTOGRAM_RECORD_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1u);
    if (first) journal.offsets[journal.count++] = first;
    if (last != first) journal.offsets[journal.count++] = last;
    flash_erase(SLOT_JOURNAL_OFFSET);
    for (uint32_t i = 0; i < journal.count; ++i) {
        memcpy(slot_sector_buffer, flash_data(journal.offsets[i]), FLASH_SECTOR_SIZE);
        journal.checksums[i] = crc32(slot_sector_buffer, FLASH_SECTOR_SIZE);
        uint32_t backup = SLOT_JOURNAL_OFFSET + (i + 1) * FLASH_SECTOR_SIZE;
        if (!flash_program_sector(backup, slot_sector_buffer)) return false;
    }
    journal.checksum = crc32((const uint8_t *)&journal, offsetof(slot_journal_t, checksum));
    flash_program(SLOT_JOURNAL_OFFSET, (const uint8_t *)&journal);
    return memcmp(flash_data(SLOT_JOURNAL_OFFSET), &journal, sizeof(journal)) == 0;
}

static void set_slot_valid(eh_pictogram_header_t *header, uint8_t kind, uint16_t slot, bool present) {
    uint8_t *valid_bits = kind == 0 ? header->macro_valid : header->tap_dance_valid;
    if (present) {
        valid_bits[slot >> 3] |= (uint8_t)(1u << (slot & 7));
    } else {
        valid_bits[slot >> 3] &= (uint8_t)~(1u << (slot & 7));
    }
}

static bool initialize_empty_storage(void) {
    for (uint32_t offset = 0; offset < EH_PICTOGRAM_PACKAGE_SIZE; offset += FLASH_SECTOR_SIZE) {
        flash_erase(offset);
    }
    memset(slot_sector_buffer, 0xFF, sizeof(slot_sector_buffer));
    eh_pictogram_header_t *header = (eh_pictogram_header_t *)slot_sector_buffer;
    header->magic = EH_PICTOGRAM_MAGIC;
    header->version = EH_PICTOGRAM_FORMAT_VERSION;
    header->width = EH_PICTOGRAM_WIDTH;
    header->height = EH_PICTOGRAM_HEIGHT;
    header->bytes_per_icon = EH_PICTOGRAM_BYTES;
    header->total_size = EH_PICTOGRAM_PACKAGE_SIZE;
    memset(header->macro_valid, 0, sizeof(header->macro_valid));
    memset(header->tap_dance_valid, 0, sizeof(header->tap_dance_valid));
    return flash_program_sector(0, slot_sector_buffer);
}

static bool commit_slot_upload(void) {
    if (!slot_journal_recover()) return false;
    valid = storage_is_valid();
    if (!valid && !initialize_empty_storage()) return false;
    uint32_t payload_slot = (slot_upload_kind == 0 ? 0u : EH_PICTOGRAM_MACRO_SLOTS) + slot_upload_slot;
    uint32_t record_offset = EH_PICTOGRAM_HEADER_SIZE + payload_slot * EH_PICTOGRAM_RECORD_SIZE;
    if (!slot_journal_begin(record_offset)) return false;
    uint32_t record_written = 0;
    while (record_written < EH_PICTOGRAM_RECORD_SIZE) {
        uint32_t offset = record_offset + record_written;
        uint32_t sector_offset = offset & ~(FLASH_SECTOR_SIZE - 1u);
        uint32_t inside = offset - sector_offset;
        uint32_t amount = MIN((uint32_t)(EH_PICTOGRAM_RECORD_SIZE - record_written), FLASH_SECTOR_SIZE - inside);
        memcpy(slot_sector_buffer, (const void *)(XIP_BASE + EH_PICTOGRAM_FLASH_OFFSET + sector_offset), FLASH_SECTOR_SIZE);
        if (slot_upload_present) {
            memcpy(slot_sector_buffer + inside, slot_upload_record + record_written, amount);
        } else {
            memset(slot_sector_buffer + inside, 0, amount);
        }
        if (sector_offset == 0) {
            set_slot_valid((eh_pictogram_header_t *)slot_sector_buffer, slot_upload_kind, slot_upload_slot, slot_upload_present);
        }
        if (!flash_program_sector(sector_offset, slot_sector_buffer)) goto rollback;
        record_written += amount;
    }

    memcpy(slot_sector_buffer, (const void *)(XIP_BASE + EH_PICTOGRAM_FLASH_OFFSET), FLASH_SECTOR_SIZE);
    eh_pictogram_header_t *header = (eh_pictogram_header_t *)slot_sector_buffer;
    set_slot_valid(header, slot_upload_kind, slot_upload_slot, slot_upload_present);
    header->data_crc32 = crc32(stored_payload, EH_PICTOGRAM_PAYLOAD_SIZE);
    header->header_crc32 = 0;
    header->header_crc32 = header_crc(slot_sector_buffer);
    if (!flash_program_sector(0, slot_sector_buffer)) goto rollback;

    valid = storage_is_valid();
    if (valid) {
        /* Commit point: the new package is complete before disarming undo. */
        flash_erase(SLOT_JOURNAL_OFFSET);
    } else {
        goto rollback;
    }
    generation++;
    return true;

rollback:
    /* Report the failed request even if undo succeeds. Failed recovery keeps
     * the descriptor/backups armed for another attempt, never disarms them.
     */
    valid = slot_journal_recover() && storage_is_valid();
    generation++;
    return false;
}

void eh_pictograms_init(void) {
    upload_active = slot_upload_active = false;
    valid = slot_journal_recover() && storage_is_valid();
    generation++;
}

uint32_t eh_pictograms_generation(void) {
    return generation;
}

static bool slot_is_valid(const uint8_t *bits, uint16_t slot) {
    return (bits[slot >> 3] & (1u << (slot & 7))) != 0;
}

const uint8_t *eh_pictogram_for_keycode(uint16_t keycode) {
    if (!valid) return NULL;
    if (keycode >= QK_MACRO && keycode <= QK_MACRO_MAX) {
        uint16_t slot = keycode - QK_MACRO;
        if (!slot_is_valid(stored_header->macro_valid, slot)) return NULL;
        return stored_payload + (uint32_t)slot * stored_record_size() + EH_PICTOGRAM_RECORD_METADATA_SIZE;
    }
    if (keycode >= QK_TAP_DANCE && keycode <= QK_TAP_DANCE_MAX) {
        uint16_t slot = keycode - QK_TAP_DANCE;
        if (!slot_is_valid(stored_header->tap_dance_valid, slot)) return NULL;
        return stored_payload + ((uint32_t)EH_PICTOGRAM_MACRO_SLOTS + slot) * stored_record_size() +
               EH_PICTOGRAM_RECORD_METADATA_SIZE;
    }
    return NULL;
}

uint32_t eh_pictogram_color_for_keycode(uint16_t keycode) {
    if (!valid) return 0;
    uint32_t record;
    if (keycode >= QK_MACRO && keycode <= QK_MACRO_MAX) {
        uint16_t slot = keycode - QK_MACRO;
        if (!slot_is_valid(stored_header->macro_valid, slot)) return 0;
        record = (uint32_t)slot * stored_record_size();
    } else if (keycode >= QK_TAP_DANCE && keycode <= QK_TAP_DANCE_MAX) {
        uint16_t slot = keycode - QK_TAP_DANCE;
        if (!slot_is_valid(stored_header->tap_dance_valid, slot)) return 0;
        record = ((uint32_t)EH_PICTOGRAM_MACRO_SLOTS + slot) * stored_record_size();
    } else {
        return 0;
    }
    return ((uint32_t)stored_payload[record] << 16) | ((uint32_t)stored_payload[record + 1] << 8) |
           stored_payload[record + 2];
}

static void start_upload(uint32_t expected_crc) {
    slot_upload_active = false;
    valid = false;
    generation++;
    upload_active = true;
    upload_received = 0;
    upload_expected_crc = expected_crc;
    upload_crc = 0xFFFFFFFFu;
    upload_next_sequence = 0;
    upload_page_offset = FLASH_PAGE_SIZE;
    upload_page_used = 0;
    memset(upload_header, 0xFF, sizeof(upload_header));
    memset(upload_page, 0xFF, sizeof(upload_page));
    flash_erase(SLOT_JOURNAL_OFFSET);
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

bool eh_pictograms_process_hid(uint8_t *data, uint8_t length) {
    if (length != 32 || data[0] < EH_PICTOGRAM_CMD_QUERY || data[0] > EH_PICTOGRAM_CMD_SLOT_READ) return false;
    uint8_t command = data[0];
    uint8_t status = EH_PICTOGRAM_STATUS_OK;

    switch (command) {
        case EH_PICTOGRAM_CMD_QUERY:
            memset(data, 0, length);
            data[0] = command;
            data[1] = EH_PICTOGRAM_STATUS_OK;
            data[2] = valid ? stored_header->version : EH_PICTOGRAM_FORMAT_VERSION;
            data[3] = valid;
            data[4] = eh_pictogram_stored_width();
            data[5] = eh_pictogram_stored_width();
            write_u16(data + 6, valid ? stored_header->bytes_per_icon : EH_PICTOGRAM_BYTES);
            write_u32(data + 8, valid ? stored_header->total_size : EH_PICTOGRAM_PACKAGE_SIZE);
            write_u16(data + 12, EH_PICTOGRAM_MACRO_SLOTS);
            write_u16(data + 14, EH_PICTOGRAM_TAP_DANCE_SLOTS);
            data[16] = 2;
            data[17] = EH_PICTOGRAM_FORMAT_VERSION;
            return true;

        case EH_PICTOGRAM_CMD_READ: {
            uint32_t offset = read_u32(data + 1);
            if (!valid || offset >= stored_header->total_size) {
                status = EH_PICTOGRAM_STATUS_BAD_OFFSET;
                break;
            }
            uint8_t amount = MIN((uint32_t)30, stored_header->total_size - offset);
            const uint8_t *source = (const uint8_t *)(XIP_BASE + EH_PICTOGRAM_FLASH_OFFSET + offset);
            memset(data, 0, length);
            data[0] = command;
            data[1] = EH_PICTOGRAM_STATUS_OK;
            memcpy(data + 2, source, amount);
            return true;
        }

        case EH_PICTOGRAM_CMD_BEGIN:
            if (read_u32(data + 1) != EH_PICTOGRAM_PACKAGE_SIZE) {
                status = EH_PICTOGRAM_STATUS_BAD_SIZE;
            } else {
                start_upload(read_u32(data + 5));
            }
            break;

        case EH_PICTOGRAM_CMD_DATA:
        case EH_PICTOGRAM_CMD_DATA_STREAM: {
            if (!upload_active) {
                status = EH_PICTOGRAM_STATUS_NOT_UPLOADING;
                break;
            }
            uint16_t sequence = read_u16(data + 1);
            if (sequence != upload_next_sequence) {
                status = EH_PICTOGRAM_STATUS_BAD_SEQUENCE;
                break;
            }
            uint32_t remaining = EH_PICTOGRAM_PACKAGE_SIZE - upload_received;
            uint8_t amount = MIN((uint32_t)29, remaining);
            consume_upload_bytes(data + 3, amount);
            upload_next_sequence++;
            break;
        }

        case EH_PICTOGRAM_CMD_COMMIT: {
            if (!upload_active) {
                status = EH_PICTOGRAM_STATUS_NOT_UPLOADING;
                break;
            }
            if (upload_received != EH_PICTOGRAM_PACKAGE_SIZE) {
                status = EH_PICTOGRAM_STATUS_BAD_SIZE;
                break;
            }
            if ((upload_crc ^ 0xFFFFFFFFu) != upload_expected_crc) {
                status = EH_PICTOGRAM_STATUS_BAD_CRC;
                upload_active = false;
                break;
            }
            eh_pictogram_header_t *header = (eh_pictogram_header_t *)upload_header;
            if (!header_is_valid(header) || header->version != EH_PICTOGRAM_FORMAT_VERSION) {
                status = EH_PICTOGRAM_STATUS_BAD_FORMAT;
                upload_active = false;
                break;
            }
            if (upload_page_used > 0) program_upload_page();
            flash_program(0, upload_header);
            upload_active = false;
            valid = storage_is_valid();
            generation++;
            if (!valid) status = EH_PICTOGRAM_STATUS_FLASH_ERROR;
            break;
        }

        case EH_PICTOGRAM_CMD_CLEAR:
            upload_active = false;
            slot_upload_active = false;
            valid = false;
            flash_erase(SLOT_JOURNAL_OFFSET);
            flash_erase(0);
            generation++;
            break;

        case EH_PICTOGRAM_CMD_SLOT_BEGIN:
            if (valid && stored_header->version != EH_PICTOGRAM_FORMAT_VERSION) {
                status = EH_PICTOGRAM_STATUS_BAD_FORMAT; break;
            }
            if (data[1] > 1 || read_u16(data + 2) >= EH_PICTOGRAM_MACRO_SLOTS || data[4] > 1) {
                status = EH_PICTOGRAM_STATUS_BAD_FORMAT;
                break;
            }
            upload_active = false;
            slot_upload_active = true;
            slot_upload_kind = data[1];
            slot_upload_slot = read_u16(data + 2);
            slot_upload_present = data[4] != 0;
            slot_upload_expected_crc = read_u32(data + 5);
            slot_upload_received = 0;
            slot_upload_next_sequence = 0;
            memset(slot_upload_record, 0, sizeof(slot_upload_record));
            break;

        case EH_PICTOGRAM_CMD_SLOT_DATA: {
            if (!slot_upload_active) {
                status = EH_PICTOGRAM_STATUS_NOT_UPLOADING;
                break;
            }
            uint16_t sequence = read_u16(data + 1);
            if (sequence != slot_upload_next_sequence) {
                status = EH_PICTOGRAM_STATUS_BAD_SEQUENCE;
                break;
            }
            uint16_t amount = MIN((uint16_t)29, (uint16_t)(EH_PICTOGRAM_RECORD_SIZE - slot_upload_received));
            memcpy(slot_upload_record + slot_upload_received, data + 3, amount);
            slot_upload_received += amount;
            slot_upload_next_sequence++;
            break;
        }

        case EH_PICTOGRAM_CMD_SLOT_COMMIT:
            if (!slot_upload_active) {
                status = EH_PICTOGRAM_STATUS_NOT_UPLOADING;
            } else if (slot_upload_received != EH_PICTOGRAM_RECORD_SIZE) {
                status = EH_PICTOGRAM_STATUS_BAD_SIZE;
            } else if (crc32(slot_upload_record, sizeof(slot_upload_record)) != slot_upload_expected_crc) {
                status = EH_PICTOGRAM_STATUS_BAD_CRC;
            } else if (!commit_slot_upload()) {
                status = EH_PICTOGRAM_STATUS_FLASH_ERROR;
            }
            slot_upload_active = false;
            break;

        case EH_PICTOGRAM_CMD_VALID_READ: {
            uint8_t kind = data[1];
            uint8_t offset = data[2];
            if (!valid || kind > 1 || offset >= EH_PICTOGRAM_VALID_BYTES) {
                status = EH_PICTOGRAM_STATUS_BAD_OFFSET;
                break;
            }
            const uint8_t *valid_bits = kind == 0 ? stored_header->macro_valid : stored_header->tap_dance_valid;
            uint8_t amount = MIN((uint8_t)30, (uint8_t)(EH_PICTOGRAM_VALID_BYTES - offset));
            memset(data, 0, length);
            data[0] = command;
            data[1] = EH_PICTOGRAM_STATUS_OK;
            memcpy(data + 2, valid_bits + offset, amount);
            return true;
        }

        case EH_PICTOGRAM_CMD_SLOT_READ: {
            uint8_t kind = data[1];
            uint16_t slot = read_u16(data + 2);
            uint16_t offset = read_u16(data + 4);
            const uint8_t *valid_bits = kind == 0 ? stored_header->macro_valid : stored_header->tap_dance_valid;
            if (!valid || kind > 1 || slot >= EH_PICTOGRAM_MACRO_SLOTS || offset >= stored_record_size() ||
                !slot_is_valid(valid_bits, slot)) {
                status = EH_PICTOGRAM_STATUS_BAD_OFFSET;
                break;
            }
            uint32_t payload_slot = (kind == 0 ? 0u : EH_PICTOGRAM_MACRO_SLOTS) + slot;
            const uint8_t *record = stored_payload + payload_slot * stored_record_size();
            uint8_t amount = MIN((uint16_t)30, (uint16_t)(stored_record_size() - offset));
            memset(data, 0, length);
            data[0] = command;
            data[1] = EH_PICTOGRAM_STATUS_OK;
            memcpy(data + 2, record + offset, amount);
            return true;
        }

        default:
            status = EH_PICTOGRAM_STATUS_BAD_COMMAND;
            break;
    }

    memset(data, 0, length);
    data[0] = command;
    data[1] = status;
    write_u16(data + 2, upload_next_sequence);
    write_u32(data + 4, upload_received);
    if (command == EH_PICTOGRAM_CMD_DATA_STREAM) *((uint64_t *)data) = VIAL_HID_MAGIC;
    return true;
}
