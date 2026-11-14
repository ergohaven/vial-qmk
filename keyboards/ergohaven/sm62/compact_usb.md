# SM62 compact FULL USB transport v1

Current resident100 additions and compatibility behavior are specified in
[resident100.md](resident100.md). The original activation framing/codecs below
remain supported. EBv1 now advertises RAW/LZ feature3 and resident count0;
EBv2/EF09 and EC stage-only are additive. Historical paragraphs saying there is
no persistence/acceptance apply only to the original runtime-only path, not to
STORE_READY/QUERY_READY in schema2.

This is an optional USB transport for the existing v8 application snapshot, not
another profile library, peer protocol, persistent format, or acknowledgement.
Only the USB SM62 bridge uses it after an explicit successful capability reply.
Macropad v7 and the v8 E1–EA format remain unchanged.

## Namespace audit

Audited `quantum/via.{c,h}`, `vial.{c,h}`, Ergohaven `hid.{c,h}`, display HID
handlers and SM62 diagnostics. VIA uses 01–13/FE/FF; host feedback BB–BF;
pictograms C0–CB; CC is a split feedback relay; startup images D0–D6;
SM62 diagnostics D7; runtime layouts E1–EA. Allocate **EB–EE** below.
F1/F2/F3 from the compression spike were placeholders and are NOT used.
MIDI status and bitmap constants occupy separate namespaces. The audit's
source-location inventory is frozen in the implementation evidence directory.

All reports are exactly 32 bytes; integers are little-endian. Unspecified
bytes are zero and receivers reject nonzero reserved bytes. Protocol byte1
stays 8 so existing transport gating and firmware identification stay intact.

## Read-only capability (EB)

Request: `[EB,08,A5,01, nonce:u32, zeros:24]`.
Reply: `[EB,08,5A,01, nonce:u32, 03,00, max_decoded:u16=4560, zeros:20]`.
03 means compact RAW plus bounded LZ v1. Host compares every byte of the
expected reply, including nonce, exact version/feature set and bounds. An echo,
unhandled FF, unknown version/features/limits or malformed response is NOT
proof; use the original 210 reports. A transport error stays unknown and
follows existing failure/retirement/reconnect behavior, not optimistic compact
writes. Capability is connection-local, reset on replacement/open/handoff;
not read from settings and not persisted. The query goes through the existing
serialized shared/dedicated HID owner; one OS read attempt, not twenty retries.
Paused shared owners return no reply/admission and defer transfer.

The firmware keyboard callback leaves EB replies for VIA's single final send,
like E6. EC–EE are write-only and activate the existing magic suppression.
Vial unlock-in-progress gates remain outside the handler and unchanged: a
blocked EB is echoed (rejected as proof); blocked writes are not processed.
The refresh allowlist admits EB requests and EC–EE only at exact v8/32-byte
framing. The existing FIFO order and eight-report loop budget are unchanged.

## Envelope

EC BEGIN:
- bytes0..3: EC,08,01,codec (0=compact RAW; 1=bounded LZ)
- bytes4..7: transaction:u32
- bytes8..9: encoded_length:u16, 1..4560
- bytes10..11: decoded_length:u16, 3440..4560
- bytes12..13: DATA report count:u16 = ceil(encoded_length/24)
- bytes14..15: zero
- bytes16..19: decoded CRC32/ISO-HDLC
- bytes20..23: encoded CRC32/ISO-HDLC
- bytes24..31: zero

ED DATA: ED,08,transaction:u32,sequence:u16,payload24. Sequence starts at zero;
last payload length is implicit from the declared encoded length, and all
unused last-report bytes must be zero. EE END: EE,08,transaction:u32,
DATA_count:u16,encoded_crc32:u32,zeros20.

BEGIN supersedes existing compact and original staging, never the live image.
A byte-identical immediately previous DATA retry is idempotent (the existing
host may retry an uncertain write once); it does not renew the timeout. A
conflicting or older duplicate, missing/reordered sequence, wrong transaction,
bad length/reserved fields/count/CRC, decoder error or malformed stream aborts
staging without publication. END without a valid transaction is ignored with
staging invalidated; retry after accepted END never publishes twice. Ordinary
BEGIN/DEACTIVATE and interleaved original data/commit clear compact state.
A fixed 2500ms transaction timeout is measured from BEGIN, not last DATA.
Disconnect has no new firmware signal: incomplete state expires through the
existing task, or is superseded by the next BEGIN/DEACTIVATE. RAM buffers need
not be zeroed when invalid; valid/length/sequence gates prevent reuse.

## Compact bytes and codec

Verbatim original BEGIN32 + COMMIT32 (INCLUDING optional 20-byte semantic-ID
digest at original COMMIT[11..31]), then the existing canonical packet order:
1984 keycode bytes; 992 visual bytes; 64 records of count1/name_length1/ccw2/cw2/
name_bytes0..12; 16 layer names of length1/name_bytes0..22. Inactive Default is
still 62 controls by 16 layers, with its original identity semantics intact.
The same grammar preserves maximum fields even though normal SM62 publication
policy rejects encoder-stack profiles. Dense stack fixtures inject only the
test-owned snapshot seam to qualify this wire capacity; production eligibility
is NOT broadened.

LZ literal tag0..127 => 1..128 literal bytes. Match tag128..255 => 3..130 bytes
plus backward distance:u16, 1..4096; overlapping copies are allowed. Source and
destination bounds and exact decoded length are mandatory. Host searches at
most 64 candidates per position and selects LZ only if strictly smaller than
RAW. If compact cannot encode the profile or does not reduce reports, use the
original packets. No unbounded loops or heap allocation on firmware.

CRC32s and compact grammar are validated before replay. Expansion submits the
original BEGIN, all original data fields and the verbatim original COMMIT to
the existing staging parser; only its original CRC16/revision/completeness
checks publish live arrays and apply held-key/default/semantic-wake behavior.
The final short visual report deliberately retains the original encoder's
padding semantics. No serial worker writes live state; no cache, renderer,
input, peer, native Default storage, pacing or lease extensions are introduced.

## Acceptance gap and qualification

**There is no final firmware acceptance ACK.** EE is write-only, just like the
original COMMIT. Host logs/caches I/O completion, not firmware acceptance. A
lost/corrupt report silently rejected by firmware can therefore require the
existing full resend/unlock/reconnect/lease recovery. Do not describe a
successful HID write or the capability reply as snapshot acceptance. Firmware
state comparisons in native tests establish modeled acceptance, not physical
USB timing or device acceptance. See the implementation manifest/README for
exact executed gates and inherited/unverified limitations before packaging.
