// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define SM62_DC_TRACE_VERSION 0x18U
#define SM62_DC_TRACE_PARTS 96U
#define SM62_DC_TRACE_WORDS (24U + SM62_DC_TRACE_PARTS * 8U)
#define SM62_DC_TRACE_PAGES (SM62_DC_TRACE_WORDS / 6U)
bool sm62_dc_diag_raw(uint8_t *data, uint8_t length); /* core0 only */
void sm62_dc_scan_rpc(uint8_t in_len, const void *in, uint8_t out_len, void *out);
bool sm62_dc_peer_scan(uint8_t command, uint8_t *reply); /* board RPC transport */
void sm62_dc_trace_poll(void); /* core1 safe boundary */
void sm62_dc_trace_loop(void); /* core0 only */
void sm62_dc_trace_scan(void); /* core0, immediately before local GPIO sampling */
void sm62_dc_trace_key(void); /* core0 only */
void sm62_dc_trace_packet(const uint8_t *data, uint8_t length); /* core0 */
void sm62_dc_trace_abort(void); /* core1 only */
void sm62_dc_trace_part(bool begin,bool first,bool last,int16_t x1,int16_t y1,int16_t x2,int16_t y2);
void sm62_dc_trace_panel(uint8_t id,bool begin);
