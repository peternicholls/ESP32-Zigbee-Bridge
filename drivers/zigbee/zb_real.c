/**
 * @file zb_real.c
 * @brief Real Zigbee adapter implementation using ESP-Zigbee stack
 *
 * ESP32-C6 Zigbee Bridge OS - Real Zigbee adapter (M4)
 *
 * Implements zb_adapter.h contract for ESP32-C6 hardware with:
 * - Coordinator network formation
 * - Device join/leave handling
 * - Event emission to OS bus
 */

#include "os_event.h"
#include "os_fibre.h"
#include "os_log.h"
#include "zb_adapter.h"
#include "zb_internal.h"

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <string.h>

#define ZB_MODULE "ZB_REAL"

/* Configuration */
#define ZB_MAX_DEVICES 64
#define ZB_MAX_PENDING 16
#define ZB_TASK_STACK_SIZE 4096
#define ZB_TASK_PRIORITY 5
#define ZB_CMD_TIMEOUT_MS 5000

/* Forward declarations - internal helpers */
static void zb_task(void *arg);
static void zb_send_status_cb(esp_zb_zcl_command_send_status_message_t message);
static esp_err_t zb_core_action_cb(esp_zb_core_action_callback_id_t callback_id,
                                   const void *message);

/* ─────────────────────────────────────────────────────────────────────────────
 * State Machine
 * ─────────────────────────────────────────────────────────────────────────────
 */

typedef enum {
  ZB_STATE_UNINITIALIZED = 0,
  ZB_STATE_INITIALIZING,
  ZB_STATE_READY,
  ZB_STATE_ERROR,
} zb_state_t;

static zb_state_t s_zb_state = ZB_STATE_UNINITIALIZED;

/* State transition helper - T018 */
static const char *zb_state_name(zb_state_t state) {
  switch (state) {
  case ZB_STATE_UNINITIALIZED:
    return "UNINITIALIZED";
  case ZB_STATE_INITIALIZING:
    return "INITIALIZING";
  case ZB_STATE_READY:
    return "READY";
  case ZB_STATE_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

static bool zb_state_transition(zb_state_t new_state) {
  /* Validate allowed transitions */
  bool valid = false;
  switch (s_zb_state) {
  case ZB_STATE_UNINITIALIZED:
    valid = (new_state == ZB_STATE_INITIALIZING);
    break;
  case ZB_STATE_INITIALIZING:
    valid = (new_state == ZB_STATE_READY || new_state == ZB_STATE_ERROR);
    break;
  case ZB_STATE_READY:
    valid = (new_state == ZB_STATE_ERROR);
    break;
  case ZB_STATE_ERROR:
    valid = false; /* Terminal state */
    break;
  }
  if (!valid) {
    LOG_W(ZB_MODULE, "Invalid state transition: %s -> %s",
          zb_state_name(s_zb_state), zb_state_name(new_state));
    return false;
  }
  LOG_I(ZB_MODULE, "State: %s -> %s", zb_state_name(s_zb_state),
        zb_state_name(new_state));
  s_zb_state = new_state;
  return true;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Address Cache (EUI64 ↔ NWK mapping)
 * ─────────────────────────────────────────────────────────────────────────────
 */

typedef struct {
  os_eui64_t eui64;
  uint16_t nwk_addr;
  uint32_t last_seen_ms;
} zb_nwk_entry_t;

static zb_nwk_entry_t s_nwk_cache[ZB_MAX_DEVICES];
static uint8_t s_nwk_count = 0;

/* Address cache helpers - T007-T009 */
static zb_nwk_entry_t *nwk_cache_insert(os_eui64_t eui64, uint16_t nwk_addr) {
  /* First check if entry exists */
  for (uint8_t i = 0; i < s_nwk_count; i++) {
    if (s_nwk_cache[i].eui64 == eui64) {
      s_nwk_cache[i].nwk_addr = nwk_addr;
      s_nwk_cache[i].last_seen_ms =
          (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
      return &s_nwk_cache[i];
    }
  }
  /* Add new entry if space available */
  if (s_nwk_count >= ZB_MAX_DEVICES) {
    LOG_W(ZB_MODULE, "NWK cache full, cannot add " OS_EUI64_FMT,
          OS_EUI64_ARG(eui64));
    return NULL;
  }
  zb_nwk_entry_t *entry = &s_nwk_cache[s_nwk_count++];
  entry->eui64 = eui64;
  entry->nwk_addr = nwk_addr;
  entry->last_seen_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
  LOG_D(ZB_MODULE, "NWK cache add: " OS_EUI64_FMT " -> 0x%04X",
        OS_EUI64_ARG(eui64), nwk_addr);
  return entry;
}

static zb_nwk_entry_t *nwk_cache_lookup_by_eui64(os_eui64_t eui64) {
  for (uint8_t i = 0; i < s_nwk_count; i++) {
    if (s_nwk_cache[i].eui64 == eui64) {
      return &s_nwk_cache[i];
    }
  }
  return NULL;
}

static bool nwk_cache_remove(os_eui64_t eui64) {
  for (uint8_t i = 0; i < s_nwk_count; i++) {
    if (s_nwk_cache[i].eui64 == eui64) {
      /* Move last entry to this slot (swap-remove) */
      if (i < s_nwk_count - 1) {
        s_nwk_cache[i] = s_nwk_cache[s_nwk_count - 1];
      }
      s_nwk_count--;
      LOG_D(ZB_MODULE, "NWK cache remove: " OS_EUI64_FMT, OS_EUI64_ARG(eui64));
      return true;
    }
  }
  return false;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Correlation Tracking (command → response matching)
 * ─────────────────────────────────────────────────────────────────────────────
 */

typedef struct {
  uint8_t tsn;
  bool tsn_valid;
  os_corr_id_t corr_id;
  uint32_t timestamp_ms;
  bool in_use;
} zb_pending_cmd_t;

static zb_pending_cmd_t s_pending_cmds[ZB_MAX_PENDING];

/* Correlation tracking helpers - T012-T015 */
static zb_pending_cmd_t *pending_cmd_alloc(os_corr_id_t corr_id) {
  for (uint8_t i = 0; i < ZB_MAX_PENDING; i++) {
    if (!s_pending_cmds[i].in_use) {
      s_pending_cmds[i].corr_id = corr_id;
      s_pending_cmds[i].tsn = 0;
      s_pending_cmds[i].tsn_valid =
          false; /* Set true via pending_cmd_set_tsn */
      s_pending_cmds[i].timestamp_ms =
          (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
      s_pending_cmds[i].in_use = true;
      return &s_pending_cmds[i];
    }
  }
  LOG_W(ZB_MODULE, "Pending cmd slots full");
  return NULL;
}

static void pending_cmd_set_tsn(zb_pending_cmd_t *slot, uint8_t tsn) {
  if (slot && slot->in_use) {
    slot->tsn = tsn;
    slot->tsn_valid = true;
  }
}

static zb_pending_cmd_t *pending_cmd_lookup_by_tsn(uint8_t tsn) {
  for (uint8_t i = 0; i < ZB_MAX_PENDING; i++) {
    if (s_pending_cmds[i].in_use && s_pending_cmds[i].tsn_valid &&
        s_pending_cmds[i].tsn == tsn) {
      return &s_pending_cmds[i];
    }
  }
  return NULL;
}

static void pending_cmd_free(zb_pending_cmd_t *slot) {
  if (slot) {
    slot->in_use = false;
  }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Contract Functions - Lifecycle (T023)
 * ─────────────────────────────────────────────────────────────────────────────
 */

zba_err_t zba_init(void) {
  if (s_zb_state != ZB_STATE_UNINITIALIZED) {
    return OS_ERR_ALREADY_EXISTS;
  }

  LOG_I(ZB_MODULE, "Initializing Zigbee stack (real)");

  /* Platform config for ESP32-C6 native radio */
  esp_zb_platform_config_t platform_cfg = {
      .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
      .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
  };
  ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

  /* Coordinator config */
  esp_zb_cfg_t zb_cfg = {
      .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
      .install_code_policy = false,
      .nwk_cfg.zczr_cfg.max_children = ZB_MAX_DEVICES,
  };
  esp_zb_init(&zb_cfg);

  /* Register callbacks */
  esp_zb_core_action_handler_register(zb_core_action_cb);
  esp_zb_zcl_command_send_status_handler_register(zb_send_status_cb);

  /* Create Zigbee task */
  BaseType_t ret = xTaskCreate(zb_task, "zigbee", ZB_TASK_STACK_SIZE, NULL,
                               ZB_TASK_PRIORITY, NULL);
  if (ret != pdPASS) {
    LOG_E(ZB_MODULE, "Failed to create Zigbee task");
    return OS_ERR_NO_MEM;
  }

  zb_state_transition(ZB_STATE_INITIALIZING);
  return OS_OK;
}

zba_err_t zba_start_coordinator(void) {
  if (s_zb_state == ZB_STATE_UNINITIALIZED) {
    return OS_ERR_NOT_INITIALIZED;
  }
  if (s_zb_state == ZB_STATE_READY || s_zb_state == ZB_STATE_INITIALIZING) {
    return OS_OK;
  }
  return OS_ERR_NOT_READY;
}

zba_err_t zba_set_permit_join(uint16_t seconds) {
  if (s_zb_state != ZB_STATE_READY) {
    return OS_ERR_NOT_READY;
  }

  LOG_I(ZB_MODULE, "Permit join for %u seconds", seconds);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_bdb_open_network(seconds);
  esp_zb_lock_release();
  return OS_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Contract Functions - Commands (implemented in zb_cmd.c)
 * ─────────────────────────────────────────────────────────────────────────────
 */

/* Note: zb_send_onoff, zb_send_level, zb_read_attrs, zb_configure_reporting,
 * zb_bind are implemented in zb_cmd.c */

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal - Zigbee Task (T022)
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void zb_task(void *arg) {
  (void)arg;
  LOG_I(ZB_MODULE, "Zigbee task started");
  esp_zb_start(false); /* No autostart - we handle commissioning via signals */
  esp_zb_stack_main_loop(); /* Never returns */
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal - Callbacks (T019-T020)
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void
zb_send_status_cb(esp_zb_zcl_command_send_status_message_t message) {
  LOG_D(ZB_MODULE, "send status cb: tsn=%u status=%d", message.tsn,
        message.status);
  /* Look up pending command by TSN */
  zb_pending_cmd_t *slot = pending_cmd_lookup_by_tsn(message.tsn);
  if (!slot) {
    LOG_W(ZB_MODULE, "No pending cmd for TSN %u", message.tsn);
    return;
  }
  /* Emit appropriate event */
  if (message.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
    os_event_emit(OS_EVENT_ZB_CMD_CONFIRM, &slot->corr_id,
                  sizeof(slot->corr_id));
    LOG_I(ZB_MODULE, "Command confirmed, corr_id=%" PRIu32, slot->corr_id);
  } else {
    os_event_emit(OS_EVENT_ZB_CMD_ERROR, &slot->corr_id, sizeof(slot->corr_id));
    LOG_W(ZB_MODULE, "Command failed, corr_id=%" PRIu32 " status=%d",
          slot->corr_id, message.status);
  }
  pending_cmd_free(slot);
}

static esp_err_t zb_core_action_cb(esp_zb_core_action_callback_id_t callback_id,
                                   const void *message) {
  LOG_D(ZB_MODULE, "core action cb called: %d", callback_id);
  /* TODO: Implement in Phase 6 */
  return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Signal Handler (called by ZBOSS stack) - T025-T028
 * ─────────────────────────────────────────────────────────────────────────────
 */

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal) {
  uint32_t *p_sig = signal->p_app_signal;
  esp_err_t status = signal->esp_err_status;
  esp_zb_app_signal_type_t sig_type = *p_sig;

  switch (sig_type) {
  case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
    /* T026: Stack framework initialized, start BDB commissioning */
    LOG_I(ZB_MODULE, "Stack initialized, starting commissioning");
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
    break;

  case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
  case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    /* T027: Device initialized or rebooted, start network formation */
    if (status == ESP_OK) {
      LOG_I(ZB_MODULE, "Device ready, starting network formation");
      esp_zb_bdb_start_top_level_commissioning(
          ESP_ZB_BDB_MODE_NETWORK_FORMATION);
    } else {
      LOG_E(ZB_MODULE, "Device init failed: %d", status);
      zb_state_transition(ZB_STATE_ERROR);
    }
    break;

  case ESP_ZB_BDB_SIGNAL_FORMATION:
    /* T028: Network formation complete */
    if (status == ESP_OK) {
      LOG_I(ZB_MODULE, "Network formed, PAN ID: 0x%04X, Channel: %d",
            esp_zb_get_pan_id(), esp_zb_get_current_channel());
      zb_state_transition(ZB_STATE_READY);
      os_event_emit(OS_EVENT_ZB_STACK_UP, NULL, 0);
      /* TODO: For production, disable auto permit-join or gate behind config.
       * Auto-enabling is a security risk - devices can join without user
       * action. */
      esp_zb_bdb_open_network(180);
      LOG_I(ZB_MODULE, "[DEV] Permit join enabled for 180 seconds");
    } else {
      LOG_E(ZB_MODULE, "Network formation failed: %d", status);
      zb_state_transition(ZB_STATE_ERROR);
    }
    break;

  case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
    /* T033: Device announcement - new device joined */
    esp_zb_zdo_signal_device_annce_params_t *dev =
        (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(
            signal->p_app_signal);
    os_eui64_t eui64 = 0;
    memcpy(&eui64, dev->ieee_addr, sizeof(os_eui64_t));
    LOG_I(ZB_MODULE, "Device joined: " OS_EUI64_FMT ", NWK: 0x%04X",
          OS_EUI64_ARG(eui64), dev->device_short_addr);
    nwk_cache_insert(eui64, dev->device_short_addr);
    /* Emit device announce event with EUI64 + NWK addr */
    struct {
      os_eui64_t eui64;
      uint16_t nwk_addr;
    } payload = {eui64, dev->device_short_addr};
    os_event_emit(OS_EVENT_ZB_ANNOUNCE, &payload, sizeof(payload));
    break;
  }

  default:
    LOG_D(ZB_MODULE, "Unhandled signal: 0x%02x, status: %d", sig_type, status);
    break;
  }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal API (exposed via zb_internal.h for zb_cmd.c)
 * ─────────────────────────────────────────────────────────────────────────────
 */

bool zb_is_ready(void) { return s_zb_state == ZB_STATE_READY; }

uint16_t zb_lookup_nwk(os_eui64_t eui64) {
  zb_nwk_entry_t *entry = nwk_cache_lookup_by_eui64(eui64);
  return entry ? entry->nwk_addr : 0xFFFF;
}

zb_pending_handle_t zb_pending_alloc(os_corr_id_t corr_id) {
  return (zb_pending_handle_t)pending_cmd_alloc(corr_id);
}

void zb_pending_set_tsn(zb_pending_handle_t slot, uint8_t tsn) {
  pending_cmd_set_tsn((zb_pending_cmd_t *)slot, tsn);
}

void zb_pending_free(zb_pending_handle_t slot) {
  pending_cmd_free((zb_pending_cmd_t *)slot);
}
