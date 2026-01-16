/**
 * @file test_local_node.c
 * @brief Local node service tests
 */

#include "local_node.h"
#include "registry.h"
#include "gpio_button.h"
#include "i2c_sensor.h"
#include "os_types.h"
#include "test_support.h"

static void test_local_node_init(void) {
    TEST_START("local_node_init");
    
    os_err_t err = local_node_init();
    ASSERT_EQ(err, OS_OK);
    
    /* Double init should return error */
    err = local_node_init();
    ASSERT_EQ(err, OS_ERR_ALREADY_EXISTS);
    
    tests_passed++;
    TEST_PASS();
}

static void test_gpio_button_driver(void) {
    TEST_START("gpio_button_driver");
    
    os_err_t err = gpio_button_init();
    ASSERT_EQ(err, OS_OK);
    
    /* Should be able to read button state */
    bool state = gpio_button_read();
    (void)state; /* State will vary based on ticks */
    
    tests_passed++;
    TEST_PASS();
}

static void test_i2c_sensor_driver(void) {
    TEST_START("i2c_sensor_driver");
    
    os_err_t err = i2c_sensor_init();
    ASSERT_EQ(err, OS_OK);
    
    /* Should be able to read temperature */
    float temp = i2c_sensor_read_temperature_c();
    /* Temperature should be within simulation bounds (20-25°C) */
    ASSERT_TRUE(temp >= 20.0f && temp <= 25.0f);
    
    tests_passed++;
    TEST_PASS();
}

void run_local_node_tests(void) {
    test_gpio_button_driver();
    test_i2c_sensor_driver();
    test_local_node_init();
}
