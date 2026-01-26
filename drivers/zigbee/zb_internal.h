/**
 * @file zb_internal.h
 * @brief Internal shared state between zb_real.c and zb_cmd.c
 *
 * ESP32-C6 Zigbee Bridge OS - Internal Zigbee types and accessors
 */

#ifndef ZB_INTERNAL_H
#define ZB_INTERNAL_H

#include "os_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* State check */
bool zb_is_ready(void);

/* NWK cache lookup - returns NWK address or 0xFFFF if not found */
uint16_t zb_lookup_nwk(os_eui64_t eui64);

/* Pending command correlation - opaque handle */
typedef struct zb_pending_slot *zb_pending_handle_t;
zb_pending_handle_t zb_pending_alloc(os_corr_id_t corr_id);
void zb_pending_set_tsn(zb_pending_handle_t slot, uint8_t tsn);
void zb_pending_free(zb_pending_handle_t slot);

#ifdef __cplusplus
}
#endif

#endif /* ZB_INTERNAL_H */
