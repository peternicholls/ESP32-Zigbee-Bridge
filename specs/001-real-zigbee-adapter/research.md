# Research: Real Zigbee Adapter (M4)

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Date**: 2026-01-20

## Research Tasks

Based on Technical Context unknowns and technology choices:

1. ESP-Zigbee coordinator initialization pattern
2. Callback handling in esp-zigbee-lib
3. ZCL command send/receive patterns
4. Device join/leave signal handling
5. Thread safety and lock requirements

---

## 1. ESP-Zigbee Coordinator Initialization

**Decision**: Use `esp_zb_cfg_t` with `ESP_ZB_ZC` role and autostart mode

**Rationale**: 
- `esp_zb_init()` requires network config struct with device role
- `esp_zb_start(true)` enables autostart which handles network formation automatically
- Formation signal `ESP_ZB_BDB_SIGNAL_FORMATION` indicates network is ready
- Stack persists network params (PAN ID, channel, keys) to NVS automatically via ZBOSS

**Alternatives Considered**:
- Manual network formation via `esp_zb_bdb_start_top_level_commissioning()` — rejected as autostart handles this
- No-autostart mode — rejected as it requires manual BDB commissioning steps

**Key API Calls**:
```c
esp_zb_cfg_t cfg = {
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
    .install_code_policy = false,
    .nwk_cfg.zczr_cfg.max_children = 64,
};
esp_zb_init(&cfg);
esp_zb_start(true);  // autostart mode
esp_zb_stack_main_loop();  // runs in Zigbee task
```

---

## 2. Signal/Callback Handling Pattern

**Decision**: Implement `esp_zb_app_signal_handler()` that enqueues events to OS bus immediately

**Rationale**:
- `esp_zb_app_signal_handler()` is called by ZBOSS from Zigbee task context
- Must return quickly (constitution: "Callbacks Enqueue Only")
- Signal types include: `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE`, `ESP_ZB_ZDO_SIGNAL_LEAVE`, formation signals
- All business logic deferred to domain event loop

**Key Signals to Handle**:
| Signal | Maps To Event | Payload |
|--------|--------------|---------|
| `ESP_ZB_BDB_SIGNAL_FORMATION` | `OS_EVENT_ZB_STACK_UP` | none |
| `ESP_ZB_COMMON_SIGNAL_DEVICE_JOINED` | `OS_EVENT_ZB_DEVICE_JOINED` | EUI64 |
| `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE` | `OS_EVENT_ZB_ANNOUNCE` | NWK addr, EUI64 |
| `ESP_ZB_ZDO_SIGNAL_LEAVE` | `OS_EVENT_ZB_DEVICE_LEFT` | EUI64 |

**Alternatives Considered**:
- Process signals synchronously in callback — rejected (violates constitution)
- Use esp_zb scheduler callbacks — rejected (adds complexity, same pattern needed)

---

## 3. ZCL Command Send Pattern

**Decision**: Use `esp_zb_zcl_on_off_cmd_req()` and similar typed APIs with command send status callback

**Rationale**:
- ESP-Zigbee provides typed command APIs per cluster (OnOff, Level, etc.)
- `esp_zb_zcl_command_send_status_handler_register()` provides async confirmation
- TSN (Transaction Sequence Number) enables correlation with original request
- Must acquire `esp_zb_lock_acquire()` before calling from non-Zigbee context

**Key Pattern**:
```c
// Register send status callback once at init
esp_zb_zcl_command_send_status_handler_register(zb_cmd_send_status_cb);

// Send command (from any context with lock)
esp_zb_lock_acquire(portMAX_DELAY);
esp_zb_zcl_on_off_cmd_t cmd = {
    .zcl_basic_cmd.dst_endpoint = endpoint,
    .zcl_basic_cmd.dst_addr_u.addr_short = nwk_addr,
    .on_off_cmd_id = on ? ESP_ZB_ZCL_CMD_ON_OFF_ON_ID : ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID,
};
esp_zb_zcl_on_off_cmd_req(&cmd);
esp_zb_lock_release();
```

**Correlation ID Strategy**:
- Store `corr_id` → `tsn` mapping in static array (bounded, 16 slots)
- On send status callback, look up `corr_id` by `tsn`, emit event with `corr_id`

**Alternatives Considered**:
- Raw APS data request — rejected (loses cluster-level encoding)
- Blocking send with result — rejected (would block domain task)

---

## 4. Attribute Report Reception

**Decision**: Use `esp_zb_core_action_handler_register()` with `ESP_ZB_CORE_REPORT_ATTR_CB_ID`

**Rationale**:
- The core action callback is the primary way to receive ZCL attribute reports
- Callback ID `ESP_ZB_CORE_REPORT_ATTR_CB_ID` (0x2000) provides `esp_zb_zcl_report_attr_message_t`
- Callback runs in Zigbee task context — must enqueue event and return immediately

**Key Pattern**:
```c
static esp_err_t zb_core_action_cb(esp_zb_core_action_callback_id_t id, const void *msg) {
    if (id == ESP_ZB_CORE_REPORT_ATTR_CB_ID) {
        const esp_zb_zcl_report_attr_message_t *report = msg;
        // Build event payload with: src_addr, endpoint, cluster_id, attr_id, value
        // os_event_emit(OS_EVENT_ZB_ATTR_REPORT, &payload, sizeof(payload));
    }
    return ESP_OK;
}
esp_zb_core_action_handler_register(zb_core_action_cb);
```

**Alternatives Considered**:
- Polling with read attribute requests — rejected (inefficient, misses unsolicited reports)
- Custom cluster handler — rejected (standard reports use core action callback)

---

## 5. Thread Safety and Lock Requirements

**Decision**: Acquire `esp_zb_lock_acquire()` when calling Zigbee APIs from non-Zigbee task

**Rationale**:
- ZBOSS stack is single-threaded; all API calls must be serialized
- Lock is NOT required when already in callback context (signal handler, action callback)
- Lock protects: command sends, permit join, configuration changes
- OS event bus is already thread-safe (ISR-safe publish)

**Pattern**:
```c
// Safe from any task
zb_err_t zb_send_onoff(...) {
    esp_zb_lock_acquire(portMAX_DELAY);
    // ... send command ...
    esp_zb_lock_release();
}
```

**Alternatives Considered**:
- Queue commands to Zigbee task — rejected (adds latency, complexity)
- Scheduler callbacks only — rejected (lock approach is documented API pattern)

---

## 6. Address Resolution: EUI64 ↔ NWK

**Decision**: Cache EUI64→NWK mapping locally; use `esp_zb_zdo_ieee_addr_req()` for lookups

**Rationale**:
- Commands use NWK (short) address; stable identity is EUI64
- Device announce signal provides both EUI64 and NWK address
- Cache mapping when device joins/announces
- For commands, look up NWK from cache by EUI64

**Implementation**:
- Static array of `{eui64, nwk_addr}` pairs (64 entries max)
- Populated on `ESP_ZB_COMMON_SIGNAL_DEVICE_JOINED` and `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE`
- Commands fail immediately if EUI64 not in cache (return `OS_ERR_NOT_FOUND`)

---

## 7. Permit Join Implementation

**Decision**: Use `esp_zb_bdb_open_network()` API

**Rationale**:
- `esp_zb_bdb_open_network(uint8_t permit_duration)` is the BDB-standard API
- Duration in seconds (0-254), 255 = permanent (avoid)
- Can be called from any context with lock

**Pattern**:
```c
zb_err_t zb_set_permit_join(uint16_t seconds) {
    if (seconds > 254) seconds = 254;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_err_t err = esp_zb_bdb_open_network((uint8_t)seconds);
    esp_zb_lock_release();
    return (err == ESP_OK) ? OS_OK : OS_ERR_BUSY;
}
```

---

## Summary of Decisions

| Area | Decision | Confidence |
|------|----------|------------|
| Init | Coordinator role, autostart | High |
| Signals | Enqueue to OS bus immediately | High |
| Commands | Typed ZCL APIs + send status cb | High |
| Reports | Core action callback | High |
| Threading | Lock around API calls | High |
| Addressing | Local EUI64→NWK cache | High |
| Permit Join | `esp_zb_bdb_open_network()` | High |

All NEEDS CLARIFICATION items resolved.
