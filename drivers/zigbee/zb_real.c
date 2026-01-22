/**
 * @file zb_real.c
 * @brief Real Zigbee adapter - lifecycle and state management
 *
 * ESP32-C6 Zigbee Bridge OS - Coordinator lifecycle management
 * Callbacks in zb_callback.c, commands in zb_cmd.c (per constitution <200 LOC)
 */

#include "esp_zigbee_core.h"
#include "zb_real_internal.h"

/* Static State */
static zb_state_t s_state = ZB_STATE_UNINITIALIZED;
static zb_nwk_entry_t s_nwk_cache[ZB_MAX_DEVICES];
static zb_pending_cmd_t s_pending_cmds[ZB_MAX_PENDING];
static uint32_t s_drop_counter = 0;

/* State Accessors */
zb_state_t zb_get_state(void) { return s_state; }
void zb_set_state_ready(void) { s_state = ZB_STATE_READY; }
void zb_set_state_error(void) { s_state = ZB_STATE_ERROR; }

/* Address Cache Helpers */
zb_nwk_entry_t *nwk_cache_find(os_eui64_t eui64) {
  for (int i = 0; i < ZB_MAX_DEVICES; i++) {
    if (s_nwk_cache[i].valid && s_nwk_cache[i].eui64 == eui64)
      return &s_nwk_cache[i];
  }
  return NULL;
}

zb_nwk_entry_t *nwk_cache_find_by_nwk(uint16_t nwk_addr) {
  for (int i = 0; i < ZB_MAX_DEVICES; i++) {
    if (s_nwk_cache[i].valid && s_nwk_cache[i].nwk_addr == nwk_addr)
      return &s_nwk_cache[i];
  }
  return NULL;
}

zb_nwk_entry_t *nwk_cache_insert(os_eui64_t eui64, uint16_t nwk_addr) {
  zb_nwk_entry_t *entry = nwk_cache_find(eui64);
  if (entry) {
    entry->nwk_addr = nwk_addr;
    return entry;
  }
  for (int i = 0; i < ZB_MAX_DEVICES; i++) {
    if (!s_nwk_cache[i].valid) {
      s_nwk_cache[i].eui64 = eui64;
      s_nwk_cache[i].nwk_addr = nwk_addr;
      s_nwk_cache[i].valid = true;
      return &s_nwk_cache[i];
    }
  }
  return NULL;
}

void nwk_cache_remove(os_eui64_t eui64) {
  zb_nwk_entry_t *entry = nwk_cache_find(eui64);
  if (entry)
    entry->valid = false;
}

/* Pending Command Helpers */
zb_pending_cmd_t *pending_alloc(os_corr_id_t corr_id, uint16_t cluster_id,
                                uint8_t cmd_id) {
  uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
  for (int i = 0; i < ZB_MAX_PENDING; i++) {
    if (!s_pending_cmds[i].in_use) {
      s_pending_cmds[i].corr_id = corr_id;
      s_pending_cmds[i].cluster_id = cluster_id;
      s_pending_cmds[i].cmd_id = cmd_id;
      s_pending_cmds[i].timestamp_ms = now;
      s_pending_cmds[i].in_use = true;
      return &s_pending_cmds[i];
    }
  }
  return NULL;
}

zb_pending_cmd_t *pending_find_by_tsn(uint8_t tsn) {
  for (int i = 0; i < ZB_MAX_PENDING; i++) {
    if (s_pending_cmds[i].in_use && s_pending_cmds[i].tsn == tsn)
      return &s_pending_cmds[i];
  }
  return NULL;
}

void pending_free(zb_pending_cmd_t *slot) {
  if (slot)
    slot->in_use = false;
}

void pending_purge_timeouts(void) {
  uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
  for (int i = 0; i < ZB_MAX_PENDING; i++) {
    if (s_pending_cmds[i].in_use &&
        (now - s_pending_cmds[i].timestamp_ms) > ZB_CMD_TIMEOUT_MS) {
      struct {
        os_corr_id_t corr_id;
        uint16_t err;
      } p = {s_pending_cmds[i].corr_id, 0xFFFF};
      emit_event(OS_EVENT_ZB_CMD_ERROR, &p, sizeof(p));
      LOG_W(ZB_MODULE, "Cmd timeout corr=%" PRIu32, s_pending_cmds[i].corr_id);
      s_pending_cmds[i].in_use = false;
    }
  }
}

/* Event Emission Helper */
void emit_event(os_event_type_t type, const void *payload, uint8_t len) {
  os_err_t err = os_event_emit(type, payload, len);
  if (err == OS_ERR_FULL) {
    s_drop_counter++;
    LOG_W(ZB_MODULE, "Event dropped (total: %" PRIu32 ")", s_drop_counter);
  }
}

/* Zigbee Task */
static void zb_task(void *pv) {
  (void)pv;
  esp_zb_start(true); /* Start Zigbee stack before main loop */
  esp_zb_stack_main_loop();
  vTaskDelete(NULL);
}

/* Public API - Lifecycle */
zb_err_t zb_init(void) {
  if (s_state != ZB_STATE_UNINITIALIZED)
    return OS_ERR_BUSY;
  s_state = ZB_STATE_INITIALIZING;
  
  /* Set boot time for SC-001 performance metric */
  zb_perf_set_boot_time(xTaskGetTickCount() * portTICK_PERIOD_MS);
  
  memset(s_nwk_cache, 0, sizeof(s_nwk_cache));
  memset(s_pending_cmds, 0, sizeof(s_pending_cmds));
  esp_zb_cfg_t cfg = {
      .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
      .install_code_policy = false,
      .nwk_cfg.zczr_cfg = {.max_children = ZB_MAX_DEVICES},
  };
  esp_zb_init(&cfg);
  esp_zb_core_action_handler_register(zb_core_action_cb);
  esp_zb_zcl_command_send_status_handler_register(zb_cmd_send_status_cb);
  xTaskCreate(zb_task, "zigbee", ZB_TASK_STACK_SIZE, NULL, ZB_TASK_PRIORITY,
              NULL);
  LOG_I(ZB_MODULE, "Zigbee adapter initialized");
  return OS_OK;
}

zb_err_t zb_start_coordinator(void) {
  if (s_state == ZB_STATE_READY)
    return OS_OK;
  if (s_state != ZB_STATE_INITIALIZING)
    return OS_ERR_NOT_READY;
  return OS_OK;
}

zb_err_t zb_set_permit_join(uint16_t seconds) {
  if (s_state != ZB_STATE_READY)
    return OS_ERR_NOT_READY;
  if (seconds > 254)
    seconds = 254;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t err = esp_zb_bdb_open_network((uint8_t)seconds);
  esp_zb_lock_release();
  return (err == ESP_OK) ? OS_OK : OS_ERR_BUSY;
}
