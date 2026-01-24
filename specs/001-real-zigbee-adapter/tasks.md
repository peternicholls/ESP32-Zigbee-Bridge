# Tasks: Real Zigbee Adapter (M4)

**Input**: Design documents from `/specs/001-real-zigbee-adapter/`  
**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, contracts/ ✓

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1-US5)
- **🔨 BUILD**: Build checkpoint — must compile without errors
- **🚀 FLASH**: Flash checkpoint — must deploy and run on hardware
- All paths relative to repository root

---

## Phase 1: Setup

**Purpose**: Project initialization and file structure

- [ ] T001 Create `drivers/zigbee/zb_real.c` skeleton with includes, forward declarations, and empty function stubs for all 8 contract functions
- [ ] T002 Create `drivers/zigbee/zb_cmd.c` skeleton with includes and forward declarations for command functions
- [ ] T003 Update `drivers/zigbee/CMakeLists.txt` — add conditional: if `CONFIG_IDF_TARGET_ESP32C6`, compile `zb_real.c` + `zb_cmd.c`; else compile `zb_fake.c`
- [ ] T004 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify compiles with stub implementations (functions return `OS_ERR_NOT_IMPLEMENTED`)

---

## Phase 2: Foundational Data Structures

**Purpose**: Core infrastructure required by ALL user stories

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

### Address Cache (EUI64 ↔ NWK mapping)

- [ ] T005 Define `zb_nwk_entry_t` struct in `zb_real.c`: `{eui64, nwk_addr, last_seen_ms}` (12 bytes)
- [ ] T006 Declare static array `s_nwk_cache[64]` and `s_nwk_count` in `zb_real.c`
- [ ] T007 Implement `nwk_cache_insert(eui64, nwk)` — find existing or add new, return pointer
- [ ] T008 Implement `nwk_cache_lookup_by_eui64(eui64)` — return entry or NULL
- [ ] T009 Implement `nwk_cache_remove(eui64)` — mark slot empty, return bool

### Correlation Tracking (command → response matching)

- [ ] T010 Define `zb_pending_cmd_t` struct in `zb_real.c`: `{tsn, corr_id, timestamp_ms, in_use}` (16 bytes)
- [ ] T011 Declare static array `s_pending_cmds[16]` in `zb_real.c`
- [ ] T012 Implement `pending_cmd_alloc(corr_id)` — find free slot, set tsn=0 initially, return pointer
- [ ] T013 Implement `pending_cmd_set_tsn(slot, tsn)` — called after esp_zb send returns TSN
- [ ] T014 Implement `pending_cmd_lookup_by_tsn(tsn)` — return entry or NULL
- [ ] T015 Implement `pending_cmd_free(slot)` — mark slot unused

### State Machine

- [ ] T016 Define `zb_state_t` enum: `{ZB_STATE_UNINITIALIZED, ZB_STATE_INITIALIZING, ZB_STATE_READY, ZB_STATE_ERROR}`
- [ ] T017 Declare static `s_zb_state` initialized to `ZB_STATE_UNINITIALIZED`
- [ ] T018 Implement `zb_state_transition(new_state)` — validate transition, log, update state

### Callback Registration Stubs

- [ ] T019 Add empty `zb_send_status_cb()` function — logs "send status cb called" and returns
- [ ] T020 Add empty `zb_core_action_cb()` function — logs "core action cb called" and returns `ESP_OK`
- [ ] T021 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify all data structures compile, no warnings

---

## Phase 3: User Story 1 — Form Zigbee Network (Priority: P1) 🎯 MVP

**Goal**: Coordinator forms network on power-up, emits `ZB_STACK_UP` event

**Acceptance**: Power cycle device, serial log shows PAN ID and channel, `ZB_STACK_UP` event emitted within 5 seconds

### Zigbee Task & Initialization

- [ ] T022 [US1] Implement `zb_task()` function — calls `esp_zb_start(false)` then `esp_zb_stack_main_loop()` (never returns)
- [ ] T023 [US1] Implement `zb_init()` body:
  - Validate state is UNINITIALIZED
  - Call `esp_zb_platform_config()` with radio/host config
  - Call `esp_zb_init()` with coordinator config (install code disabled, primary channel mask)
  - Register callbacks: `esp_zb_core_action_handler_register(zb_core_action_cb)`
  - Register callbacks: `esp_zb_zcl_command_send_status_handler_register(zb_send_status_cb)`
  - Create task with `xTaskCreate(zb_task, "zigbee", 4096, NULL, 5, NULL)`
  - Transition state to INITIALIZING
  - Return `OS_OK`
- [ ] T024 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify init compiles

### Signal Handler (Network Formation)

- [ ] T025 [US1] Implement `esp_zb_app_signal_handler()` skeleton — switch on `sig_type`
- [ ] T026 [US1] Handle `ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP` — call `esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION)`
- [ ] T027 [US1] Handle `ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START` / `ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT`:
  - If status OK, call `esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION)`
  - If status not OK, log error, transition to ERROR state
- [ ] T028 [US1] Handle `ESP_ZB_BDB_SIGNAL_FORMATION`:
  - If status OK: log PAN ID + channel, transition to READY, emit `OS_EVENT_ZB_STACK_UP`
  - If status not OK: log error, transition to ERROR state
- [ ] T029 [US1] Implement `zb_start_coordinator()` — return `OS_OK` if state is READY or INITIALIZING, else `OS_ERR_INVALID_STATE`
- [ ] T030 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify signal handler compiles
- [ ] T031 🚀 **FLASH CHECKPOINT**: Flash to ESP32-C6, boot, verify in serial monitor:
  - Log: "Network formed, PAN ID: 0xXXXX, Channel: XX"
  - Event: `OS_EVENT_ZB_STACK_UP` emitted
  - Task: `ps` shows zigbee task running (if shell available)

**✅ US1 COMPLETE**: Network forms on boot

---

## Phase 4: User Story 2 — Device Join via Permit Join (Priority: P1)

**Goal**: Enable pairing mode, detect device joins, emit `ZB_DEVICE_JOINED` events

**Acceptance**: Enable permit join, pair a Zigbee bulb, verify `ZB_DEVICE_JOINED` event with EUI64 in logs

### Permit Join

- [ ] T032 [US2] Implement `zb_set_permit_join(seconds)`:
  - Validate state is READY
  - Acquire `esp_zb_lock_acquire(portMAX_DELAY)`
  - Call `esp_zb_bdb_open_network(seconds)`
  - Release `esp_zb_lock_release()`
  - Return `OS_OK`

### Device Join Signal Handling

- [ ] T033 [US2] Handle `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE` in signal handler:
  - Extract `esp_zb_zdo_signal_device_annce_params_t` from signal
  - Get EUI64 via `esp_zb_address_short_to_ieee(nwk_addr, &ieee_addr)`
  - Call `nwk_cache_insert(eui64, nwk_addr)`
  - Build event payload with EUI64 + NWK
  - Emit `OS_EVENT_ZB_ANNOUNCE`
- [ ] T034 [US2] Handle join indication (device association):
  - On new device association, extract EUI64
  - Call `nwk_cache_insert(eui64, nwk_addr)`
  - Emit `OS_EVENT_ZB_DEVICE_JOINED` with EUI64

- [ ] T035 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify permit join compiles
- [ ] T036 🚀 **FLASH CHECKPOINT**: Flash, test with real Zigbee device:
  - Call `zb_set_permit_join(180)` (via shell or test code)
  - Put test device in pairing mode
  - Verify log: "Device joined: EUI64=XX:XX:XX:XX:XX:XX:XX:XX"
  - Verify event: `OS_EVENT_ZB_DEVICE_JOINED` emitted

**✅ US2 COMPLETE**: Devices can join network

---

## Phase 5: User Story 3 — Send On/Off Command (Priority: P2)

**Goal**: Send On/Off commands to joined devices, receive confirmations

**Acceptance**: After device joins, call `zb_send_onoff()`, observe light toggles and `ZB_CMD_CONFIRM` event

### Command Sending (in zb_cmd.c)

- [ ] T037 [US3] Add `#include` for zb_real.c internal helpers (declare in header or use extern)
- [ ] T038 [US3] Implement `zb_send_onoff(node_id, endpoint, on, corr_id)` in `zb_cmd.c`:
  - Validate state is READY
  - Lookup NWK from cache by EUI64 — return `OS_ERR_NOT_FOUND` if missing
  - Allocate pending cmd slot — return `OS_ERR_NO_MEM` if full
  - Acquire lock
  - Build `esp_zb_zcl_on_off_cmd_s` struct
  - Call `esp_zb_zcl_on_off_cmd_req(&cmd)`
  - Store returned TSN in pending slot via `pending_cmd_set_tsn()`
  - Release lock
  - Return `OS_OK`

### Send Status Callback (in zb_real.c)

- [ ] T039 [US3] Implement `zb_send_status_cb()` body:
  - Extract TSN and status from callback params
  - Lookup pending cmd by TSN
  - If not found, log warning and return
  - Build event payload with `corr_id` and status
  - If status OK: emit `OS_EVENT_ZB_CMD_CONFIRM`
  - If status not OK: emit `OS_EVENT_ZB_CMD_ERROR`
  - Free pending cmd slot

### Level Command (parallel with T038-T039)

- [ ] T040 [P] [US3] Implement `zb_send_level(node_id, endpoint, level, transition_ms, corr_id)` in `zb_cmd.c`:
  - Same pattern as onoff
  - Use `esp_zb_zcl_level_move_to_level_cmd_req()`

- [ ] T041 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify commands compile
- [ ] T042 🚀 **FLASH CHECKPOINT**: Flash, test with joined light:
  - Send `zb_send_onoff(device_eui64, 1, true, corr_id)`
  - Verify light turns on
  - Verify `OS_EVENT_ZB_CMD_CONFIRM` received with matching `corr_id`
  - Send off command, verify light turns off

**✅ US3 COMPLETE**: Commands work end-to-end

---

## Phase 6: User Story 4 — Receive Attribute Reports (Priority: P2)

**Goal**: Receive unsolicited attribute reports from devices, emit events

**Acceptance**: Toggle paired light manually, verify `ZB_ATTR_REPORT` event with OnOff value

### Core Action Callback (in zb_real.c)

- [ ] T043 [US4] Implement `zb_core_action_cb()` — switch on `callback_id`
- [ ] T044 [US4] Handle `ESP_ZB_CORE_REPORT_ATTR_CB_ID`:
  - Extract `esp_zb_zcl_report_attr_message_t` from data
  - Lookup EUI64 from NWK cache (reverse lookup by short addr)
  - Build event payload: `{eui64, endpoint, cluster_id, attr_id, value}`
  - Emit `OS_EVENT_ZB_ATTR_REPORT`
  - Return `ESP_OK`
- [ ] T045 [US4] Handle `ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID` (read response):
  - Extract attribute data
  - Emit `OS_EVENT_ZB_ATTR_REPORT` for each attribute
  - Return `ESP_OK`

### Attribute Commands (in zb_cmd.c)

- [ ] T046 [US4] Implement `zb_read_attrs(node_id, endpoint, cluster, attr_ids, count, corr_id)`:
  - Validate state, lookup NWK, allocate pending slot
  - Acquire lock
  - Build `esp_zb_zcl_read_attr_cmd_s`
  - Call `esp_zb_zcl_read_attr_cmd_req()`
  - Release lock
  - Return `OS_OK`
- [ ] T047 [P] [US4] Implement `zb_configure_reporting(node_id, endpoint, cluster, attr_id, min_s, max_s, change)`:
  - Acquire lock
  - Build config report request
  - Call `esp_zb_zcl_config_report_cmd_req()`
  - Release lock
- [ ] T048 [P] [US4] Implement `zb_bind(node_id, endpoint, cluster, dst)`:
  - Acquire lock
  - Call `esp_zb_zdo_bind_req()`
  - Release lock

- [ ] T049 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify attribute handling compiles
- [ ] T050 🚀 **FLASH CHECKPOINT**: Flash, test attribute reports:
  - Manually toggle paired light switch
  - Verify `OS_EVENT_ZB_ATTR_REPORT` with OnOff cluster value in logs
  - Call `zb_read_attrs()` and verify response event

**✅ US4 COMPLETE**: Attribute reports received and emitted

---

## Phase 7: User Story 5 — Device Leave Detection (Priority: P3)

**Goal**: Detect device leave, emit `ZB_DEVICE_LEFT` event, clean up cache

**Acceptance**: Factory reset a paired device, verify `ZB_DEVICE_LEFT` event

- [ ] T051 [US5] Handle `ESP_ZB_ZDO_SIGNAL_LEAVE` / `ESP_ZB_NWK_SIGNAL_DEVICE_LEFT` in signal handler:
  - Extract EUI64 from leave indication
  - Call `nwk_cache_remove(eui64)`
  - Emit `OS_EVENT_ZB_DEVICE_LEFT` with EUI64

- [ ] T052 🔨 **BUILD CHECKPOINT**: Run `idf.py build` — verify leave handling compiles
- [ ] T053 🚀 **FLASH CHECKPOINT**: Flash, test device leave:
  - Factory reset a paired device
  - Verify `OS_EVENT_ZB_DEVICE_LEFT` emitted

**✅ US5 COMPLETE**: Device leave detected

---

## Phase 8: Polish & Robustness

**Purpose**: Edge cases, validation, performance verification

### Event Queue Handling

- [ ] T054 Add event queue full handling in all event emission points:
  - Check return value of `os_event_post()`
  - If queue full: increment `s_event_drop_count`, log warning (rate-limited)

### Command Timeout

- [ ] T055 Add `pending_cmd_purge_expired()` function:
  - Iterate pending slots
  - If `in_use && (now - timestamp_ms) > 5000`: emit `OS_EVENT_ZB_CMD_ERROR` with timeout, free slot
- [ ] T056 Call `pending_cmd_purge_expired()` periodically (e.g., in a timer or idle check)

### Code Quality

- [ ] T057 Run line count: `wc -l drivers/zigbee/zb_real.c drivers/zigbee/zb_cmd.c`
  - If either >200 lines, refactor to reduce
- [ ] T058 Review all callbacks for dynamic allocation (malloc/calloc) — must be zero
- [ ] T059 Add reverse lookup helper `nwk_cache_lookup_by_nwk(nwk_addr)` if needed for reports

### Validation

- [ ] T060 🔨 **BUILD CHECKPOINT**: Final clean build, no warnings
- [ ] T061 🚀 **INTEGRATION TEST**: Full quickstart.md validation:
  - Build, flash
  - Verify network formation <5s (SC-001)
  - Join device, verify detection <2s (SC-002)
  - Send command, verify RTT <500ms (SC-003)
  - Receive attribute report, verify latency <100ms (SC-004)
- [ ] T062 Run sustained event test: 10 events/sec for 60s, verify no drops (SC-005)

---

## Dependencies & Execution Order

### Phase Dependencies

```
Phase 1: Setup (T001-T004)
    ↓
Phase 2: Foundational (T005-T021)
    ↓
Phase 3: US1 Network Formation (T022-T031)
    ↓
Phase 4: US2 Device Join (T032-T036)
    ↓
┌───────────┬───────────┐
↓           ↓           ↓
Phase 5     Phase 6     Phase 7
US3 Cmds    US4 Reports US5 Leave
(T037-T042) (T043-T050) (T051-T053)
↓           ↓           ↓
└───────────┴───────────┘
    ↓
Phase 8: Polish (T054-T062)
```

### Parallel Opportunities

| Tasks | Can Parallel? | Rationale |
|-------|---------------|-----------|
| T005-T009, T010-T015, T016-T018 | ✓ | Different data structures |
| T040 | ✓ with T038-T039 | Independent command |
| T047, T048 | ✓ with T046 | Independent functions |
| Phase 5, 6, 7 | ✓ | Different functionality |

---

## Implementation Strategy

### MVP First (US1 + US2 Only)

1. Complete Phase 1: Setup (T001-T004)
2. Complete Phase 2: Foundational (T005-T021)
3. Complete Phase 3: US1 Network Formation (T022-T031)
4. Complete Phase 4: US2 Device Join (T032-T036)
5. **STOP and VALIDATE**: Form network, join device, see events
6. Deploy/demo — basic Zigbee coordinator working

### Full Implementation

1. MVP above
2. Phase 5: US3 Commands (T037-T042)
3. Phase 6: US4 Reports (T043-T050)
4. Phase 7: US5 Leave (T051-T053)
5. Phase 8: Polish (T054-T062)

---

## Notes

- Files: `drivers/zigbee/zb_real.c` (lifecycle, callbacks) + `drivers/zigbee/zb_cmd.c` (commands)
- Constitution: Callbacks enqueue events and return <1ms — no business logic
- Thread safety: `esp_zb_lock_acquire()` before API calls from non-Zigbee task
- Event payloads max 32 bytes (OS_EVENT_PAYLOAD_SIZE)
- Contract: Must match `zb_adapter.h` exactly — same function signatures as `zb_fake.c`
- Line budget: <200 lines per file

---

## Summary

| Metric | Value |
|--------|-------|
| Total Tasks | 62 |
| Setup Tasks | 4 |
| Foundational Tasks | 17 |
| US1 Tasks (P1) | 10 |
| US2 Tasks (P1) | 5 |
| US3 Tasks (P2) | 6 |
| US4 Tasks (P2) | 8 |
| US5 Tasks (P3) | 3 |
| Polish Tasks | 9 |
| Build Checkpoints | 9 |
| Flash Checkpoints | 7 |
| MVP Scope | US1 + US2 (36 tasks) |
