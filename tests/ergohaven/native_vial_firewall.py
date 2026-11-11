"""Source-backed Vial dependency for native HID extraction fixtures.

Pass the caller's source reader so --ref/--source applies to every dependency.
Only linkage is made translation-unit local (split fixtures link two roles).
The production lock initialization and firewall body are otherwise unchanged.
"""
import re


def firewall_source(read, *, keycodes_included=False):
    # Historical --ref snapshots predating the external firewall calls need
    # no new dependency. Keep their original parser baseline executable.
    if not any(re.search(r'\bvial_keycode_firewall\s*\(', read(path))
               for path in ('keyboards/ergohaven/hid.c', 'quantum/via.c')):
        return ''
    vial = read('quantum/vial.c')
    header = read('quantum/vial.h')
    keycodes = read('quantum/keycodes.h')
    constants = []
    for name in ('QK_BOOTLOADER', 'QK_BOOT'):
        match = re.search(r'^\s*' + name + r'\s*=\s*[^,\n]+,', keycodes, re.M)
        assert match, name
        constants.append(match.group().strip())
    declaration = re.search(r'^uint16_t vial_keycode_firewall\(uint16_t in\);', header, re.M)
    assert declaration, 'production firewall declaration'
    match = re.search(r'^uint16_t vial_keycode_firewall\(uint16_t in\)\s*\{', vial, re.M)
    assert match, 'production firewall definition'
    end = vial.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (vial[end] == '{') - (vial[end] == '}')
        end += 1
    state = re.search(r'^#ifdef VIAL_INSECURE\n.*?^#endif\n(?=bool vial_unlock_in_progress)', vial, re.M | re.S)
    assert state, 'production lock initialization'
    lock = re.sub(r'^bool vial_unlocked', 'static bool vial_unlocked', state.group(), flags=re.M)
    enums = '' if keycodes_included else 'enum {\n' + '\n'.join(constants) + '\n};\n'
    return (enums + lock + 'static ' + declaration.group() + '\n'
            + 'static ' + vial[match.start():end] + '\n')
