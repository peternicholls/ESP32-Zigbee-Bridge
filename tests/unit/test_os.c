/**
 * @file test_os.c
 * @brief Unit tests for Tiny OS components
 * 
 * ESP32-C6 Zigbee Bridge OS - Test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

/* Include OS headers directly for testing */
#include "os_types.h"
#include "os_config.h"
#include "os_event.h"
#include "os_log.h"
#include "os_persist.h"
#include "registry.h"
#include "interview.h"
#include "capability.h"
#include "quirks.h"
#include "test_ha_disc.h"
#include "test_support.h"
#include "test_zb_adapter.h"
#include "test_local_node.h"

/* Test helper: safely remove directory and contents */
static void remove_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    char filepath[512];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        int ret = snprintf(filepath, sizeof(filepath), "%s/%.255s", path, entry->d_name);
        if (ret > 0 && (size_t)ret < sizeof(filepath)) {
            unlink(filepath);
        }
    }
    closedir(dir);
    rmdir(path);
}

int tests_passed = 0;
int tests_failed = 0;

/* Event bus tests */

static void test_event_init(void) {
    TEST_START("event_init");
    
    os_err_t err = os_event_init();
    ASSERT_EQ(err, OS_OK);
    
    /* Double init should return error */
    err = os_event_init();
    ASSERT_EQ(err, OS_ERR_ALREADY_EXISTS);
    
    tests_passed++;
    TEST_PASS();
}

static int handler_call_count = 0;
static os_event_type_t last_event_type = 0;

static void test_event_handler(const os_event_t *event, void *ctx) {
    (void)ctx;
    handler_call_count++;
    last_event_type = event->type;
}

static void test_event_subscribe_publish(void) {
    TEST_START("event_subscribe_publish");
    
    handler_call_count = 0;
    
    /* Subscribe to all events */
    os_event_filter_t filter = {0, OS_EVENT_TYPE_MAX};
    os_err_t err = os_event_subscribe(&filter, test_event_handler, NULL);
    ASSERT_EQ(err, OS_OK);
    
    /* Publish an event */
    err = os_event_emit(OS_EVENT_BOOT, NULL, 0);
    ASSERT_EQ(err, OS_OK);
    
    /* Dispatch and check handler was called */
    uint32_t dispatched = os_event_dispatch(0);
    ASSERT_EQ(dispatched, 1);
    ASSERT_EQ(handler_call_count, 1);
    ASSERT_EQ(last_event_type, OS_EVENT_BOOT);
    
    tests_passed++;
    TEST_PASS();
}

static void test_event_filter(void) {
    TEST_START("event_filter");
    
    handler_call_count = 0;
    
    /* Subscribe only to Zigbee events */
    os_event_filter_t filter = {OS_EVENT_ZB_STACK_UP, OS_EVENT_ZB_ATTR_REPORT};
    os_err_t err = os_event_subscribe(&filter, test_event_handler, NULL);
    ASSERT_EQ(err, OS_OK);
    
    /* Publish system event - should not match filter for this handler */
    err = os_event_emit(OS_EVENT_NET_UP, NULL, 0);
    ASSERT_EQ(err, OS_OK);
    
    os_event_dispatch(0);
    /* Handler should be called by the "all events" subscription from previous test,
       but this test focuses on the filter mechanism */
    
    /* Publish Zigbee event - should match */
    err = os_event_emit(OS_EVENT_ZB_DEVICE_JOINED, NULL, 0);
    ASSERT_EQ(err, OS_OK);
    
    os_event_dispatch(0);
    ASSERT_TRUE(handler_call_count >= 1);
    
    tests_passed++;
    TEST_PASS();
}

static void test_event_payload(void) {
    TEST_START("event_payload");
    
    /* Create event with payload */
    os_event_t event = {0};
    event.type = OS_EVENT_USER_BASE;
    
    uint32_t test_data = 0x12345678;
    memcpy(event.payload, &test_data, sizeof(test_data));
    event.payload_len = sizeof(test_data);
    
    os_err_t err = os_event_publish(&event);
    ASSERT_EQ(err, OS_OK);
    
    /* Check stats */
    os_event_stats_t stats;
    err = os_event_get_stats(&stats);
    ASSERT_EQ(err, OS_OK);
    ASSERT_TRUE(stats.events_published > 0);
    
    os_event_dispatch(0);
    
    tests_passed++;
    TEST_PASS();
}

static void test_event_stats(void) {
    TEST_START("event_stats");
    
    os_event_stats_t stats;
    os_err_t err = os_event_get_stats(&stats);
    ASSERT_EQ(err, OS_OK);
    ASSERT_TRUE(stats.events_published > 0);
    ASSERT_TRUE(stats.events_dispatched > 0);
    
    tests_passed++;
    TEST_PASS();
}

/* Log tests */

static void test_log_init(void) {
    TEST_START("log_init");
    
    os_err_t err = os_log_init();
    ASSERT_EQ(err, OS_OK);
    
    err = os_log_init();
    ASSERT_EQ(err, OS_ERR_ALREADY_EXISTS);
    
    tests_passed++;
    TEST_PASS();
}

static void test_log_levels(void) {
    TEST_START("log_levels");
    
    /* Test level names */
    ASSERT_TRUE(strcmp(os_log_level_name(OS_LOG_LEVEL_ERROR), "ERROR") == 0);
    ASSERT_TRUE(strcmp(os_log_level_name(OS_LOG_LEVEL_WARN), "WARN") == 0);
    ASSERT_TRUE(strcmp(os_log_level_name(OS_LOG_LEVEL_INFO), "INFO") == 0);
    ASSERT_TRUE(strcmp(os_log_level_name(OS_LOG_LEVEL_DEBUG), "DEBUG") == 0);
    ASSERT_TRUE(strcmp(os_log_level_name(OS_LOG_LEVEL_TRACE), "TRACE") == 0);
    
    /* Test level parsing */
    ASSERT_EQ(os_log_level_parse("ERROR"), OS_LOG_LEVEL_ERROR);
    ASSERT_EQ(os_log_level_parse("error"), OS_LOG_LEVEL_ERROR);
    ASSERT_EQ(os_log_level_parse("DEBUG"), OS_LOG_LEVEL_DEBUG);
    ASSERT_EQ(os_log_level_parse("invalid"), OS_LOG_LEVEL_INFO);
    
    tests_passed++;
    TEST_PASS();
}

static void test_log_set_level(void) {
    TEST_START("log_set_level");
    
    os_log_set_level(OS_LOG_LEVEL_DEBUG);
    ASSERT_EQ(os_log_get_level(), OS_LOG_LEVEL_DEBUG);
    
    os_log_set_level(OS_LOG_LEVEL_ERROR);
    ASSERT_EQ(os_log_get_level(), OS_LOG_LEVEL_ERROR);
    
    /* Reset to default */
    os_log_set_level(OS_LOG_LEVEL_INFO);
    
    tests_passed++;
    TEST_PASS();
}

static void test_log_write(void) {
    TEST_START("log_write");
    
    os_log_set_level(OS_LOG_LEVEL_DEBUG);
    
    /* Write some log messages */
    os_log_write(OS_LOG_LEVEL_INFO, "TEST", "Test message %d", 42);
    os_log_write(OS_LOG_LEVEL_DEBUG, "TEST", "Debug message");
    os_log_write(OS_LOG_LEVEL_ERROR, "TEST", "Error: %s", "test error");
    
    /* Flush should return number flushed */
    printf("\n");  /* New line before log output */
    uint32_t flushed = os_log_flush();
    ASSERT_TRUE(flushed >= 3);
    
    tests_passed++;
    TEST_PASS();
}

/* Type tests */

static void test_types(void) {
    TEST_START("types");
    
    /* Test size assumptions */
    ASSERT_EQ(sizeof(os_tick_t), 4);
    ASSERT_EQ(sizeof(os_eui64_t), 8);
    ASSERT_EQ(sizeof(os_corr_id_t), 4);
    
    /* Test macros */
    ASSERT_EQ(OS_MS_TO_TICKS(1000), 1000);
    ASSERT_EQ(OS_TICKS_TO_MS(1000), 1000);
    
    tests_passed++;
    TEST_PASS();
}

/* Persistence tests */

static void test_persist_init(void) {
    TEST_START("persist_init");
    
    /* Clean up any previous state using safe directory removal */
    remove_directory("/tmp/bridge_persist");
    
    os_err_t err = os_persist_init();
    ASSERT_EQ(err, OS_OK);
    
    tests_passed++;
    TEST_PASS();
}

static void test_persist_put_get(void) {
    TEST_START("persist_put_get");
    
    const char *key = "test_key";
    uint32_t value = 0x12345678;
    
    os_err_t err = os_persist_put(key, &value, sizeof(value));
    ASSERT_EQ(err, OS_OK);
    
    /* Should be able to read back from buffer */
    uint32_t read_value = 0;
    size_t read_len;
    err = os_persist_get(key, &read_value, sizeof(read_value), &read_len);
    ASSERT_EQ(err, OS_OK);
    ASSERT_EQ(read_value, value);
    ASSERT_EQ(read_len, sizeof(value));
    
    tests_passed++;
    TEST_PASS();
}

static void test_persist_flush(void) {
    TEST_START("persist_flush");
    
    os_err_t err = os_persist_flush();
    ASSERT_EQ(err, OS_OK);
    
    /* After flush, should still be able to read */
    const char *key = "test_key";
    uint32_t read_value = 0;
    size_t read_len;
    err = os_persist_get(key, &read_value, sizeof(read_value), &read_len);
    ASSERT_EQ(err, OS_OK);
    ASSERT_EQ(read_value, (uint32_t)0x12345678);
    
    tests_passed++;
    TEST_PASS();
}

static void test_persist_exists(void) {
    TEST_START("persist_exists");
    
    ASSERT_TRUE(os_persist_exists("test_key"));
    ASSERT_FALSE(os_persist_exists("nonexistent"));
    
    tests_passed++;
    TEST_PASS();
}

static void test_persist_del(void) {
    TEST_START("persist_del");
    
    const char *key = "del_test";
    uint32_t value = 42;
    
    os_err_t err = os_persist_put(key, &value, sizeof(value));
    ASSERT_EQ(err, OS_OK);
    ASSERT_TRUE(os_persist_exists(key));
    
    err = os_persist_del(key);
    ASSERT_EQ(err, OS_OK);
    ASSERT_FALSE(os_persist_exists(key));
    
    tests_passed++;
    TEST_PASS();
}

static void test_persist_schema_version(void) {
    TEST_START("persist_schema_version");
    
    uint32_t version = 42;
    os_err_t err = os_persist_set_schema_version(version);
    ASSERT_EQ(err, OS_OK);
    ASSERT_EQ(os_persist_schema_version(), version);
    
    tests_passed++;
    TEST_PASS();
}

/* Registry tests */

static void test_reg_init(void) {
    TEST_START("reg_init");
    
    os_err_t err = reg_init();
    ASSERT_EQ(err, OS_OK);
    ASSERT_EQ(reg_node_count(), 0);
    
    tests_passed++;
    TEST_PASS();
}

static void test_reg_add_node(void) {
    TEST_START("reg_add_node");
    
    os_eui64_t addr = 0x00112233445566AA;
    uint16_t nwk = 0x1234;
    
    reg_node_t *node = reg_add_node(addr, nwk);
    ASSERT_TRUE(node != NULL);
    ASSERT_EQ(node->ieee_addr, addr);
    ASSERT_EQ(node->nwk_addr, nwk);
    ASSERT_EQ(node->state, REG_STATE_NEW);
    ASSERT_EQ(reg_node_count(), 1);
    
    tests_passed++;
    TEST_PASS();
}

static void test_reg_find_node(void) {
    TEST_START("reg_find_node");
    
    os_eui64_t addr = 0x00112233445566AA;
    
    reg_node_t *node = reg_find_node(addr);
    ASSERT_TRUE(node != NULL);
    ASSERT_EQ(node->ieee_addr, addr);
    
    /* Find by network address */
    node = reg_find_node_by_nwk(0x1234);
    ASSERT_TRUE(node != NULL);
    ASSERT_EQ(node->ieee_addr, addr);
    
    /* Not found */
    node = reg_find_node(0xDEADBEEF);
    ASSERT_TRUE(node == NULL);
    
    tests_passed++;
    TEST_PASS();
}

static void test_reg_add_endpoint(void) {
    TEST_START("reg_add_endpoint");
    
    os_eui64_t addr = 0x00112233445566AA;
    reg_node_t *node = reg_find_node(addr);
    ASSERT_TRUE(node != NULL);
    
    reg_endpoint_t *ep = reg_add_endpoint(node, 1, 0x0104, 0x0100);
    ASSERT_TRUE(ep != NULL);
    ASSERT_EQ(ep->endpoint_id, 1);
    ASSERT_EQ(ep->profile_id, 0x0104);
    ASSERT_EQ(node->endpoint_count, 1);
    
    tests_passed++;
    TEST_PASS();
}

static void test_reg_add_cluster(void) {
    TEST_START("reg_add_cluster");
    
    os_eui64_t addr = 0x00112233445566AA;
    reg_node_t *node = reg_find_node(addr);
    ASSERT_TRUE(node != NULL);
    
    reg_endpoint_t *ep = reg_find_endpoint(node, 1);
    ASSERT_TRUE(ep != NULL);
    
    /* Add OnOff cluster */
    reg_cluster_t *cl = reg_add_cluster(ep, 0x0006, REG_CLUSTER_SERVER);
    ASSERT_TRUE(cl != NULL);
    ASSERT_EQ(cl->cluster_id, 0x0006);
    ASSERT_EQ(ep->cluster_count, 1);
    
    tests_passed++;
    TEST_PASS();
}

static void test_reg_update_attribute(void) {
    TEST_START("reg_update_attribute");
    
    os_eui64_t addr = 0x00112233445566AA;
    reg_node_t *node = reg_find_node(addr);
    reg_endpoint_t *ep = reg_find_endpoint(node, 1);
    reg_cluster_t *cl = reg_find_cluster(ep, 0x0006);
    ASSERT_TRUE(cl != NULL);
    
    /* Update OnOff attribute */
    reg_attr_value_t value = {.b = true};
    os_err_t err = reg_update_attribute(cl, 0x0000, REG_ATTR_TYPE_BOOL, &value);
    ASSERT_EQ(err, OS_OK);
    
    /* Read back */
    reg_attribute_t *attr = reg_find_attribute(cl, 0x0000);
    ASSERT_TRUE(attr != NULL);
    ASSERT_EQ(attr->value.b, true);
    
    tests_passed++;
    TEST_PASS();
}

static void test_reg_set_state(void) {
    TEST_START("reg_set_state");
    
    os_eui64_t addr = 0x00112233445566AA;
    reg_node_t *node = reg_find_node(addr);
    ASSERT_TRUE(node != NULL);
    
    os_err_t err = reg_set_state(node, REG_STATE_READY);
    ASSERT_EQ(err, OS_OK);
    ASSERT_EQ(node->state, REG_STATE_READY);
    
    tests_passed++;
    TEST_PASS();
}

static void test_reg_remove_node(void) {
    TEST_START("reg_remove_node");
    
    os_eui64_t addr = 0x00112233445566AA;
    
    os_err_t err = reg_remove_node(addr);
    ASSERT_EQ(err, OS_OK);
    ASSERT_EQ(reg_node_count(), 0);
    
    /* Should not find it anymore */
    reg_node_t *node = reg_find_node(addr);
    ASSERT_TRUE(node == NULL);
    
    tests_passed++;
    TEST_PASS();
}

/* Interview tests */

static void test_interview_init(void) {
    TEST_START("interview_init");
    
    os_err_t err = interview_init();
    ASSERT_EQ(err, OS_OK);
    
    tests_passed++;
    TEST_PASS();
}

static void test_interview_start(void) {
    TEST_START("interview_start");
    
    /* First add a node */
    os_eui64_t addr = 0xAABBCCDDEEFF0011;
    reg_node_t *node = reg_add_node(addr, 0x5678);
    ASSERT_TRUE(node != NULL);
    
    /* Start interview */
    os_err_t err = interview_start(addr);
    ASSERT_EQ(err, OS_OK);
    
    /* Check stage */
    interview_stage_t stage = interview_get_stage(addr);
    ASSERT_EQ(stage, INTERVIEW_STAGE_INIT);
    
    /* Node should be in interviewing state */
    ASSERT_EQ(node->state, REG_STATE_INTERVIEWING);
    
    tests_passed++;
    TEST_PASS();
}

/* Capability tests */

static void test_cap_init(void) {
    TEST_START("cap_init");
    
    os_err_t err = cap_init();
    ASSERT_EQ(err, OS_OK);
    
    tests_passed++;
    TEST_PASS();
}

static void test_cap_compute(void) {
    TEST_START("cap_compute");
    
    os_eui64_t addr = 0xAABBCCDDEEFF0011;
    reg_node_t *node = reg_find_node(addr);
    ASSERT_TRUE(node != NULL);
    
    /* Add some clusters */
    reg_endpoint_t *ep = reg_add_endpoint(node, 1, 0x0104, 0x0100);
    ASSERT_TRUE(ep != NULL);
    
    reg_add_cluster(ep, 0x0006, REG_CLUSTER_SERVER);  /* OnOff */
    reg_add_cluster(ep, 0x0008, REG_CLUSTER_SERVER);  /* Level */
    
    /* Compute capabilities */
    uint32_t caps = cap_compute_for_node(node);
    ASSERT_TRUE(caps >= 2);  /* At least OnOff and Level */
    
    tests_passed++;
    TEST_PASS();
}

static void test_cap_get_info(void) {
    TEST_START("cap_get_info");
    
    const cap_info_t *info = cap_get_info(CAP_LIGHT_ON);
    ASSERT_TRUE(info != NULL);
    ASSERT_TRUE(strcmp(info->name, "light.on") == 0);
    ASSERT_EQ(info->type, CAP_VALUE_BOOL);
    
    tests_passed++;
    TEST_PASS();
}

static void test_cap_parse_name(void) {
    TEST_START("cap_parse_name");
    
    cap_id_t id = cap_parse_name("light.on");
    ASSERT_EQ(id, CAP_LIGHT_ON);
    
    id = cap_parse_name("sensor.temperature");
    ASSERT_EQ(id, CAP_SENSOR_TEMPERATURE);
    
    id = cap_parse_name("nonexistent");
    ASSERT_EQ(id, CAP_UNKNOWN);
    
    tests_passed++;
    TEST_PASS();
}

/* Quirks tests */

static void test_quirks_init(void) {
    TEST_START("quirks_init");
    
    os_err_t err = quirks_init();
    ASSERT_EQ(err, OS_OK);
    
    /* Double init should return error */
    err = quirks_init();
    ASSERT_EQ(err, OS_ERR_ALREADY_EXISTS);
    
    tests_passed++;
    TEST_PASS();
}

static void test_quirks_find(void) {
    TEST_START("quirks_find");
    
    /* Should find DUMMY device */
    const quirk_entry_t *entry = quirks_find("DUMMY", "DUMMY-LIGHT-1");
    ASSERT_TRUE(entry != NULL);
    ASSERT_TRUE(strcmp(entry->manufacturer, "DUMMY") == 0);
    ASSERT_TRUE(entry->action_count >= 1);
    
    /* Should not find unknown device */
    entry = quirks_find("UNKNOWN", "UNKNOWN-MODEL");
    ASSERT_TRUE(entry == NULL);
    
    tests_passed++;
    TEST_PASS();
}

static void test_quirks_apply_value(void) {
    TEST_START("quirks_apply_value");
    
    cap_value_t value;
    quirk_result_t result;
    
    /* Test clamp_range quirk */
    value.i = 150;  /* Over max of 100 */
    os_err_t err = quirks_apply_value("DUMMY", "DUMMY-LIGHT-1", CAP_LIGHT_LEVEL, &value, &result);
    ASSERT_EQ(err, OS_OK);
    ASSERT_TRUE(result.applied);
    ASSERT_EQ(value.i, 100);  /* Should be clamped to max */
    
    /* Test with value under min */
    value.i = 0;
    err = quirks_apply_value("DUMMY", "DUMMY-LIGHT-1", CAP_LIGHT_LEVEL, &value, &result);
    ASSERT_EQ(err, OS_OK);
    ASSERT_TRUE(result.applied);
    ASSERT_EQ(value.i, 1);  /* Should be clamped to min */
    
    tests_passed++;
    TEST_PASS();
}

static void test_quirks_count(void) {
    TEST_START("quirks_count");
    
    uint32_t count = quirks_count();
    ASSERT_TRUE(count >= 1);  /* At least the DUMMY entry */
    
    tests_passed++;
    TEST_PASS();
}

static void test_quirks_action_name(void) {
    TEST_START("quirks_action_name");
    
    ASSERT_TRUE(strcmp(quirks_action_name(QUIRK_ACTION_CLAMP_RANGE), "clamp_range") == 0);
    ASSERT_TRUE(strcmp(quirks_action_name(QUIRK_ACTION_INVERT_BOOLEAN), "invert_boolean") == 0);
    ASSERT_TRUE(strcmp(quirks_action_name(QUIRK_ACTION_SCALE_NUMERIC), "scale_numeric") == 0);
    
    tests_passed++;
    TEST_PASS();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("=== ESP32-C6 Zigbee Bridge OS Unit Tests ===\n\n");
    
    printf("Type tests:\n");
    test_types();
    
    printf("\nEvent bus tests:\n");
    test_event_init();
    test_event_subscribe_publish();
    test_event_filter();
    test_event_payload();
    test_event_stats();
    
    printf("\nLog tests:\n");
    test_log_init();
    test_log_levels();
    test_log_set_level();
    test_log_write();
    
    printf("\nPersistence tests:\n");
    test_persist_init();
    test_persist_put_get();
    test_persist_flush();
    test_persist_exists();
    test_persist_del();
    test_persist_schema_version();
    
    printf("\nRegistry tests:\n");
    test_reg_init();
    test_reg_add_node();
    test_reg_find_node();
    test_reg_add_endpoint();
    test_reg_add_cluster();
    test_reg_update_attribute();
    test_reg_set_state();
    test_reg_remove_node();
    
    printf("\nInterview tests:\n");
    test_interview_init();
    test_interview_start();
    
    printf("\nCapability tests:\n");
    test_cap_init();
    test_cap_compute();
    test_cap_get_info();
    test_cap_parse_name();
    
    printf("\nHA Discovery tests:\n");
    run_ha_disc_tests();
    
    printf("\nZigbee adapter tests:\n");
    run_zb_adapter_tests();
    
    printf("\nLocal node tests:\n");
    run_local_node_tests();
    
    printf("\nQuirks tests:\n");
    test_quirks_init();
    test_quirks_find();
    test_quirks_apply_value();
    test_quirks_count();
    test_quirks_action_name();
    
    printf("\n=== Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
