/**
 * @file mqtt_adapter.h
 * @brief MQTT adapter API
 * 
 * ESP32-C6 Zigbee Bridge OS - MQTT northbound adapter
 * 
 * Topic scheme:
 * - State:   bridge/<node_id>/<capability>/state
 * - Command: bridge/<node_id>/<capability>/set
 * - Meta:    bridge/<node_id>/meta
 * - Status:  bridge/status
 */

#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

#include "os_types.h"
#include "capability.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT connection state */
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR,
} mqtt_state_t;

/* MQTT configuration */
typedef struct {
    const char *broker_uri;
    const char *client_id;
    const char *username;
    const char *password;
    uint16_t keepalive_sec;
} mqtt_config_t;

/* MQTT statistics */
typedef struct {
    uint32_t messages_published;
    uint32_t messages_received;
    uint32_t reconnects;
    uint32_t errors;
} mqtt_stats_t;

/**
 * @brief Initialize MQTT adapter
 * @param config Configuration
 * @return OS_OK on success
 */
os_err_t mqtt_init(const mqtt_config_t *config);

/**
 * @brief Connect to MQTT broker
 * @return OS_OK on success
 */
os_err_t mqtt_connect(void);

/**
 * @brief Disconnect from MQTT broker
 * @return OS_OK on success
 */
os_err_t mqtt_disconnect(void);

/**
 * @brief Get connection state
 * @return Current state
 */
mqtt_state_t mqtt_get_state(void);

/**
 * @brief Publish capability state
 * @param node_addr Node IEEE address
 * @param cap_id Capability ID
 * @param value Capability value
 * @return OS_OK on success
 */
os_err_t mqtt_publish_state(os_eui64_t node_addr, cap_id_t cap_id, const cap_value_t *value);

/**
 * @brief Publish device metadata
 * @param node_addr Node IEEE address
 * @param manufacturer Manufacturer string
 * @param model Model string
 * @return OS_OK on success
 */
os_err_t mqtt_publish_meta(os_eui64_t node_addr, const char *manufacturer, const char *model);

/**
 * @brief Publish bridge status
 * @param online true if online
 * @return OS_OK on success
 */
os_err_t mqtt_publish_status(bool online);

/**
 * @brief Publish arbitrary message
 * @param topic Topic string
 * @param payload Payload data
 * @param len Payload length
 * @return OS_OK on success
 */
os_err_t mqtt_publish(const char *topic, const void *payload, size_t len);

/**
 * @brief Subscribe to command topics
 * @return OS_OK on success
 */
os_err_t mqtt_subscribe_commands(void);

/**
 * @brief Get MQTT statistics
 * @param stats Output statistics
 * @return OS_OK on success
 */
os_err_t mqtt_get_stats(mqtt_stats_t *stats);

/**
 * @brief MQTT task entry (run as fibre)
 * @param arg Unused
 */
void mqtt_task(void *arg);

/**
 * @brief Get state name string
 * @param state State value
 * @return State name
 */
const char *mqtt_state_name(mqtt_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_ADAPTER_H */
