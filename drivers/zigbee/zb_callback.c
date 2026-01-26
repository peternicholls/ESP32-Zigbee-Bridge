/**
 * @file zb_callback.c
 * @brief Zigbee stack callbacks - signal and attribute handling
 *
 * ESP32-C6 Zigbee Bridge OS - Callback handlers for esp-zigbee-lib
 * Lifecycle in zb_real.c, commands in zb_cmd.c (split per constitution)
 *
 * Performance metrics (SC-001..SC-004):
 * - SC-001: Network formation time logged at FORMATION signal
 * - SC-002: Device join detection time logged at DEVICE_ANNCE
 * - SC-003: Command RTT logged in send status callback
 * - SC-004: Attribute report latency logged in core action callback
 */

#include "esp_zigbee_core.h"
#include "zb_real_internal.h"

/* Performance metric: boot timestamp for SC-001 */
static uint32_t s_boot_time_ms = 0;

void zb_perf_set_boot_time(uint32_t ms) { s_boot_time_ms = ms; }

/* Command Send Status Callback - SC-003: Command RTT */
void zb_cmd_send_status_cb(esp_zb_zcl_command_send_status_message_t msg) {
  zb_pending_cmd_t *slot = pending_find_by_tsn(msg.tsn);
  if (!slot)
    return;
  /* SC-003: Log command round-trip time */
  uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
  uint32_t rtt_ms = now_ms - slot->timestamp_ms;
  LOG_I(ZB_MODULE, "CMD_RTT: %" PRIu32 "ms (corr=%" PRIu32 ", status=%d)",
        rtt_ms, slot->corr_id, msg.status);
  if (msg.status == ESP_OK) {
    struct {
      os_corr_id_t corr_id;
      uint8_t status;
    } p = {slot->corr_id, 0};
    emit_event(OS_EVENT_ZB_CMD_CONFIRM, &p, sizeof(p));
  } else {
    struct {
      os_corr_id_t corr_id;
      uint16_t err;
    } p = {slot->corr_id, (uint16_t)msg.status};
    emit_event(OS_EVENT_ZB_CMD_ERROR, &p, sizeof(p));
  }
  pending_free(slot);
}

/* Core Action Callback for Attribute Reports - SC-004: Report latency */
esp_err_t zb_core_action_cb(esp_zb_core_action_callback_id_t id,
                            const void *msg) {
  if (id == ESP_ZB_CORE_REPORT_ATTR_CB_ID) {
    const esp_zb_zcl_report_attr_message_t *r = msg;
    if (!r || !r->status)
      return ESP_OK;
    /* SC-004: Log attribute report reception (latency measured from emission)
     */
    LOG_I(ZB_MODULE, "ATTR_REPORT: cluster=0x%04x, attr=0x%04x from NWK=0x%04x",
          r->cluster, r->attribute.id, r->src_address.u.short_addr);
    struct {
      os_eui64_t eui64;
      uint8_t ep;
      uint16_t cluster;
      uint16_t attr;
      uint8_t type;
      uint8_t val[18];
    } p = {0};
    zb_nwk_entry_t *e = nwk_cache_find_by_nwk(r->src_address.u.short_addr);
    if (e)
      p.eui64 = e->eui64;
    p.ep = r->src_endpoint;
    p.cluster = r->cluster;
    p.attr = r->attribute.id;
    p.type = r->attribute.data.type;
    size_t vlen = r->attribute.data.size;
    if (vlen > sizeof(p.val))
      vlen = sizeof(p.val);
    memcpy(p.val, r->attribute.data.value, vlen);
    emit_event(OS_EVENT_ZB_ATTR_REPORT, &p, sizeof(p));
  }
  return ESP_OK;
}

/* Signal Handler - called by esp-zigbee-lib
 * SC-001: Network formation time
 * SC-002: Device join detection time
 */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal) {
  esp_zb_app_signal_type_t sig = *signal->p_app_signal;
  esp_err_t st = signal->esp_err_status;
  uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

  switch (sig) {
  case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
  case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (st == ESP_OK) {
      LOG_I(ZB_MODULE, "Network %s",
            sig == ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START ? "starting"
                                                        : "restored");
      esp_zb_bdb_start_top_level_commissioning(
          ESP_ZB_BDB_MODE_NETWORK_FORMATION);
    } else {
      LOG_E(ZB_MODULE, "Stack start failed: %d", st);
      zb_set_state_error();
      emit_event(OS_EVENT_ZB_STACK_DOWN, NULL, 0);
    }
    break;
  case ESP_ZB_BDB_SIGNAL_FORMATION:
    if (st == ESP_OK) {
      /* SC-001: Log network formation time from boot */
      uint32_t formation_ms = s_boot_time_ms ? (now_ms - s_boot_time_ms) : 0;
      LOG_I(ZB_MODULE,
            "PERF_SC001: Network formed in %" PRIu32 "ms, PAN: 0x%04x, Ch: %d",
            formation_ms, esp_zb_get_pan_id(), esp_zb_get_current_channel());
      zb_set_state_ready();
      emit_event(OS_EVENT_ZB_STACK_UP, NULL, 0);
    } else {
      LOG_E(ZB_MODULE, "Formation failed: %d", st);
      zb_set_state_error();
      emit_event(OS_EVENT_ZB_STACK_DOWN, NULL, 0);
    }
    break;
  case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
    esp_zb_zdo_signal_device_annce_params_t *a =
        esp_zb_app_signal_get_params(signal->p_app_signal);
    if (a) {
      os_eui64_t eui64 = 0;
      memcpy(&eui64, a->ieee_addr, sizeof(eui64));
      nwk_cache_insert(eui64, a->device_short_addr);
      struct {
        os_eui64_t eui64;
        uint16_t nwk;
      } p = {eui64, a->device_short_addr};
      emit_event(OS_EVENT_ZB_ANNOUNCE, &p, sizeof(p));
      /* SC-002: Log device join detection */
      LOG_I(ZB_MODULE, "PERF_SC002: Device announce: " OS_EUI64_FMT " @ 0x%04x",
            OS_EUI64_ARG(eui64), a->device_short_addr);
    }
    break;
  }
  case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
    LOG_I(ZB_MODULE, "Permit join %s", st ? "on" : "off");
    break;
  case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION: {
    esp_zb_zdo_signal_leave_indication_params_t *l =
        esp_zb_app_signal_get_params(signal->p_app_signal);
    if (l) {
      os_eui64_t eui64 = 0;
      memcpy(&eui64, l->device_addr, sizeof(eui64));
      nwk_cache_remove(eui64);
      struct {
        os_eui64_t eui64;
      } p = {eui64};
      emit_event(OS_EVENT_ZB_DEVICE_LEFT, &p, sizeof(p));
      LOG_I(ZB_MODULE, "Left: " OS_EUI64_FMT, OS_EUI64_ARG(eui64));
    }
    break;
  }
  default:
    LOG_D(ZB_MODULE, "Signal: %d, status: %d", sig, st);
    break;
  }
}
