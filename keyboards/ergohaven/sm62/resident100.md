# SM62 host-managed application layouts and legacy archive

Entropy owns the application-profile library and foreground-application rules. SM62 executes only the current profile in RAM through the existing E1–EA protocol8 publisher. Macropad uses the same ownership/runtime model with its existing geometry/protocol adapter. Native Default remains the normal nonvolatile keyboard layout; EA or the existing 2500 ms session expiry leaves the temporary application profile. No matrix, renderer, split-current, EEPROM or flash partition format changes are involved.

## Legacy archive: preserve, read, never execute or modify

The former resident100 journal is retained exclusively for lossless migration into Entropy. New firmware has no journal erase/program entrypoints, no STORE/DELETE implementation, no resident SELECT, and no stage-only persistent-profile transfer. It neither erases old profiles nor automatically replays them. Hosts must retain a durable complete backup before considering adoption complete; an unreadable/corrupt archive must not be treated as an empty successful migration.

The journal layout is unchanged:

- `[0x200000,0x2ca000)`: 101 physical records of 8192 bytes, 100 logical slots0..99. Slot0 is an application slot, not native Default.
- The older `[0x3f2000,0x3fa000)` region, extension settings and EEPROM remain untouched. This schema2 reader does not reinterpret older unrelated formats.
- Header256 at record offset0: magic `45 43 52 32` (LE `0x32524345`), schema byte4=2, slot5, kind6 (1FULL,2tombstone), zero7, generation:u32@8, payload_length:u16@12, zero14..15, payload CRC32@16, semantic identity20@20, header CRC32@40 over bytes0..39. Remaining header padding is erased0xff.
- FULL at offset256, length3440..4560. Embedded COMMITv2 identity and active1 must agree with the header. Cold scan validates structure and semantic CRC16 through the existing HID parser with its publisher gated off; validation does not activate a profile.
- Tombstones retain deleted identity/CRC and suppress older FULL records. They are read-only historical records now.
- Commit256 at7936: magic `45 43 4d 32` (`0x324d4345`), generation@4, header CRC@8, commit CRC32@12 over its first12 bytes; remaining padding0xff.
- CRC32 is ISO-HDLC polynomial0xedb88320, initial/final xor0xffffffff. It detects corruption, not malicious coordinated forgery.
- A cold scan builds the existing100-byte latest-record index and generation; it does not allocate100 decoded maps or scan flash on keypresses. A scan deferred by an in-flight ordinary profile transfer returns NOT_READY, never an incomplete index.
- Warm QUERY/ENUM/READ/INVENTORY recheck header/commit, payload CRC and embedded identity without semantic replay or staging mutation. Indexed corruption returns BAD_CRC; no implicit repair, write or success acknowledgement occurs.

UF2 code remains below the existing `SM62_CODE_FLASH_LIMIT`; `layout_reserve.ld` and the unchanged reserve addresses protect the archive from ordinary code updates. Do not perform a factory reset or whole-chip erase when migrating.

## Capability negotiation

All packets are32 bytes; nonce is four bytes at4..7 and reserved request bytes8..31 must be zero.

- EBv3 request `[eb,08,a5,03,nonce:u32LE,zeros24]` replies with `[eb,08,5a,03,nonce,...]`.
- Reply8=3 (RAW and LZ),9=0 writable resident slots,10..11=4560LE,12=16 layers,13=62 controls,14=22 name bytes,15=0,16..19=4194304LE physical flash bytes,20..23=1LE (read-only legacy archive),24=2 archive record schema,25=100 archive slot capacity,26=1 host-managed RAM policy,27..31=0.
- EBv2 is intentionally unsupported and returns the exact request echo. Existing hosts validate this correlated unsupported response and fall back to EBv1 RAM streaming instead of attempting the former flash runtime.
- EBv1 `[eb,08,a5,01,nonce32,zeros24]` keeps its existing response `[eb,08,5a,01,nonce32,03,00,d0,11,zeros20]`: RAW/LZ supported, resident count0.
- Malformed/unknown capability requests remain unsupported echoes. Existing outer VIA unlock/settings fences remain effective.

## Streaming runtime

EC byte2 must be1 (ordinary current-profile activation). Byte2=2 (former resident stage-only mode) is rejected. ED/EE codecs, sequence, exact duplicate retry, lengths and CRCs are unchanged. The existing sole publisher applies the complete profile atomically, with normal held-key release, semantic wake, display and split-current handling. No flash archive contents participate in active-profile selection.

## Read-only archive wire

EF09 accepts only0QUERY,3ENUM_SLOT,6READ and7INVENTORY. Former1STORE_READY,2SELECT,4DELETE and5QUERY_READY return BAD_ARGUMENT(2) without activating a layout, resetting its session or touching flash. Unsupported operations do not become successful no-ops.

- QUERY/ENUM use the unchanged nonce/identity/CRC correlation. ENUM requires zero query payload and returns the stored20-byte identity and payload CRC.
- READ retains bounded20-byte chunks, offset and length validation, nonce correlation and explicit status. Errors return zero actual bytes and zero data padding.
- INVENTORY request `[ef,09,07,00,nonce:u32LE,zeros24]` replies `[ef,09,87,status,nonce:u32LE,generation:u32LE,capacity:u8,bitmap_length:u8,bitmap...,zeros]`. Capacity100, bitmap length13, slot s is bit `s%8` of byte `14+s/8`, LSB first. Unused bits and bytes are zero.
- INVENTORY validates every currently indexed FULL or tombstone. Only valid FULL records set occupied bits. Status3 NOT_READY identifies a blocked cold scan; status7 BAD_CRC identifies corruption. Invalid slot/reserved bytes return2. Every failure preserves nonce and zeroes bytes8..31; no partial catalogue is a successful result.
- Hosts bracket a complete archive read with matching inventory generation/bitmap. Generation is not a persistent physical-device/reboot identifier and cannot by itself identify which USB half/archive was backed up.
- Deprecated EF08 STORE/SELECT still explicitly fail; EF08 QUERY remains read-only. No ordinary host operation may write or execute this archive.

## Verification scope

The focused host-managed native probe compiles the actual production HID parser, VIA gate/tail and archive reader, substituting only an explicit NOR/clock/QMK-sink model. It checks first/last maximum FULL record reads, capability negotiation, rejected legacy mutations, RAM key dispatch and lease fallback, corruption reporting and zero model NOR writes. This does not establish physical HID latency, a successful migration on a user's computer, or hardware acceptance. Historical STORE/SELECT/fault-injection harnesses describe the retired writable runtime and are not evidence for this read-only contract.
