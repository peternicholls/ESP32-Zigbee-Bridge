/**
 * @file registry.c
 * @brief Device registry implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Device registry service
 */

#include "registry.h"
#include "os.h"
#include <string.h>
#include <stdio.h>

#define REG_MODULE "REG"

/* Registry storage */
static struct {
    bool initialized;
    reg_node_t nodes[REG_MAX_NODES];
    uint32_t node_count;
} registry = {0};

/* State names */
static const char *state_names[] = {
    "NEW",
    "INTERVIEWING",
    "READY",
    "STALE",
    "LEFT"
};

os_err_t reg_init(void) {
    if (registry.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&registry, 0, sizeof(registry));
    registry.initialized = true;
    
    LOG_I(REG_MODULE, "Device registry initialized (max %d nodes)", REG_MAX_NODES);
    
    return OS_OK;
}

reg_node_t *reg_add_node(os_eui64_t ieee_addr, uint16_t nwk_addr) {
    if (!registry.initialized) {
        return NULL;
    }
    
    /* Check if already exists */
    reg_node_t *existing = reg_find_node(ieee_addr);
    if (existing) {
        LOG_D(REG_MODULE, "Node " OS_EUI64_FMT " already exists, updating nwk_addr", 
              OS_EUI64_ARG(ieee_addr));
        existing->nwk_addr = nwk_addr;
        reg_touch_node(existing);
        return existing;
    }
    
    /* Find free slot */
    reg_node_t *node = NULL;
    for (uint32_t i = 0; i < REG_MAX_NODES; i++) {
        if (!registry.nodes[i].valid) {
            node = &registry.nodes[i];
            break;
        }
    }
    
    if (!node) {
        LOG_E(REG_MODULE, "Registry full, cannot add node");
        return NULL;
    }
    
    /* Initialize node */
    memset(node, 0, sizeof(*node));
    node->ieee_addr = ieee_addr;
    node->nwk_addr = nwk_addr;
    node->state = REG_STATE_NEW;
    node->join_time = os_now_ticks();
    node->last_seen = node->join_time;
    node->valid = true;
    
    registry.node_count++;
    
    LOG_I(REG_MODULE, "Added node " OS_EUI64_FMT " (nwk=0x%04X)", 
          OS_EUI64_ARG(ieee_addr), nwk_addr);
    
    /* Emit event */
    struct {
        os_eui64_t ieee_addr;
        uint16_t nwk_addr;
    } payload = {ieee_addr, nwk_addr};
    os_event_emit(OS_EVENT_ZB_DEVICE_JOINED, &payload, sizeof(payload));
    
    return node;
}

reg_node_t *reg_find_node(os_eui64_t ieee_addr) {
    if (!registry.initialized) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < REG_MAX_NODES; i++) {
        if (registry.nodes[i].valid && registry.nodes[i].ieee_addr == ieee_addr) {
            return &registry.nodes[i];
        }
    }
    
    return NULL;
}

reg_node_t *reg_find_node_by_nwk(uint16_t nwk_addr) {
    if (!registry.initialized) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < REG_MAX_NODES; i++) {
        if (registry.nodes[i].valid && registry.nodes[i].nwk_addr == nwk_addr) {
            return &registry.nodes[i];
        }
    }
    
    return NULL;
}

os_err_t reg_remove_node(os_eui64_t ieee_addr) {
    reg_node_t *node = reg_find_node(ieee_addr);
    if (!node) {
        return OS_ERR_NOT_FOUND;
    }
    
    LOG_I(REG_MODULE, "Removing node " OS_EUI64_FMT, OS_EUI64_ARG(ieee_addr));
    
    /* Emit event before removal */
    os_event_emit(OS_EVENT_ZB_DEVICE_LEFT, &ieee_addr, sizeof(ieee_addr));
    
    node->valid = false;
    registry.node_count--;
    
    return OS_OK;
}

os_err_t reg_set_state(reg_node_t *node, reg_state_t state) {
    if (!node || !node->valid) {
        return OS_ERR_INVALID_ARG;
    }
    
    reg_state_t old_state = node->state;
    if (old_state == state) {
        return OS_OK;
    }
    
    node->state = state;
    
    LOG_I(REG_MODULE, "Node " OS_EUI64_FMT " state: %s -> %s",
          OS_EUI64_ARG(node->ieee_addr),
          state_names[old_state], state_names[state]);
    
    return OS_OK;
}

void reg_touch_node(reg_node_t *node) {
    if (node && node->valid) {
        node->last_seen = os_now_ticks();
    }
}

uint32_t reg_node_count(void) {
    return registry.node_count;
}

os_err_t reg_get_node_info(uint32_t index, reg_node_info_t *info) {
    if (!registry.initialized || !info) {
        return OS_ERR_INVALID_ARG;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < REG_MAX_NODES; i++) {
        if (registry.nodes[i].valid) {
            if (count == index) {
                reg_node_t *node = &registry.nodes[i];
                info->ieee_addr = node->ieee_addr;
                info->nwk_addr = node->nwk_addr;
                info->state = node->state;
                info->manufacturer = node->manufacturer;
                info->model = node->model;
                info->friendly_name = node->friendly_name;
                info->lqi = node->lqi;
                info->endpoint_count = node->endpoint_count;
                return OS_OK;
            }
            count++;
        }
    }
    
    return OS_ERR_NOT_FOUND;
}

reg_endpoint_t *reg_add_endpoint(reg_node_t *node, uint8_t endpoint_id,
                                  uint16_t profile_id, uint16_t device_id) {
    if (!node || !node->valid) {
        return NULL;
    }
    
    /* Check if exists */
    reg_endpoint_t *ep = reg_find_endpoint(node, endpoint_id);
    if (ep) {
        return ep;
    }
    
    /* Find free slot */
    for (uint8_t i = 0; i < REG_MAX_ENDPOINTS; i++) {
        if (!node->endpoints[i].valid) {
            ep = &node->endpoints[i];
            break;
        }
    }
    
    if (!ep) {
        LOG_E(REG_MODULE, "Max endpoints reached for node");
        return NULL;
    }
    
    memset(ep, 0, sizeof(*ep));
    ep->endpoint_id = endpoint_id;
    ep->profile_id = profile_id;
    ep->device_id = device_id;
    ep->valid = true;
    node->endpoint_count++;
    
    LOG_D(REG_MODULE, "Node " OS_EUI64_FMT " added endpoint %d (profile=0x%04X, device=0x%04X)",
          OS_EUI64_ARG(node->ieee_addr), endpoint_id, profile_id, device_id);
    
    return ep;
}

reg_endpoint_t *reg_find_endpoint(reg_node_t *node, uint8_t endpoint_id) {
    if (!node || !node->valid) {
        return NULL;
    }
    
    for (uint8_t i = 0; i < REG_MAX_ENDPOINTS; i++) {
        if (node->endpoints[i].valid && node->endpoints[i].endpoint_id == endpoint_id) {
            return &node->endpoints[i];
        }
    }
    
    return NULL;
}

reg_cluster_t *reg_add_cluster(reg_endpoint_t *endpoint, uint16_t cluster_id,
                                reg_cluster_dir_t direction) {
    if (!endpoint || !endpoint->valid) {
        return NULL;
    }
    
    /* Check if exists */
    reg_cluster_t *cluster = reg_find_cluster(endpoint, cluster_id);
    if (cluster) {
        return cluster;
    }
    
    /* Find free slot */
    for (uint8_t i = 0; i < REG_MAX_CLUSTERS; i++) {
        if (!endpoint->clusters[i].valid) {
            cluster = &endpoint->clusters[i];
            break;
        }
    }
    
    if (!cluster) {
        LOG_E(REG_MODULE, "Max clusters reached for endpoint");
        return NULL;
    }
    
    memset(cluster, 0, sizeof(*cluster));
    cluster->cluster_id = cluster_id;
    cluster->direction = direction;
    cluster->valid = true;
    endpoint->cluster_count++;
    
    LOG_T(REG_MODULE, "Endpoint %d added cluster 0x%04X (%s)",
          endpoint->endpoint_id, cluster_id,
          direction == REG_CLUSTER_SERVER ? "server" : "client");
    
    return cluster;
}

reg_cluster_t *reg_find_cluster(reg_endpoint_t *endpoint, uint16_t cluster_id) {
    if (!endpoint || !endpoint->valid) {
        return NULL;
    }
    
    for (uint8_t i = 0; i < REG_MAX_CLUSTERS; i++) {
        if (endpoint->clusters[i].valid && endpoint->clusters[i].cluster_id == cluster_id) {
            return &endpoint->clusters[i];
        }
    }
    
    return NULL;
}

os_err_t reg_update_attribute(reg_cluster_t *cluster, uint16_t attr_id,
                               reg_attr_type_t type, const reg_attr_value_t *value) {
    if (!cluster || !cluster->valid || !value) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Find or create attribute */
    reg_attribute_t *attr = reg_find_attribute(cluster, attr_id);
    if (!attr) {
        /* Find free slot */
        for (uint8_t i = 0; i < REG_MAX_ATTRIBUTES; i++) {
            if (!cluster->attributes[i].valid) {
                attr = &cluster->attributes[i];
                break;
            }
        }
    }
    
    if (!attr) {
        LOG_E(REG_MODULE, "Max attributes reached for cluster");
        return OS_ERR_FULL;
    }
    
    bool is_new = !attr->valid;
    
    attr->attr_id = attr_id;
    attr->type = type;
    attr->value = *value;
    attr->last_updated = os_now_ticks();
    attr->valid = true;
    
    if (is_new) {
        cluster->attr_count++;
    }
    
    return OS_OK;
}

reg_attribute_t *reg_find_attribute(reg_cluster_t *cluster, uint16_t attr_id) {
    if (!cluster || !cluster->valid) {
        return NULL;
    }
    
    for (uint8_t i = 0; i < REG_MAX_ATTRIBUTES; i++) {
        if (cluster->attributes[i].valid && cluster->attributes[i].attr_id == attr_id) {
            return &cluster->attributes[i];
        }
    }
    
    return NULL;
}

/* Persistence key format */
#define REG_PERSIST_KEY_PREFIX "node/"
#define REG_PERSIST_COUNT_KEY "reg/count"
/* Key buffer size: prefix (5) + IEEE addr hex (16) + null (1) = 22, use 32 for safety */
#define REG_PERSIST_KEY_SIZE 32

os_err_t reg_persist(void) {
    if (!registry.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    uint32_t persisted = 0;
    
    for (uint32_t i = 0; i < REG_MAX_NODES; i++) {
        if (registry.nodes[i].valid) {
            char key[REG_PERSIST_KEY_SIZE];
            snprintf(key, sizeof(key), REG_PERSIST_KEY_PREFIX OS_EUI64_FMT,
                     OS_EUI64_ARG(registry.nodes[i].ieee_addr));
            
            os_err_t err = os_persist_put(key, &registry.nodes[i], sizeof(reg_node_t));
            if (err == OS_OK) {
                persisted++;
            }
        }
    }
    
    /* Store count */
    os_persist_put(REG_PERSIST_COUNT_KEY, &persisted, sizeof(persisted));
    
    LOG_I(REG_MODULE, "Persisted %u nodes", persisted);
    
    return OS_OK;
}

os_err_t reg_restore(void) {
    if (!registry.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    /* This is a simplified restore - in a real implementation,
       we would enumerate stored keys */
    uint32_t count = 0;
    size_t len;
    
    if (os_persist_get(REG_PERSIST_COUNT_KEY, &count, sizeof(count), &len) != OS_OK) {
        LOG_D(REG_MODULE, "No persisted registry found");
        return OS_OK;
    }
    
    LOG_I(REG_MODULE, "Restoring %u nodes from storage", count);
    
    /* In a real implementation, we would iterate through stored keys
       For now, nodes are restored on-demand or via explicit load */
    
    return OS_OK;
}

const char *reg_state_name(reg_state_t state) {
    if (state < sizeof(state_names) / sizeof(state_names[0])) {
        return state_names[state];
    }
    return "UNKNOWN";
}
