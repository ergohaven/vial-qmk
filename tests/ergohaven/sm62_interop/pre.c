#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <setjmp.h>
#include "macro_storage.h"
#include "flash_guard.h"
#include "vial_extension.h"
static uint8_t eeprom[8192],reply[32];
