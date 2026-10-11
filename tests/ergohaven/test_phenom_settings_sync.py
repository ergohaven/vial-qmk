#!/usr/bin/env python3
"""Host regression probes for Phenom settings synchronization.

Run: python3 tests/ergohaven/test_phenom_settings_sync.py
Requires a C11 host compiler (CC, default gcc). Extracts the production
housekeeping, receiver and QMK RPC functions verbatim into a host harness.
Transport, clock, connection state and settings storage are simulated;
this is not a firmware build or physical serial/sensor test.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
VARIANTS = ("phenom", "phenom_mini", "phenom_micro")


def function(text, name):
    match = re.search(r"^(?:static )?(?:void|bool) " + name + r"\([^\n]*\) \{", text, re.M)
    if match is None:
        raise ValueError(f"Missing production function: {name}")
    start = match.start()
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


PREAMBLE = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EH_KEYBOARD_SPLIT_POINTING_V2
#define EH_SPLIT_POINTING_INVERT_AXES
#define SPLIT_POINTING_DEVICE_COUNT 4
#define SPLIT_POINTING_SIDE_COUNT 2
#define RPC_S2M_BUFFER_SIZE 32
'''

SUPPORT = r'''
enum { PUT_RPC_INFO, PUT_RPC_REQ_DATA, EXECUTE_RPC, GET_RPC_RESP_DATA,
       RPC_PHENOM_CONFIG, RPC_PHENOM_SPLIT_POINTING_SETTINGS, RPC_PHENOM_LED_COLORS,
       NUM_TOTAL_TRANSACTIONS };
typedef void (*slave_callback_t)(uint8_t, const void *, uint8_t, void *);
typedef struct {
    uint8_t initiator2target_buffer_size, target2initiator_buffer_size;
    slave_callback_t slave_callback;
} split_transaction_desc_t;
typedef struct {
    uint8_t checksum;
    struct { int8_t transaction_id; uint8_t m2s_length, s2m_length; } payload;
} rpc_sync_info_t;
static struct {
    rpc_sync_info_t rpc_info;
    uint8_t rpc_m2s_buffer[RPC_M2S_BUFFER_SIZE];
    uint8_t rpc_s2m_buffer[RPC_S2M_BUFFER_SIZE];
} memory;
static __typeof__(memory) *split_shmem = &memory;
static split_transaction_desc_t split_transaction_table[NUM_TOTAL_TRANSACTIONS];
static kb_settings_split_pointing_t desired, peer, phenom_synced_devices;
static bool phenom_synced_devices_valid = false, phenom_applied_devices_valid = true;
static uint32_t phenom_synced_raw;
static struct { uint32_t raw; } phenom_via_config;
typedef struct { uint8_t raw[4]; } kb_settings_led_colors_t;
static kb_settings_led_colors_t phenom_synced_led_colors;
static bool master = true, connected = true;
static uint32_t now;
static int attempts, fail_stage = -1;
static bool is_keyboard_master(void) { return master; }
static bool is_transport_connected(void) { return connected; }
static uint32_t timer_read32(void) { return now; }
static uint32_t timer_elapsed32(uint32_t previous) { return now - previous; }
static kb_settings_split_pointing_t get_split_pointing_settings(void) { return desired; }
static void set_split_pointing_settings(kb_settings_split_pointing_t value) { peer = value; }
static kb_settings_led_colors_t get_settings_led_colors(void) {
    return (kb_settings_led_colors_t){0};
}
/* Checksum details are outside this probe's scope; preserve matching checks. */
static uint8_t crc8(const void *data, size_t len) {
    const uint8_t *p = data; uint8_t value = 0;
    while (len--) value ^= *p++;
    return value;
}
static void slave_rpc_info_callback(uint8_t, const void *, uint8_t, void *);
static void slave_rpc_exec_callback(uint8_t, const void *, uint8_t, void *);
static bool transport_write(int id, const void *data, uint8_t len) {
    if (id == PUT_RPC_INFO) ++attempts;
    if (fail_stage == id) return false;
    if (id == PUT_RPC_INFO) {
        memcpy(&memory.rpc_info, data, len);
        slave_rpc_info_callback(len, data, 0, NULL);
    } else if (id == PUT_RPC_REQ_DATA) {
        memcpy(memory.rpc_m2s_buffer, data, len);
    } else if (id == EXECUTE_RPC) {
        slave_rpc_exec_callback(len, data, 0, NULL);
    }
    return true;
}
static bool transport_read(int id, void *data, uint8_t len) {
    (void)data; (void)len;
    return fail_stage != id;
}
#define transaction_rpc_send(id, len, data) transaction_rpc_exec(id, len, data, 0, NULL)
'''

SCENARIOS = r'''
#define CHECK(condition, message) do { if (!(condition)) { \
    fprintf(stderr, "FAIL: %s (master_axis=%u peer_axis=%u cache_valid=%d attempts=%d)\n", \
            message, desired.axis[1], peer.axis[1], phenom_synced_devices_valid, attempts); \
    return 1; } } while (0)
static void tick(void) { now += 100; housekeeping_task_user(); }
static void set_a(void) {
    for (unsigned i = 0; i < SPLIT_POINTING_DEVICE_COUNT; ++i) {
        desired.axis[i] = 1; desired.dpi_idx[i] = 3;
    }
}
static void set_b(void) {
    for (unsigned i = 0; i < SPLIT_POINTING_DEVICE_COUNT; ++i) {
        desired.axis[i] = 2; desired.dpi_idx[i] = 5;
    }
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    _Static_assert(sizeof(kb_settings_split_pointing_t) == 40, "Unexpected payload layout");
    split_transaction_table[RPC_PHENOM_SPLIT_POINTING_SETTINGS].slave_callback =
        phenom_sync_split_pointing_settings_rpc;
    set_a();
    if (!strcmp(argv[1], "slave")) {
        master = false; tick();
        CHECK(attempts == 0, "slave must not initiate RPC");
    } else if (!strcmp(argv[1], "throttle")) {
        now = 99; housekeeping_task_user();
        CHECK(attempts == 0, "do not sync before 100 ms");
        now = 100; housekeeping_task_user();
        CHECK(attempts == 1, "sync at 100 ms");
        set_b(); now = 199; housekeeping_task_user();
        CHECK(attempts == 1, "throttle changed settings");
        now = 200; housekeeping_task_user();
        CHECK(attempts == 2 && !memcmp(&desired, &peer, sizeof(peer)), "next allowed sync");
    } else {
        tick();
        CHECK(attempts == 1 && !memcmp(&desired, &peer, sizeof(peer)), "startup sync");
        if (!strcmp(argv[1], "startup")) {
            for (int i = 0; i < 10; ++i) tick();
            CHECK(attempts == 1, "suppress unchanged settings");
        } else if (!strcmp(argv[1], "reconnect")) {
            connected = false; tick();
            CHECK(attempts == 1, "no send while disconnected");
            CHECK(!phenom_synced_devices_valid, "invalidate on observed disconnect");
            memset(&peer, 0, sizeof(peer)); connected = true; tick();
            CHECK(attempts == 2 && !memcmp(&desired, &peer, sizeof(peer)), "resend on reconnect");
        } else if (!strcmp(argv[1], "revert_after_final_failure")) {
            set_b(); fail_stage = GET_RPC_RESP_DATA; tick();
            CHECK(peer.axis[1] == 2 && peer.dpi_idx[1] == 5, "peer applied B before failure");
            CHECK(!memcmp(&phenom_synced_devices.axis, "\1\1\1\1", 4), "old cache remains A");
            CHECK(connected, "ordinary transport remains connected");
            set_a(); fail_stage = -1;
            for (int i = 0; i < 10; ++i) tick();
            CHECK(!memcmp(&desired, &peer, sizeof(peer)), "restore A after indeterminate failure");
            CHECK(attempts == 3, "one corrective resend, then suppression");
        } else if (!strncmp(argv[1], "retry_", 6)) {
            int stage = atoi(argv[1] + 6);
            CHECK(stage >= PUT_RPC_INFO && stage <= GET_RPC_RESP_DATA, "valid failure stage");
            set_b(); fail_stage = stage; tick();
            CHECK(attempts == 2, "attempt B");
            fail_stage = -1; tick();
            CHECK(attempts == 3 && !memcmp(&desired, &peer, sizeof(peer)), "retry B after error");
            tick(); CHECK(attempts == 3, "suppress after successful retry");
        } else return 2;
    }
    puts("PASS"); return 0;
}
'''


def harness(variant):
    pointing = (ROOT / f"keyboards/ergohaven/{variant}/rev1/pointing.c").read_text()
    header = (ROOT / "keyboards/ergohaven/src/eh_pointing.h").read_text()
    payload_match = re.search(r"typedef struct \{.*?\} kb_settings_split_pointing_t;", header, re.S)
    if payload_match is None:
        raise ValueError("Missing settings payload type")
    payload = payload_match.group()
    config = (ROOT / f"keyboards/ergohaven/{variant}/rev1/config.h").read_text()
    buffer_match = re.search(r"^#define RPC_M2S_BUFFER_SIZE \d+", config, re.M)
    if buffer_match is None:
        raise ValueError(f"Missing RPC buffer size: {variant}")
    buffer = buffer_match.group()
    rpc = (ROOT / "quantum/split_common/transactions.c").read_text()
    return "\n".join([
        PREAMBLE, buffer, payload, SUPPORT,
        function(pointing, "phenom_sync_split_pointing_settings_rpc"),
        function(rpc, "slave_rpc_info_callback"),
        function(rpc, "slave_rpc_exec_callback"),
        function(rpc, "transaction_rpc_exec"),
        function(pointing, "housekeeping_task_user"), SCENARIOS,
    ])


class SettingsSyncTests(unittest.TestCase):
    def test_production_sync(self):
        cases = ("startup", "slave", "throttle", "reconnect",
                 "revert_after_final_failure", "retry_0", "retry_1", "retry_2", "retry_3")
        with tempfile.TemporaryDirectory(prefix="phenom-sync-", dir=os.environ.get("TMPDIR")) as tmp:
            for variant in VARIANTS:
                source = Path(tmp) / f"{variant}.c"
                executable = Path(tmp) / variant
                source.write_text(harness(variant))
                subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu11", "-Wall", "-Wextra",
                                "-Werror", "-Wno-unused-parameter", "-fsanitize=undefined",
                                "-fno-sanitize-recover=all", str(source), "-o", str(executable)], check=True)
                for case in cases:
                    with self.subTest(variant=variant, case=case):
                        result = subprocess.run([str(executable), case], capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        print(f"{variant}: {case}: PASS", flush=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
