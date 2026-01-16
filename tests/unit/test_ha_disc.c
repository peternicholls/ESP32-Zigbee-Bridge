/**
 * @file test_ha_disc.c
 * @brief Home Assistant discovery tests
 */

#include <string.h>

#include "ha_disc.h"
#include "capability.h"
#include "os_types.h"
#include "test_support.h"

static void test_ha_disc_init(void) {
    TEST_START("ha_disc_init");
    
    os_err_t err = ha_disc_init();
    ASSERT_EQ(err, OS_OK);
    
    /* Double init should return error */
    err = ha_disc_init();
    ASSERT_EQ(err, OS_ERR_ALREADY_EXISTS);
    
    tests_passed++;
    TEST_PASS();
}

static void test_ha_disc_component_name(void) {
    TEST_START("ha_disc_component_name");
    
    ASSERT_TRUE(strcmp(ha_disc_component_name(HA_COMPONENT_LIGHT), "light") == 0);
    ASSERT_TRUE(strcmp(ha_disc_component_name(HA_COMPONENT_SWITCH), "switch") == 0);
    ASSERT_TRUE(strcmp(ha_disc_component_name(HA_COMPONENT_SENSOR), "sensor") == 0);
    ASSERT_TRUE(strcmp(ha_disc_component_name(HA_COMPONENT_BINARY_SENSOR), "binary_sensor") == 0);
    
    tests_passed++;
    TEST_PASS();
}

static void test_ha_disc_generate_config(void) {
    TEST_START("ha_disc_generate_config");
    
    os_eui64_t addr = 0xAABBCCDDEEFF0011;
    ha_disc_config_t config;
    
    os_err_t err = ha_disc_generate_config(addr, CAP_LIGHT_ON, &config);
    ASSERT_EQ(err, OS_OK);
    ASSERT_EQ(config.component, HA_COMPONENT_LIGHT);
    ASSERT_TRUE(strlen(config.unique_id) > 0);
    ASSERT_TRUE(strlen(config.state_topic) > 0);
    ASSERT_TRUE(strlen(config.command_topic) > 0);
    
    tests_passed++;
    TEST_PASS();
}

void run_ha_disc_tests(void) {
    test_ha_disc_init();
    test_ha_disc_component_name();
    test_ha_disc_generate_config();
}
