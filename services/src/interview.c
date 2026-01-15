/**
 * @file interview.c
 * @brief Interview/provisioner service implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Device interview service
 */

#include "interview.h"
#include "registry.h"
#include "os.h"
#include <string.h>

#define INTERVIEW_MODULE "INTV"

/* Maximum concurrent interviews */
#define MAX_INTERVIEWS 4

/* Interview timeout in ms */
#define INTERVIEW_TIMEOUT_MS 30000

/* Step timeout in ms */
#define STEP_TIMEOUT_MS 5000

/* Interview context */
typedef struct {
    os_eui64_t ieee_addr;
    interview_stage_t stage;
    uint8_t retry_count;
    uint8_t current_ep_index;
    os_tick_t start_time;
    os_tick_t step_start_time;
    bool active;
} interview_ctx_t;

/* Service state */
static struct {
    bool initialized;
    interview_ctx_t interviews[MAX_INTERVIEWS];
    uint32_t active_count;
} service = {0};

/* Stage names */
static const char *stage_names[] = {
    "INIT",
    "ACTIVE_EP",
    "SIMPLE_DESC",
    "BASIC_ATTR",
    "BINDINGS",
    "COMPLETE",
    "FAILED"
};

/* Forward declarations */
static interview_ctx_t *find_interview(os_eui64_t ieee_addr);
static interview_ctx_t *alloc_interview(os_eui64_t ieee_addr);
static void free_interview(interview_ctx_t *ctx);
static void advance_interview(interview_ctx_t *ctx);

os_err_t interview_init(void) {
    if (service.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&service, 0, sizeof(service));
    service.initialized = true;
    
    LOG_I(INTERVIEW_MODULE, "Interview service initialized");
    
    return OS_OK;
}

os_err_t interview_start(os_eui64_t ieee_addr) {
    if (!service.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    /* Check if already interviewing */
    interview_ctx_t *existing = find_interview(ieee_addr);
    if (existing) {
        LOG_D(INTERVIEW_MODULE, "Interview already in progress for " OS_EUI64_FMT,
              OS_EUI64_ARG(ieee_addr));
        return OS_OK;
    }
    
    /* Allocate context */
    interview_ctx_t *ctx = alloc_interview(ieee_addr);
    if (!ctx) {
        LOG_E(INTERVIEW_MODULE, "Max interviews reached");
        return OS_ERR_FULL;
    }
    
    /* Initialize */
    ctx->ieee_addr = ieee_addr;
    ctx->stage = INTERVIEW_STAGE_INIT;
    ctx->retry_count = 0;
    ctx->current_ep_index = 0;
    ctx->start_time = os_now_ticks();
    ctx->step_start_time = ctx->start_time;
    ctx->active = true;
    
    service.active_count++;
    
    /* Update node state */
    reg_node_t *node = reg_find_node(ieee_addr);
    if (node) {
        reg_set_state(node, REG_STATE_INTERVIEWING);
    }
    
    LOG_I(INTERVIEW_MODULE, "Starting interview for " OS_EUI64_FMT,
          OS_EUI64_ARG(ieee_addr));
    
    return OS_OK;
}

void interview_process(void) {
    if (!service.initialized) {
        return;
    }
    
    for (int i = 0; i < MAX_INTERVIEWS; i++) {
        interview_ctx_t *ctx = &service.interviews[i];
        if (!ctx->active) {
            continue;
        }
        
        /* Check for overall timeout */
        os_tick_t elapsed = os_now_ticks() - ctx->start_time;
        if (OS_TICKS_TO_MS(elapsed) > INTERVIEW_TIMEOUT_MS) {
            LOG_W(INTERVIEW_MODULE, "Interview timeout for " OS_EUI64_FMT,
                  OS_EUI64_ARG(ctx->ieee_addr));
            ctx->stage = INTERVIEW_STAGE_FAILED;
            free_interview(ctx);
            continue;
        }
        
        /* Check for step timeout */
        elapsed = os_now_ticks() - ctx->step_start_time;
        if (OS_TICKS_TO_MS(elapsed) > STEP_TIMEOUT_MS) {
            ctx->retry_count++;
            if (ctx->retry_count > 3) {
                LOG_W(INTERVIEW_MODULE, "Step timeout, moving to next stage");
                ctx->retry_count = 0;
                /* Move to next stage on timeout */
                ctx->stage++;
            }
            ctx->step_start_time = os_now_ticks();
        }
        
        /* Advance interview */
        advance_interview(ctx);
    }
}

interview_stage_t interview_get_stage(os_eui64_t ieee_addr) {
    interview_ctx_t *ctx = find_interview(ieee_addr);
    if (ctx) {
        return ctx->stage;
    }
    return INTERVIEW_STAGE_INIT;
}

os_err_t interview_cancel(os_eui64_t ieee_addr) {
    interview_ctx_t *ctx = find_interview(ieee_addr);
    if (!ctx) {
        return OS_ERR_NOT_FOUND;
    }
    
    LOG_I(INTERVIEW_MODULE, "Cancelling interview for " OS_EUI64_FMT,
          OS_EUI64_ARG(ieee_addr));
    
    free_interview(ctx);
    return OS_OK;
}

void interview_task(void *arg) {
    (void)arg;
    
    LOG_I(INTERVIEW_MODULE, "Interview task started");
    
    while (1) {
        interview_process();
        os_sleep(100);  /* Process every 100ms */
    }
}

const char *interview_stage_name(interview_stage_t stage) {
    if (stage < sizeof(stage_names) / sizeof(stage_names[0])) {
        return stage_names[stage];
    }
    return "UNKNOWN";
}

/* Internal functions */

static interview_ctx_t *find_interview(os_eui64_t ieee_addr) {
    for (int i = 0; i < MAX_INTERVIEWS; i++) {
        if (service.interviews[i].active && 
            service.interviews[i].ieee_addr == ieee_addr) {
            return &service.interviews[i];
        }
    }
    return NULL;
}

static interview_ctx_t *alloc_interview(os_eui64_t ieee_addr) {
    (void)ieee_addr;
    for (int i = 0; i < MAX_INTERVIEWS; i++) {
        if (!service.interviews[i].active) {
            return &service.interviews[i];
        }
    }
    return NULL;
}

static void free_interview(interview_ctx_t *ctx) {
    if (ctx && ctx->active) {
        ctx->active = false;
        service.active_count--;
    }
}

/* Simulate Zigbee responses for host testing */
static void simulate_active_endpoints(interview_ctx_t *ctx) {
    reg_node_t *node = reg_find_node(ctx->ieee_addr);
    if (!node) return;
    
    /* Simulate: endpoints 1 and 2 */
    reg_add_endpoint(node, 1, 0x0104, 0x0100);  /* HA profile, On/Off Light */
    reg_add_endpoint(node, 2, 0x0104, 0x0302);  /* HA profile, Temperature Sensor */
    
    LOG_D(INTERVIEW_MODULE, "Simulated active endpoints: 1, 2");
}

static void simulate_simple_descriptor(interview_ctx_t *ctx) {
    reg_node_t *node = reg_find_node(ctx->ieee_addr);
    if (!node) return;
    
    /* Add clusters to endpoint 1 */
    reg_endpoint_t *ep1 = reg_find_endpoint(node, 1);
    if (ep1) {
        reg_add_cluster(ep1, 0x0000, REG_CLUSTER_SERVER);  /* Basic */
        reg_add_cluster(ep1, 0x0006, REG_CLUSTER_SERVER);  /* OnOff */
        reg_add_cluster(ep1, 0x0008, REG_CLUSTER_SERVER);  /* Level Control */
    }
    
    /* Add clusters to endpoint 2 */
    reg_endpoint_t *ep2 = reg_find_endpoint(node, 2);
    if (ep2) {
        reg_add_cluster(ep2, 0x0000, REG_CLUSTER_SERVER);  /* Basic */
        reg_add_cluster(ep2, 0x0402, REG_CLUSTER_SERVER);  /* Temperature */
    }
    
    LOG_D(INTERVIEW_MODULE, "Simulated simple descriptors");
}

static void simulate_basic_attributes(interview_ctx_t *ctx) {
    reg_node_t *node = reg_find_node(ctx->ieee_addr);
    if (!node) return;
    
    /* Set node metadata */
    strncpy(node->manufacturer, "Test Manufacturer", REG_MANUFACTURER_LEN - 1);
    strncpy(node->model, "Test Model", REG_MODEL_LEN - 1);
    node->sw_build = 1;
    node->power_source = REG_POWER_MAINS;
    
    LOG_D(INTERVIEW_MODULE, "Simulated basic attributes");
}

static void advance_interview(interview_ctx_t *ctx) {
    reg_node_t *node = reg_find_node(ctx->ieee_addr);
    if (!node) {
        LOG_E(INTERVIEW_MODULE, "Node not found in registry");
        ctx->stage = INTERVIEW_STAGE_FAILED;
        free_interview(ctx);
        return;
    }
    
    switch (ctx->stage) {
        case INTERVIEW_STAGE_INIT:
            LOG_D(INTERVIEW_MODULE, "Stage: INIT -> ACTIVE_EP");
            ctx->stage = INTERVIEW_STAGE_ACTIVE_EP;
            ctx->step_start_time = os_now_ticks();
            break;
            
        case INTERVIEW_STAGE_ACTIVE_EP:
            /* Request active endpoints */
            /* In real implementation, this would send ZDP Active_EP_req */
            simulate_active_endpoints(ctx);
            ctx->stage = INTERVIEW_STAGE_SIMPLE_DESC;
            ctx->step_start_time = os_now_ticks();
            break;
            
        case INTERVIEW_STAGE_SIMPLE_DESC:
            /* Request simple descriptor for each endpoint */
            simulate_simple_descriptor(ctx);
            ctx->stage = INTERVIEW_STAGE_BASIC_ATTR;
            ctx->step_start_time = os_now_ticks();
            break;
            
        case INTERVIEW_STAGE_BASIC_ATTR:
            /* Read basic cluster attributes */
            simulate_basic_attributes(ctx);
            ctx->stage = INTERVIEW_STAGE_BINDINGS;
            ctx->step_start_time = os_now_ticks();
            break;
            
        case INTERVIEW_STAGE_BINDINGS:
            /* Set up bindings for reporting */
            /* Skip for now */
            ctx->stage = INTERVIEW_STAGE_COMPLETE;
            break;
            
        case INTERVIEW_STAGE_COMPLETE:
            LOG_I(INTERVIEW_MODULE, "Interview complete for " OS_EUI64_FMT,
                  OS_EUI64_ARG(ctx->ieee_addr));
            
            /* Update node state */
            reg_set_state(node, REG_STATE_READY);
            
            /* Emit event */
            os_event_emit(OS_EVENT_CAP_STATE_CHANGED, &ctx->ieee_addr, sizeof(ctx->ieee_addr));
            
            /* Free context */
            free_interview(ctx);
            break;
            
        case INTERVIEW_STAGE_FAILED:
            LOG_E(INTERVIEW_MODULE, "Interview failed for " OS_EUI64_FMT,
                  OS_EUI64_ARG(ctx->ieee_addr));
            
            /* Update node state */
            reg_set_state(node, REG_STATE_STALE);
            
            /* Free context */
            free_interview(ctx);
            break;
    }
}
