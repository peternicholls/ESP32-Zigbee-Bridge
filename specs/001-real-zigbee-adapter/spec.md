# Feature Specification: Real Zigbee Adapter (M4)

**Feature Branch**: `001-real-zigbee-adapter`  
**Created**: 2026-01-20  
**Status**: Draft  
**Input**: Implement real Zigbee adapter using ESP-IDF esp-zigbee-lib to enable coordinator mode, device joining, and Zigbee command/event handling

## Overview

Replace the fake Zigbee adapter (`zb_fake.c`) with a real implementation (`zb_real.c`) using ESP-IDF's `esp-zigbee-lib`. The adapter MUST implement the existing `zb_adapter.h` contract exactly, ensuring domain logic remains unchanged. All Zigbee stack callbacks MUST enqueue events to the bus and return immediately (constitution principle: "Callbacks Enqueue Only").

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Form Zigbee Network (Priority: P1)

As a user, I want the bridge to form a Zigbee network on power-up so that devices can join.

**Why this priority**: Without a formed network, no Zigbee functionality is possible. This is the foundation.

**Independent Test**: Power cycle device, observe via serial monitor that coordinator forms network with PAN ID and channel logged.

**Acceptance Scenarios**:

1. **Given** device boots, **When** `zb_start_coordinator()` is called, **Then** `ZB_STACK_UP` event is emitted within 5 seconds
2. **Given** network is formed, **When** `ps` shell command runs, **Then** Zigbee task shows as running
3. **Given** network is formed, **When** device reboots, **Then** same PAN ID is restored from NVS

---

### User Story 2 - Device Join via Permit Join (Priority: P1)

As a user, I want to enable device pairing mode so that new Zigbee devices can join the network.

**Why this priority**: Device joining is the core use case - without it, the bridge has no devices to manage.

**Independent Test**: Enable permit join, put a Zigbee bulb in pairing mode, verify `ZB_DEVICE_JOINED` event appears in logs.

**Acceptance Scenarios**:

1. **Given** network is up, **When** `zb_set_permit_join(180)` is called, **Then** network accepts joins for 180 seconds
2. **Given** permit join is active, **When** a device joins, **Then** `ZB_DEVICE_JOINED` event is emitted with EUI64
3. **Given** device has joined, **When** device announces, **Then** `ZB_ANNOUNCE` event is emitted with NWK address
4. **Given** permit join expires, **When** a device tries to join, **Then** join is rejected

---

### User Story 3 - Send On/Off Command (Priority: P2)

As a user, I want to control a light's on/off state so that I can verify end-to-end control.

**Why this priority**: Demonstrates command path from domain → adapter → Zigbee stack → device.

**Independent Test**: After device joins and interview completes, call `zb_send_onoff()` and observe light toggles.

**Acceptance Scenarios**:

1. **Given** device is READY, **When** `zb_send_onoff(node_id, ep, true, corr_id)` called, **Then** On command sent to device
2. **Given** command is sent, **When** device ACKs, **Then** `ZB_CMD_CONFIRM` event emitted with matching `corr_id`
3. **Given** command is sent, **When** device NACKs or timeout, **Then** `ZB_CMD_ERROR` event emitted

---

### User Story 4 - Receive Attribute Reports (Priority: P2)

As a user, I want the bridge to receive unsolicited attribute reports from devices so that state stays current.

**Why this priority**: Most Zigbee devices push state changes via reports rather than requiring polling.

**Independent Test**: Toggle a paired light manually, verify `ZB_ATTR_REPORT` event appears with updated OnOff value.

**Acceptance Scenarios**:

1. **Given** device is READY, **When** device sends attribute report, **Then** `ZB_ATTR_REPORT` event emitted
2. **Given** report received, **When** event payload examined, **Then** contains node_id, endpoint, cluster, attr_id, value
3. **Given** multiple reports arrive, **When** processing, **Then** no reports dropped (within queue capacity)

---

### User Story 5 - Device Leave Detection (Priority: P3)

As a user, I want the bridge to detect when devices leave so that registry reflects reality.

**Why this priority**: Important for housekeeping but not critical for basic functionality.

**Independent Test**: Remove a device from network (factory reset it), verify `ZB_DEVICE_LEFT` event appears.

**Acceptance Scenarios**:

1. **Given** device is paired, **When** device leaves network, **Then** `ZB_DEVICE_LEFT` event emitted with EUI64
2. **Given** device left, **When** registry checked, **Then** node state transitions to OFFLINE

---

### Edge Cases

- What happens when Zigbee stack fails to initialize? → Log error, emit `ZB_STACK_DOWN`, system continues without Zigbee
- What happens when event queue is full during callback? → Drop event, increment drop counter, log warning
- What happens when command targets non-existent device? → Return error immediately, no event emitted
- What happens during Wi-Fi/Zigbee coexistence conflict? → ESP-IDF coex handles arbitration; adapter is transparent

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Adapter MUST implement all functions in `zb_adapter.h` contract
- **FR-002**: Adapter MUST use ESP-IDF `esp-zigbee-lib` for Zigbee stack interaction
- **FR-003**: All Zigbee callbacks MUST enqueue events and return within 1ms (no business logic)
- **FR-004**: Adapter MUST emit ZB_STACK_UP when coordinator forms network successfully
- **FR-005**: Adapter MUST emit ZB_DEVICE_JOINED with EUI64 when device completes join
- **FR-006**: Adapter MUST emit ZB_ATTR_REPORT for all incoming attribute reports
- **FR-007**: Adapter MUST emit ZB_CMD_CONFIRM or ZB_CMD_ERROR for all command responses
- **FR-008**: Adapter MUST persist network parameters (PAN ID, channel, keys) via Zigbee stack's NVS
- **FR-009**: Adapter MUST support correlation ID round-trip for commands
- **FR-010**: Adapter MUST NOT allocate memory dynamically in callback context
- **FR-011**: Adapter code size MUST be under 200 lines per source file (constitution constraint)

### Key Entities

- **zb_node_id_t**: EUI64 identifying a Zigbee device (stable identity)
- **zb_cmd_confirm_t**: Command acknowledgment payload (node_id, endpoint, cluster_id, status)
- **zb_cmd_error_t**: Command error payload (node_id, endpoint, cluster_id, status)
- **os_event_t**: Event envelope carrying Zigbee events to the bus

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Coordinator forms network within 5 seconds of boot
- **SC-002**: Device join is detected within 2 seconds of device completing association
- **SC-003**: On/Off command round-trip (send → confirm) completes within 500ms for responsive devices
- **SC-004**: Attribute reports are emitted within 100ms of reception
- **SC-005**: No events dropped under normal operation (≤10 events/second sustained)
- **SC-006**: Adapter passes contract tests (same event shapes as zb_fake)
- **SC-007**: Shell command `zb permit-join 60` successfully enables pairing mode

## Assumptions

- ESP-IDF v5.5+ with Zigbee stack support is available
- esp-zigbee-lib and esp-zboss-lib components are already in project dependencies
- Coordinator role (not router or end device)
- Single PAN (no multi-network support in v0)
- Standard Zigbee 3.0 clusters (OnOff 0x0006, Level 0x0008, etc.)

## Dependencies

- `espressif/esp-zigbee-lib` (already in idf_component.yml)
- `espressif/esp-zboss-lib` (already in idf_component.yml)
- Event bus (`os_event.h`)
- Existing `zb_adapter.h` contract

## Out of Scope

- Green Power devices
- Zigbee OTA updates
- Custom cluster implementations
- Touchlink commissioning
