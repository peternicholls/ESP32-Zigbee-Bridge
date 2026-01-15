/**
 * @file capability.h
 * @brief Capability abstraction layer API
 * 
 * ESP32-C6 Zigbee Bridge OS - Capability mapping
 * 
 * Maps Zigbee clusters/attributes to stable capability abstractions.
 */

#ifndef CAPABILITY_H
#define CAPABILITY_H

#include "os_types.h"
#include "reg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Capability IDs */
typedef enum {
    CAP_UNKNOWN = 0,
    
    /* Actuators */
    CAP_SWITCH_ON,       /* bool */
    CAP_LIGHT_ON,        /* bool */
    CAP_LIGHT_LEVEL,     /* int 0-100 */
    CAP_LIGHT_COLOR_TEMP,/* int (mireds) */
    
    /* Sensors */
    CAP_SENSOR_TEMPERATURE,  /* float degC */
    CAP_SENSOR_HUMIDITY,     /* float % */
    CAP_SENSOR_CONTACT,      /* bool */
    CAP_SENSOR_MOTION,       /* bool */
    CAP_SENSOR_ILLUMINANCE,  /* int lux */
    
    /* Power */
    CAP_POWER_WATTS,     /* float W */
    CAP_ENERGY_KWH,      /* float kWh */
    
    CAP_MAX
} cap_id_t;

/* Capability value type */
typedef enum {
    CAP_VALUE_BOOL = 0,
    CAP_VALUE_INT,
    CAP_VALUE_FLOAT,
    CAP_VALUE_STRING,
} cap_value_type_t;

/* Capability value union */
typedef union {
    bool b;
    int32_t i;
    float f;
    char str[32];
} cap_value_t;

/* Capability state structure */
typedef struct {
    cap_id_t id;
    cap_value_type_t type;
    cap_value_t value;
    os_tick_t timestamp;
    bool valid;
} cap_state_t;

/* Capability info */
typedef struct {
    cap_id_t id;
    const char *name;
    cap_value_type_t type;
    const char *unit;
} cap_info_t;

/* Command types */
typedef enum {
    CAP_CMD_SET = 0,     /* Set value directly */
    CAP_CMD_TOGGLE,      /* Toggle boolean */
    CAP_CMD_INCREMENT,   /* Increment by amount */
    CAP_CMD_DECREMENT,   /* Decrement by amount */
} cap_cmd_type_t;

/* Capability command structure */
typedef struct {
    os_eui64_t node_addr;
    uint8_t endpoint_id;
    cap_id_t cap_id;
    cap_cmd_type_t cmd_type;
    cap_value_t value;
    os_corr_id_t corr_id;
} cap_command_t;

/**
 * @brief Initialize capability service
 * @return OS_OK on success
 */
os_err_t cap_init(void);

/**
 * @brief Compute capabilities for a node from its clusters
 * @param node Node pointer
 * @return Number of capabilities found
 */
uint32_t cap_compute_for_node(reg_node_t *node);

/**
 * @brief Get capability state for a node
 * @param node_addr Node IEEE address
 * @param cap_id Capability ID
 * @param out_state Output state
 * @return OS_OK on success
 */
os_err_t cap_get_state(os_eui64_t node_addr, cap_id_t cap_id, cap_state_t *out_state);

/**
 * @brief Update capability state from Zigbee attribute report
 * @param node_addr Node IEEE address
 * @param endpoint_id Endpoint ID
 * @param cluster_id Cluster ID
 * @param attr_id Attribute ID
 * @param value Attribute value
 * @return OS_OK on success
 */
os_err_t cap_handle_attribute_report(os_eui64_t node_addr, uint8_t endpoint_id,
                                      uint16_t cluster_id, uint16_t attr_id,
                                      const reg_attr_value_t *value);

/**
 * @brief Execute a capability command
 * @param cmd Command to execute
 * @return OS_OK on success
 */
os_err_t cap_execute_command(const cap_command_t *cmd);

/**
 * @brief Get capability info
 * @param id Capability ID
 * @return Pointer to info, or NULL if invalid
 */
const cap_info_t *cap_get_info(cap_id_t id);

/**
 * @brief Get capability ID from name
 * @param name Capability name
 * @return Capability ID, or CAP_UNKNOWN if not found
 */
cap_id_t cap_parse_name(const char *name);

/**
 * @brief Capability task entry (run as fibre)
 * @param arg Unused
 */
void cap_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* CAPABILITY_H */
