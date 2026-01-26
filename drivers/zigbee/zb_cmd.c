/**
 * @file zb_cmd.c
 * @brief Zigbee command sending implementation
 *
 * ESP32-C6 Zigbee Bridge OS - Zigbee command functions (M4)
 *
 * Implements command functions from zb_adapter.h:
 * - zba_send_onoff
 * - zba_send_level
 * - zba_read_attrs
 * - zba_configure_reporting
 * - zba_bind
 */

#include "os_log.h"
#include "zb_adapter.h"
#include "zb_internal.h"

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"

#define ZB_MODULE "ZB_CMD"

/* ─────────────────────────────────────────────────────────────────────────────
 * Command Functions
 * ─────────────────────────────────────────────────────────────────────────────
 */

zba_err_t zba_send_onoff(zba_node_id_t node_id, uint8_t endpoint, bool on,
                         os_corr_id_t corr_id) {
  if (!zb_is_ready()) {
    return OS_ERR_NOT_READY;
  }

  /* Lookup NWK address from EUI64 */
  uint16_t nwk = zb_lookup_nwk(node_id);
  if (nwk == 0xFFFF) {
    LOG_W(ZB_MODULE, "Node " OS_EUI64_FMT " not in cache",
          OS_EUI64_ARG(node_id));
    return OS_ERR_NOT_FOUND;
  }

  /* Allocate pending slot for correlation */
  zb_pending_handle_t slot = zb_pending_alloc(corr_id);
  if (!slot) {
    return OS_ERR_NO_MEM;
  }

  LOG_I(ZB_MODULE, "Sending OnOff %s to " OS_EUI64_FMT " (NWK 0x%04X) ep=%u",
        on ? "ON" : "OFF", OS_EUI64_ARG(node_id), nwk, endpoint);

  /* Build and send command */
  esp_zb_lock_acquire(portMAX_DELAY);

  esp_zb_zcl_on_off_cmd_t cmd = {
      .zcl_basic_cmd =
          {
              .dst_addr_u.addr_short = nwk,
              .dst_endpoint = endpoint,
              .src_endpoint = 1,
          },
      .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
      .on_off_cmd_id =
          on ? ESP_ZB_ZCL_CMD_ON_OFF_ON_ID : ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID,
  };

  esp_err_t err = esp_zb_zcl_on_off_cmd_req(&cmd);
  esp_zb_lock_release();

  if (err != ESP_OK) {
    LOG_E(ZB_MODULE, "Failed to send OnOff cmd: %d", err);
    zb_pending_free(slot);
    return OS_ERR_INVALID_ARG;
  }

  /* Capture TSN assigned by the stack for correlation in send status cb */
  zb_pending_set_tsn(slot, cmd.zcl_basic_cmd.tsn);

  return OS_OK;
}

zba_err_t zba_send_level(zba_node_id_t node_id, uint8_t endpoint,
                         uint8_t level_0_100, uint16_t transition_ms,
                         os_corr_id_t corr_id) {
  if (!zb_is_ready()) {
    return OS_ERR_NOT_READY;
  }

  /* Lookup NWK address from EUI64 */
  uint16_t nwk = zb_lookup_nwk(node_id);
  if (nwk == 0xFFFF) {
    LOG_W(ZB_MODULE, "Node " OS_EUI64_FMT " not in cache",
          OS_EUI64_ARG(node_id));
    return OS_ERR_NOT_FOUND;
  }

  /* Allocate pending slot for correlation */
  zb_pending_handle_t slot = zb_pending_alloc(corr_id);
  if (!slot) {
    return OS_ERR_NO_MEM;
  }

  /* Convert 0-100% to 0-254 Zigbee level (with rounding) */
  uint8_t zb_level = (uint8_t)((level_0_100 * 254 + 50) / 100);
  /* Convert ms to 100ms units (Zigbee transition time) */
  uint16_t zb_trans = transition_ms / 100;

  LOG_I(ZB_MODULE, "Sending Level %u%% to " OS_EUI64_FMT " (NWK 0x%04X) ep=%u",
        level_0_100, OS_EUI64_ARG(node_id), nwk, endpoint);

  /* Build and send command */
  esp_zb_lock_acquire(portMAX_DELAY);

  esp_zb_zcl_move_to_level_cmd_t cmd = {
      .zcl_basic_cmd =
          {
              .dst_addr_u.addr_short = nwk,
              .dst_endpoint = endpoint,
              .src_endpoint = 1,
          },
      .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
      .level = zb_level,
      .transition_time = zb_trans,
  };

  esp_err_t err = esp_zb_zcl_level_move_to_level_cmd_req(&cmd);
  esp_zb_lock_release();

  if (err != ESP_OK) {
    LOG_E(ZB_MODULE, "Failed to send Level cmd: %d", err);
    zb_pending_free(slot);
    return OS_ERR_INVALID_ARG;
  }

  /* Capture TSN assigned by the stack for correlation in send status cb */
  zb_pending_set_tsn(slot, cmd.zcl_basic_cmd.tsn);

  return OS_OK;
}

zba_err_t zba_read_attrs(zba_node_id_t node_id, uint8_t endpoint,
                         uint16_t cluster_id, const uint16_t *attr_ids,
                         size_t attr_count, os_corr_id_t corr_id) {
  (void)node_id;
  (void)endpoint;
  (void)cluster_id;
  (void)attr_ids;
  (void)attr_count;
  (void)corr_id;

  LOG_I(ZB_MODULE, "zba_read_attrs (not implemented)");
  /* TODO: Implement in Phase 6 */
  return OS_ERR_NOT_READY;
}

zba_err_t zba_configure_reporting(zba_node_id_t node_id, uint8_t endpoint,
                                  uint16_t cluster_id, uint16_t attr_id,
                                  uint16_t min_s, uint16_t max_s,
                                  uint32_t change) {
  (void)node_id;
  (void)endpoint;
  (void)cluster_id;
  (void)attr_id;
  (void)min_s;
  (void)max_s;
  (void)change;

  LOG_I(ZB_MODULE, "zba_configure_reporting (not implemented)");
  /* TODO: Implement in Phase 6 */
  return OS_ERR_NOT_READY;
}

zba_err_t zba_bind(zba_node_id_t node_id, uint8_t endpoint, uint16_t cluster_id,
                   uint64_t dst) {
  (void)node_id;
  (void)endpoint;
  (void)cluster_id;
  (void)dst;

  LOG_I(ZB_MODULE, "zba_bind (not implemented)");
  /* TODO: Implement in Phase 6 */
  return OS_ERR_NOT_READY;
}
