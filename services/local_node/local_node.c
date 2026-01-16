/**
 * @file local_node.c
 * @brief Local hardware node service
 */

#include "local_node.h"
#include "capability.h"
#include "gpio_button.h"
#include "i2c_sensor.h"
#include "os.h"
#include "registry.h"
#include <string.h>

#define LOCAL_NODE_MODULE "LOCAL_NODE"

/* Fixed identifier for the local hardware node in the simulation.
 * This should be unique in a real deployment. */
#define LOCAL_NODE_EUI64 0xABCDEF0000000001ULL

#define ZCL_CLUSTER_ONOFF        0x0006
#define ZCL_CLUSTER_TEMPERATURE  0x0402
#define ZCL_ATTR_ONOFF           0x0000
#define ZCL_ATTR_TEMPERATURE     0x0000

#define LOCAL_NODE_POLL_MS 1000

static struct {
    bool initialized;
    bool last_button;
    int16_t last_temperature;
} local_node = {0};

static void publish_button_state(bool pressed) {
    reg_attr_value_t value = {0};
    value.b = pressed;
    cap_handle_attribute_report(LOCAL_NODE_EUI64, 1, ZCL_CLUSTER_ONOFF, ZCL_ATTR_ONOFF, &value);
}

static void publish_temperature(float temp_c) {
    reg_attr_value_t value = {0};
    value.s16 = (int16_t)(temp_c * 100.0f);
    cap_handle_attribute_report(LOCAL_NODE_EUI64, 1, ZCL_CLUSTER_TEMPERATURE, ZCL_ATTR_TEMPERATURE, &value);
}

os_err_t local_node_init(void) {
    if (local_node.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }

    os_err_t err = gpio_button_init();
    if (err != OS_OK) {
        return err;
    }

    err = i2c_sensor_init();
    if (err != OS_OK) {
        return err;
    }

    reg_node_t *node = reg_add_node(LOCAL_NODE_EUI64, 0x0000);
    if (!node) {
        return OS_ERR_NO_MEM;
    }

    strncpy(node->manufacturer, "ESP32", REG_MANUFACTURER_LEN - 1);
    strncpy(node->model, "local-node", REG_MODEL_LEN - 1);
    strncpy(node->friendly_name, "Bridge Node", REG_NAME_MAX_LEN - 1);

    reg_endpoint_t *ep = reg_add_endpoint(node, 1, 0x0104, 0x0000);
    if (!ep) {
        return OS_ERR_NO_MEM;
    }

    reg_add_cluster(ep, ZCL_CLUSTER_ONOFF, REG_CLUSTER_SERVER);
    reg_add_cluster(ep, ZCL_CLUSTER_TEMPERATURE, REG_CLUSTER_SERVER);

    cap_compute_for_node(node);
    reg_set_state(node, REG_STATE_READY);

    local_node.initialized = true;
    local_node.last_button = gpio_button_read();
    local_node.last_temperature = (int16_t)(i2c_sensor_read_temperature_c() * 100.0f);

    publish_button_state(local_node.last_button);
    publish_temperature(local_node.last_temperature / 100.0f);

    LOG_I(LOCAL_NODE_MODULE, "Local node initialized");

    return OS_OK;
}

void local_node_task(void *arg) {
    (void)arg;

    LOG_I(LOCAL_NODE_MODULE, "Local node task started");

    while (1) {
        bool button = gpio_button_read();
        if (button != local_node.last_button) {
            local_node.last_button = button;
            publish_button_state(button);
        }

        float temp_c = i2c_sensor_read_temperature_c();
        int16_t temp_c_scaled = (int16_t)(temp_c * 100.0f);
        if (temp_c_scaled != local_node.last_temperature) {
            local_node.last_temperature = temp_c_scaled;
            publish_temperature(temp_c);
        }

        os_sleep(LOCAL_NODE_POLL_MS);
    }
}
