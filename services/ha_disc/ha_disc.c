/**
 * @file ha_disc.c
 * @brief Home Assistant MQTT Discovery implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - HA Discovery service (T080)
 * 
 * Generates and publishes Home Assistant MQTT discovery messages.
 */

#include "ha_disc.h"
#include "registry.h"
#include "capability.h"
#include "mqtt_adapter.h"
#include "os.h"
#include <string.h>
#include <stdio.h>

#define HA_MODULE "HA_DISC"

/* HA Discovery topic prefix */
#define HA_DISCOVERY_PREFIX "homeassistant"

/* Availability topic */
#define HA_AVAILABILITY_TOPIC "bridge/status"

/* Bridge ID (should come from persistence in production) */
#define HA_BRIDGE_ID "zigbee_bridge"

/* Topic base for state/commands */
#define TOPIC_BASE "bridge"

/* Maximum payload size for discovery config JSON */
#define HA_MAX_PAYLOAD_SIZE 1024

/* Timing constants */
#define HA_DISC_STARTUP_DELAY_MS   2000
#define HA_DISC_POLLING_INTERVAL_MS 5000

/* Component names */
static const char *component_names[] = {
    "light",
    "switch",
    "sensor",
    "binary_sensor"
};

/* Pending publish tracking */
#define HA_MAX_PENDING 32

typedef struct {
    os_eui64_t node_addr;
    bool pending;
} ha_pending_t;

/* Service state */
static struct {
    bool initialized;
    ha_pending_t pending[HA_MAX_PENDING];
    uint32_t pending_count;
} service = {0};

/* Forward declarations */
static os_err_t publish_light_discovery(os_eui64_t node_addr, bool has_level);
static os_err_t publish_sensor_discovery(os_eui64_t node_addr, cap_id_t cap_id);
static void handle_reg_node_ready(const os_event_t *event, void *ctx);
static void handle_mqtt_connected(const os_event_t *event, void *ctx);
static void handle_node_removed(const os_event_t *event, void *ctx);
static void add_pending(os_eui64_t node_addr);
static bool check_node_has_cap(os_eui64_t node_addr, cap_id_t cap_id);

/**
 * @brief Escape a string for JSON encoding
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param src Source string
 * @return Number of characters written (excluding null terminator)
 */
static size_t json_escape_string(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0 || !src) {
        return 0;
    }
    
    size_t written = 0;
    const char *p = src;
    
    while (*p && written < dest_size - 1) {
        switch (*p) {
            case '"':
            case '\\':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = *p;
                break;
            case '\n':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = 'n';
                break;
            case '\r':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = 'r';
                break;
            case '\t':
                if (written + 2 >= dest_size) goto done;
                dest[written++] = '\\';
                dest[written++] = 't';
                break;
            default:
                dest[written++] = *p;
                break;
        }
        p++;
    }
    
done:
    dest[written] = '\0';
    return written;
}

os_err_t ha_disc_init(void) {
    if (service.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&service, 0, sizeof(service));
    service.initialized = true;
    
    /* Subscribe to relevant events */
    os_event_filter_t filter_cap = {OS_EVENT_CAP_STATE_CHANGED, OS_EVENT_CAP_STATE_CHANGED};
    os_event_subscribe(&filter_cap, handle_reg_node_ready, NULL);
    
    os_event_filter_t filter_net = {OS_EVENT_NET_UP, OS_EVENT_NET_UP};
    os_event_subscribe(&filter_net, handle_mqtt_connected, NULL);
    
    os_event_filter_t filter_left = {OS_EVENT_ZB_DEVICE_LEFT, OS_EVENT_ZB_DEVICE_LEFT};
    os_event_subscribe(&filter_left, handle_node_removed, NULL);
    
    LOG_I(HA_MODULE, "HA Discovery service initialized");
    
    return OS_OK;
}

os_err_t ha_disc_publish_node(os_eui64_t node_addr) {
    if (!service.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    /* Check if MQTT is connected */
    if (mqtt_get_state() != MQTT_STATE_CONNECTED) {
        LOG_D(HA_MODULE, "MQTT not connected, queuing publish for " OS_EUI64_FMT,
              OS_EUI64_ARG(node_addr));
        add_pending(node_addr);
        return OS_OK;
    }
    
    reg_node_t *node = reg_find_node(node_addr);
    if (!node || node->state != REG_STATE_READY) {
        return OS_ERR_NOT_FOUND;
    }
    
    LOG_I(HA_MODULE, "Publishing discovery for node " OS_EUI64_FMT,
          OS_EUI64_ARG(node_addr));
    
    os_err_t result = OS_OK;
    
    /* Check for light capabilities and merge if both present */
    bool has_light_on = check_node_has_cap(node_addr, CAP_LIGHT_ON);
    bool has_light_level = check_node_has_cap(node_addr, CAP_LIGHT_LEVEL);
    
    if (has_light_on) {
        /* Publish merged light discovery (with or without brightness) */
        os_err_t err = publish_light_discovery(node_addr, has_light_level);
        if (err != OS_OK) {
            LOG_E(HA_MODULE, "Failed to publish light discovery for node " OS_EUI64_FMT " (err=%d)",
                  OS_EUI64_ARG(node_addr), err);
            if (result == OS_OK) result = err;
        }
    }
    
    /* Publish sensor discoveries */
    if (check_node_has_cap(node_addr, CAP_SENSOR_TEMPERATURE)) {
        os_err_t err = publish_sensor_discovery(node_addr, CAP_SENSOR_TEMPERATURE);
        if (err != OS_OK) {
            LOG_E(HA_MODULE, "Failed to publish temperature sensor discovery for node " OS_EUI64_FMT " (err=%d)",
                  OS_EUI64_ARG(node_addr), err);
            if (result == OS_OK) result = err;
        }
    }
    if (check_node_has_cap(node_addr, CAP_SENSOR_HUMIDITY)) {
        os_err_t err = publish_sensor_discovery(node_addr, CAP_SENSOR_HUMIDITY);
        if (err != OS_OK) {
            LOG_E(HA_MODULE, "Failed to publish humidity sensor discovery for node " OS_EUI64_FMT " (err=%d)",
                  OS_EUI64_ARG(node_addr), err);
            if (result == OS_OK) result = err;
        }
    }
    if (check_node_has_cap(node_addr, CAP_SENSOR_CONTACT)) {
        os_err_t err = publish_sensor_discovery(node_addr, CAP_SENSOR_CONTACT);
        if (err != OS_OK) {
            LOG_E(HA_MODULE, "Failed to publish contact sensor discovery for node " OS_EUI64_FMT " (err=%d)",
                  OS_EUI64_ARG(node_addr), err);
            if (result == OS_OK) result = err;
        }
    }
    if (check_node_has_cap(node_addr, CAP_SENSOR_MOTION)) {
        os_err_t err = publish_sensor_discovery(node_addr, CAP_SENSOR_MOTION);
        if (err != OS_OK) {
            LOG_E(HA_MODULE, "Failed to publish motion sensor discovery for node " OS_EUI64_FMT " (err=%d)",
                  OS_EUI64_ARG(node_addr), err);
            if (result == OS_OK) result = err;
        }
    }
    
    return result;
}

os_err_t ha_disc_unpublish_node(os_eui64_t node_addr) {
    if (!service.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    if (mqtt_get_state() != MQTT_STATE_CONNECTED) {
        return OS_ERR_NOT_READY;
    }
    
    LOG_I(HA_MODULE, "Unpublishing discovery for node " OS_EUI64_FMT,
          OS_EUI64_ARG(node_addr));
    
    os_err_t result = OS_OK;
    
    /* Publish empty payloads to remove entities (with retain) */
    char topic[256];
    
    /* Remove light entity */
    snprintf(topic, sizeof(topic), "%s/light/%s_" OS_EUI64_FMT "_light/config",
             HA_DISCOVERY_PREFIX, HA_BRIDGE_ID, OS_EUI64_ARG(node_addr));
    os_err_t err = mqtt_publish(topic, "", 0);
    if (err != OS_OK) {
        LOG_E(HA_MODULE, "Failed to unpublish light for node " OS_EUI64_FMT " (err=%d)",
              OS_EUI64_ARG(node_addr), err);
        if (result == OS_OK) result = err;
    }
    
    /* Remove sensor entities */
    const cap_id_t sensors[] = {
        CAP_SENSOR_TEMPERATURE, CAP_SENSOR_HUMIDITY,
        CAP_SENSOR_CONTACT, CAP_SENSOR_MOTION
    };
    
    for (size_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); i++) {
        const cap_info_t *info = cap_get_info(sensors[i]);
        if (info) {
            /* Sanitize capability name: replace '.' with '_' */
            char cap_sanitized[32];
            strncpy(cap_sanitized, info->name, sizeof(cap_sanitized) - 1);
            cap_sanitized[sizeof(cap_sanitized) - 1] = '\0';
            for (char *p = cap_sanitized; *p; p++) {
                if (*p == '.') *p = '_';
            }
            
            snprintf(topic, sizeof(topic), "%s/sensor/%s_" OS_EUI64_FMT "_%s/config",
                     HA_DISCOVERY_PREFIX, HA_BRIDGE_ID, OS_EUI64_ARG(node_addr),
                     cap_sanitized);
            err = mqtt_publish(topic, "", 0);
            if (err != OS_OK) {
                LOG_E(HA_MODULE, "Failed to unpublish sensor %s for node " OS_EUI64_FMT " (err=%d)",
                      info->name, OS_EUI64_ARG(node_addr), err);
                if (result == OS_OK) result = err;
            }
        }
    }
    
    return result;
}

uint32_t ha_disc_publish_all(void) {
    if (!service.initialized) {
        return 0;
    }
    
    uint32_t count = 0;
    uint32_t node_count = reg_node_count();
    
    for (uint32_t i = 0; i < node_count; i++) {
        reg_node_info_t info;
        if (reg_get_node_info(i, &info) == OS_OK) {
            if (info.state == REG_STATE_READY) {
                if (ha_disc_publish_node(info.ieee_addr) == OS_OK) {
                    count++;
                }
            }
        }
    }
    
    LOG_I(HA_MODULE, "Published discovery for %u nodes", count);
    return count;
}

uint32_t ha_disc_flush_pending(void) {
    if (!service.initialized) {
        return 0;
    }
    
    uint32_t flushed = 0;
    
    for (uint32_t i = 0; i < HA_MAX_PENDING; i++) {
        if (service.pending[i].pending) {
            if (ha_disc_publish_node(service.pending[i].node_addr) == OS_OK) {
                service.pending[i].pending = false;
                service.pending_count--;
                flushed++;
            }
        }
    }
    
    if (flushed > 0) {
        LOG_I(HA_MODULE, "Flushed %u pending discovery publishes", flushed);
    }
    
    return flushed;
}

const char *ha_disc_component_name(ha_component_t component) {
    if (component < HA_COMPONENT_MAX) {
        return component_names[component];
    }
    return "unknown";
}

os_err_t ha_disc_generate_config(os_eui64_t node_addr, cap_id_t cap_id,
                                  ha_disc_config_t *out_config) {
    if (!service.initialized || !out_config) {
        return OS_ERR_INVALID_ARG;
    }
    
    const cap_info_t *cap_info = cap_get_info(cap_id);
    if (!cap_info) {
        return OS_ERR_INVALID_ARG;
    }
    
    memset(out_config, 0, sizeof(*out_config));
    
    /* Determine component type */
    switch (cap_id) {
        case CAP_LIGHT_ON:
        case CAP_LIGHT_LEVEL:
            out_config->component = HA_COMPONENT_LIGHT;
            break;
        case CAP_SWITCH_ON:
            out_config->component = HA_COMPONENT_SWITCH;
            break;
        case CAP_SENSOR_CONTACT:
        case CAP_SENSOR_MOTION:
            out_config->component = HA_COMPONENT_BINARY_SENSOR;
            break;
        default:
            out_config->component = HA_COMPONENT_SENSOR;
            break;
    }
    
    /* Generate unique_id */
    snprintf(out_config->unique_id, sizeof(out_config->unique_id),
             "%s_" OS_EUI64_FMT "_%s", HA_BRIDGE_ID, OS_EUI64_ARG(node_addr),
             cap_info->name);
    
    /* Sanitize unique_id (replace '.' with '_') */
    for (char *p = out_config->unique_id; *p; p++) {
        if (*p == '.') *p = '_';
    }
    
    /* Get node info for name */
    reg_node_t *node = reg_find_node(node_addr);
    if (node && node->friendly_name[0]) {
        strncpy(out_config->name, node->friendly_name, sizeof(out_config->name) - 1);
        out_config->name[sizeof(out_config->name) - 1] = '\0';
    } else if (node && node->model[0]) {
        strncpy(out_config->name, node->model, sizeof(out_config->name) - 1);
        out_config->name[sizeof(out_config->name) - 1] = '\0';
    } else {
        snprintf(out_config->name, sizeof(out_config->name),
                 "Zigbee " OS_EUI64_FMT, OS_EUI64_ARG(node_addr));
    }
    
    /* Generate topics */
    snprintf(out_config->state_topic, sizeof(out_config->state_topic),
             TOPIC_BASE "/" OS_EUI64_FMT "/%s/state",
             OS_EUI64_ARG(node_addr), cap_info->name);
    
    snprintf(out_config->command_topic, sizeof(out_config->command_topic),
             TOPIC_BASE "/" OS_EUI64_FMT "/%s/set",
             OS_EUI64_ARG(node_addr), cap_info->name);
    
    strncpy(out_config->availability_topic, HA_AVAILABILITY_TOPIC,
            sizeof(out_config->availability_topic) - 1);
    out_config->availability_topic[sizeof(out_config->availability_topic) - 1] = '\0';
    
    return OS_OK;
}

void ha_disc_task(void *arg) {
    (void)arg;
    
    LOG_I(HA_MODULE, "HA Discovery task started");
    
    /* Wait for initial setup */
    os_sleep(HA_DISC_STARTUP_DELAY_MS);
    
    while (1) {
        /* Check for pending publishes when MQTT is connected */
        if (mqtt_get_state() == MQTT_STATE_CONNECTED && service.pending_count > 0) {
            ha_disc_flush_pending();
        }
        
        os_sleep(HA_DISC_POLLING_INTERVAL_MS);
    }
}

/* Internal functions */

static os_err_t publish_light_discovery(os_eui64_t node_addr, bool has_level) {
    char topic[256];
    char payload[HA_MAX_PAYLOAD_SIZE];
    
    reg_node_t *node = reg_find_node(node_addr);
    
    /* Get device info and escape for JSON */
    char name_escaped[64];
    char manufacturer_escaped[64];
    char model_escaped[64];
    
    const char *name_raw = (node && node->friendly_name[0]) ? node->friendly_name :
                           (node && node->model[0]) ? node->model : "Zigbee Light";
    const char *manufacturer_raw = (node && node->manufacturer[0]) ? node->manufacturer : "";
    const char *model_raw = (node && node->model[0]) ? node->model : "";
    
    json_escape_string(name_escaped, sizeof(name_escaped), name_raw);
    json_escape_string(manufacturer_escaped, sizeof(manufacturer_escaped), manufacturer_raw);
    json_escape_string(model_escaped, sizeof(model_escaped), model_raw);
    
    /* Build discovery topic */
    snprintf(topic, sizeof(topic), "%s/light/%s_" OS_EUI64_FMT "_light/config",
             HA_DISCOVERY_PREFIX, HA_BRIDGE_ID, OS_EUI64_ARG(node_addr));
    
    /* Build discovery payload JSON */
    if (has_level) {
        /* Merged light with brightness */
        snprintf(payload, sizeof(payload),
                 "{"
                 "\"name\":\"%s\","
                 "\"unique_id\":\"%s_" OS_EUI64_FMT "_light\","
                 "\"availability_topic\":\"%s\","
                 "\"payload_available\":\"online\","
                 "\"payload_not_available\":\"offline\","
                 "\"state_topic\":\"" TOPIC_BASE "/" OS_EUI64_FMT "/light.on/state\","
                 "\"command_topic\":\"" TOPIC_BASE "/" OS_EUI64_FMT "/light.on/set\","
                 "\"value_template\":\"{{ value_json.v }}\","
                 "\"state_value_template\":\"{{ 'ON' if value_json.v else 'OFF' }}\","
                 "\"payload_on\":\"{\\\"v\\\":true}\","
                 "\"payload_off\":\"{\\\"v\\\":false}\","
                 "\"brightness_state_topic\":\"" TOPIC_BASE "/" OS_EUI64_FMT "/light.level/state\","
                 "\"brightness_command_topic\":\"" TOPIC_BASE "/" OS_EUI64_FMT "/light.level/set\","
                 "\"brightness_value_template\":\"{{ (value_json.v | float * 2.55) | int }}\","
                 "\"brightness_scale\":255,"
                 "\"device\":{"
                   "\"identifiers\":[\"%s_" OS_EUI64_FMT "\"],"
                   "\"name\":\"%s\","
                   "\"manufacturer\":\"%s\","
                   "\"model\":\"%s\""
                 "}"
                 "}",
                 name_escaped,
                 HA_BRIDGE_ID, OS_EUI64_ARG(node_addr),
                 HA_AVAILABILITY_TOPIC,
                 OS_EUI64_ARG(node_addr),
                 OS_EUI64_ARG(node_addr),
                 OS_EUI64_ARG(node_addr),
                 OS_EUI64_ARG(node_addr),
                 HA_BRIDGE_ID, OS_EUI64_ARG(node_addr),
                 name_escaped, manufacturer_escaped, model_escaped);
    } else {
        /* Simple on/off light */
        snprintf(payload, sizeof(payload),
                 "{"
                 "\"name\":\"%s\","
                 "\"unique_id\":\"%s_" OS_EUI64_FMT "_light\","
                 "\"availability_topic\":\"%s\","
                 "\"payload_available\":\"online\","
                 "\"payload_not_available\":\"offline\","
                 "\"state_topic\":\"" TOPIC_BASE "/" OS_EUI64_FMT "/light.on/state\","
                 "\"command_topic\":\"" TOPIC_BASE "/" OS_EUI64_FMT "/light.on/set\","
                 "\"value_template\":\"{{ value_json.v }}\","
                 "\"state_value_template\":\"{{ 'ON' if value_json.v else 'OFF' }}\","
                 "\"payload_on\":\"{\\\"v\\\":true}\","
                 "\"payload_off\":\"{\\\"v\\\":false}\","
                 "\"device\":{"
                   "\"identifiers\":[\"%s_" OS_EUI64_FMT "\"],"
                   "\"name\":\"%s\","
                   "\"manufacturer\":\"%s\","
                   "\"model\":\"%s\""
                 "}"
                 "}",
                 name_escaped,
                 HA_BRIDGE_ID, OS_EUI64_ARG(node_addr),
                 HA_AVAILABILITY_TOPIC,
                 OS_EUI64_ARG(node_addr),
                 OS_EUI64_ARG(node_addr),
                 HA_BRIDGE_ID, OS_EUI64_ARG(node_addr),
                 name_escaped, manufacturer_escaped, model_escaped);
    }
    
    return mqtt_publish(topic, payload, strlen(payload));
}

static os_err_t publish_sensor_discovery(os_eui64_t node_addr, cap_id_t cap_id) {
    char topic[256];
    char payload[HA_MAX_PAYLOAD_SIZE];
    
    const cap_info_t *cap_info = cap_get_info(cap_id);
    if (!cap_info) {
        return OS_ERR_INVALID_ARG;
    }
    
    reg_node_t *node = reg_find_node(node_addr);
    
    /* Get device info and escape for JSON */
    char device_name_escaped[64];
    char manufacturer_escaped[64];
    char model_escaped[64];
    char unit_escaped[16];
    
    const char *device_name_raw = (node && node->friendly_name[0]) ? node->friendly_name :
                                  (node && node->model[0]) ? node->model : "Zigbee Sensor";
    const char *manufacturer_raw = (node && node->manufacturer[0]) ? node->manufacturer : "";
    const char *model_raw = (node && node->model[0]) ? node->model : "";
    
    json_escape_string(device_name_escaped, sizeof(device_name_escaped), device_name_raw);
    json_escape_string(manufacturer_escaped, sizeof(manufacturer_escaped), manufacturer_raw);
    json_escape_string(model_escaped, sizeof(model_escaped), model_raw);
    json_escape_string(unit_escaped, sizeof(unit_escaped), cap_info->unit);
    
    /* Sanitize capability name */
    char cap_sanitized[32];
    strncpy(cap_sanitized, cap_info->name, sizeof(cap_sanitized) - 1);
    cap_sanitized[sizeof(cap_sanitized) - 1] = '\0';
    for (char *p = cap_sanitized; *p; p++) {
        if (*p == '.') *p = '_';
    }
    
    /* Determine HA component and device class */
    const char *component = "sensor";
    const char *device_class = "";
    
    switch (cap_id) {
        case CAP_SENSOR_TEMPERATURE:
            device_class = "temperature";
            break;
        case CAP_SENSOR_HUMIDITY:
            device_class = "humidity";
            break;
        case CAP_SENSOR_CONTACT:
            component = "binary_sensor";
            device_class = "door";
            break;
        case CAP_SENSOR_MOTION:
            component = "binary_sensor";
            device_class = "motion";
            break;
        default:
            break;
    }
    
    /* Build discovery topic */
    snprintf(topic, sizeof(topic), "%s/%s/%s_" OS_EUI64_FMT "_%s/config",
             HA_DISCOVERY_PREFIX, component, HA_BRIDGE_ID,
             OS_EUI64_ARG(node_addr), cap_sanitized);
    
    /* Build discovery payload JSON */
    snprintf(payload, sizeof(payload),
             "{"
             "\"name\":\"%s %s\","
             "\"unique_id\":\"%s_" OS_EUI64_FMT "_%s\","
             "\"device_class\":\"%s\","
             "\"state_topic\":\"" TOPIC_BASE "/" OS_EUI64_FMT "/%s/state\","
             "\"value_template\":\"{{ value_json.v }}\","
             "\"unit_of_measurement\":\"%s\","
             "\"availability_topic\":\"%s\","
             "\"payload_available\":\"online\","
             "\"payload_not_available\":\"offline\","
             "\"device\":{"
               "\"identifiers\":[\"%s_" OS_EUI64_FMT "\"],"
               "\"name\":\"%s\","
               "\"manufacturer\":\"%s\","
               "\"model\":\"%s\""
             "}"
             "}",
             device_name_escaped, cap_info->name,
             HA_BRIDGE_ID, OS_EUI64_ARG(node_addr), cap_sanitized,
             device_class,
             OS_EUI64_ARG(node_addr), cap_info->name,
             unit_escaped,
             HA_AVAILABILITY_TOPIC,
             HA_BRIDGE_ID, OS_EUI64_ARG(node_addr),
             device_name_escaped, manufacturer_escaped, model_escaped);
    
    return mqtt_publish(topic, payload, strlen(payload));
}

static void handle_reg_node_ready(const os_event_t *event, void *ctx) {
    (void)ctx;
    (void)event;
    
    /*
     * This handler is subscribed to OS_EVENT_CAP_STATE_CHANGED.
     * In the current implementation, ha_disc_publish_node() is called explicitly
     * when nodes transition to READY state (typically from the interview service).
     * 
     * In a full implementation, this handler would:
     * 1. Track which nodes have had discovery published
     * 2. Detect capability additions/changes
     * 3. Republish discovery when capabilities change
     * 
     * For now, discovery is triggered explicitly via ha_disc_publish_node()
     * and ha_disc_publish_all() calls from other services.
     */
}

static void handle_mqtt_connected(const os_event_t *event, void *ctx) {
    (void)ctx;
    (void)event;
    
    LOG_D(HA_MODULE, "MQTT connected, flushing pending discovery");
    ha_disc_flush_pending();
}

static void handle_node_removed(const os_event_t *event, void *ctx) {
    (void)ctx;
    
    if (event->payload_len >= sizeof(os_eui64_t)) {
        os_eui64_t node_addr;
        memcpy(&node_addr, event->payload, sizeof(node_addr));
        ha_disc_unpublish_node(node_addr);
    }
}

static void add_pending(os_eui64_t node_addr) {
    /* Check if already pending */
    for (uint32_t i = 0; i < HA_MAX_PENDING; i++) {
        if (service.pending[i].pending && service.pending[i].node_addr == node_addr) {
            return;
        }
    }
    
    /* Find free slot */
    for (uint32_t i = 0; i < HA_MAX_PENDING; i++) {
        if (!service.pending[i].pending) {
            service.pending[i].node_addr = node_addr;
            service.pending[i].pending = true;
            service.pending_count++;
            return;
        }
    }
    
    LOG_W(HA_MODULE, "Pending queue full, cannot add node " OS_EUI64_FMT,
          OS_EUI64_ARG(node_addr));
}

static bool check_node_has_cap(os_eui64_t node_addr, cap_id_t cap_id) {
    cap_state_t state;
    return cap_get_state(node_addr, cap_id, &state) == OS_OK;
}
