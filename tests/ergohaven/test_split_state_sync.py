#!/usr/bin/env python3
"""Execute split settings owners with simulated RPC/storage, clock and connection.

The production declarations, initialization, setters, receivers and housekeeping
are compiled verbatim. RPC failure can happen before or after peer application.
This host test does not certify physical serial timing or EEPROM persistence.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_phenom_settings_sync import ROOT, VARIANTS, function

SUPPORT = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EH_SYNC_LED_COLORS
#ifdef TEST_POINTING
#    define EH_SYNC_POINTING_SETTINGS
#endif
#define EH_RGB_LAYER_COUNT 16
#define SPLIT_POINTING_DEVICE_COUNT 4
#define SPLIT_POINTING_SIDE_COUNT 2
#define VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT 0x80444494u
#define dprintf(...) ((void)0)
typedef struct { uint32_t raw; } phenom_via_config_t;
typedef union { uint64_t raw; } kb_settings_pointing_t;
'''

PERIPHERALS = r'''
enum { RPC_PHENOM_CONFIG, RPC_PHENOM_SPLIT_POINTING_SETTINGS, RPC_PHENOM_LED_COLORS,
       RPC_SYNC_LED_COLORS, RPC_SYNC_POINTING_SETTINGS, RPC_SYNC_SPLIT_POINTING_SETTINGS,
       RPC_COUNT };
typedef void (*callback_t)(uint8_t, const void *, uint8_t, void *);
static callback_t callbacks[RPC_COUNT];
static unsigned attempts[RPC_COUNT];
static bool master = true, connected = true, fail_before, fail_after;
static uint32_t now, peer_raw, local_raw;
static bool in_receiver;
static bool initializing, injection_pending, injected;
static kb_settings_led_colors_t desired_led, peer_led;
static kb_settings_split_pointing_t desired_devices, peer_devices;
static kb_settings_pointing_t desired_pointing, peer_pointing;
static uint32_t phenom_applied_raw;
static bool phenom_applied_devices_valid;
static bool is_keyboard_master(void) { return master; }
static bool is_transport_connected(void) { return connected; }
static uint32_t timer_read32(void) { return now; }
static uint32_t timer_elapsed32(uint32_t previous) { return now - previous; }
static void transaction_register_rpc(int id, callback_t callback) { callbacks[id] = callback; }
static void invoke_receiver(callback_t, uint8_t, const void *, uint8_t, void *);
static bool transaction_rpc_exec(int id, uint8_t length, const void *data, uint8_t out_length, void *out_data) {
    ++attempts[id];
    if (!connected || fail_before) return false;
    uint8_t request[length];
    memcpy(request, data, length);
    if (out_length) memset(out_data, 0, out_length);
    if (callbacks[id]) invoke_receiver(callbacks[id], length, request, out_length, out_data);
    return !fail_after;
}
static kb_settings_led_colors_t get_settings_led_colors(void) { return desired_led; }
static void set_settings_led_colors(kb_settings_led_colors_t value) { peer_led = value; }
static kb_settings_split_pointing_t get_split_pointing_settings(void) { return desired_devices; }
static void set_split_pointing_settings(kb_settings_split_pointing_t value) { peer_devices = value; }
static kb_settings_pointing_t get_settings_pointing(void) { return desired_pointing; }
static void set_settings_pointing(kb_settings_pointing_t value) { peer_pointing = value; }
static uint32_t phenom_upgrade_raw(uint32_t value) { return value; }
static void phenom_apply_device_config(void) {}
static uint32_t via_get_layout_options(void) {
    uint32_t value = in_receiver ? peer_raw : local_raw;
    // Model SlaveThread preempting a local EEPROM read during post-init.
    if (initializing && injection_pending) {
        injection_pending = false;
        if (callbacks[RPC_PHENOM_CONFIG]) {
            uint32_t incoming = 2;
            injected = true;
            callbacks[RPC_PHENOM_CONFIG](sizeof(incoming), &incoming, 0, NULL);
        }
    }
    return value;
}
static void via_set_layout_options_kb(uint32_t value);
static void via_set_layout_options(uint32_t value) {
    if (in_receiver) peer_raw = value; else local_raw = value;
    via_set_layout_options_kb(value);
}
static void kb_settings_init(void) {}
static void keyboard_post_init_hid(void) {}
static uint32_t last_input_activity_elapsed(void) { return 0; }
static uint32_t get_led_rgb_timeout_ms(void) { return 0; }
static void housekeeping_task_ruen(void) {}
'''

SCENARIOS = r'''
#define CHECK(condition, message) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s: %s (attempts=%u)\n", argv[2], message, attempts[channel]); \
    return 1; } } while (0)
static void tick(void) { now += 100; HOUSEKEEPING(); }
static void set_value(int channel, unsigned value) {
    if (channel == RPC_PHENOM_CONFIG) { local_raw = value; via_set_layout_options_kb(value); }
    else if (channel == RPC_PHENOM_LED_COLORS || channel == RPC_SYNC_LED_COLORS)
        memset(&desired_led, value, sizeof(desired_led));
    else if (channel == RPC_SYNC_POINTING_SETTINGS) desired_pointing.raw = value;
    else memset(&desired_devices, value, sizeof(desired_devices));
}
static bool matches(int channel) {
    if (channel == RPC_PHENOM_CONFIG) return peer_raw == phenom_via_config.raw && peer_config.raw == phenom_via_config.raw;
    if (channel == RPC_PHENOM_LED_COLORS || channel == RPC_SYNC_LED_COLORS)
        return !memcmp(&desired_led, &peer_led, sizeof(peer_led));
    if (channel == RPC_SYNC_POINTING_SETTINGS) return desired_pointing.raw == peer_pointing.raw;
    return !memcmp(&desired_devices, &peer_devices, sizeof(peer_devices));
}
static void erase_peer(void) {
    peer_raw = 0; peer_config.raw = 0; memset(&peer_led, 0, sizeof(peer_led));
    memset(&peer_devices, 0, sizeof(peer_devices)); peer_pointing.raw = 0;
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    int channel = atoi(argv[1]);
    set_value(channel, 1);
    initializing = injection_pending = !strcmp(argv[2], "initialization_order");
    BOOT(); initializing = false;
    if (!strcmp(argv[2], "initialization_order")) {
        CHECK(!injected || phenom_via_config.raw == 2, "local init must not overwrite an accepted receive");
        uint32_t incoming = 2;
        callbacks[RPC_PHENOM_CONFIG](sizeof(incoming), &incoming, 0, NULL);
        CHECK(phenom_via_config.raw == 2 && local_raw == 2, "receiver is ready after initialization");
        return 0;
    }
    if (!strcmp(argv[2], "slave")) {
        master = false; tick(); CHECK(attempts[channel] == 0, "peripheral must not send");
        return 0;
    }
    if (!strcmp(argv[2], "throttle")) {
        now = 99; HOUSEKEEPING(); CHECK(attempts[channel] == 0, "100ms throttle");
    }
    if (!strcmp(argv[2], "peer_booting")) {
        callback_t callback = callbacks[channel]; callbacks[channel] = NULL;
        tick(); CHECK(attempts[channel] == 1 && !matches(channel), "transport success without ready receiver");
        callbacks[channel] = callback; tick();
        CHECK(attempts[channel] == 2 && matches(channel), "retry after peer initialization");
    } else {
        tick(); CHECK(attempts[channel] == 1 && matches(channel), "initial reconciliation");
    }
    if (!strcmp(argv[2], "edit")) {
        set_value(channel, 2); tick(); CHECK(matches(channel), "propagate setter change");
    } else if (!strcmp(argv[2], "reconnect") || !strcmp(argv[2], "reconnect_booting")) {
        connected = false; tick(); CHECK(attempts[channel] == 1, "no disconnected send");
        erase_peer(); connected = true;
        callback_t callback = callbacks[channel];
        if (!strcmp(argv[2], "reconnect_booting")) callbacks[channel] = NULL;
        tick();
        if (!callbacks[channel]) {
            CHECK(attempts[channel] == 2 && !matches(channel), "booting peer has not applied settings");
            callbacks[channel] = callback; tick();
            CHECK(attempts[channel] == 3, "retry after reconnect initialization");
        }
        CHECK(matches(channel), "restore unchanged state on reconnect");
    } else if (!strcmp(argv[2], "retry") || !strcmp(argv[2], "revert_after_failure")) {
        set_value(channel, 2); fail_before = !strcmp(argv[2], "retry");
        fail_after = !fail_before; tick();
        CHECK(attempts[channel] == 2, "attempt changed state");
        if (fail_after) {
            CHECK(matches(channel), "peer may apply before error");
            set_value(channel, 1);
        }
        fail_before = fail_after = false; tick();
        CHECK(attempts[channel] == 3 && matches(channel), "reconcile after failed RPC");
    }
    unsigned count = attempts[channel];
    for (int i = 0; i < 10; ++i) tick();
    CHECK(attempts[channel] == count, "suppress successfully synchronized state");
    return 0;
}
'''


def declarations(source, names):
    return "\n".join(
        match.group() for match in re.finditer(r"^static [^;\n]+;", source, re.M)
        if any(re.search(r"\b" + name + r"\b", match.group()) for name in names)
    )


def harness(variant):
    shared = variant.startswith("shared")
    source = (ROOT / ("keyboards/ergohaven/ergohaven_main.c" if shared else
                     f"keyboards/ergohaven/{variant}/rev1/pointing.c")).read_text()
    header = (ROOT / "keyboards/ergohaven/src/eh_pointing.h").read_text()
    payload = re.search(r"typedef struct \{.*?\} kb_settings_split_pointing_t;", header, re.S).group()
    rgb = (ROOT / "keyboards/ergohaven/ergohaven_rgb.h").read_text()
    led = re.search(r"typedef struct \{.*?\} kb_settings_led_colors_t;", rgb, re.S).group()
    names = (["synced_led_colors", "synced_led_colors_valid", "synced_pointing_settings",
              "synced_pointing_settings_valid", "synced_split_pointing_settings",
              "synced_split_pointing_settings_valid"] if shared else
             ["phenom_via_config", "phenom_synced_raw", "phenom_synced_raw_valid",
              "phenom_synced_devices", "phenom_synced_devices_valid",
              "phenom_synced_led_colors", "phenom_synced_led_colors_valid"])
    helpers = (ROOT / "keyboards/ergohaven/src/eh_split_sync.h").read_text()
    parts = [SUPPORT, payload, led, PERIPHERALS, declarations(source, names),
             function(helpers, "eh_sync_settings"), function(helpers, "eh_sync_settings_ack")]
    if shared:
        parts.append("static phenom_via_config_t phenom_via_config;")
    parts.append(r'''
static phenom_via_config_t peer_config;
static void invoke_receiver(callback_t callback, uint8_t length, const void *data, uint8_t out_length, void *out_data) {
    phenom_via_config_t master_config = phenom_via_config;
    phenom_via_config = peer_config;
    in_receiver = true;
    callback(length, data, out_length, out_data);
    in_receiver = false;
    peer_config = phenom_via_config;
    phenom_via_config = master_config;
}
''')
    if shared:
        parts += ["static void via_set_layout_options_kb(uint32_t value) { phenom_via_config.raw = value; }",
                  "static void keyboard_post_init_user(void) {}",
                  "static void housekeeping_task_user(void) {}"]
        receivers = ["sync_led_colors_rpc", "sync_pointing_settings_rpc", "sync_split_pointing_settings_rpc"]
        parts += [function(source, name) for name in receivers]
        parts += [function(source, "keyboard_post_init_kb"), function(source, "housekeeping_task_kb"),
                  "#define BOOT keyboard_post_init_kb\n#define HOUSEKEEPING housekeeping_task_kb"]
    else:
        parts += [function(source, "via_set_layout_options_kb")]
        receivers = ["phenom_sync_config_rpc", "phenom_sync_split_pointing_settings_rpc", "phenom_sync_led_colors_rpc"]
        parts += [function(source, name) for name in receivers]
        parts += [function(source, "keyboard_post_init_user"), function(source, "housekeeping_task_user"),
                  "#define BOOT keyboard_post_init_user\n#define HOUSEKEEPING housekeeping_task_user"]
    parts.append(SCENARIOS)
    return "\n".join(parts)


class SplitStateSyncTests(unittest.TestCase):
    def test_state_owners(self):
        cases = ("startup", "edit", "retry", "revert_after_failure", "reconnect", "slave", "throttle", "peer_booting", "reconnect_booting", "initialization_order")
        with tempfile.TemporaryDirectory(prefix="split-state-", dir=os.environ.get("TMPDIR")) as tmp:
            for variant in (*VARIANTS, "shared-led", "shared-pointing"):
                source, executable = Path(tmp) / f"{variant}.c", Path(tmp) / variant
                source.write_text(harness(variant))
                flags = ["-DTEST_POINTING"] if variant == "shared-pointing" else []
                subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                                "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable",
                                "-fsanitize=undefined", "-fno-sanitize-recover=all", *flags,
                                str(source), "-o", str(executable)], check=True)
                channels = (3, 4, 5) if variant == "shared-pointing" else (3,) if variant == "shared-led" else (0, 1, 2)
                for channel in channels:
                    for case in cases:
                        if case == "initialization_order" and (variant.startswith("shared") or channel != 0):
                            continue
                        with self.subTest(variant=variant, channel=channel, case=case):
                            result = subprocess.run([str(executable), str(channel), case], capture_output=True, text=True)
                            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
