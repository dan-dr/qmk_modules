// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>

#ifdef FSR_SENTINEL_NVM_DISABLE

static inline void fsr_sentinel_nvm_load(void) {}
static inline void fsr_sentinel_nvm_mark_dirty(void) {}
static inline void fsr_sentinel_nvm_task(void) {}

#else

#ifndef FSR_SENTINEL_NVM_SIZE
#    define FSR_SENTINEL_NVM_SIZE 128
#endif

#ifndef FSR_SENTINEL_NVM_ADDR
#    define FSR_SENTINEL_NVM_ADDR \
        ((uint16_t)(TOTAL_EEPROM_BYTE_COUNT - FSR_SENTINEL_NVM_SIZE))
#endif

#ifndef FSR_SENTINEL_NVM_SAVE_DELAY_MS
#    define FSR_SENTINEL_NVM_SAVE_DELAY_MS 500
#endif

void fsr_sentinel_nvm_load(void);
void fsr_sentinel_nvm_mark_dirty(void);
void fsr_sentinel_nvm_task(void);

#endif
