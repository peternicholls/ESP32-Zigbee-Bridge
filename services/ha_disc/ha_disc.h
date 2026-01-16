/**
 * @file ha_disc.h
 * @brief Home Assistant MQTT Discovery API
 * 
 * ESP32-C6 Zigbee Bridge OS - HA Discovery service (T080)
 * 
 * Generates Home Assistant MQTT discovery messages for registered devices.
 * Follows HA MQTT discovery protocol.
 * 
 * Topic format: homeassistant/<component>/<unique_id>/config
 */

#ifndef HA_DISC_H
#define HA_DISC_H

#include "os_types.h"
#include "capability.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HA component types */
typedef enum {
    HA_COMPONENT_LIGHT = 0,
    HA_COMPONENT_SWITCH,
    HA_COMPONENT_SENSOR,
    HA_COMPONENT_BINARY_SENSOR,
    HA_COMPONENT_MAX
} ha_component_t;

/* Discovery config for a single entity */
typedef struct {
    ha_component_t component;
    char unique_id[64];
    char name[32];
    char state_topic[128];
    char command_topic[128];
    char availability_topic[32];
    bool has_brightness;
    char brightness_state_topic[128];
    char brightness_command_topic[128];
} ha_disc_config_t;

/**
 * @brief Initialize HA discovery service
 * @return OS_OK on success
 */
os_err_t ha_disc_init(void);

/**
 * @brief Publish discovery config for a node
 * @param node_addr Node IEEE address
 * @return OS_OK on success
 */
os_err_t ha_disc_publish_node(os_eui64_t node_addr);

/**
 * @brief Unpublish discovery config for a node (remove from HA)
 * @param node_addr Node IEEE address
 * @return OS_OK on success
 */
os_err_t ha_disc_unpublish_node(os_eui64_t node_addr);

/**
 * @brief Publish discovery for all READY nodes
 * @return Number of nodes published
 */
uint32_t ha_disc_publish_all(void);

/**
 * @brief Flush pending discovery publishes (after MQTT reconnect)
 * @return Number of pending publishes flushed
 */
uint32_t ha_disc_flush_pending(void);

/**
 * @brief Get component name string
 * @param component Component type
 * @return Component name
 */
const char *ha_disc_component_name(ha_component_t component);

/**
 * @brief Generate discovery config for a node capability
 * @param node_addr Node IEEE address
 * @param cap_id Capability ID
 * @param out_config Output configuration
 * @return OS_OK on success
 */
os_err_t ha_disc_generate_config(os_eui64_t node_addr, cap_id_t cap_id,
                                  ha_disc_config_t *out_config);

/**
 * @brief HA discovery task entry (run as fibre)
 * @param arg Unused
 */
void ha_disc_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* HA_DISC_H */
