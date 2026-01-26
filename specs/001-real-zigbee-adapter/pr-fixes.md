# PR #13 Code Review Fixes

**Branch**: `001-fix` → `main`  
**Reviewers**: Gemini Code Assist, GitHub Copilot  
**Date**: 2026-01-26

## Critical (Must Fix)

- [x] **T1** [zb_cmd.c:77] Capture TSN after `esp_zb_zcl_on_off_cmd_req()` — add `zb_pending_set_tsn(slot, cmd.zcl_basic_cmd.tsn);`
- [x] **T2** [zb_cmd.c:133] Capture TSN after `esp_zb_zcl_level_move_to_level_cmd_req()` — same fix

## High Priority

- [x] **T3** [Makefile:129] Remove hardcoded `IDF_EXPORT` path — require ESP-IDF env sourced, call `idf.py` directly
- [x] **T4** [zb_real.c:398] Fix `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE` handler:
  - Change event type from `OS_EVENT_ZB_DEVICE_JOINED` to `OS_EVENT_ZB_ANNOUNCE`
  - Include both `eui64` and `nwk_addr` in payload (per data-model.md)

## Medium Priority

- [x] **T5** [Makefile:74] Change `PORT` default from `/dev/tty.usbmodem5AAF1845231` to `/dev/ttyUSB0`
- [x] **T6** [zb_adapter.h:56, zb_shell.c, main.c] Rename `zb_shell_init` → `zba_shell_init` for naming consistency
- [x] **T7** [main.c:139] Add `#ifndef OS_PLATFORM_HOST` guard around `zb_shell_init()` call to fix host build
- [x] **T8** [zb_real.c:213] Remove `__attribute__((unused))` from `pending_cmd_free` — function is used
- [x] **T9** [sdkconfig.defaults:10] Add comment explaining why 32KB main task stack is needed
- [x] **T10** [tasks.md] Update function names from `zb_*` to `zba_*` convention

## Fix Details

### T1/T2: TSN Capture (Critical)

The Transaction Sequence Number must be captured after sending ZCL commands for correlation in `zb_send_status_cb`. Without this, command confirmations will never match.

```c
// After esp_zb_zcl_on_off_cmd_req(&cmd):
esp_err_t err = esp_zb_zcl_on_off_cmd_req(&cmd);
esp_zb_lock_release();

if (err != ESP_OK) {
  LOG_E(ZB_MODULE, "Failed to send OnOff cmd: %d", err);
  zb_pending_free(slot);
  return OS_ERR_INVALID_ARG;
}

/* Capture TSN assigned by the stack */
zb_pending_set_tsn(slot, cmd.zcl_basic_cmd.tsn);

return OS_OK;
```

### T4: Device Announce Event

Per `data-model.md`, the announce event should include:
- `eui64` (64-bit IEEE address)
- `nwk_addr` (16-bit network address)

Event type should be `OS_EVENT_ZB_ANNOUNCE`, not `OS_EVENT_ZB_DEVICE_JOINED`.

### T7: Host Build Guard

```c
#ifndef OS_PLATFORM_HOST
  /* Initialize Zigbee shell commands (ESP32 only) */
  zb_shell_init();
#endif
```

## Execution Order

1. T1, T2 (critical — blocks testing)
2. T7 (blocks host build/tests)
3. T4 (semantic correctness)
4. T3, T5 (portability)
5. T6, T8, T9, T10 (cleanup)
