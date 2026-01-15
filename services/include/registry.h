/**
 * @file registry.h
 * @brief Device registry API
 * 
 * ESP32-C6 Zigbee Bridge OS - Device registry service
 * 
 * Manages the device graph: Node → Endpoint → Cluster → Attribute
 */

#ifndef REGISTRY_H
#define REGISTRY_H

#include "reg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the registry
 * @return OS_OK on success
 */
os_err_t reg_init(void);

/**
 * @brief Add a new node to the registry
 * @param ieee_addr IEEE EUI64 address
 * @param nwk_addr Network short address
 * @return Pointer to node, or NULL on failure
 */
reg_node_t *reg_add_node(os_eui64_t ieee_addr, uint16_t nwk_addr);

/**
 * @brief Find a node by IEEE address
 * @param ieee_addr IEEE EUI64 address
 * @return Pointer to node, or NULL if not found
 */
reg_node_t *reg_find_node(os_eui64_t ieee_addr);

/**
 * @brief Find a node by network address
 * @param nwk_addr Network short address
 * @return Pointer to node, or NULL if not found
 */
reg_node_t *reg_find_node_by_nwk(uint16_t nwk_addr);

/**
 * @brief Remove a node from the registry
 * @param ieee_addr IEEE EUI64 address
 * @return OS_OK on success
 */
os_err_t reg_remove_node(os_eui64_t ieee_addr);

/**
 * @brief Set node state
 * @param node Node pointer
 * @param state New state
 * @return OS_OK on success
 */
os_err_t reg_set_state(reg_node_t *node, reg_state_t state);

/**
 * @brief Update node last seen time
 * @param node Node pointer
 */
void reg_touch_node(reg_node_t *node);

/**
 * @brief Get total node count
 * @return Number of valid nodes
 */
uint32_t reg_node_count(void);

/**
 * @brief Get node info by index
 * @param index Node index
 * @param info Output info structure
 * @return OS_OK on success
 */
os_err_t reg_get_node_info(uint32_t index, reg_node_info_t *info);

/**
 * @brief Add endpoint to a node
 * @param node Node pointer
 * @param endpoint_id Endpoint ID
 * @param profile_id Profile ID
 * @param device_id Device ID
 * @return Pointer to endpoint, or NULL on failure
 */
reg_endpoint_t *reg_add_endpoint(reg_node_t *node, uint8_t endpoint_id,
                                  uint16_t profile_id, uint16_t device_id);

/**
 * @brief Find endpoint on a node
 * @param node Node pointer
 * @param endpoint_id Endpoint ID
 * @return Pointer to endpoint, or NULL if not found
 */
reg_endpoint_t *reg_find_endpoint(reg_node_t *node, uint8_t endpoint_id);

/**
 * @brief Add cluster to an endpoint
 * @param endpoint Endpoint pointer
 * @param cluster_id Cluster ID
 * @param direction Cluster direction
 * @return Pointer to cluster, or NULL on failure
 */
reg_cluster_t *reg_add_cluster(reg_endpoint_t *endpoint, uint16_t cluster_id,
                                reg_cluster_dir_t direction);

/**
 * @brief Find cluster on an endpoint
 * @param endpoint Endpoint pointer
 * @param cluster_id Cluster ID
 * @return Pointer to cluster, or NULL if not found
 */
reg_cluster_t *reg_find_cluster(reg_endpoint_t *endpoint, uint16_t cluster_id);

/**
 * @brief Update attribute value
 * @param cluster Cluster pointer
 * @param attr_id Attribute ID
 * @param type Attribute type
 * @param value Attribute value
 * @return OS_OK on success
 */
os_err_t reg_update_attribute(reg_cluster_t *cluster, uint16_t attr_id,
                               reg_attr_type_t type, const reg_attr_value_t *value);

/**
 * @brief Find attribute in a cluster
 * @param cluster Cluster pointer
 * @param attr_id Attribute ID
 * @return Pointer to attribute, or NULL if not found
 */
reg_attribute_t *reg_find_attribute(reg_cluster_t *cluster, uint16_t attr_id);

/**
 * @brief Persist registry to storage
 * @return OS_OK on success
 */
os_err_t reg_persist(void);

/**
 * @brief Restore registry from storage
 * @return OS_OK on success
 */
os_err_t reg_restore(void);

/**
 * @brief Get state name string
 * @param state State value
 * @return State name
 */
const char *reg_state_name(reg_state_t state);

/**
 * @brief Initialize registry shell commands
 * @return OS_OK on success
 */
os_err_t reg_shell_init(void);

#ifdef __cplusplus
}
#endif

#endif /* REGISTRY_H */
