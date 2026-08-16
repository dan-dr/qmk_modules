// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
 * Carve persistent Sentinel settings from the end of logical EEPROM.
 * Argos (if present) already reserves its own block via
 * DYNAMIC_KEYMAP_EEPROM_MAX_ADDR; shrink further so Argos stays contiguous
 * and FSR owns the absolute tail.
 */
#ifndef FSR_SENTINEL_NVM_SIZE
#    define FSR_SENTINEL_NVM_SIZE 128
#endif

#ifdef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#    undef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#endif

#ifdef ARGOS_EEPROM_SIZE_CALC
#    define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR          \
        (TOTAL_EEPROM_BYTE_COUNT - 1 - ARGOS_EEPROM_SIZE_CALC - \
         FSR_SENTINEL_NVM_SIZE)
#else
#    define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR \
        (TOTAL_EEPROM_BYTE_COUNT - 1 - FSR_SENTINEL_NVM_SIZE)
#endif

#define FSR_SENTINEL_NVM_ADDR \
    ((uint16_t)(TOTAL_EEPROM_BYTE_COUNT - FSR_SENTINEL_NVM_SIZE))
