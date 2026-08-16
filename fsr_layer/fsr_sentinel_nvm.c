// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fsr_sentinel_nvm.h"

#include "fsr_sentinel_runtime.h"
#include "eeprom.h"
#include "quantum.h"

#include <string.h>

enum {
    FSR_SENTINEL_NVM_MAGIC = 0x46535253u, /* 'FSRS' */
    FSR_SENTINEL_NVM_V1_COUNT = 7,
    FSR_SENTINEL_NVM_V2_COUNT = 9,
    FSR_SENTINEL_NVM_V3_COUNT = 13,
    FSR_SENTINEL_NVM_ATLAS_COUNT = 17,
    FSR_SENTINEL_NVM_PARAM_COUNT = FSR_SENTINEL_NVM_V1_COUNT +
                                   FSR_SENTINEL_NVM_V2_COUNT +
                                   FSR_SENTINEL_NVM_V3_COUNT +
                                   FSR_SENTINEL_NVM_ATLAS_COUNT,
};

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  schema;
    uint8_t  active_algorithm;
    int16_t  params[FSR_SENTINEL_NVM_PARAM_COUNT];
    uint16_t crc;
} fsr_sentinel_nvm_blob_t;

_Static_assert(sizeof(fsr_sentinel_nvm_blob_t) <= FSR_SENTINEL_NVM_SIZE,
               "FSR Sentinel NVM blob exceeds reserved EEPROM");
_Static_assert(FSR_SENTINEL_NVM_PARAM_COUNT == 46,
               "FSR Sentinel NVM param packing must match runtime registries");

static bool     nvm_dirty;
static bool     nvm_loading;
static uint16_t nvm_dirty_since;

static uint16_t nvm_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void pack_algorithm(uint8_t algorithm, int16_t *out, uint8_t count) {
    for (uint8_t index = 0; index < count; index++) {
        int16_t value = 0;
        fsr_sentinel_runtime_param_get(algorithm, index, &value);
        out[index] = value;
    }
}

static bool unpack_algorithm(uint8_t algorithm, const int16_t *in,
                             uint8_t count) {
    for (uint8_t index = 0; index < count; index++) {
        if (!fsr_sentinel_runtime_param_set(algorithm, index, in[index])) {
            return false;
        }
    }
    return true;
}

static void pack_blob(fsr_sentinel_nvm_blob_t *blob) {
    memset(blob, 0, sizeof(*blob));
    blob->magic            = FSR_SENTINEL_NVM_MAGIC;
    blob->schema           = FSR_SENTINEL_RUNTIME_SCHEMA_VERSION;
    blob->active_algorithm = fsr_sentinel_runtime_active_algorithm();
    int16_t *cursor        = blob->params;
    pack_algorithm(FSR_SENTINEL_ALGORITHM_V1, cursor, FSR_SENTINEL_NVM_V1_COUNT);
    cursor += FSR_SENTINEL_NVM_V1_COUNT;
    pack_algorithm(FSR_SENTINEL_ALGORITHM_V2, cursor, FSR_SENTINEL_NVM_V2_COUNT);
    cursor += FSR_SENTINEL_NVM_V2_COUNT;
    pack_algorithm(FSR_SENTINEL_ALGORITHM_V3, cursor, FSR_SENTINEL_NVM_V3_COUNT);
    cursor += FSR_SENTINEL_NVM_V3_COUNT;
    pack_algorithm(FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1, cursor,
                   FSR_SENTINEL_NVM_ATLAS_COUNT);
    blob->crc = nvm_crc16((const uint8_t *)blob,
                          (uint16_t)(sizeof(*blob) - sizeof(blob->crc)));
}

static bool apply_blob(const fsr_sentinel_nvm_blob_t *blob) {
    if (blob->magic != FSR_SENTINEL_NVM_MAGIC ||
        blob->schema != FSR_SENTINEL_RUNTIME_SCHEMA_VERSION) {
        return false;
    }
    uint16_t crc = nvm_crc16((const uint8_t *)blob,
                             (uint16_t)(sizeof(*blob) - sizeof(blob->crc)));
    if (crc != blob->crc) {
        return false;
    }
    if (blob->active_algorithm < FSR_SENTINEL_ALGORITHM_V1 ||
        blob->active_algorithm > FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        return false;
    }

    nvm_loading = true;
    const int16_t *cursor = blob->params;
    bool ok = unpack_algorithm(FSR_SENTINEL_ALGORITHM_V1, cursor,
                               FSR_SENTINEL_NVM_V1_COUNT);
    cursor += FSR_SENTINEL_NVM_V1_COUNT;
    ok = ok && unpack_algorithm(FSR_SENTINEL_ALGORITHM_V2, cursor,
                                FSR_SENTINEL_NVM_V2_COUNT);
    cursor += FSR_SENTINEL_NVM_V2_COUNT;
    ok = ok && unpack_algorithm(FSR_SENTINEL_ALGORITHM_V3, cursor,
                                FSR_SENTINEL_NVM_V3_COUNT);
    cursor += FSR_SENTINEL_NVM_V3_COUNT;
    ok = ok && unpack_algorithm(FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1, cursor,
                                FSR_SENTINEL_NVM_ATLAS_COUNT);
    ok = ok && fsr_sentinel_runtime_set_algorithm(blob->active_algorithm);
    if (!ok) {
        fsr_sentinel_runtime_params_reset(FSR_SENTINEL_ALGORITHM_V1);
        fsr_sentinel_runtime_params_reset(FSR_SENTINEL_ALGORITHM_V2);
        fsr_sentinel_runtime_params_reset(FSR_SENTINEL_ALGORITHM_V3);
        fsr_sentinel_runtime_params_reset(FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1);
        fsr_sentinel_runtime_set_algorithm(FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1);
    }
    nvm_loading = false;
    nvm_dirty   = false;
    return ok;
}

static void save_now(void) {
    fsr_sentinel_nvm_blob_t blob;
    pack_blob(&blob);
    eeprom_update_block(&blob, (void *)(uintptr_t)FSR_SENTINEL_NVM_ADDR,
                        sizeof(blob));
    nvm_dirty = false;
}

void fsr_sentinel_nvm_load(void) {
    fsr_sentinel_nvm_blob_t blob;
    eeprom_read_block(&blob, (const void *)(uintptr_t)FSR_SENTINEL_NVM_ADDR,
                      sizeof(blob));
    if (!apply_blob(&blob)) {
        nvm_dirty = false;
    }
}

void fsr_sentinel_nvm_mark_dirty(void) {
    if (nvm_loading) {
        return;
    }
    nvm_dirty       = true;
    nvm_dirty_since = timer_read();
}

void fsr_sentinel_nvm_task(void) {
    if (!nvm_dirty) {
        return;
    }
    if (timer_elapsed(nvm_dirty_since) < FSR_SENTINEL_NVM_SAVE_DELAY_MS) {
        return;
    }
    save_now();
}
