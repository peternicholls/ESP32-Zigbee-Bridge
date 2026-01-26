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

typedef os_err_t zba_err_t;
typedef os_eui64_t zba_node_id_t;

typedef struct {
  zba_node_id_t node_id;
  uint8_t endpoint;
  uint16_t cluster_id;
  uint8_t status;
} zba_cmd_confirm_t;

typedef struct {
  zba_node_id_t node_id;
  uint8_t endpoint;
  uint16_t cluster_id;
  uint8_t status;
} zba_cmd_error_t;

zba_err_t zba_init(void);
zba_err_t zba_start_coordinator(void);
zba_err_t zba_set_permit_join(uint16_t seconds);

zba_err_t zba_send_onoff(zba_node_id_t node_id, uint8_t endpoint, bool on,
                         os_corr_id_t corr_id);
zba_err_t zba_send_level(zba_node_id_t node_id, uint8_t endpoint,
                         uint8_t level_0_100, uint16_t transition_ms,
                         os_corr_id_t corr_id);

zba_err_t zba_read_attrs(zba_node_id_t node_id, uint8_t endpoint,
                         uint16_t cluster_id, const uint16_t *attr_ids,
                         size_t attr_count, os_corr_id_t corr_id);
zba_err_t zba_configure_reporting(zba_node_id_t node_id, uint8_t endpoint,
                                  uint16_t cluster_id, uint16_t attr_id,
                                  uint16_t min_s, uint16_t max_s,
                                  uint32_t change);
zba_err_t zba_bind(zba_node_id_t node_id, uint8_t endpoint, uint16_t cluster_id,
                   uint64_t dst);

#if defined(CONFIG_IDF_TARGET_ESP32C6)
/* Shell commands (only available on ESP32-C6 target) */
os_err_t zba_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZB_ADAPTER_H */
