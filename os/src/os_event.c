/**
 * @file os_event.c
 * @brief Event bus implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Event bus
 * 
 * Ring buffer based event queue with subscriber dispatch.
 */

#include "os_event.h"
#include "os_fibre.h"
#include <string.h>

/* Subscriber entry */
typedef struct {
    os_event_filter_t filter;
    os_event_handler_t handler;
    void *ctx;
    bool active;
} subscriber_t;

/* Event bus state */
static struct {
    bool initialized;
    
    /* Ring buffer queue */
    os_event_t queue[OS_EVENT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    
    /* Subscribers */
    subscriber_t subscribers[OS_MAX_SUBSCRIBERS];
    uint32_t sub_count;
    
    /* Statistics */
    os_event_stats_t stats;
    
    /* Correlation ID generator */
    os_corr_id_t next_corr_id;
} bus = {0};

os_err_t os_event_init(void) {
    if (bus.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&bus, 0, sizeof(bus));
    bus.next_corr_id = 1;
    bus.initialized = true;
    
    return OS_OK;
}

os_err_t os_event_publish(const os_event_t *event) {
    if (!bus.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    if (event == NULL) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Check if queue is full */
    if (bus.count >= OS_EVENT_QUEUE_SIZE) {
        bus.stats.events_dropped++;
        return OS_ERR_FULL;
    }
    
    /* Copy event to queue */
    os_event_t *slot = &bus.queue[bus.tail];
    *slot = *event;
    
    /* Set timestamp if not provided */
    if (slot->timestamp == 0) {
        slot->timestamp = os_now_ticks();
    }
    
    /* Advance tail */
    bus.tail = (bus.tail + 1) % OS_EVENT_QUEUE_SIZE;
    bus.count++;
    bus.stats.events_published++;
    
    /* Update high water mark */
    if (bus.count > bus.stats.queue_high_water) {
        bus.stats.queue_high_water = bus.count;
    }
    
    return OS_OK;
}

os_err_t os_event_emit(os_event_type_t type, const void *payload, uint8_t payload_len) {
    os_event_t event = {0};
    event.type = type;
    event.timestamp = os_now_ticks();
    
    if (payload && payload_len > 0) {
        if (payload_len > OS_EVENT_PAYLOAD_SIZE) {
            payload_len = OS_EVENT_PAYLOAD_SIZE;
        }
        memcpy(event.payload, payload, payload_len);
        event.payload_len = payload_len;
    }
    
    return os_event_publish(&event);
}

os_err_t os_event_subscribe(const os_event_filter_t *filter,
                            os_event_handler_t handler, void *ctx) {
    if (!bus.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    if (handler == NULL) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Find free slot */
    for (uint32_t i = 0; i < OS_MAX_SUBSCRIBERS; i++) {
        if (!bus.subscribers[i].active) {
            bus.subscribers[i].filter = filter ? *filter : (os_event_filter_t){0, OS_EVENT_TYPE_MAX};
            bus.subscribers[i].handler = handler;
            bus.subscribers[i].ctx = ctx;
            bus.subscribers[i].active = true;
            bus.sub_count++;
            return OS_OK;
        }
    }
    
    return OS_ERR_FULL;
}

os_err_t os_event_unsubscribe(os_event_handler_t handler) {
    if (!bus.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    for (uint32_t i = 0; i < OS_MAX_SUBSCRIBERS; i++) {
        if (bus.subscribers[i].active && bus.subscribers[i].handler == handler) {
            bus.subscribers[i].active = false;
            bus.sub_count--;
            return OS_OK;
        }
    }
    
    return OS_ERR_NOT_FOUND;
}

static bool filter_matches(const os_event_filter_t *filter, os_event_type_t type) {
    return type >= filter->type_min && type <= filter->type_max;
}

uint32_t os_event_dispatch(uint32_t max_events) {
    if (!bus.initialized || bus.count == 0) {
        return 0;
    }
    
    uint32_t dispatched = 0;
    uint32_t to_dispatch = (max_events == 0) ? bus.count : 
                           (max_events < bus.count ? max_events : bus.count);
    
    while (dispatched < to_dispatch && bus.count > 0) {
        /* Get event from head */
        os_event_t *event = &bus.queue[bus.head];
        
        /* Dispatch to matching subscribers */
        for (uint32_t i = 0; i < OS_MAX_SUBSCRIBERS; i++) {
            subscriber_t *sub = &bus.subscribers[i];
            if (sub->active && filter_matches(&sub->filter, event->type)) {
                sub->handler(event, sub->ctx);
            }
        }
        
        /* Advance head */
        bus.head = (bus.head + 1) % OS_EVENT_QUEUE_SIZE;
        bus.count--;
        dispatched++;
        bus.stats.events_dispatched++;
    }
    
    bus.stats.current_queue_size = bus.count;
    
    return dispatched;
}

os_err_t os_event_get_stats(os_event_stats_t *stats) {
    if (!bus.initialized || stats == NULL) {
        return OS_ERR_INVALID_ARG;
    }
    
    *stats = bus.stats;
    stats->current_queue_size = bus.count;
    
    return OS_OK;
}

os_corr_id_t os_event_new_corr_id(void) {
    return bus.next_corr_id++;
}
