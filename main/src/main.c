/**
 * @file main.c
 * @brief Application entry point
 *
 * ESP32-C6 Zigbee Bridge OS - Main application
 *
 * This file supports both ESP-IDF target builds and host simulation builds.
 * - ESP-IDF: Uses app_main() as entry point
 * - Host: Uses main() with pthread-based tick simulation
 */

// #include "app_blink.h" // Disabled - blink task not used
#include "capability.h"
#include "ha_disc.h"
#include "interview.h"
#include "local_node.h"
#include "mqtt_adapter.h"
#include "os.h"
#include "registry.h"
#include "zb_adapter.h"
#include <inttypes.h>
#include <stdio.h>

#define MAIN_MODULE "MAIN"

#ifdef OS_PLATFORM_HOST
/* Host-only includes */
#include <signal.h>
#endif

#ifdef OS_PLATFORM_HOST
/* Simulation timer for host builds */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

static volatile bool running = true;

static void *tick_thread(void *arg) {
  (void)arg;
  while (running) {
    usleep(1000); /* 1ms tick */
    os_tick_advance();
  }
  return NULL;
}

static void signal_handler(int sig) {
  (void)sig;
  running = false;
  printf("\nShutting down...\n");
  exit(0);
}
#endif

/* Event dispatcher fibre */
static void dispatcher_task(void *arg) {
  (void)arg;

  LOG_I(MAIN_MODULE, "Event dispatcher started");

  while (1) {
    os_event_dispatch(10);
    os_sleep(1);
  }
}

/**
 * @brief Common bridge initialization and startup
 *
 * Called by main() on host builds and app_main() on ESP-IDF builds.
 */
static int bridge_main(void) {
  printf("ESP32-C6 Zigbee Bridge OS\n");
  printf("Build: %s %s\n\n", __DATE__, __TIME__);

  /* Initialize OS */
  os_err_t err = os_init();
  if (err != OS_OK) {
    printf("OS init failed: %d\n", err);
    return 1;
  }

  /* Initialize device registry */
  err = reg_init();
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Registry init failed: %d", err);
  }

  /* Initialize interview service */
  err = interview_init();
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Interview init failed: %d", err);
  }

  /* Initialize capability service */
  err = cap_init();
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Capability init failed: %d", err);
  }

  /* Initialize MQTT adapter */
  err = mqtt_init(NULL);
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "MQTT init failed: %d", err);
  }

  /* Initialize Zigbee adapter */
  err = zba_init();
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Zigbee adapter init failed: %d", err);
  }

  err = zba_start_coordinator();
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Zigbee coordinator start failed: %d", err);
  }

#if OS_FEATURE_HA_DISC
  /* Initialize HA discovery service */
  err = ha_disc_init();
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "HA discovery init failed: %d", err);
  }
#endif

#if OS_FEATURE_LOCAL_NODE
  /* Initialize local node service */
  err = local_node_init();
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Local node init failed: %d", err);
  }
#endif

  /* Initialize registry shell commands */
  reg_shell_init();

  /* Initialize Zigbee shell commands */
#if defined(CONFIG_IDF_TARGET_ESP32C6)
  zba_shell_init();
#endif

  /* Create application fibres */
  err = os_fibre_create(os_shell_task, NULL, "shell", 4096, NULL);
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Failed to create shell task: %d", err);
  }

  // Blink task disabled - testing complete
  // err = os_fibre_create(app_blink_task, NULL, "blink", 2048, NULL);
  // if (err != OS_OK) {
  //   LOG_E(MAIN_MODULE, "Failed to create blink task: %d", err);
  // }

  err = os_fibre_create(dispatcher_task, NULL, "dispatch", 2048, NULL);
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Failed to create dispatcher task: %d", err);
  }

  err = os_fibre_create(interview_task, NULL, "interview", 2048, NULL);
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Failed to create interview task: %d", err);
  }

  err = os_fibre_create(mqtt_task, NULL, "mqtt", 2048, NULL);
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Failed to create mqtt task: %d", err);
  }

#if OS_FEATURE_HA_DISC
  err = os_fibre_create(ha_disc_task, NULL, "ha_disc", 2048, NULL);
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Failed to create HA discovery task: %d", err);
  }
#endif

#if OS_FEATURE_LOCAL_NODE
  err = os_fibre_create(local_node_task, NULL, "local", 2048, NULL);
  if (err != OS_OK) {
    LOG_E(MAIN_MODULE, "Failed to create local node task: %d", err);
  }
#endif

  LOG_I(MAIN_MODULE, "Created %" PRIu32 " fibres", os_fibre_count());

  /* Start scheduler (never returns) */
  os_start();

  return 0;
}

#ifdef OS_PLATFORM_HOST
/* Host build entry point */
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  /* Set up signal handler for clean exit */
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* Start tick timer thread */
  pthread_t tick_tid;
  pthread_create(&tick_tid, NULL, tick_thread, NULL);

  return bridge_main();
}
#else
/* ESP-IDF entry point */
void app_main(void) {
  /* ESP-IDF tick is handled by FreeRTOS/esp_timer, no pthread tick thread
   * needed */
  bridge_main();
}
#endif
