"""Actual settings label owners; harnesses inject only keymap/storage I/O."""
import re


def layer_label_source(root):
    settings = (root / 'keyboards/ergohaven/src/eh_settings.c').read_text()
    introspection = (root / 'quantum/keymap_introspection.c').read_text()
    match = re.search(r'^bool keymap_layer_has_assignments_raw\([^;\n]*\)\s*\{', introspection, re.M)
    assert match, 'keymap_layer_has_assignments_raw'
    end = introspection.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (introspection[end] == '{') - (introspection[end] == '}')
        end += 1
    # Keep the real empty label, feature-conditioned standard names, factory
    # assignment scan, default-name selection, reset and migration as one unit.
    return ('#include <strings.h>\n#include "quantum/util.h"\n#define PROGMEM\n'
            + introspection[match.start():end] + '\n'
            + settings[settings.index('static const char empty_layer_label['):
                       settings.index('void kb_settings_reset(void)')])
