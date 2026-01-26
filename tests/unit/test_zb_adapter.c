/**
 * @file test_zb_adapter.c
 * @brief Zigbee adapter contract tests
 */

#include <string.h>

#include "os_event.h"
#include "test_support.h"
#include "zb_adapter.h"

static os_event_type_t last_event_type;
static os_corr_id_t last_corr_id;

static void zb_event_handler(const os_event_t *event, void *ctx) {
  (void)ctx;
  last_event_type = event->type;
  last_corr_id = event->corr_id;
}

static void test_zba_send_onoff_corr_id(void) {
  TEST_START("zba_send_onoff_corr_id");

  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, zb_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  last_event_type = OS_EVENT_TYPE_MAX;
  last_corr_id = 0;

  os_corr_id_t corr_id = 42;
  err = zba_send_onoff(0x0102030405060708ULL, 1, true, corr_id);
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(last_event_type, OS_EVENT_ZB_CMD_CONFIRM);
  ASSERT_EQ(last_corr_id, corr_id);

  os_event_unsubscribe(zb_event_handler);
  tests_passed++;
  TEST_PASS();
}

static void test_zba_stack_up_event(void) {
  TEST_START("zba_stack_up_event");

  os_event_filter_t filter = {OS_EVENT_ZB_STACK_UP, OS_EVENT_ZB_STACK_UP};
  os_err_t err = os_event_subscribe(&filter, zb_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  last_event_type = OS_EVENT_TYPE_MAX;
  err = zba_start_coordinator();
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(last_event_type, OS_EVENT_ZB_STACK_UP);

  os_event_unsubscribe(zb_event_handler);
  tests_passed++;
  TEST_PASS();
}

void run_zb_adapter_tests(void) {
  zba_init();
  test_zba_send_onoff_corr_id();
  test_zba_stack_up_event();
}
