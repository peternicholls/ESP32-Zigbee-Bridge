/**
 * @file zb_real_internal.h
 * @brief Internal header for zb_real.c and zb_cmd.c
 *
 * Shared types and helpers between lifecycle and command modules.
 * NOT part of public API.
 */

#ifndef ZB_REAL_INTERNAL_H
#define ZB_REAL_INTERNAL_H

#include "os_event.h"
#include "os_log.h"
#include "os_types.h"
#include "zb_adapter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define ZB_MODULE "ZB_REAL"

/* ─────────────────────────────────────────────────────────────────────────────
 * Configuration
 * ─────────────────────────────────────────────────────────────────────────────
 */

#define ZB_MAX_DEVICES 64
#define ZB_MAX_PENDING 16
#define ZB_CMD_TIMEOUT_MS 10000
#define ZB_TASK_STACK_SIZE 4096
#define ZB_TASK_PRIORITY 5

/* ─────────────────────────────────────────────────────────────────────────────
 * Types
 * ─────────────────────────────────────────────────────────────────────────────
 */

typedef enum {
  ZB_STATE_UNINITIALIZED = 0,
  ZB_STATE_INITIALIZING,
  ZB_STATE_READY,
  ZB_STATE_ERROR,
} zb_state_t;

typedef struct {
  os_eui64_t eui64;
  uint16_t nwk_addr;
  bool valid;
} zb_nwk_entry_t;

typedef struct {
  uint8_t tsn;
  os_corr_id_t corr_id;
  uint16_t cluster_id;
  uint8_t cmd_id;
  uint32_t timestamp_ms;
  bool in_use;
} zb_pending_cmd_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal API (implemented in zb_real.c, used by zb_cmd.c/zb_callback.c)
 * ─────────────────────────────────────────────────────────────────────────────
 */

zb_state_t zb_get_state(void);
void zb_set_state_ready(void);
void zb_set_state_error(void);
zb_nwk_entry_t *nwk_cache_find(os_eui64_t eui64);
zb_nwk_entry_t *nwk_cache_find_by_nwk(uint16_t nwk_addr);
zb_nwk_entry_t *nwk_cache_insert(os_eui64_t eui64, uint16_t nwk_addr);
void nwk_cache_remove(os_eui64_t eui64);
zb_pending_cmd_t *pending_alloc(os_corr_id_t corr_id, uint16_t cluster_id,
                                uint8_t cmd_id);
zb_pending_cmd_t *pending_find_by_tsn(uint8_t tsn);
void pending_free(zb_pending_cmd_t *slot);
void pending_purge_timeouts(void);
void emit_event(os_event_type_t type, const void *payload, uint8_t len);
void zb_perf_set_boot_time(uint32_t ms);

#ifdef CONFIG_IDF_TARGET_ESP32C6
/* Callbacks (implemented in zb_callback.c, registered in zb_real.c) */
#include "esp_zigbee_core.h"
void zb_cmd_send_status_cb(esp_zb_zcl_command_send_status_message_t msg);
esp_err_t zb_core_action_cb(esp_zb_core_action_callback_id_t id,
                            const void *msg);
#endif

#endif /* ZB_REAL_INTERNAL_H */
