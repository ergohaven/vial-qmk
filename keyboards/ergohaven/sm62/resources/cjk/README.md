# SM62 CJK 24 px monochrome — experimental choice, not approved final

This combined COMMON prototype target builds and links normally. It includes
the exact verified idle restart delta and cached FPS repair. Both halves must
be updated (internal settings RPC is 30 bytes). No EEPROM reset is required.

CJK is 24 px **1 bpp**, not the rejected 24 px 2 bpp antialiased resource.
This preserves the CJK size but changes its antialiasing. Show the actual
LVGL comparison preview and obtain the user's choice before final delivery.
The original Latin Regular/Bold 24 data, metrics, kerning and bitmaps remain
unchanged; private fallback descriptors apply only to player title/artist.

## Font and coverage

Noto Sans CJK KR Regular 2.004, copyright Adobe 2014–2021, SIL OFL 1.1.
Source https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/OTF/Korean/NotoSansCJKkr-Regular.otf
SHA256 6bcb2a0703aa137e874fc2dffa85f6c21ba9a67fa329e81b8c801663af7e992a
License retained as OFL.txt. Korean regional glyph forms; no JP/SC/TC selector.

All 39,558 intended source glyphs retained, ranges inclusive:
1100–11FF, 3000–303F, 3041–3096, 3099–30FF, 3131–318E,
3400–4DB5, 4E00–9FEF, AC00–D7A3, FF01–FFBE, FFC2–FFC7,
FFCA–FFCF, FFD2–FFD7, FFDA–FFDC, FFE0–FFE6, FFE8–FFEE.
This is not all Unicode CJK: Noto lacks 4DB6–4DBF and 9FF0–9FFF;
Ext B+, emoji, unlisted compatibility ideographs and variation sequences
are excluded. Full modern Hangul, jamo, kana, Han/Ext A above are retained.
U+3031/U+3032 remain taller than the unchanged 32 px field and clip;
the exhaustive native test reports four field/codepoint cases explicitly.

## Representation and integration

lv_font_conv 1.5.3 --size 24 --bpp 1 --no-kerning --no-compress
with the exact ranges above supplies the conversion input. The generator
aligns rows to three bytes before compressing independent 128-glyph pages
using raw DEFLATE. It keeps five-byte metrics and a page index, not LVGL's
20-bit bitmap offsets. Strict range and decoded-page bounds are checked.

Read-only compressed bitmap 1,326,731 B; metrics 197,790 B; page index 1,244 B.
Page scratch 8,460 B, bounded 64-glyph FIFO cache, static decoder scratch;
no frame allocation, no font heap allocation, no large stack arrays.
tinf 1.2.1 by Joergen Ibsen is included under its zlib license. Source is
plainly marked as altered: decoder data and code-length scratch are static,
so it is intentionally non-reentrant and called only on the display thread.

Normal ARM link: 2,001,004 B flash; 96,148 B flash headroom. Linker heap
182,504 B; main process stack 2,048 B. Font TU ARM stack-usage frames <=64 B;
this is not a measurement of complete physical main-loop high-water use.

Native real pinned LVGL: all 79,116 title/artist codepoint lookups, 18,096
Latin/fixture checks, six pixel-identical Latin marquee frames, moving mixed
marquee and 1,000 long/change/clear/short cycles with identical final short
pixels and stable 40,200 B LVGL free space. Separate ASan+UBSan codec test
compares all 39,558 exact generated monochrome glyph bitmaps/metrics and
100,000 cache-churn lookups. New decoder retains all sanitizer checks.
Only the inherited LVGL compressed-font TU excludes its known shift UB.

Idle sender/receiver suite: 66 PASS lines. Complete source inventory proves
idle bytes exactly match the verified idle worker and unrelated FPS, GPIO,
EEPROM, HID and UTF-8 transport bytes are unchanged. Both UF2 payload and
RP2040 family/address/block checks are validated against its own ELF.

Physical SPI/FPS and cold-glyph latency are NOT measured. The preview is
native LVGL label rendering, not a photo of a physical panel or full screen.
No publication, canonical edits or user delivery occurred.
