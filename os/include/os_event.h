/**
 * @file os_event.h
 * @brief Event bus API
 * 
 * ESP32-C6 Zigbee Bridge OS - Event bus for decoupled communication
 * 
 * Features:
 * - Fixed-size ring buffer queue
 * - Type-based event filtering
 * - Subscribe/publish pattern
 * - Safe for ISR context (publish only)
 */

#ifndef OS_EVENT_H
#define OS_EVENT_H

#include "os_types.h"
#include "os_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event types */
typedef enum {
    /* System events */
    OS_EVENT_BOOT = 0,
    OS_EVENT_LOG,
    OS_EVENT_NET_UP,
    OS_EVENT_NET_DOWN,
    
    /* Zigbee events */
    OS_EVENT_ZB_STACK_UP,
    OS_EVENT_ZB_DEVICE_JOINED,
    OS_EVENT_ZB_DEVICE_LEFT,
    OS_EVENT_ZB_ANNOUNCE,
    OS_EVENT_ZB_DESC_ENDPOINTS,
    OS_EVENT_ZB_DESC_CLUSTERS,
    OS_EVENT_ZB_ATTR_REPORT,
    
    /* Capability events */
    OS_EVENT_CAP_STATE_CHANGED,
    OS_EVENT_CAP_COMMAND,
    
    /* Persistence events */
    OS_EVENT_PERSIST_FLUSH,
    
    /* User/test events */
    OS_EVENT_USER_BASE = 100,
    
    OS_EVENT_TYPE_MAX = 255
} os_event_type_t;

/* Event priorities (for future use) */
typedef enum {
    OS_EVENT_PRIO_LOW = 0,
    OS_EVENT_PRIO_NORMAL,
    OS_EVENT_PRIO_HIGH,
    OS_EVENT_PRIO_CRITICAL,
} os_event_prio_t;

/* Maximum payload size */
#define OS_EVENT_PAYLOAD_SIZE  32

/* Event structure */
typedef struct {
    os_event_type_t type;
    os_tick_t timestamp;
    os_corr_id_t corr_id;
    uint8_t src_id;
    uint8_t payload_len;
    uint8_t payload[OS_EVENT_PAYLOAD_SIZE];
} os_event_t;

/* Event handler callback */
typedef void (*os_event_handler_t)(const os_event_t *event, void *ctx);

/* Filter for subscriptions */
typedef struct {
    os_event_type_t type_min;   /* Minimum type (inclusive) */
    os_event_type_t type_max;   /* Maximum type (inclusive) */
} os_event_filter_t;

/* Bus statistics */
typedef struct {
    uint32_t events_published;
    uint32_t events_dispatched;
    uint32_t events_dropped;
    uint32_t queue_high_water;
    uint32_t current_queue_size;
} os_event_stats_t;

/**
 * @brief Initialize the event bus
 * @return OS_OK on success
 */
os_err_t os_event_init(void);

/**
 * @brief Publish an event to the bus
 * @param event Event to publish
 * @return OS_OK on success, OS_ERR_FULL if queue full
 * @note Safe to call from ISR context
 */
os_err_t os_event_publish(const os_event_t *event);

/**
 * @brief Publish an event with just type and payload
 * @param type Event type
 * @param payload Payload data (can be NULL)
 * @param payload_len Payload length
 * @return OS_OK on success
 */
os_err_t os_event_emit(os_event_type_t type, const void *payload, uint8_t payload_len);

/**
 * @brief Subscribe to events matching filter
 * @param filter Event filter
 * @param handler Handler callback
 * @param ctx Context passed to handler
 * @return OS_OK on success
 */
os_err_t os_event_subscribe(const os_event_filter_t *filter,
                            os_event_handler_t handler, void *ctx);

/**
 * @brief Unsubscribe a handler
 * @param handler Handler to remove
 * @return OS_OK on success
 */
os_err_t os_event_unsubscribe(os_event_handler_t handler);

/**
 * @brief Dispatch pending events to subscribers
 * @param max_events Maximum events to dispatch (0 = all)
 * @return Number of events dispatched
 */
uint32_t os_event_dispatch(uint32_t max_events);

/**
 * @brief Get bus statistics
 * @param stats Output statistics
 * @return OS_OK on success
 */
os_err_t os_event_get_stats(os_event_stats_t *stats);

/**
 * @brief Generate a new correlation ID
 * @return Unique correlation ID
 */
os_corr_id_t os_event_new_corr_id(void);

/* Helper macros for common event types */
#define OS_EVENT_FILTER_ALL     {0, OS_EVENT_TYPE_MAX}
#define OS_EVENT_FILTER_ZB      {OS_EVENT_ZB_STACK_UP, OS_EVENT_ZB_ATTR_REPORT}
#define OS_EVENT_FILTER_CAP     {OS_EVENT_CAP_STATE_CHANGED, OS_EVENT_CAP_COMMAND}

#ifdef __cplusplus
}
#endif

#endif /* OS_EVENT_H */
