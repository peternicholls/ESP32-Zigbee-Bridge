/**
 * @file zb_fake.c
 * @brief Host-only Zigbee adapter simulation
 */

#include "os_fibre.h"
#include "os_log.h"
#include "zb_adapter.h"
#include <inttypes.h>
#include <string.h>

#define ZB_MODULE "ZB_FAKE"

static os_corr_id_t ensure_corr_id(os_corr_id_t corr_id) {
  if (corr_id == 0) {
    return os_event_new_corr_id();
  }
  return corr_id;
}

zb_err_t zb_init(void) {
  LOG_I(ZB_MODULE, "Zigbee adapter initialized (fake)");
  return OS_OK;
}

zb_err_t zb_start_coordinator(void) {
  os_event_t event = {0};
  event.type = OS_EVENT_ZB_STACK_UP;
  event.timestamp = os_now_ticks();
  event.src_id = 1;
  return os_event_publish(&event);
}

zb_err_t zb_set_permit_join(uint16_t seconds) {
  LOG_I(ZB_MODULE, "Permit join for %" PRIu16 " seconds (fake)", seconds);
  return OS_OK;
}

zb_err_t zb_send_onoff(zb_node_id_t node_id, uint8_t endpoint, bool on,
                       os_corr_id_t corr_id) {
  zb_cmd_confirm_t payload = {
      .node_id = node_id,
      .endpoint = endpoint,
      .cluster_id = 0x0006,
      .status = on ? 0 : 1,
  };

  os_event_t event = {0};
  event.type = OS_EVENT_ZB_CMD_CONFIRM;
  event.timestamp = os_now_ticks();
  event.corr_id = ensure_corr_id(corr_id);
  event.payload_len = sizeof(payload);
  memcpy(event.payload, &payload, sizeof(payload));

  return os_event_publish(&event);
}

zb_err_t zb_send_level(zb_node_id_t node_id, uint8_t endpoint, uint8_t level,
                       uint16_t transition_ds, os_corr_id_t corr_id) {
  (void)transition_ds;
  zb_cmd_confirm_t payload = {
      .node_id = node_id,
      .endpoint = endpoint,
      .cluster_id = 0x0008,
      .status = level > 254 ? 1 : 0,
  };

  os_event_t event = {0};
  event.type = OS_EVENT_ZB_CMD_CONFIRM;
  event.timestamp = os_now_ticks();
  event.corr_id = ensure_corr_id(corr_id);
  event.payload_len = sizeof(payload);
  memcpy(event.payload, &payload, sizeof(payload));

  return os_event_publish(&event);
}

zb_err_t zb_read_attrs(zb_node_id_t node_id, uint8_t endpoint,
                       uint16_t cluster_id, const uint16_t *attr_ids,
                       uint8_t attr_count, os_corr_id_t corr_id) {
  (void)attr_ids;
  (void)attr_count;
  zb_cmd_confirm_t payload = {
      .node_id = node_id,
      .endpoint = endpoint,
      .cluster_id = cluster_id,
      .status = 0,
  };

  os_event_t event = {0};
  event.type = OS_EVENT_ZB_CMD_CONFIRM;
  event.timestamp = os_now_ticks();
  event.corr_id = ensure_corr_id(corr_id);
  event.payload_len = sizeof(payload);
  memcpy(event.payload, &payload, sizeof(payload));

  return os_event_publish(&event);
}

zb_err_t zb_configure_reporting(zb_node_id_t node_id, uint8_t endpoint,
                                uint16_t cluster_id, uint16_t attr_id,
                                uint8_t attr_type, uint16_t min_s,
                                uint16_t max_s, os_corr_id_t corr_id) {
  (void)node_id;
  (void)endpoint;
  (void)cluster_id;
  (void)attr_id;
  (void)attr_type;
  (void)min_s;
  (void)max_s;
  (void)corr_id;
  LOG_I(ZB_MODULE, "Configure reporting (fake)");
  return OS_OK;
}

zb_err_t zb_bind(zb_node_id_t node_id, uint8_t endpoint, uint16_t cluster_id,
                 os_corr_id_t corr_id) {
  (void)node_id;
  (void)endpoint;
  (void)cluster_id;
  (void)corr_id;
  LOG_I(ZB_MODULE, "Bind request (fake)");
  return OS_OK;
}
