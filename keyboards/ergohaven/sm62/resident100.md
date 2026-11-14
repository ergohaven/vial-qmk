# SM62 resident100 schema2

Authoritative wire: `CONTRACT.md` in the resident100 system root. Runtime E1–EA remains protocol8. Native Default remains outside this journal and uses EA/native lease expiry. No renderer, matrix, split-runtime or EEPROM format changes.

## Flash and recovery

- New journal `[0x200000,0x2ca000)`: 101 physical records of8192, 100 logical application slots0..99. Slot0 is an application; DELETE0 is allowed.
- Old journal `[0x3f2000,0x3fa000)` is untouched, as are extension/EEPROM. No automatic legacy replay/fallback or migration is performed. Optional legacy1/2 fallback is intentionally not enabled; legacy Default0 can never execute as app0.
- Header256 at record offset0: magic `45 43 52 32` (LE `0x32524345`), schema byte4=2, slot5, kind6 (1FULL,2tombstone), zero7, generation:u32@8, payload_length:u16@12, zero14..15, payload CRC32@16, semantic identity20@20, header CRC32@40 (bytes0..39). Remaining header padding is erased0xff.
- FULL bytes at256, length3440..4560. Semantic identity must match embedded COMMITv2, active1. Structure and semantic CRC16 are checked through the actual existing HID parser, but its publisher is gated off for validation.
- Tombstone length0, retains deleted identity/CRC. New tombstone is latest for its logical slot, so earlier FULL cannot resurrect on restart.
- Commit256 at7936: magic `45 43 4d 32` (`0x324d4345`), generation@4, header CRC@8, commit CRC32@12 over first12 bytes. Remaining padding0xff. Programmed last, after header/payload readback. ACK follows full record validation/readback.
- CRC32 is ISO-HDLC polynomial0xedb88320, initial/final xor0xffffffff.
- One-time scan builds100-byte record+1 index, plus uint32 generation. No100 decoded maps and no keypress-path flash scans. One pre-existing256-byte page buffer and compact buffers are reused. Generation UINT32_MAX refuses new writes/deletes; exact hit is no-wear.
- Erase/program always targets a record outside the100 latest indices. Failed IO invalidates the index: next read-only reconciliation scans actual flash, including commit that physically completed despite failure return. No activation or optimistic ACK.

## Wire

- EBv1 `[eb,08,a5,01,nonce32,zeros24]` -> `[eb,08,5a,01,nonce32,03,00,d0,11,zeros20]`: RAW/LZ supported, **resident count0**, so old host cannot write deprecated journal.
- EBv2 response follows contract: schema2, capacity100, max4560, layers16, controls62, name22, flash4194304, feature mask15, record schema2.
- EC byte2=2 stages only; EC byte2=1 retains ordinary activation. ED/EE codecs/order/CRCs unchanged. Stage validates using same real COMMIT conditions with a pre-publication return, never publish-and-undo. Ready requires same transaction/id/CRC and TTL<2500ms from valid EE. Ordinary begin/deactivate, invalid/new transfer, SELECT and expiry invalidate it. Keepalive/events do not.
- EF09 ops0QUERY,1STORE_READY,2SELECT,3ENUM_SLOT,4DELETE,5QUERY_READY,6READ. Status0OK,1NOT_FOUND,2BAD_ARGUMENT,3NOT_READY,4BUSY(reserved),5FLASH_ERROR,6IDENTITY_CONFLICT,7BAD_CRC,8GENERATION_EXHAUSTED. Exact layouts and correlation are in CONTRACT. No unsolicited second reader/reply; production VIA tail emits the single response.
- Indexed records have already passed the real semantic COMMIT validator during scan or successful STORE. QUERY/ENUM/READ (including matching EF08 QUERY) recheck bounded header/commit, current payload CRC32 and embedded identity **without semantic replay or staging mutation**. STORE retains full incoming semantic validation and post-write validation; SELECT rechecks durable integrity then passes the payload through the sole semantic publisher. A post-scan payload fault returns explicit BAD_CRC (7); READ returns zero bytes/padding, STORE does not acknowledge a false no-wear hit or attempt an implicit repair. The damaged record/index and unrelated slots remain untouched. Direct journal appends likewise refuse an indexed damaged record. Healthy exact hits remain no-wear, including at generation exhaustion. CRC32 is corruption detection, not authentication against coordinated payload/checksum forgery; cold scans still reject semantically invalid records with forged CRC32/header/commit.
- A cold index scan needs the shared semantic staging image. While an ordinary E1→E3 transfer owns it, scan is deferred: EF09 returns NOT_READY (3), READ returns zero actual/data, EF08 QUERY returns its legacy negative response. No partial/stale index is exposed. After ordinary COMMIT (or staging expiry), a subsequent command performs the full semantic scan. Warm-index read responses remain unchanged even immediately after E1 or before E3. Stage-only ready tokens survive both warm reads and cold scans. No decoded map, heap allocation or new static storage is added.
- STORE never implicitly replaces a different semantic identity. Same identity may update CRC through COW. QUERY/ENUM/READ are non-activating. SELECT uses the existing sole publisher and normal held-release, semantic wake, lease and split-current behavior. SELECT never stores/replays native Default.
- Deprecated EF08 STORE and SELECT return explicit old-format failure. Its QUERY is read-only. Unlock/settings fences remain in outer VIA gate. Refresh allowlist admits EF09 read operations and SELECT; STORE/DELETE wait for normal flash/render exclusion.

## Native evidence and interop peer

Run from firmware export:

```
python tests/ergohaven/run_resident100.py OUTPUT
python tests/ergohaven/run_resident_flash_backend.py OUTPUT_BACKEND
python tests/ergohaven/run_resident100_peer.py OUTPUT_PEER
```

`SM62_SKIP_FAULTS=1` skips the expensive exhaustive fault loop for focused development only. `SM62_FIXTURE_DIR=EXISTING_DIRECTORY` writes100 actual C-produced maximum FULL fixtures. Each is4560 bytes and includes16x62 keys, visuals,64 maximal stack names and16 maximal22-byte layer names.

Peer launch: `OUTPUT_PEER/resident100_peer FLASH_IMAGE`. Input is one64-character hex report per line; output one64-character hex actual reply, or `-` for write-only reports. `STATE` returns real live state JSON. `KEY row col 0|1` exercises actual production dispatch into modeled QMK key sinks. `TIME ms` advances only modeled time and calls the real task. `BUDGET io_count torn_byte_count` injects NOR failure. File mutations are flushed+fsynced before returning. Restart the actual process with same image to recover with fresh C RAM. No hardware access.

`test_resident100_restart.py PEER_EXE FIXTURE_DIR NEW_OUTPUT_DIR` stores100 distinct maximum FULL through the actual parser; restarts; ENUM/QUERY/SELECT/READs all100; verifies exact456000 bytes, real key callbacks/display name and zero selection writes; deletes0/99 and restarts again.

These native checks model OS HID, clock, NOR and QMK action sinks. They do not establish physical hardware timing or fix the separate left-freeze-after-right-reflash defect. Parent owns independent review, actual ARM delta/build/map/UF2 and paired real Rust-owner acceptance.
