/**
 * @file os_fibre.c
 * @brief Cooperative fibre scheduler implementation
 *
 * ESP32-C6 Zigbee Bridge OS - Fibre scheduler
 *
 * Platform-specific implementations:
 * - ESP-IDF: Uses FreeRTOS tasks for proper stack management
 * - Host: Uses setjmp/longjmp (limited, for simulation only)
 */

#include "os_fibre.h"
#include <stdlib.h>
#include <string.h>

#ifdef OS_PLATFORM_HOST
/*===========================================================================
 * HOST PLATFORM IMPLEMENTATION - setjmp/longjmp based
 *===========================================================================*/

#include <setjmp.h>

/* Fibre control block - Host version */
typedef struct os_fibre {
  char name[OS_NAME_MAX_LEN];
  os_fibre_state_t state;
  os_fibre_fn_t entry;
  void *arg;
  uint32_t stack_size;
  uint8_t *stack_base;
  jmp_buf context;
  bool context_valid;
  os_tick_t wake_tick;
  uint32_t run_count;
  os_tick_t last_run_tick;
  os_tick_t total_run_ticks;
  struct os_fibre *next;
} os_fibre_t;

/* Scheduler state - Host version */
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

  os_fibre_t *fibre = (os_fibre_t *)malloc(sizeof(os_fibre_t));
  if (fibre == NULL) {
    return OS_ERR_NO_MEM;
  }
  memset(fibre, 0, sizeof(os_fibre_t));

  fibre->stack_base = (uint8_t *)malloc(stack_size);
  if (fibre->stack_base == NULL) {
    free(fibre);
    return OS_ERR_NO_MEM;
  }
  memset(fibre->stack_base, 0xCD, stack_size);

  strncpy(fibre->name, name ? name : "unnamed", OS_NAME_MAX_LEN - 1);
  fibre->name[OS_NAME_MAX_LEN - 1] = '\0';
  fibre->entry = fn;
  fibre->arg = arg;
  fibre->stack_size = stack_size;
  fibre->state = OS_FIBRE_STATE_READY;
  fibre->context_valid = false;

  fibre->next = sched.head;
  sched.head = fibre;
  sched.count++;

  if (out_handle) {
    *out_handle = fibre;
  }

  return OS_OK;
}

static uint32_t calc_stack_used(os_fibre_t *fibre) {
  if (fibre->stack_size == 0 || fibre->stack_base == NULL) {
    return 0;
  }
  for (uint32_t i = 0; i < fibre->stack_size; i++) {
    if (fibre->stack_base[i] != 0xCD) {
      return fibre->stack_size - i;
    }
  }
  return 0;
}

void os_fibre_start(void) {
  if (!sched.initialized || sched.running) {
    return;
  }

  sched.running = true;

  while (1) {
    os_fibre_t *next = find_next_ready();

    if (next == NULL) {
      next = sched.idle;
    }

    if (next && next->state == OS_FIBRE_STATE_READY) {
      sched.current = next;
      next->state = OS_FIBRE_STATE_RUNNING;
      next->run_count++;
      next->last_run_tick = sched.ticks;
      next->total_run_ticks++;

      if (setjmp(sched.scheduler_context) == 0) {
        if (next->context_valid) {
          longjmp(next->context, 1);
        } else {
          next->context_valid = true;
          next->entry(next->arg);
          next->state = OS_FIBRE_STATE_DEAD;
        }
      }
    }
  }
}

static os_fibre_t *find_next_ready(void) {
  os_tick_t now = sched.ticks;
  os_fibre_t *fibre = sched.head;
  os_fibre_t *found = NULL;

  while (fibre) {
    if (fibre->state == OS_FIBRE_STATE_SLEEPING) {
      int32_t diff = (int32_t)(fibre->wake_tick - now);
      if (diff <= 0) {
        fibre->state = OS_FIBRE_STATE_READY;
      }
    }
    fibre = fibre->next;
  }

  os_fibre_t *start = sched.current ? sched.current->next : sched.head;
  if (start == NULL)
    start = sched.head;

  fibre = start;
  do {
    if (fibre && fibre->state == OS_FIBRE_STATE_READY && fibre != sched.idle) {
      found = fibre;
      break;
    }
    fibre = fibre->next;
    if (fibre == NULL)
      fibre = sched.head;
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

  if (setjmp(current->context) == 0) {
    current->context_valid = true;
    longjmp(sched.scheduler_context, 1);
  }
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

  if (setjmp(current->context) == 0) {
    current->context_valid = true;
    longjmp(sched.scheduler_context, 1);
  }
}

os_tick_t os_now_ticks(void) { return sched.ticks; }

os_time_ms_t os_uptime_ms(void) { return OS_TICKS_TO_MS(sched.ticks); }

uint32_t os_fibre_count(void) { return sched.count; }

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
  info->last_run_tick = fibre->last_run_tick;
  info->total_run_ticks = fibre->total_run_ticks;
  info->wake_tick = fibre->wake_tick;

  return OS_OK;
}

os_fibre_handle_t os_fibre_current(void) { return sched.current; }

os_err_t os_fibre_get_stats(os_sched_stats_t *stats) {
  if (!sched.initialized || stats == NULL) {
    return OS_ERR_INVALID_ARG;
  }

  uint32_t ready = 0;
  uint32_t sleeping = 0;
  os_fibre_t *fibre = sched.head;

  while (fibre) {
    if (fibre->state == OS_FIBRE_STATE_READY) {
      ready++;
    } else if (fibre->state == OS_FIBRE_STATE_SLEEPING) {
      sleeping++;
    }
    fibre = fibre->next;
  }

  stats->ticks = sched.ticks;
  stats->fibre_count = sched.count;
  stats->ready_count = ready;
  stats->sleeping_count = sleeping;

  return OS_OK;
}

void os_tick_advance(void) { sched.ticks++; }

static void fibre_idle_task(void *arg) {
  (void)arg;
  while (1) {
    os_yield();
  }
}

#else
/*===========================================================================
 * ESP-IDF PLATFORM IMPLEMENTATION - FreeRTOS task based
 *===========================================================================*/

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Fibre control block - ESP-IDF version */
typedef struct os_fibre {
  char name[OS_NAME_MAX_LEN];
  os_fibre_state_t state;
  os_fibre_fn_t entry;
  void *arg;
  uint32_t stack_size;
  TaskHandle_t task_handle;
  os_tick_t wake_tick;
  uint32_t run_count;
  os_tick_t last_run_tick;
  os_tick_t total_run_ticks;
  struct os_fibre *next;
} os_fibre_t;

/* Scheduler state - ESP-IDF version */
static struct {
  bool initialized;
  bool running;
  os_fibre_t *current;
  os_fibre_t *head;
  os_fibre_t *idle;
  uint32_t count;
  volatile os_tick_t ticks;
} sched = {0};

/* Forward declarations */
static void fibre_idle_task(void *arg);
static void fibre_wrapper(void *arg);

os_err_t os_fibre_init(void) {
  if (sched.initialized) {
    return OS_ERR_ALREADY_EXISTS;
  }

  memset(&sched, 0, sizeof(sched));
  sched.initialized = true;

  /* Create idle fibre - it will be a low-priority FreeRTOS task */
  os_err_t err = os_fibre_create(fibre_idle_task, NULL, "idle",
                                 OS_IDLE_STACK_SIZE, &sched.idle);
  if (err != OS_OK) {
    sched.initialized = false;
    return err;
  }

  return OS_OK;
}

/* Wrapper function that FreeRTOS calls - updates stats and calls user entry */
static void fibre_wrapper(void *arg) {
  os_fibre_t *fibre = (os_fibre_t *)arg;

  /* Suspend ourselves until os_fibre_start() resumes us */
  vTaskSuspend(NULL);

  fibre->state = OS_FIBRE_STATE_RUNNING;
  fibre->run_count++;
  fibre->last_run_tick = sched.ticks;

  /* Call the user's entry function */
  fibre->entry(fibre->arg);

  /* If entry returns, mark fibre as dead */
  fibre->state = OS_FIBRE_STATE_DEAD;

  /* Delete this task */
  vTaskDelete(NULL);
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

  os_fibre_t *fibre = (os_fibre_t *)malloc(sizeof(os_fibre_t));
  if (fibre == NULL) {
    return OS_ERR_NO_MEM;
  }
  memset(fibre, 0, sizeof(os_fibre_t));

  strncpy(fibre->name, name ? name : "unnamed", OS_NAME_MAX_LEN - 1);
  fibre->name[OS_NAME_MAX_LEN - 1] = '\0';
  fibre->entry = fn;
  fibre->arg = arg;
  fibre->stack_size = stack_size;
  fibre->state = OS_FIBRE_STATE_READY;

  /* Create the FreeRTOS task in suspended state - will be resumed when
   * os_fibre_start() is called */
  BaseType_t ret = xTaskCreate(
      fibre_wrapper, fibre->name,
      stack_size / sizeof(StackType_t), /* FreeRTOS wants stack in words */
      fibre, tskIDLE_PRIORITY + 1,      /* Just above idle */
      &fibre->task_handle);

  if (ret != pdPASS) {
    free(fibre);
    return OS_ERR_NO_MEM;
  }

  /* Task suspends itself in fibre_wrapper - will be resumed in os_fibre_start()
   */

  /* Add to list */
  fibre->next = sched.head;
  sched.head = fibre;
  sched.count++;

  if (out_handle) {
    *out_handle = fibre;
  }

  return OS_OK;
}

static uint32_t calc_stack_used(os_fibre_t *fibre) {
  if (fibre->task_handle == NULL) {
    return 0;
  }
  /* FreeRTOS provides stack high water mark */
  UBaseType_t free_stack = uxTaskGetStackHighWaterMark(fibre->task_handle);
  uint32_t free_bytes = free_stack * sizeof(StackType_t);
  if (free_bytes > fibre->stack_size) {
    return fibre->stack_size; /* Something wrong, return max */
  }
  return fibre->stack_size - free_bytes;
}

void os_fibre_start(void) {
  if (!sched.initialized || sched.running) {
    return;
  }

  sched.running = true;

  /* Resume all suspended fibres now that the scheduler is ready */
  os_fibre_t *fibre = sched.head;
  while (fibre) {
    if (fibre->task_handle != NULL) {
      vTaskResume(fibre->task_handle);
    }
    fibre = fibre->next;
  }

  /* On ESP-IDF, FreeRTOS is already running. The tasks we created are
   * now resumed. We just need to keep this function from returning
   * so app_main doesn't exit. We'll use a simple delay loop that also
   * advances our tick counter. */
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1));
    sched.ticks++;
  }
}

void os_yield(void) {
  if (!sched.running) {
    return;
  }

  /* FreeRTOS yield - gives other tasks a chance to run */
  taskYIELD();
}

void os_sleep(os_time_ms_t ms) {
  if (!sched.running) {
    return;
  }

  if (ms == 0) {
    os_yield();
    return;
  }

  /* Use FreeRTOS delay */
  vTaskDelay(pdMS_TO_TICKS(ms));
}

os_tick_t os_now_ticks(void) { return sched.ticks; }

os_time_ms_t os_uptime_ms(void) { return OS_TICKS_TO_MS(sched.ticks); }

uint32_t os_fibre_count(void) { return sched.count; }

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
  info->last_run_tick = fibre->last_run_tick;
  info->total_run_ticks = fibre->total_run_ticks;
  info->wake_tick = fibre->wake_tick;

  return OS_OK;
}

os_fibre_handle_t os_fibre_current(void) {
  /* Find the fibre whose task_handle matches the current task */
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  os_fibre_t *fibre = sched.head;

  while (fibre) {
    if (fibre->task_handle == current_task) {
      return fibre;
    }
    fibre = fibre->next;
  }

  return NULL;
}

os_err_t os_fibre_get_stats(os_sched_stats_t *stats) {
  if (!sched.initialized || stats == NULL) {
    return OS_ERR_INVALID_ARG;
  }

  uint32_t ready = 0;
  uint32_t sleeping = 0;
  os_fibre_t *fibre = sched.head;

  while (fibre) {
    if (fibre->state == OS_FIBRE_STATE_READY ||
        fibre->state == OS_FIBRE_STATE_RUNNING) {
      ready++;
    } else if (fibre->state == OS_FIBRE_STATE_SLEEPING) {
      sleeping++;
    }
    fibre = fibre->next;
  }

  stats->ticks = sched.ticks;
  stats->fibre_count = sched.count;
  stats->ready_count = ready;
  stats->sleeping_count = sleeping;

  return OS_OK;
}

void os_tick_advance(void) { sched.ticks++; }

static void fibre_idle_task(void *arg) {
  (void)arg;
  while (1) {
    /* On FreeRTOS, just delay - the scheduler handles idle */
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

#endif /* OS_PLATFORM_HOST */
