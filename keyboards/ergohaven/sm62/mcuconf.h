// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include_next <mcuconf.h>

/* SM62 isolated owner experiment. QMK native/input remains on core0. */
#if defined(SM62_DUALCORE_RENDER)
#    undef RP_CORE1_START
#    define RP_CORE1_START TRUE
#endif

#undef RP_SPI_USE_SPI0
#define RP_SPI_USE_SPI0 TRUE

#undef RP_PWM_USE_PWM3
#define RP_PWM_USE_PWM3 TRUE
