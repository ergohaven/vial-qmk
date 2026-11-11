/* SM62-only supplementary script: absolute markers, no allocated sections.
 * This must accompany the unchanged RP2040 linker script and FLASH_LEN cap. */
__sm62_layout_reserve_start = 0x10200000;
__sm62_layout_reserve_end = 0x102ca000;
ASSERT(DEFINED(FLASH_LEN), "SM62 reservation requires explicit FLASH_LEN")
ASSERT(FLASH_LEN <= (__sm62_layout_reserve_start - 0x10000000), "SM62 code overlaps layout reservation")
