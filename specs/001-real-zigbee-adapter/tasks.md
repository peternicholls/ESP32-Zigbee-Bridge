# Tasks: Real Zigbee Adapter (M4)

**Input**: Design documents from `/specs/001-real-zigbee-adapter/`  
**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, contracts/ ✓

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1-US5)
- All paths relative to repository root

---

## Phase 1: Setup

**Purpose**: Project initialization and file structure

- [X] T001 Create `drivers/zigbee/zb_real.c` skeleton with includes and static state variables
- [X] T002 Update `drivers/zigbee/CMakeLists.txt` to conditionally compile zb_real.c for ESP32-C6 target

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure required by ALL user stories

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T003 Implement `zb_nwk_entry_t` address cache (64 slots) with lookup/insert/remove helpers in `drivers/zigbee/zb_real.c`
- [X] T004 Implement `zb_pending_cmd_t` correlation tracking (16 slots) with alloc/free/lookup by TSN in `drivers/zigbee/zb_real.c`
- [X] T005 Implement `zb_state_t` state machine (UNINITIALIZED→INITIALIZING→READY→ERROR) in `drivers/zigbee/zb_real.c`
- [X] T006 Register `esp_zb_zcl_command_send_status_handler_register()` callback stub in `drivers/zigbee/zb_real.c`
- [X] T007 Register `esp_zb_core_action_handler_register()` callback stub in `drivers/zigbee/zb_real.c`

**Checkpoint**: Foundation ready — all data structures and callback registration in place

---

## Phase 3: User Story 1 — Form Zigbee Network (Priority: P1) 🎯 MVP

**Goal**: Coordinator forms network on power-up, emits `ZB_STACK_UP` event

**Independent Test**: Power cycle device, observe serial log shows PAN ID and channel, `ZB_STACK_UP` event emitted within 5 seconds

### Implementation for User Story 1

- [X] T008 [US1] Implement `zb_init()` — create Zigbee task, call `esp_zb_init()` with coordinator config in `drivers/zigbee/zb_real.c`
- [X] T009 [US1] Implement Zigbee task function — call `esp_zb_start(true)` then `esp_zb_stack_main_loop()` in `drivers/zigbee/zb_real.c`
- [X] T010 [US1] Implement `esp_zb_app_signal_handler()` — handle `ESP_ZB_BDB_SIGNAL_FORMATION`, emit `OS_EVENT_ZB_STACK_UP` in `drivers/zigbee/zb_real.c`
- [X] T011 [US1] Implement `zb_start_coordinator()` — validate state, return OK if already started in `drivers/zigbee/zb_real.c`
- [X] T012 [US1] Add formation error handling — emit `ZB_STACK_DOWN` on failure, transition to ERROR state in `drivers/zigbee/zb_real.c`

**Checkpoint**: Network forms on boot, `ZB_STACK_UP` emitted — US1 complete

---

## Phase 4: User Story 2 — Device Join via Permit Join (Priority: P1)

**Goal**: Enable pairing mode, detect device joins, emit `ZB_DEVICE_JOINED` events

**Independent Test**: Enable permit join, pair a Zigbee bulb, verify `ZB_DEVICE_JOINED` event with EUI64 in logs

### Implementation for User Story 2

- [X] T013 [US2] Implement `zb_set_permit_join()` — acquire lock, call `esp_zb_bdb_open_network()`, release lock in `drivers/zigbee/zb_real.c`
- [X] T014 [US2] Handle `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE` in signal handler — extract EUI64+NWK, update cache, emit `OS_EVENT_ZB_ANNOUNCE` in `drivers/zigbee/zb_real.c`
- [X] T015 [US2] Handle `ESP_ZB_COMMON_SIGNAL_DEVICE_JOINED` in signal handler — extract EUI64, update cache, emit `OS_EVENT_ZB_DEVICE_JOINED` in `drivers/zigbee/zb_real.c`

**Checkpoint**: Devices can join when permit join enabled, join events emitted — US2 complete

---

## Phase 5: User Story 3 — Send On/Off Command (Priority: P2)

**Goal**: Send On/Off commands to joined devices, receive confirmations

**Independent Test**: After device joins, call `zb_send_onoff()`, observe light toggles and `ZB_CMD_CONFIRM` event

### Implementation for User Story 3

- [X] T016 [US3] Implement `zb_send_onoff()` — lookup NWK from cache, allocate pending slot, acquire lock, call `esp_zb_zcl_on_off_cmd_req()` in `drivers/zigbee/zb_cmd.c`
- [X] T017 [US3] Implement send status callback body — lookup corr_id by TSN, emit `OS_EVENT_ZB_CMD_CONFIRM` or `OS_EVENT_ZB_CMD_ERROR` in `drivers/zigbee/zb_real.c`
- [X] T018 [P] [US3] Implement `zb_send_level()` — same pattern as onoff with `esp_zb_zcl_level_move_to_level_cmd_req()` in `drivers/zigbee/zb_cmd.c`

**Checkpoint**: On/Off and Level commands work end-to-end — US3 complete

---

## Phase 6: User Story 4 — Receive Attribute Reports (Priority: P2)

**Goal**: Receive unsolicited attribute reports from devices, emit events

**Independent Test**: Toggle paired light manually, verify `ZB_ATTR_REPORT` event with OnOff value

### Implementation for User Story 4

- [X] T019 [US4] Implement core action callback body — handle `ESP_ZB_CORE_REPORT_ATTR_CB_ID`, build payload, emit `OS_EVENT_ZB_ATTR_REPORT` in `drivers/zigbee/zb_real.c`
- [X] T020 [US4] Handle attribute read response in core action callback — emit `OS_EVENT_ZB_ATTR_REPORT` for each attr in `drivers/zigbee/zb_real.c`
- [X] T021 [US4] Implement `zb_read_attrs()` — acquire lock, call `esp_zb_zcl_read_attr_cmd_req()` in `drivers/zigbee/zb_cmd.c`
- [X] T022 [P] [US4] Implement `zb_configure_reporting()` — acquire lock, call `esp_zb_zcl_config_report_cmd_req()` in `drivers/zigbee/zb_cmd.c`
- [X] T023 [P] [US4] Implement `zb_bind()` — acquire lock, call `esp_zb_zdo_bind_req()` in `drivers/zigbee/zb_cmd.c`

**Checkpoint**: Attribute reports received and emitted as events — US4 complete

---

## Phase 7: User Story 5 — Device Leave Detection (Priority: P3)

**Goal**: Detect device leave, emit `ZB_DEVICE_LEFT` event, clean up cache

**Independent Test**: Factory reset a paired device, verify `ZB_DEVICE_LEFT` event

### Implementation for User Story 5

- [X] T024 [US5] Handle `ESP_ZB_ZDO_SIGNAL_LEAVE` in signal handler — extract EUI64, remove from cache, emit `OS_EVENT_ZB_DEVICE_LEFT` in `drivers/zigbee/zb_real.c`

**Checkpoint**: Device leave detected and reported — US5 complete

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Robustness, edge cases, validation

- [X] T025 Add event queue full handling — increment drop counter, log warning in `drivers/zigbee/zb_real.c`
- [X] T026 Add pending command timeout purge — check timestamps, emit timeout errors in `drivers/zigbee/zb_real.c`
- [X] T027 Verify `zb_real.c` is under 200 lines; if over, split into `zb_real.c` (lifecycle) + `zb_cmd.c` (commands)
- [X] T028 Run quickstart.md validation — build, flash, form network, join device, send command
- [X] T029 Extend `tests/unit/test_zb_adapter.c` to cover zb_real.c contract compliance (event shapes match zb_fake.c)
- [X] T030 Capture performance metrics for SC-001..SC-004 — log timestamps for formation, join detection, command RTT, attr report latency; add assertions/thresholds in quickstart or test harness
- [X] T031 Add event drop/throughput observation for SC-005 — simulate 10 events/sec sustained, confirm no drops or log drop counter behavior

---

## Dependencies & Execution Order

### Phase Dependencies

```
Phase 1: Setup
    ↓
Phase 2: Foundational ← BLOCKS ALL USER STORIES
    ↓
┌───┴───┐
↓       ↓
US1    US2 (both P1, can parallel after Phase 2)
↓       ↓
└───┬───┘
    ↓
US3 (P2, depends on US2 for joined devices)
    ↓
US4 (P2, can parallel with US3)
    ↓
US5 (P3, independent)
    ↓
Phase 8: Polish
```

### User Story Dependencies

| Story | Depends On | Rationale |
|-------|------------|-----------|
| US1 | Phase 2 | Needs state machine, callbacks registered |
| US2 | Phase 2 | Needs address cache for storing joined devices |
| US3 | US2 | Needs joined devices to send commands to |
| US4 | Phase 2 | Needs core action callback for reports |
| US5 | Phase 2 | Needs address cache to remove entries |

### Within Each Story

- Data structures before API implementations
- Callback registration before callback body implementation
- Lock acquire/release around all `esp_zb_*` calls from non-Zigbee task

### Parallel Opportunities

- T018 (send_level) can parallel with T016-T017 (send_onoff) — different command clusters
- T022, T023 (configure_reporting, bind) can parallel — independent functions
- US1 and US2 can proceed in parallel after Phase 2 (both P1)
- US3 and US4 can proceed in parallel (both P2, after US2)

---

## Parallel Example: Phase 2 Foundation

```bash
# These can run in parallel (different data structures):
T003: Address cache implementation
T004: Pending command slots implementation
T005: State machine implementation

# These must follow (registration uses state):
T006: Send status handler registration
T007: Core action handler registration
```

## Parallel Example: User Story 3 + 4

```bash
# After US2 complete, launch in parallel:
T016-T018: US3 command sending
T019-T023: US4 attribute reports
```

---

## Implementation Strategy

### MVP First (US1 + US2 Only)

1. Complete Phase 1: Setup (T001-T002)
2. Complete Phase 2: Foundational (T003-T007)
3. Complete Phase 3: US1 Network Formation (T008-T012)
4. Complete Phase 4: US2 Device Join (T013-T015)
5. **STOP and VALIDATE**: Form network, join device, see events
6. Deploy/demo — basic Zigbee coordinator working

### Full Implementation

1. MVP above
2. Add US3: Commands (T016-T018) — control devices
3. Add US4: Reports (T019-T023) — receive state updates
4. Add US5: Leave (T024) — detect removals
5. Polish (T025-T029) — robustness, validation

---

## Notes

- All implementation in `drivers/zigbee/zb_real.c` (may split if >200 lines)
- Constitution: Callbacks enqueue events and return <1ms — no business logic
- Thread safety: `esp_zb_lock_acquire()` before API calls from non-Zigbee task
- Event payloads max 32 bytes (OS_EVENT_PAYLOAD_SIZE)
- Contract: Must match `zb_adapter.h` exactly — same function signatures as `zb_fake.c`

---

## Summary

| Metric | Value |
|--------|-------|
| Total Tasks | 31 |
| Setup Tasks | 2 |
| Foundational Tasks | 5 |
| US1 Tasks (P1) | 5 |
| US2 Tasks (P1) | 3 |
| US3 Tasks (P2) | 3 |
| US4 Tasks (P2) | 5 |
| US5 Tasks (P3) | 1 |
| Polish Tasks | 7 |
| Parallel Opportunities | 6 task groups |
| MVP Scope | US1 + US2 (15 tasks) |
