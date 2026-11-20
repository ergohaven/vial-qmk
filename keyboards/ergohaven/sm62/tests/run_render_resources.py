#!/usr/bin/env python3
"""Characterize the linked ARM renderer's allocations without a keyboard.

Requires unicorn and pyelftools. Executes actual ARM LVGL/screens/newlib; panel,
clock, view inputs, divider and allocator locks are substituted. This does not
emulate RP2040 DMA, interrupts, timing, boot or the cross-core scheduler.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_PC, UC_ARM_REG_LR

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default='arm-none-eabi-gcc', help='ARM compiler for value-struct ABI offsets only')
parser.add_argument('--objdump', default='arm-none-eabi-objdump')
parser.add_argument('--firmware', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--side', choices=('left', 'right'), default='left')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
with args.firmware.open('rb') as stream:
    elf = ELFFile(stream)
    assert elf['e_machine'] == 'EM_ARM'
    symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols() if s.name}
    sizes = {s.name: s['st_size'] for s in elf.get_section_by_name('.symtab').iter_symbols() if s.name}
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    cpu.mem_map(0x10000000, 0x400000)
    cpu.mem_map(0x20000000, 0x42000)
    for segment in elf.iter_segments():
        if segment['p_type'] == 'PT_LOAD' and segment['p_filesz']:
            cpu.mem_write(segment['p_vaddr'], segment.data())

heap_base = symbols['__heap_base__']
heap_limit = symbols['__heap_end__']
heap = heap_base
stack_top = symbols['dc_render_wa'] + sizes['dc_render_wa']
assert sizes['dc_render_wa'] >= 6144, 'render working area was reduced'
assert sizes['dc_exchange'] == 6416, 'snapshot slots or exchange allocation changed'
park_address = symbols['sm62_flash_worker_park'] & ~1
assert 0x20000000 <= park_address < 0x20040000, 'park must execute from SRAM'
assert heap_base <= symbols['__heap_end__'] <= 0x20040000
park_assembly = subprocess.check_output([args.objdump, '-d', '--disassemble=sm62_flash_worker_park',
                                        str(args.firmware)], text=True)
(args.output/'flash-park.asm').write_text(park_assembly)
instructions = [line.split('\t')[2:] for line in park_assembly.splitlines()
                if re.match(r'^\s*[0-9a-f]+:\s', line)]
assert instructions and not any(parts[0] in ('bl', 'blx') for parts in instructions), 'park calls another function'
for parts in instructions:
    operands = ' '.join(parts[1:])
    assert not (parts[0].startswith('ldr') and 'pc' in operands), 'park loads a literal dependency'
    target = re.match(r'([0-9a-f]+) <', operands)
    if target:
        assert park_address <= int(target[1], 16) < park_address + sizes['sm62_flash_worker_park'], 'park branches outside SRAM closure'
sentinel = 0x10000080
hooks = {}
steps = 0
lowest_stack = stack_top
allocations_failed = []
calls = []
panel_transfers = 0
local_keymap_queries = 0


def return_to_caller(value=0):
    cpu.reg_write(UC_ARM_REG_R0, value & 0xFFFFFFFF)
    cpu.reg_write(UC_ARM_REG_PC, cpu.reg_read(UC_ARM_REG_LR))


def substitute(name, callback):
    if name in symbols:
        hooks[symbols[name] & ~1] = callback


def sbrk():
    global heap
    increment = cpu.reg_read(UC_ARM_REG_R1)
    if increment >= 0x80000000:
        increment -= 0x100000000
    if heap + increment > heap_limit or heap + increment < heap_base:
        allocations_failed.append({'request': increment, 'available': heap_limit - heap})
        return_to_caller(0xFFFFFFFF)
    else:
        previous = heap
        heap += increment
        return_to_caller(previous)


substitute('_sbrk_r', sbrk)
for name in ['__malloc_lock', '__malloc_unlock', 'sm62_diag_stage', 'display_diag_mark',
             'backlight_init', 'backlight_level_noeeprom', 'sm62_flash_core1_poll', 'sm62_render_strip_completed']:
    substitute(name, return_to_caller)
for name in ['sm62_render_is_owner', 'sm62_spi_quiescent', 'qp_comms_spi_transaction_begin',
             'qp_comms_spi_transaction_end', 'qp_viewport', 'qp_flush']:
    substitute(name, lambda: return_to_caller(1))


def panel_pixels():
    global panel_transfers
    panel_transfers += 1
    return_to_caller(1)


substitute('qp_pixdata', panel_pixels)


def panel_init():
    # ARM painter_driver_t: two 32-bit vtable pointers precede validate_ok.
    # Only physical panel validation is substituted; QP/LVGL attach runs.
    cpu.mem_write(cpu.reg_read(UC_ARM_REG_R0) + 8, b'\1')
    return_to_caller(1)


substitute('qp_init', panel_init)
clock_ms = 10000
substitute('timer_read32', lambda: return_to_caller(clock_ms))
for name, signed in [('__wrap___aeabi_uidivmod', False), ('__wrap___aeabi_idivmod', True)]:
    def divide(signed=signed):
        dividend = cpu.reg_read(UC_ARM_REG_R0)
        divisor = cpu.reg_read(UC_ARM_REG_R1)
        if signed:
            dividend = dividend if dividend < 2**31 else dividend - 2**32
            divisor = divisor if divisor < 2**31 else divisor - 2**32
        quotient = abs(dividend) // abs(divisor) if divisor else 0
        if (dividend < 0) != (divisor < 0):
            quotient = -quotient
        cpu.reg_write(UC_ARM_REG_R1, (dividend - quotient * divisor) & 0xFFFFFFFF)
        return_to_caller(quotient)
    substitute(name, divide)

# The actual snapshot layout starts at the exchange's 8-byte-aligned slots,
# following two u32 counters and a consumer bool. The slots are zero-initialized.
cpu.mem_write(symbols['dc_view'], struct.pack('<I', symbols['dc_exchange'] + 16))
settings, name_address = 0x20041000, 0x20041100
cpu.mem_write(settings, b'\1' * 32)
cpu.mem_write(name_address, b'Layer 00\0')
for name, value in {'sm62_view_local_settings': settings, 'sm62_view_remote_settings': settings,
                    'sm62_view_name': name_address, 'sm62_view_left': int(args.side == 'left'), 'sm62_view_master': 1,
                    'sm62_view_live': 1, 'sm62_view_time_live': 1, 'sm62_view_volume_live': 1,
                    'sm62_view_date': 1, 'sm62_view_stars': 1, 'sm62_view_language_enabled': 1,
                    'sm62_view_brightness': 100, 'sm62_view_boot': 10000}.items():
    substitute(name, lambda value=value: return_to_caller(value))


def local_keycode():
    global local_keymap_queries
    row = cpu.reg_read(UC_ARM_REG_R1)
    assert row < 10 and (row < 5) == (args.side == 'left'), ('peer-row screen access', args.side, row)
    local_keymap_queries += 1
    return_to_caller(4)


for name in ['sm62_view_native_code', 'sm62_view_effective_code']:
    substitute(name, local_keycode)


def instruction(uc, address, size, data):
    global steps, lowest_stack
    steps += 1
    lowest_stack = min(lowest_stack, uc.reg_read(UC_ARM_REG_SP))
    if address == sentinel:
        uc.emu_stop()
    elif address in hooks:
        hooks[address]()


cpu.hook_add(UC_HOOK_CODE, instruction)


def call(name):
    global steps, lowest_stack
    steps = 0
    lowest_stack = stack_top
    cpu.reg_write(UC_ARM_REG_SP, stack_top)
    cpu.reg_write(UC_ARM_REG_LR, sentinel | 1)
    cpu.emu_start(symbols[name] | 1, 0, count=50000000)
    assert cpu.reg_read(UC_ARM_REG_PC) == sentinel, 'instruction budget exhausted: ' + name
    assert stack_top - lowest_stack <= 6144, 'render stack exhausted in ARM workload'
    calls.append({'function': name, 'result': cpu.reg_read(UC_ARM_REG_R0),
                  'sbrk_bytes': heap - heap_base, 'unclaimed_heap_bytes': heap_limit - heap,
                  'emulated_stack_bytes': stack_top - lowest_stack, 'instructions': steps})
    print(json.dumps(calls[-1]), flush=True)
    return cpu.reg_read(UC_ARM_REG_R0)


# Obtain offsets from the real production type with the ARM ABI, rather than
# assuming the native host's packing. Only this tiny layout object is compiled.
root = Path(__file__).resolve().parents[4]
hid = (root/'keyboards/ergohaven/hid.h').read_text()
hid = hid[hid.index('typedef struct {'):hid.index('} hid_data_t;')+len('} hid_data_t;')]
layout = args.output/'layout.c'
layout.write_text('#define KEYBOARD_ergohaven_sm62\n'
    'typedef unsigned char uint8_t;typedef unsigned short uint16_t;'
    'typedef unsigned int uint32_t;typedef unsigned long long uint64_t;typedef _Bool bool;\n'+hid+
    '\nconst unsigned layout[]={sizeof(hid_data_t),__builtin_offsetof(hid_data_t,media_title),'
    '__builtin_offsetof(hid_data_t,media_artist)};\n')
subprocess.run([args.cc,'-std=gnu11','-mcpu=cortex-m0plus','-mthumb','-c',str(layout),'-o',str(args.output/'layout.o')],check=True)
with (args.output/'layout.o').open('rb') as stream:
    offsets = struct.unpack('<III',ELFFile(stream).get_section_by_name('.rodata').data())
assert offsets[0] == sizes['dc_hid']
# Long CJK title and artist exercise marquee/font allocation with Orbit kept.
cpu.mem_write(symbols['dc_hid']+offsets[1], ('漢字' * 42).encode()+b'\0')
cpu.mem_write(symbols['dc_hid']+offsets[2], ('漢字' * 5).encode()+b'\0')
assert call('display_init_kb') == 1, 'fixture must initialize actual screens'
call('screen_orbit_init')
animation_ready = bool(cpu.mem_read(symbols['animation_ready'], 1)[0])
if animation_ready:
    # Keep the allocated Orbit scene while visiting every initialized view.
    # Actual LVGL refresh/decode/transient allocation runs for each load.
    for name in ['screen_orbit_load', 'screen_keymap_load', 'screen_right_player_load', 'screen_main_load']:
        call(name)
        clock_ms += 100
        before = panel_transfers
        call('qp_lvgl_internal_tick')
        assert panel_transfers > before, 'fixture bypassed actual LVGL drawing'
assert heap_limit - heap >= 2048, 'less than 2 KiB allocation margin in ARM workload'
assert local_keymap_queries > 0, 'real screen keycode reads were bypassed'
result = dict(firmware_sha256=hashlib.sha256(args.firmware.read_bytes()).hexdigest(),
              side=args.side, local_keymap_queries=local_keymap_queries,
              heap_bytes=heap_limit - heap_base, calls=calls, allocations_failed=allocations_failed,
              sram_park_address=hex(park_address), render_working_area_bytes=sizes['dc_render_wa'],
              animated_orbit_allocated=animation_ready, hardware_qualification=False,
              substituted_symbols=[name for name, address in symbols.items() if (address & ~1) in hooks])
(args.output / 'resources.json').write_text(json.dumps(result, indent=2) + '\n')
print('Animated Orbit allocated:', animation_ready, '(ARM emulation; no timing or hardware proof)')
raise SystemExit(0 if animation_ready and not allocations_failed else 2)
