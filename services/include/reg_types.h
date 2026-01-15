/**
 * @file reg_types.h
 * @brief Device registry type definitions
 * 
 * ESP32-C6 Zigbee Bridge OS - Zigbee device model types
 * 
 * Canonical model: Node → Endpoint → Cluster → Attribute
 */

#ifndef REG_TYPES_H
#define REG_TYPES_H

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Limits */
#define REG_MAX_NODES           32
#define REG_MAX_ENDPOINTS       8
#define REG_MAX_CLUSTERS        16
#define REG_MAX_ATTRIBUTES      32
#define REG_NAME_MAX_LEN        32
#define REG_MANUFACTURER_LEN    32
#define REG_MODEL_LEN           32

/* Device lifecycle states */
typedef enum {
    REG_STATE_NEW = 0,       /* Just joined, not interviewed */
    REG_STATE_INTERVIEWING,  /* Interview in progress */
    REG_STATE_READY,         /* Fully interviewed, operational */
    REG_STATE_STALE,         /* Device not responding */
    REG_STATE_LEFT,          /* Device left the network */
} reg_state_t;

/* Cluster direction */
typedef enum {
    REG_CLUSTER_SERVER = 0,
    REG_CLUSTER_CLIENT,
} reg_cluster_dir_t;

/* Power source type */
typedef enum {
    REG_POWER_UNKNOWN = 0,
    REG_POWER_MAINS,
    REG_POWER_BATTERY,
    REG_POWER_DC,
} reg_power_source_t;

/* Attribute data type */
typedef enum {
    REG_ATTR_TYPE_UNKNOWN = 0,
    REG_ATTR_TYPE_BOOL,
    REG_ATTR_TYPE_U8,
    REG_ATTR_TYPE_U16,
    REG_ATTR_TYPE_U32,
    REG_ATTR_TYPE_S8,
    REG_ATTR_TYPE_S16,
    REG_ATTR_TYPE_S32,
    REG_ATTR_TYPE_STRING,
    REG_ATTR_TYPE_ARRAY,
} reg_attr_type_t;

/* Attribute value union */
typedef union {
    bool b;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    int8_t s8;
    int16_t s16;
    int32_t s32;
    char str[32];
} reg_attr_value_t;

/* Attribute structure */
typedef struct {
    uint16_t attr_id;
    reg_attr_type_t type;
    reg_attr_value_t value;
    os_tick_t last_updated;
    bool valid;
} reg_attribute_t;

/* Cluster structure */
typedef struct {
    uint16_t cluster_id;
    reg_cluster_dir_t direction;
    reg_attribute_t attributes[REG_MAX_ATTRIBUTES];
    uint8_t attr_count;
    bool valid;
} reg_cluster_t;

/* Endpoint structure */
typedef struct {
    uint8_t endpoint_id;
    uint16_t profile_id;
    uint16_t device_id;
    reg_cluster_t clusters[REG_MAX_CLUSTERS];
    uint8_t cluster_count;
    bool valid;
} reg_endpoint_t;

/* Node (device) structure */
typedef struct {
    /* Identity */
    os_eui64_t ieee_addr;
    uint16_t nwk_addr;
    
    /* State */
    reg_state_t state;
    
    /* Metadata */
    char manufacturer[REG_MANUFACTURER_LEN];
    char model[REG_MODEL_LEN];
    char friendly_name[REG_NAME_MAX_LEN];
    uint32_t sw_build;
    
    /* Telemetry */
    uint8_t lqi;
    int8_t rssi;
    reg_power_source_t power_source;
    
    /* Endpoints */
    reg_endpoint_t endpoints[REG_MAX_ENDPOINTS];
    uint8_t endpoint_count;
    
    /* Timestamps */
    os_tick_t join_time;
    os_tick_t last_seen;
    
    /* Interview progress */
    uint8_t interview_stage;
    
    /* Slot management */
    bool valid;
} reg_node_t;

/* Node info for shell/API (minimal subset) */
typedef struct {
    os_eui64_t ieee_addr;
    uint16_t nwk_addr;
    reg_state_t state;
    const char *manufacturer;
    const char *model;
    const char *friendly_name;
    uint8_t lqi;
    uint8_t endpoint_count;
} reg_node_info_t;

#ifdef __cplusplus
}
#endif

#endif /* REG_TYPES_H */
