"""Deterministic protocol-v7 packets; no desktop application or private captures.

Each scenario runs in a fresh native consumer. CRC is calculated independently
from the wire snapshot (CCITT-FALSE), not by calling the firmware CRC helper.
"""
import binascii
import struct


def packet(data):
    assert len(data) <= 32
    return 'P ' + bytes(data).ljust(32, b'\0').hex(' ') + '\n'


def snapshot(identity=1, revision=1, active=True, key=0x10, name=b'Editor', legacy=False):
    keys = [key] * 13 + [0xffff, 0xffff]  # inherit firmware encoder bindings
    layer_name = b'Editing'
    wire_keys = struct.pack('<15H', *keys)
    crc_data = wire_keys * 16 + bytes(15 * 16)
    crc_data += (bytes([0]) + bytes(4 * 5)) * 16  # disabled stacks/empty names
    crc_data += bytes([len(name)]) + name
    crc_data += (bytes([len(layer_name)]) + layer_name) * 16
    crc = binascii.crc_hqx(crc_data, 0xffff)
    commands = packet(bytes([0xe1, 7, active]) + struct.pack('<I', revision) + bytes([15, 16, len(name)]) + name)
    for layer in range(16):
        commands += packet(bytes([0xe2, 7, layer, 0, 13]) + wire_keys[:26])
        commands += packet(bytes([0xe2, 7, layer, 13, 2]) + wire_keys[26:])
        commands += packet(bytes([0xe7, 7, layer, 15]) + bytes(15))
        commands += packet(bytes([0xe4, 7, layer, len(layer_name)]) + layer_name)
        for slot in range(4):
            commands += packet(bytes([0xe9, 7, layer, slot]) + bytes(6))
    extension = b'' if legacy else bytes([0x57, 2]) + bytes([identity]) * 20
    commands += packet(bytes([0xe3, 7, active]) + struct.pack('<IH', revision, crc) + extension)
    return commands


def scenarios():
    first = snapshot()
    changed = snapshot(identity=2, revision=2, name=b'Terminal')
    return {
        'semantic-wake-and-replay': 'S\n' + first + 'W\nS\n' + first + 'N\n' + changed + 'W\n',
        'content-only-and-keepalive': 'S\n' + first + 'W\nS\n' + snapshot(revision=2, key=0x04) + 'N\n' + packet(bytes([0xe5, 7, 1]) + struct.pack('<I', 2)) + 'N\n',
        'legacy-no-wake': 'S\n' + snapshot(legacy=True) + 'N\n',
        'disabled-and-manual-off': 'S\nD\n' + first + 'N\nE\nO\n' + changed + 'F\n',
        'deactivate-expire-reconnect': 'S\n' + first + 'W\nS\nU\n' + first + 'N\n' + changed + 'W\nS\nX\n' + changed + 'N\n' + snapshot(identity=3) + 'W\n',
        'default-transition': 'S\n' + first + 'W\nS\n' + snapshot(identity=2, active=False, name=b'Default') + 'W\n',
        'manual-key-and-encoders': 'S\nR\nV\nK\n',
        'volume-precedence': 'S\nK\nG\n' + first + 'Z\n',
        'lcd-near-timeout': 'S\nT\n' + first + 'W\nY\n',
    }
