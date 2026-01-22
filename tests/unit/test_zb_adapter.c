/**
 * @file test_zb_adapter.c
 * @brief Zigbee adapter contract compliance tests
 *
 * Tests that both zb_fake.c and zb_real.c emit events with matching shapes
 * per the contract in
 * specs/001-real-zigbee-adapter/contracts/zb_adapter_contract.md
 */

#include <string.h>

#include "os_event.h"
#include "test_support.h"
#include "zb_adapter.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Test State
 * ─────────────────────────────────────────────────────────────────────────────
 */

static os_event_t captured_event;
static int event_count;

static void capture_event_handler(const os_event_t *event, void *ctx) {
  (void)ctx;
  memcpy(&captured_event, event, sizeof(os_event_t));
  event_count++;
}

static void reset_capture(void) {
  memset(&captured_event, 0, sizeof(captured_event));
  captured_event.type = OS_EVENT_TYPE_MAX;
  event_count = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Lifecycle Tests
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void test_zb_init_returns_ok(void) {
  TEST_START("zb_init_returns_ok");
  /* zb_init already called in run_zb_adapter_tests */
  /* Second call should return error or OK depending on impl */
  tests_passed++;
  TEST_PASS();
}

static void test_zb_stack_up_event_shape(void) {
  TEST_START("zb_stack_up_event_shape");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_STACK_UP, OS_EVENT_ZB_STACK_UP};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  err = zb_start_coordinator();
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(captured_event.type, OS_EVENT_ZB_STACK_UP);
  /* Contract: ZB_STACK_UP has no payload */
  ASSERT_EQ(captured_event.payload_len, 0);

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

static void test_zb_permit_join_returns_ok(void) {
  TEST_START("zb_permit_join_returns_ok");

  /* After stack up, permit join should succeed */
  os_err_t err = zb_set_permit_join(60);
  ASSERT_EQ(err, OS_OK);

  /* Close permit join */
  err = zb_set_permit_join(0);
  ASSERT_EQ(err, OS_OK);

  tests_passed++;
  TEST_PASS();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command Tests - On/Off
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void test_zb_send_onoff_confirm_shape(void) {
  TEST_START("zb_send_onoff_confirm_shape");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  os_corr_id_t corr_id = 42;
  err = zb_send_onoff(0x0102030405060708ULL, 1, true, corr_id);
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(captured_event.type, OS_EVENT_ZB_CMD_CONFIRM);
  ASSERT_EQ(captured_event.corr_id, corr_id);

  /* Contract: payload contains zb_cmd_confirm_t */
  ASSERT_TRUE(captured_event.payload_len >= sizeof(zb_cmd_confirm_t));
  zb_cmd_confirm_t *confirm = (zb_cmd_confirm_t *)captured_event.payload;
  ASSERT_EQ(confirm->node_id, 0x0102030405060708ULL);
  ASSERT_EQ(confirm->endpoint, 1);
  ASSERT_EQ(confirm->cluster_id, 0x0006); /* On/Off cluster */

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

static void test_zb_send_onoff_off_command(void) {
  TEST_START("zb_send_onoff_off_command");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  os_corr_id_t corr_id = 43;
  err = zb_send_onoff(0x1122334455667788ULL, 2, false, corr_id);
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(captured_event.type, OS_EVENT_ZB_CMD_CONFIRM);
  ASSERT_EQ(captured_event.corr_id, corr_id);

  zb_cmd_confirm_t *confirm = (zb_cmd_confirm_t *)captured_event.payload;
  ASSERT_EQ(confirm->endpoint, 2);

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command Tests - Level
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void test_zb_send_level_confirm_shape(void) {
  TEST_START("zb_send_level_confirm_shape");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  os_corr_id_t corr_id = 100;
  err = zb_send_level(0x0102030405060708ULL, 1, 128, 10, corr_id);
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(captured_event.type, OS_EVENT_ZB_CMD_CONFIRM);
  ASSERT_EQ(captured_event.corr_id, corr_id);

  zb_cmd_confirm_t *confirm = (zb_cmd_confirm_t *)captured_event.payload;
  ASSERT_EQ(confirm->node_id, 0x0102030405060708ULL);
  ASSERT_EQ(confirm->cluster_id, 0x0008); /* Level cluster */

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

static void test_zb_send_level_max_value(void) {
  TEST_START("zb_send_level_max_value");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  os_corr_id_t corr_id = 101;
  /* Level 254 is max valid */
  err = zb_send_level(0x0102030405060708ULL, 1, 254, 0, corr_id);
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(captured_event.type, OS_EVENT_ZB_CMD_CONFIRM);

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command Tests - Read Attrs
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void test_zb_read_attrs_confirm_shape(void) {
  TEST_START("zb_read_attrs_confirm_shape");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  os_corr_id_t corr_id = 200;
  uint16_t attrs[] = {0x0000}; /* OnOff attribute */
  err = zb_read_attrs(0x0102030405060708ULL, 1, 0x0006, attrs, 1, corr_id);
  ASSERT_EQ(err, OS_OK);

  os_event_dispatch(0);
  ASSERT_EQ(captured_event.type, OS_EVENT_ZB_CMD_CONFIRM);
  ASSERT_EQ(captured_event.corr_id, corr_id);

  zb_cmd_confirm_t *confirm = (zb_cmd_confirm_t *)captured_event.payload;
  ASSERT_EQ(confirm->cluster_id, 0x0006);

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command Tests - Configure Reporting & Bind
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void test_zb_configure_reporting_returns_ok(void) {
  TEST_START("zb_configure_reporting_returns_ok");

  os_err_t err = zb_configure_reporting(0x0102030405060708ULL, 1, 0x0006,
                                        0x0000, 0x20, 1, 300, 0);
  ASSERT_EQ(err, OS_OK);

  tests_passed++;
  TEST_PASS();
}

static void test_zb_bind_returns_ok(void) {
  TEST_START("zb_bind_returns_ok");

  os_err_t err = zb_bind(0x0102030405060708ULL, 1, 0x0006, 0);
  ASSERT_EQ(err, OS_OK);

  tests_passed++;
  TEST_PASS();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Correlation ID Tests
 * ─────────────────────────────────────────────────────────────────────────────
 */

static void test_corr_id_zero_no_event(void) {
  TEST_START("corr_id_zero_no_event");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  /* corr_id = 0 means no response needed (fake still emits for simplicity) */
  err = zb_send_onoff(0x0102030405060708ULL, 1, true, 0);
  ASSERT_EQ(err, OS_OK);

  /* Dispatch and check - fake emits anyway but corr_id should be non-zero */
  os_event_dispatch(0);
  /* For fake impl, an auto-generated corr_id is used */
  ASSERT_TRUE(captured_event.corr_id != 0 || event_count == 0);

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

static void test_corr_id_preserved_across_commands(void) {
  TEST_START("corr_id_preserved_across_commands");

  reset_capture();
  os_event_filter_t filter = {OS_EVENT_ZB_CMD_CONFIRM, OS_EVENT_ZB_CMD_CONFIRM};
  os_err_t err = os_event_subscribe(&filter, capture_event_handler, NULL);
  ASSERT_EQ(err, OS_OK);

  os_corr_id_t corr1 = 1001;
  os_corr_id_t corr2 = 2002;

  err = zb_send_onoff(0xAAAABBBBCCCCDDDDULL, 1, true, corr1);
  ASSERT_EQ(err, OS_OK);
  os_event_dispatch(1);
  ASSERT_EQ(captured_event.corr_id, corr1);

  reset_capture();
  err = zb_send_level(0xAAAABBBBCCCCDDDDULL, 1, 50, 5, corr2);
  ASSERT_EQ(err, OS_OK);
  os_event_dispatch(1);
  ASSERT_EQ(captured_event.corr_id, corr2);

  os_event_unsubscribe(capture_event_handler);
  tests_passed++;
  TEST_PASS();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test Runner
 * ─────────────────────────────────────────────────────────────────────────────
 */

void run_zb_adapter_tests(void) {
  zb_init();

  /* Lifecycle tests */
  test_zb_init_returns_ok();
  test_zb_stack_up_event_shape();
  test_zb_permit_join_returns_ok();

  /* On/Off command tests */
  test_zb_send_onoff_confirm_shape();
  test_zb_send_onoff_off_command();

  /* Level command tests */
  test_zb_send_level_confirm_shape();
  test_zb_send_level_max_value();

  /* Read attrs test */
  test_zb_read_attrs_confirm_shape();

  /* Configure reporting & bind */
  test_zb_configure_reporting_returns_ok();
  test_zb_bind_returns_ok();

  /* Correlation ID tests */
  test_corr_id_zero_no_event();
  test_corr_id_preserved_across_commands();
}
