/**
 * @file os_fibre.c
 * @brief Cooperative fibre scheduler implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Fibre scheduler
 * 
 * Uses setjmp/longjmp for portable context switching on host.
 * On ESP32, can be optimized to use RISC-V context switch.
 */

#include "os_fibre.h"
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>

/* Fibre control block */
typedef struct os_fibre {
    char name[OS_NAME_MAX_LEN];
    os_fibre_state_t state;
    os_fibre_fn_t entry;
    void *arg;
    
    /* Stack */
    uint8_t *stack_base;
    uint32_t stack_size;
    
    /* Context (setjmp buffer) */
    jmp_buf context;
    bool context_valid;
    
    /* Sleep support */
    os_tick_t wake_tick;
    
    /* Statistics */
    uint32_t run_count;
    
    /* Linked list */
    struct os_fibre *next;
} os_fibre_t;

/* Scheduler state */
static struct {
    bool initialized;
    bool running;
    os_fibre_t *current;
    os_fibre_t *head;
    os_fibre_t *idle;
    uint32_t count;
    volatile os_tick_t ticks;
    jmp_buf scheduler_context;
} sched = {0};

/* Forward declarations */
static void fibre_idle_task(void *arg);
static os_fibre_t *find_next_ready(void);

/* Stack guard pattern */
#define STACK_GUARD_PATTERN  0xDEADBEEF

os_err_t os_fibre_init(void) {
    if (sched.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&sched, 0, sizeof(sched));
    sched.initialized = true;
    
    /* Create idle fibre */
    os_err_t err = os_fibre_create(fibre_idle_task, NULL, "idle",
                                    OS_IDLE_STACK_SIZE, &sched.idle);
    if (err != OS_OK) {
        sched.initialized = false;
        return err;
    }
    
    return OS_OK;
}

os_err_t os_fibre_create(os_fibre_fn_t fn, void *arg, const char *name,
                         uint32_t stack_size, os_fibre_handle_t *out_handle) {
    if (!sched.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    if (fn == NULL) {
        return OS_ERR_INVALID_ARG;
    }
    if (sched.count >= OS_MAX_FIBRES) {
        return OS_ERR_NO_MEM;
    }
    
    if (stack_size == 0) {
        stack_size = OS_DEFAULT_STACK_SIZE;
    }
    
    /* Allocate fibre control block */
    os_fibre_t *fibre = (os_fibre_t *)malloc(sizeof(os_fibre_t));
    if (fibre == NULL) {
        return OS_ERR_NO_MEM;
    }
    memset(fibre, 0, sizeof(os_fibre_t));
    
    /* Allocate stack */
    fibre->stack_base = (uint8_t *)malloc(stack_size);
    if (fibre->stack_base == NULL) {
        free(fibre);
        return OS_ERR_NO_MEM;
    }
    
    /* Initialize stack with pattern for usage detection */
    memset(fibre->stack_base, 0xCD, stack_size);
    
    /* Set up fibre */
    strncpy(fibre->name, name ? name : "unnamed", OS_NAME_MAX_LEN - 1);
    fibre->name[OS_NAME_MAX_LEN - 1] = '\0';
    fibre->entry = fn;
    fibre->arg = arg;
    fibre->stack_size = stack_size;
    fibre->state = OS_FIBRE_STATE_READY;
    fibre->context_valid = false;
    
    /* Add to list */
    fibre->next = sched.head;
    sched.head = fibre;
    sched.count++;
    
    if (out_handle) {
        *out_handle = fibre;
    }
    
    return OS_OK;
}

/* Calculate stack usage by finding first non-pattern byte */
static uint32_t calc_stack_used(os_fibre_t *fibre) {
    uint32_t used = 0;
    for (uint32_t i = 0; i < fibre->stack_size; i++) {
        if (fibre->stack_base[i] != 0xCD) {
            used = fibre->stack_size - i;
            break;
        }
    }
    /* If no non-pattern byte was found but stack exists, assume full usage */
    if (used == 0 && fibre->stack_size > 0 && fibre->stack_base[0] == 0xCD) {
        /* Stack is entirely unused - return 0 */
        used = 0;
    } else if (used == 0 && fibre->stack_size > 0) {
        /* Entire stack has been used (no pattern bytes remain) */
        used = fibre->stack_size;
    }
    return used;
}

void os_fibre_start(void) {
    if (!sched.initialized || sched.running) {
        return;
    }
    
    sched.running = true;
    
    /* Main scheduler loop */
    while (1) {
        os_fibre_t *next = find_next_ready();
        
        if (next == NULL) {
            /* No ready fibres - run idle */
            next = sched.idle;
        }
        
        if (next && next->state == OS_FIBRE_STATE_READY) {
            sched.current = next;
            next->state = OS_FIBRE_STATE_RUNNING;
            next->run_count++;
            
            /* Save scheduler context */
            if (setjmp(sched.scheduler_context) == 0) {
                if (next->context_valid) {
                    /* Resume existing fibre */
                    longjmp(next->context, 1);
                } else {
                    /* First run - call entry function */
                    next->context_valid = true;
                    next->entry(next->arg);
                    /* If entry returns, mark fibre as dead */
                    next->state = OS_FIBRE_STATE_DEAD;
                }
            }
            /* Returns here after yield/sleep */
        }
    }
}

static os_fibre_t *find_next_ready(void) {
    os_tick_t now = sched.ticks;
    os_fibre_t *fibre = sched.head;
    os_fibre_t *found = NULL;
    
    /* First pass: wake up sleeping fibres */
    while (fibre) {
        if (fibre->state == OS_FIBRE_STATE_SLEEPING) {
            /* Check for tick wraparound-safe comparison */
            int32_t diff = (int32_t)(fibre->wake_tick - now);
            if (diff <= 0) {
                fibre->state = OS_FIBRE_STATE_READY;
            }
        }
        fibre = fibre->next;
    }
    
    /* Second pass: find first ready fibre (round-robin from current) */
    os_fibre_t *start = sched.current ? sched.current->next : sched.head;
    if (start == NULL) start = sched.head;
    
    fibre = start;
    do {
        if (fibre && fibre->state == OS_FIBRE_STATE_READY && fibre != sched.idle) {
            found = fibre;
            break;
        }
        fibre = fibre->next;
        if (fibre == NULL) fibre = sched.head;
    } while (fibre != start);
    
    return found;
}

void os_yield(void) {
    if (!sched.running || !sched.current) {
        return;
    }
    
    os_fibre_t *current = sched.current;
    
    if (current->state == OS_FIBRE_STATE_RUNNING) {
        current->state = OS_FIBRE_STATE_READY;
    }
    
    /* Save current context and return to scheduler */
    if (setjmp(current->context) == 0) {
        current->context_valid = true;
        longjmp(sched.scheduler_context, 1);
    }
    /* Returns here when resumed */
}

void os_sleep(os_time_ms_t ms) {
    if (!sched.running || !sched.current) {
        return;
    }
    
    if (ms == 0) {
        os_yield();
        return;
    }
    
    os_fibre_t *current = sched.current;
    current->state = OS_FIBRE_STATE_SLEEPING;
    current->wake_tick = sched.ticks + OS_MS_TO_TICKS(ms);
    
    /* Save current context and return to scheduler */
    if (setjmp(current->context) == 0) {
        current->context_valid = true;
        longjmp(sched.scheduler_context, 1);
    }
    /* Returns here when resumed */
}

os_tick_t os_now_ticks(void) {
    return sched.ticks;
}

os_time_ms_t os_uptime_ms(void) {
    return OS_TICKS_TO_MS(sched.ticks);
}

uint32_t os_fibre_count(void) {
    return sched.count;
}

os_err_t os_fibre_get_info(uint32_t index, os_fibre_info_t *info) {
    if (!sched.initialized || info == NULL) {
        return OS_ERR_INVALID_ARG;
    }
    
    os_fibre_t *fibre = sched.head;
    uint32_t i = 0;
    
    while (fibre && i < index) {
        fibre = fibre->next;
        i++;
    }
    
    if (fibre == NULL) {
        return OS_ERR_NOT_FOUND;
    }
    
    info->name = fibre->name;
    info->state = fibre->state;
    info->stack_size = fibre->stack_size;
    info->stack_used = calc_stack_used(fibre);
    info->run_count = fibre->run_count;
    info->wake_tick = fibre->wake_tick;
    
    return OS_OK;
}

os_fibre_handle_t os_fibre_current(void) {
    return sched.current;
}

void os_tick_advance(void) {
    sched.ticks++;
}

/* Idle task - just yields continuously */
static void fibre_idle_task(void *arg) {
    (void)arg;
    while (1) {
        os_yield();
    }
}
