/**
 * @file capability.c
 * @brief Capability abstraction layer implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Capability mapping
 * 
 * Maps Zigbee clusters to stable capability abstractions.
 */

#include "capability.h"
#include "registry.h"
#include "os.h"
#include <string.h>

#define CAP_MODULE "CAP"

/* Well-known Zigbee cluster IDs */
#define ZCL_CLUSTER_BASIC        0x0000
#define ZCL_CLUSTER_ONOFF        0x0006
#define ZCL_CLUSTER_LEVEL        0x0008
#define ZCL_CLUSTER_COLOR        0x0300
#define ZCL_CLUSTER_TEMPERATURE  0x0402
#define ZCL_CLUSTER_HUMIDITY     0x0405
#define ZCL_CLUSTER_METERING     0x0702
#define ZCL_CLUSTER_ELEC_MEASURE 0x0B04

/* Well-known attribute IDs */
#define ZCL_ATTR_ONOFF           0x0000
#define ZCL_ATTR_LEVEL           0x0000
#define ZCL_ATTR_COLOR_TEMP      0x0007

/* Zigbee level control range */
#define ZCL_LEVEL_MAX            254
#define ZCL_ATTR_TEMPERATURE     0x0000
#define ZCL_ATTR_HUMIDITY        0x0000

/* Capability info table */
static const cap_info_t cap_info_table[] = {
    {CAP_UNKNOWN,           "unknown",              CAP_VALUE_INT,   ""},
    {CAP_SWITCH_ON,         "switch.on",            CAP_VALUE_BOOL,  ""},
    {CAP_LIGHT_ON,          "light.on",             CAP_VALUE_BOOL,  ""},
    {CAP_LIGHT_LEVEL,       "light.level",          CAP_VALUE_INT,   "%"},
    {CAP_LIGHT_COLOR_TEMP,  "light.color_temp",     CAP_VALUE_INT,   "mireds"},
    {CAP_SENSOR_TEMPERATURE,"sensor.temperature",   CAP_VALUE_FLOAT, "°C"},
    {CAP_SENSOR_HUMIDITY,   "sensor.humidity",      CAP_VALUE_FLOAT, "%"},
    {CAP_SENSOR_CONTACT,    "sensor.contact",       CAP_VALUE_BOOL,  ""},
    {CAP_SENSOR_MOTION,     "sensor.motion",        CAP_VALUE_BOOL,  ""},
    {CAP_SENSOR_ILLUMINANCE,"sensor.illuminance",   CAP_VALUE_INT,   "lux"},
    {CAP_POWER_WATTS,       "power.watts",          CAP_VALUE_FLOAT, "W"},
    {CAP_ENERGY_KWH,        "energy.kwh",           CAP_VALUE_FLOAT, "kWh"},
};

/* Cluster to capability mapping */
typedef struct {
    uint16_t cluster_id;
    uint16_t attr_id;
    cap_id_t cap_id;
} cluster_cap_map_t;

static const cluster_cap_map_t cluster_map[] = {
    {ZCL_CLUSTER_ONOFF,       ZCL_ATTR_ONOFF,       CAP_LIGHT_ON},
    {ZCL_CLUSTER_LEVEL,       ZCL_ATTR_LEVEL,       CAP_LIGHT_LEVEL},
    {ZCL_CLUSTER_COLOR,       ZCL_ATTR_COLOR_TEMP,  CAP_LIGHT_COLOR_TEMP},
    {ZCL_CLUSTER_TEMPERATURE, ZCL_ATTR_TEMPERATURE, CAP_SENSOR_TEMPERATURE},
    {ZCL_CLUSTER_HUMIDITY,    ZCL_ATTR_HUMIDITY,    CAP_SENSOR_HUMIDITY},
};

/* Maximum capabilities per node */
#define MAX_NODE_CAPS 8

/* Node capability cache */
typedef struct {
    os_eui64_t node_addr;
    cap_state_t caps[MAX_NODE_CAPS];
    uint8_t cap_count;
    bool valid;
} node_cap_cache_t;

#define MAX_CAP_CACHE 32

/* Service state */
static struct {
    bool initialized;
    node_cap_cache_t cache[MAX_CAP_CACHE];
} service = {0};

/* Internal functions */
static node_cap_cache_t *find_cache(os_eui64_t node_addr);
static node_cap_cache_t *alloc_cache(os_eui64_t node_addr);
static cap_state_t *find_cap_in_cache(node_cap_cache_t *cache, cap_id_t cap_id);
static void emit_state_changed(os_eui64_t node_addr, cap_id_t cap_id, const cap_value_t *value);

os_err_t cap_init(void) {
    if (service.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&service, 0, sizeof(service));
    service.initialized = true;
    
    LOG_I(CAP_MODULE, "Capability service initialized");
    
    return OS_OK;
}

uint32_t cap_compute_for_node(reg_node_t *node) {
    if (!service.initialized || !node || !node->valid) {
        return 0;
    }
    
    node_cap_cache_t *cache = find_cache(node->ieee_addr);
    if (!cache) {
        cache = alloc_cache(node->ieee_addr);
    }
    if (!cache) {
        LOG_E(CAP_MODULE, "Failed to allocate capability cache");
        return 0;
    }
    
    /* Clear existing caps */
    cache->cap_count = 0;
    
    /* Scan all endpoints/clusters */
    for (uint8_t ep_idx = 0; ep_idx < REG_MAX_ENDPOINTS; ep_idx++) {
        reg_endpoint_t *ep = &node->endpoints[ep_idx];
        if (!ep->valid) continue;
        
        for (uint8_t cl_idx = 0; cl_idx < REG_MAX_CLUSTERS; cl_idx++) {
            reg_cluster_t *cl = &ep->clusters[cl_idx];
            if (!cl->valid) continue;
            
            /* Check cluster mapping */
            for (size_t m = 0; m < sizeof(cluster_map) / sizeof(cluster_map[0]); m++) {
                if (cluster_map[m].cluster_id == cl->cluster_id) {
                    /* Found a match - add capability */
                    if (cache->cap_count < MAX_NODE_CAPS) {
                        cap_state_t *cap = &cache->caps[cache->cap_count];
                        cap->id = cluster_map[m].cap_id;
                        cap->type = cap_info_table[cap->id].type;
                        cap->valid = false;  /* No value yet */
                        cap->timestamp = 0;
                        
                        /* Initialize default value */
                        memset(&cap->value, 0, sizeof(cap->value));
                        
                        cache->cap_count++;
                        
                        LOG_D(CAP_MODULE, "Node " OS_EUI64_FMT " ep%d: added %s",
                              OS_EUI64_ARG(node->ieee_addr), ep->endpoint_id,
                              cap_info_table[cap->id].name);
                    }
                    break;
                }
            }
        }
    }
    
    LOG_I(CAP_MODULE, "Node " OS_EUI64_FMT ": computed %d capabilities",
          OS_EUI64_ARG(node->ieee_addr), cache->cap_count);
    
    return cache->cap_count;
}

os_err_t cap_get_state(os_eui64_t node_addr, cap_id_t cap_id, cap_state_t *out_state) {
    if (!service.initialized || !out_state) {
        return OS_ERR_INVALID_ARG;
    }
    
    node_cap_cache_t *cache = find_cache(node_addr);
    if (!cache) {
        return OS_ERR_NOT_FOUND;
    }
    
    cap_state_t *cap = find_cap_in_cache(cache, cap_id);
    if (!cap) {
        return OS_ERR_NOT_FOUND;
    }
    
    *out_state = *cap;
    return OS_OK;
}

os_err_t cap_handle_attribute_report(os_eui64_t node_addr, uint8_t endpoint_id,
                                      uint16_t cluster_id, uint16_t attr_id,
                                      const reg_attr_value_t *value) {
    if (!service.initialized || !value) {
        return OS_ERR_INVALID_ARG;
    }
    
    (void)endpoint_id;  /* For future use */
    
    /* Find matching capability */
    cap_id_t cap_id = CAP_UNKNOWN;
    for (size_t m = 0; m < sizeof(cluster_map) / sizeof(cluster_map[0]); m++) {
        if (cluster_map[m].cluster_id == cluster_id &&
            cluster_map[m].attr_id == attr_id) {
            cap_id = cluster_map[m].cap_id;
            break;
        }
    }
    
    if (cap_id == CAP_UNKNOWN) {
        return OS_OK;  /* Not a mapped attribute */
    }
    
    /* Find cache */
    node_cap_cache_t *cache = find_cache(node_addr);
    if (!cache) {
        return OS_ERR_NOT_FOUND;
    }
    
    /* Find capability */
    cap_state_t *cap = find_cap_in_cache(cache, cap_id);
    if (!cap) {
        return OS_ERR_NOT_FOUND;
    }
    
    /* Update value based on type */
    cap_value_t new_value = {0};
    
    switch (cap_id) {
        case CAP_LIGHT_ON:
        case CAP_SWITCH_ON:
            new_value.b = value->b;
            break;
            
        case CAP_LIGHT_LEVEL:
            /* Scale 0-ZCL_LEVEL_MAX to 0-100 */
            new_value.i = (value->u8 * 100) / ZCL_LEVEL_MAX;
            break;
            
        case CAP_SENSOR_TEMPERATURE:
            /* Zigbee temperature is in 0.01°C units */
            new_value.f = (float)value->s16 / 100.0f;
            break;
            
        case CAP_SENSOR_HUMIDITY:
            /* Zigbee humidity is in 0.01% units */
            new_value.f = (float)value->u16 / 100.0f;
            break;
            
        default:
            new_value.i = value->u32;
            break;
    }
    
    /* Update state */
    cap->value = new_value;
    cap->timestamp = os_now_ticks();
    cap->valid = true;
    
    /* Emit event */
    emit_state_changed(node_addr, cap_id, &new_value);
    
    LOG_D(CAP_MODULE, "Node " OS_EUI64_FMT " %s updated",
          OS_EUI64_ARG(node_addr), cap_info_table[cap_id].name);
    
    return OS_OK;
}

os_err_t cap_execute_command(const cap_command_t *cmd) {
    if (!service.initialized || !cmd) {
        return OS_ERR_INVALID_ARG;
    }
    
    LOG_I(CAP_MODULE, "Execute command: node=" OS_EUI64_FMT " cap=%s cmd=%d",
          OS_EUI64_ARG(cmd->node_addr), cap_info_table[cmd->cap_id].name, cmd->cmd_type);
    
    /* Find the cluster mapping */
    uint16_t cluster_id = 0;
    for (size_t m = 0; m < sizeof(cluster_map) / sizeof(cluster_map[0]); m++) {
        if (cluster_map[m].cap_id == cmd->cap_id) {
            cluster_id = cluster_map[m].cluster_id;
            break;
        }
    }
    
    if (cluster_id == 0) {
        LOG_E(CAP_MODULE, "No cluster mapping for capability %d", cmd->cap_id);
        return OS_ERR_NOT_FOUND;
    }
    
    /* In real implementation, this would send Zigbee command */
    /* For now, emit a CAP_COMMAND event for the Zigbee adapter to handle */
    os_event_emit(OS_EVENT_CAP_COMMAND, cmd, sizeof(*cmd));
    
    return OS_OK;
}

const cap_info_t *cap_get_info(cap_id_t id) {
    if (id < CAP_MAX) {
        return &cap_info_table[id];
    }
    return NULL;
}

cap_id_t cap_parse_name(const char *name) {
    if (!name) return CAP_UNKNOWN;
    
    for (int i = 0; i < CAP_MAX; i++) {
        if (strcmp(name, cap_info_table[i].name) == 0) {
            return (cap_id_t)i;
        }
    }
    return CAP_UNKNOWN;
}

void cap_task(void *arg) {
    (void)arg;
    
    LOG_I(CAP_MODULE, "Capability task started");
    
    while (1) {
        /* Process capability events */
        /* In real implementation, this would handle reconciliation */
        os_sleep(1000);
    }
}

/* Internal functions */

static node_cap_cache_t *find_cache(os_eui64_t node_addr) {
    for (int i = 0; i < MAX_CAP_CACHE; i++) {
        if (service.cache[i].valid && service.cache[i].node_addr == node_addr) {
            return &service.cache[i];
        }
    }
    return NULL;
}

static node_cap_cache_t *alloc_cache(os_eui64_t node_addr) {
    for (int i = 0; i < MAX_CAP_CACHE; i++) {
        if (!service.cache[i].valid) {
            service.cache[i].node_addr = node_addr;
            service.cache[i].cap_count = 0;
            service.cache[i].valid = true;
            return &service.cache[i];
        }
    }
    return NULL;
}

static cap_state_t *find_cap_in_cache(node_cap_cache_t *cache, cap_id_t cap_id) {
    if (!cache) return NULL;
    
    for (uint8_t i = 0; i < cache->cap_count; i++) {
        if (cache->caps[i].id == cap_id) {
            return &cache->caps[i];
        }
    }
    return NULL;
}

static void emit_state_changed(os_eui64_t node_addr, cap_id_t cap_id, const cap_value_t *value) {
    /* Payload for CAP_STATE_CHANGED event */
    struct {
        os_eui64_t node_addr;
        cap_id_t cap_id;
        cap_value_t value;
    } payload = {node_addr, cap_id, *value};
    
    os_event_emit(OS_EVENT_CAP_STATE_CHANGED, &payload, sizeof(payload));
}
