/**
 * @file main.c
 * @brief Application entry point
 * 
 * ESP32-C6 Zigbee Bridge OS - Main application
 */

#include "os.h"
#include "registry.h"
#include "interview.h"
#include "capability.h"
#include "mqtt_adapter.h"
#include "app_blink.h"
#include <stdio.h>
#include <signal.h>

#define MAIN_MODULE "MAIN"

#ifdef OS_PLATFORM_HOST
/* Simulation timer for host builds */
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

static volatile bool running = true;

static void *tick_thread(void *arg) {
    (void)arg;
    while (running) {
        usleep(1000);  /* 1ms tick */
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

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("ESP32-C6 Zigbee Bridge OS\n");
    printf("Build: %s %s\n\n", __DATE__, __TIME__);

#ifdef OS_PLATFORM_HOST
    /* Set up signal handler for clean exit */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Start tick timer thread */
    pthread_t tick_tid;
    pthread_create(&tick_tid, NULL, tick_thread, NULL);
#endif
    
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
    
    /* Initialize registry shell commands */
    reg_shell_init();
    
    /* Create application fibres */
    err = os_fibre_create(os_shell_task, NULL, "shell", 4096, NULL);
    if (err != OS_OK) {
        LOG_E(MAIN_MODULE, "Failed to create shell task: %d", err);
    }
    
    err = os_fibre_create(app_blink_task, NULL, "blink", 2048, NULL);
    if (err != OS_OK) {
        LOG_E(MAIN_MODULE, "Failed to create blink task: %d", err);
    }
    
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
    
    LOG_I(MAIN_MODULE, "Created %u fibres", os_fibre_count());
    
    /* Start scheduler (never returns) */
    os_start();
    
    return 0;
}
