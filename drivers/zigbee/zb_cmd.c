/**
 * @file zb_cmd.c
 * @brief Zigbee command functions (On/Off, Level, Read, Configure, Bind)
 *
 * ESP32-C6 Zigbee Bridge OS - Command implementations
 */

#include "esp_zigbee_core.h"
#include "zb_real_internal.h"

/* On/Off Command */
zb_err_t zb_send_onoff(zb_node_id_t node_id, uint8_t endpoint, bool on,
                       os_corr_id_t corr_id) {
  if (zb_get_state() != ZB_STATE_READY)
    return OS_ERR_NOT_READY;
  zb_nwk_entry_t *entry = nwk_cache_find(node_id);
  if (!entry)
    return OS_ERR_NOT_FOUND;
  zb_pending_cmd_t *slot = corr_id ? pending_alloc(corr_id, 0x0006, on) : NULL;
  if (corr_id && !slot)
    return OS_ERR_NO_MEM;

  esp_zb_zcl_on_off_cmd_t cmd = {
      .zcl_basic_cmd.dst_endpoint = endpoint,
      .zcl_basic_cmd.dst_addr_u.addr_short = entry->nwk_addr,
      .zcl_basic_cmd.src_endpoint = 1,
      .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
      .on_off_cmd_id =
          on ? ESP_ZB_ZCL_CMD_ON_OFF_ON_ID : ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID,
  };
  esp_zb_lock_acquire(portMAX_DELAY);
  uint8_t tsn = esp_zb_zcl_on_off_cmd_req(&cmd);
  esp_zb_lock_release();
  if (slot)
    slot->tsn = tsn;
  return OS_OK;
}

/* Level Command */
zb_err_t zb_send_level(zb_node_id_t node_id, uint8_t endpoint, uint8_t level,
                       uint16_t transition_ds, os_corr_id_t corr_id) {
  if (zb_get_state() != ZB_STATE_READY)
    return OS_ERR_NOT_READY;
  zb_nwk_entry_t *entry = nwk_cache_find(node_id);
  if (!entry)
    return OS_ERR_NOT_FOUND;
  zb_pending_cmd_t *slot = corr_id ? pending_alloc(corr_id, 0x0008, 0) : NULL;
  if (corr_id && !slot)
    return OS_ERR_NO_MEM;

  esp_zb_zcl_move_to_level_cmd_t cmd = {
      .zcl_basic_cmd.dst_endpoint = endpoint,
      .zcl_basic_cmd.dst_addr_u.addr_short = entry->nwk_addr,
      .zcl_basic_cmd.src_endpoint = 1,
      .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
      .level = level,
      .transition_time = transition_ds,
  };
  esp_zb_lock_acquire(portMAX_DELAY);
  uint8_t tsn = esp_zb_zcl_level_move_to_level_cmd_req(&cmd);
  esp_zb_lock_release();
  if (slot)
    slot->tsn = tsn;
  return OS_OK;
}

/* Read Attributes */
zb_err_t zb_read_attrs(zb_node_id_t node_id, uint8_t endpoint,
                       uint16_t cluster_id, const uint16_t *attr_ids,
                       uint8_t attr_count, os_corr_id_t corr_id) {
  if (zb_get_state() != ZB_STATE_READY)
    return OS_ERR_NOT_READY;
  if (attr_count > 8)
    return OS_ERR_INVALID_ARG;
  zb_nwk_entry_t *entry = nwk_cache_find(node_id);
  if (!entry)
    return OS_ERR_NOT_FOUND;
  zb_pending_cmd_t *slot =
      corr_id ? pending_alloc(corr_id, cluster_id, 0) : NULL;
  if (corr_id && !slot)
    return OS_ERR_NO_MEM;

  esp_zb_zcl_read_attr_cmd_t cmd = {
      .zcl_basic_cmd.dst_endpoint = endpoint,
      .zcl_basic_cmd.dst_addr_u.addr_short = entry->nwk_addr,
      .zcl_basic_cmd.src_endpoint = 1,
      .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
      .clusterID = cluster_id,
      .attr_number = attr_count,
      .attr_field = (uint16_t *)attr_ids,
  };
  esp_zb_lock_acquire(portMAX_DELAY);
  uint8_t tsn = esp_zb_zcl_read_attr_cmd_req(&cmd);
  esp_zb_lock_release();
  if (slot)
    slot->tsn = tsn;
  return OS_OK;
}

/* Configure Reporting */
zb_err_t zb_configure_reporting(zb_node_id_t node_id, uint8_t endpoint,
                                uint16_t cluster_id, uint16_t attr_id,
                                uint8_t attr_type, uint16_t min_s,
                                uint16_t max_s, os_corr_id_t corr_id) {
  if (zb_get_state() != ZB_STATE_READY)
    return OS_ERR_NOT_READY;
  zb_nwk_entry_t *entry = nwk_cache_find(node_id);
  if (!entry)
    return OS_ERR_NOT_FOUND;
  zb_pending_cmd_t *slot =
      corr_id ? pending_alloc(corr_id, cluster_id, 6) : NULL;
  if (corr_id && !slot)
    return OS_ERR_NO_MEM;

  esp_zb_zcl_config_report_record_t rec = {
      .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
      .attributeID = attr_id,
      .attrType = attr_type,
      .min_interval = min_s,
      .max_interval = max_s,
      .reportable_change = NULL,
  };
  esp_zb_zcl_config_report_cmd_t cmd = {
      .zcl_basic_cmd.dst_endpoint = endpoint,
      .zcl_basic_cmd.dst_addr_u.addr_short = entry->nwk_addr,
      .zcl_basic_cmd.src_endpoint = 1,
      .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
      .clusterID = cluster_id,
      .record_number = 1,
      .record_field = &rec,
  };
  esp_zb_lock_acquire(portMAX_DELAY);
  uint8_t tsn = esp_zb_zcl_config_report_cmd_req(&cmd);
  esp_zb_lock_release();
  if (slot)
    slot->tsn = tsn;
  return OS_OK;
}

/* ZDO Bind Response Callback */
static void zb_bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  zb_pending_cmd_t *slot = (zb_pending_cmd_t *)user_ctx;
  if (!slot)
    return;
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    struct {
      os_corr_id_t corr_id;
      uint8_t status;
    } p = {slot->corr_id, 0};
    emit_event(OS_EVENT_ZB_CMD_CONFIRM, &p, sizeof(p));
  } else {
    struct {
      os_corr_id_t corr_id;
      uint16_t err;
    } p = {slot->corr_id, (uint16_t)zdo_status};
    emit_event(OS_EVENT_ZB_CMD_ERROR, &p, sizeof(p));
  }
  pending_free(slot);
}

/* Bind Request */
zb_err_t zb_bind(zb_node_id_t node_id, uint8_t endpoint, uint16_t cluster_id,
                 os_corr_id_t corr_id) {
  if (zb_get_state() != ZB_STATE_READY)
    return OS_ERR_NOT_READY;
  zb_nwk_entry_t *entry = nwk_cache_find(node_id);
  if (!entry)
    return OS_ERR_NOT_FOUND;
  zb_pending_cmd_t *slot =
      corr_id ? pending_alloc(corr_id, cluster_id, 0x21) : NULL;
  if (corr_id && !slot)
    return OS_ERR_NO_MEM;

  esp_zb_ieee_addr_t coord;
  esp_zb_get_long_address(coord);
  esp_zb_zdo_bind_req_param_t req = {
      .src_endp = endpoint,
      .dst_endp = 1,
      .cluster_id = cluster_id,
      .dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED,
  };
  memcpy(req.src_address, &node_id, sizeof(req.src_address));
  memcpy(req.dst_address_u.addr_long, coord,
         sizeof(req.dst_address_u.addr_long));

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zdo_device_bind_req(&req, zb_bind_cb, slot);
  esp_zb_lock_release();
  return OS_OK;
}
