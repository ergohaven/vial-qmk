#pragma once
#include <stdint.h>
#include <stdbool.h>
#if defined(SM62_DUALCORE_RENDER)
#include "keyboards/ergohaven/sm62/dualcore_trace.h"
#define sm62_trace_loop() sm62_dc_trace_loop()
#define sm62_trace_abort() sm62_dc_trace_abort()
#define sm62_trace_edge(a,b) ((void)0)
#define sm62_trace_count(a) ((void)0)
#define sm62_trace_part(a,b,c,d,e,f,g) sm62_dc_trace_part(a,b,c,d,e,f,g)
#define sm62_trace_panel(a,b) sm62_dc_trace_panel(a,b)
#elif defined(SM62_STAGE_TRACE)
void sm62_trace_loop(void);
void sm62_trace_abort(void);
void sm62_trace_edge(uint8_t id,bool begin);
void sm62_trace_count(uint8_t id);
void sm62_trace_part(bool begin,bool first,bool last,int16_t x1,int16_t y1,int16_t x2,int16_t y2);
void sm62_trace_panel(uint8_t id,bool begin);
#else
#define sm62_trace_loop() ((void)0)
#define sm62_trace_abort() ((void)0)
#define sm62_trace_edge(a,b) ((void)0)
#define sm62_trace_count(a) ((void)0)
#define sm62_trace_part(a,b,c,d,e,f,g) ((void)0)
#define sm62_trace_panel(a,b) ((void)0)
#endif
