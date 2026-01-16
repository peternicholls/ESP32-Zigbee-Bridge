/**
 * @file mqtt_adapter.c
 * @brief MQTT adapter implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - MQTT northbound adapter
 * 
 * On host: Simulated MQTT (logs to console).
 * On ESP32: Uses ESP-IDF MQTT client.
 */

#include "mqtt_adapter.h"
#include "capability.h"
#include "registry.h"
#include "os.h"
#include <string.h>
#include <stdio.h>

#define MQTT_MODULE "MQTT"

/* Topic base */
#define TOPIC_BASE "bridge"

/* Maximum topic length */
#define MAX_TOPIC_LEN 128

/* Maximum payload length */
#define MAX_PAYLOAD_LEN 256

/* Default configuration values */
#define MQTT_DEFAULT_BROKER_URI  "mqtt://localhost:1883"
#define MQTT_DEFAULT_CLIENT_ID   "zigbee-bridge"
#define MQTT_DEFAULT_KEEPALIVE   30

/* State names */
static const char *state_names[] = {
    "DISCONNECTED",
    "CONNECTING",
    "CONNECTED",
    "ERROR"
};

/* Service state */
static struct {
    bool initialized;
    mqtt_state_t state;
    mqtt_config_t config;
    mqtt_stats_t stats;
} adapter = {0};

/* Forward declarations */
static void handle_cap_state_changed(const os_event_t *event, void *ctx);

os_err_t mqtt_init(const mqtt_config_t *config) {
    if (adapter.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&adapter, 0, sizeof(adapter));
    
    if (config) {
        adapter.config = *config;
    } else {
        /* Default config */
        adapter.config.broker_uri = MQTT_DEFAULT_BROKER_URI;
        adapter.config.client_id = MQTT_DEFAULT_CLIENT_ID;
        adapter.config.keepalive_sec = MQTT_DEFAULT_KEEPALIVE;
    }
    
    adapter.state = MQTT_STATE_DISCONNECTED;
    adapter.initialized = true;
    
    /* Subscribe to capability state changes */
    os_event_filter_t filter = {OS_EVENT_CAP_STATE_CHANGED, OS_EVENT_CAP_STATE_CHANGED};
    os_event_subscribe(&filter, handle_cap_state_changed, NULL);
    
    LOG_I(MQTT_MODULE, "MQTT adapter initialized (broker: %s)", adapter.config.broker_uri);
    
    return OS_OK;
}

os_err_t mqtt_connect(void) {
    if (!adapter.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    if (adapter.state == MQTT_STATE_CONNECTED) {
        return OS_OK;
    }
    
    LOG_I(MQTT_MODULE, "Connecting to %s...", adapter.config.broker_uri);
    adapter.state = MQTT_STATE_CONNECTING;
    
#ifdef OS_PLATFORM_HOST
    /* Simulate connection */
    adapter.state = MQTT_STATE_CONNECTED;
    LOG_I(MQTT_MODULE, "Connected (simulated)");
    
    /* Publish online status */
    mqtt_publish_status(true);
#else
    /* Real ESP32 MQTT implementation would go here */
#endif
    
    return OS_OK;
}

os_err_t mqtt_disconnect(void) {
    if (!adapter.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    /* Publish offline status before disconnect */
    mqtt_publish_status(false);
    
    adapter.state = MQTT_STATE_DISCONNECTED;
    LOG_I(MQTT_MODULE, "Disconnected");
    
    return OS_OK;
}

mqtt_state_t mqtt_get_state(void) {
    return adapter.state;
}

os_err_t mqtt_publish_state(os_eui64_t node_addr, cap_id_t cap_id, const cap_value_t *value) {
    if (!adapter.initialized || adapter.state != MQTT_STATE_CONNECTED) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    const cap_info_t *info = cap_get_info(cap_id);
    if (!info) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Build topic: bridge/<node_id>/<capability>/state */
    char topic[MAX_TOPIC_LEN];
    snprintf(topic, sizeof(topic), TOPIC_BASE "/" OS_EUI64_FMT "/%s/state",
             OS_EUI64_ARG(node_addr), info->name);
    
    /* Build payload */
    char payload[MAX_PAYLOAD_LEN];
    switch (info->type) {
        case CAP_VALUE_BOOL:
            snprintf(payload, sizeof(payload), "{\"v\":%s,\"ts\":%u}",
                     value->b ? "true" : "false", (unsigned)os_now_ticks());
            break;
        case CAP_VALUE_INT:
            snprintf(payload, sizeof(payload), "{\"v\":%d,\"ts\":%u}",
                     value->i, (unsigned)os_now_ticks());
            break;
        case CAP_VALUE_FLOAT:
            snprintf(payload, sizeof(payload), "{\"v\":%.2f,\"ts\":%u}",
                     (double)value->f, (unsigned)os_now_ticks());
            break;
        default:
            snprintf(payload, sizeof(payload), "{\"v\":\"%s\",\"ts\":%u}",
                     value->str, (unsigned)os_now_ticks());
            break;
    }
    
    return mqtt_publish(topic, payload, strlen(payload));
}

os_err_t mqtt_publish_meta(os_eui64_t node_addr, const char *manufacturer, const char *model) {
    if (!adapter.initialized || adapter.state != MQTT_STATE_CONNECTED) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    /* Build topic: bridge/<node_id>/meta */
    char topic[MAX_TOPIC_LEN];
    snprintf(topic, sizeof(topic), TOPIC_BASE "/" OS_EUI64_FMT "/meta",
             OS_EUI64_ARG(node_addr));
    
    /* Build payload */
    char payload[MAX_PAYLOAD_LEN];
    snprintf(payload, sizeof(payload), 
             "{\"ieee\":\"" OS_EUI64_FMT "\",\"manufacturer\":\"%s\",\"model\":\"%s\"}",
             OS_EUI64_ARG(node_addr),
             manufacturer ? manufacturer : "",
             model ? model : "");
    
    return mqtt_publish(topic, payload, strlen(payload));
}

os_err_t mqtt_publish_status(bool online) {
    if (!adapter.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    char topic[MAX_TOPIC_LEN];
    snprintf(topic, sizeof(topic), TOPIC_BASE "/status");
    
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"v\":\"%s\"}", online ? "online" : "offline");
    
    return mqtt_publish(topic, payload, strlen(payload));
}

os_err_t mqtt_publish(const char *topic, const void *payload, size_t len) {
    if (!adapter.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    if (adapter.state != MQTT_STATE_CONNECTED) {
        LOG_W(MQTT_MODULE, "Not connected, cannot publish");
        return OS_ERR_BUSY;
    }
    
#ifdef OS_PLATFORM_HOST
    /* Simulate publish - just log */
    LOG_I(MQTT_MODULE, "PUB %s: %.*s", topic, (int)len, (const char *)payload);
#else
    /* Real ESP32 MQTT publish would go here */
#endif
    
    adapter.stats.messages_published++;
    return OS_OK;
}

os_err_t mqtt_subscribe_commands(void) {
    if (!adapter.initialized || adapter.state != MQTT_STATE_CONNECTED) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    /* Subscribe to command topics: bridge/+/+/set */
    char topic[MAX_TOPIC_LEN];
    snprintf(topic, sizeof(topic), TOPIC_BASE "/+/+/set");
    
    LOG_I(MQTT_MODULE, "Subscribing to %s", topic);
    
#ifdef OS_PLATFORM_HOST
    /* Simulate subscribe */
    LOG_D(MQTT_MODULE, "Subscribed (simulated)");
#else
    /* Real ESP32 MQTT subscribe would go here */
#endif
    
    return OS_OK;
}

os_err_t mqtt_get_stats(mqtt_stats_t *stats) {
    if (!adapter.initialized || !stats) {
        return OS_ERR_INVALID_ARG;
    }
    
    *stats = adapter.stats;
    return OS_OK;
}

void mqtt_task(void *arg) {
    (void)arg;
    
    LOG_I(MQTT_MODULE, "MQTT task started");
    
    /* Initial connect */
    os_sleep(1000);  /* Wait for system to stabilize */
    mqtt_connect();
    mqtt_subscribe_commands();
    
    while (1) {
        /* Check connection and reconnect if needed */
        if (adapter.state == MQTT_STATE_DISCONNECTED) {
            LOG_I(MQTT_MODULE, "Reconnecting...");
            adapter.stats.reconnects++;
            mqtt_connect();
        }
        
        os_sleep(5000);
    }
}

const char *mqtt_state_name(mqtt_state_t state) {
    if (state < sizeof(state_names) / sizeof(state_names[0])) {
        return state_names[state];
    }
    return "UNKNOWN";
}

/* Event handler for capability state changes */
static void handle_cap_state_changed(const os_event_t *event, void *ctx) {
    (void)ctx;
    
    if (event->type != OS_EVENT_CAP_STATE_CHANGED) {
        return;
    }
    
    /* Extract payload */
    struct {
        os_eui64_t node_addr;
        cap_id_t cap_id;
        cap_value_t value;
    } *payload = (void *)event->payload;
    
    /* Publish to MQTT */
    mqtt_publish_state(payload->node_addr, payload->cap_id, &payload->value);
}
