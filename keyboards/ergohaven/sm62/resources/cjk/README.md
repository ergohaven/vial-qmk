# SM62 CJK font

The CJK resource uses 24 px, 1 bpp monochrome glyphs. Private fallback
descriptors apply only to player title and artist text; they do not replace
the Latin Regular/Bold 24 data, metrics, kerning or bitmaps.

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
U+3031/U+3032 are taller than the 32 px field and clip.

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

These resource details do not establish physical SPI/FPS or cold-glyph latency.
