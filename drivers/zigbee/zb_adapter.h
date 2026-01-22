/**
 * @file zb_adapter.h
 * @brief Zigbee adapter contract
 *
 * ESP32-C6 Zigbee Bridge OS - Zigbee adapter boundary (M4 contract)
 */

#ifndef ZB_ADAPTER_H
#define ZB_ADAPTER_H

#include "os_event.h"
#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef os_err_t zb_err_t;
typedef os_eui64_t zb_node_id_t;

typedef struct {
  zb_node_id_t node_id;
  uint8_t endpoint;
  uint16_t cluster_id;
  uint8_t status;
} zb_cmd_confirm_t;

typedef struct {
  zb_node_id_t node_id;
  uint8_t endpoint;
  uint16_t cluster_id;
  uint8_t status;
} zb_cmd_error_t;

zb_err_t zb_init(void);
zb_err_t zb_start_coordinator(void);
zb_err_t zb_set_permit_join(uint16_t seconds);

zb_err_t zb_send_onoff(zb_node_id_t node_id, uint8_t endpoint, bool on,
                       os_corr_id_t corr_id);
zb_err_t zb_send_level(zb_node_id_t node_id, uint8_t endpoint, uint8_t level,
                       uint16_t transition_ds, os_corr_id_t corr_id);

zb_err_t zb_read_attrs(zb_node_id_t node_id, uint8_t endpoint,
                       uint16_t cluster_id, const uint16_t *attr_ids,
                       uint8_t attr_count, os_corr_id_t corr_id);
zb_err_t zb_configure_reporting(zb_node_id_t node_id, uint8_t endpoint,
                                uint16_t cluster_id, uint16_t attr_id,
                                uint8_t attr_type, uint16_t min_s,
                                uint16_t max_s, os_corr_id_t corr_id);
zb_err_t zb_bind(zb_node_id_t node_id, uint8_t endpoint, uint16_t cluster_id,
                 os_corr_id_t corr_id);

#ifdef __cplusplus
}
#endif

#endif /* ZB_ADAPTER_H */
