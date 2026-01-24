# Implementation Plan: Real Zigbee Adapter (M4)

**Branch**: `001-real-zigbee-adapter` | **Date**: 2026-01-20 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-real-zigbee-adapter/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

Replace the fake Zigbee adapter (`zb_fake.c`) with a real implementation (`zb_real.c`) using ESP-IDF's `esp-zigbee-lib`. The adapter MUST implement the existing `zb_adapter.h` contract exactly (8 functions), ensuring all Zigbee stack callbacks enqueue events to the bus and return immediately. The implementation uses coordinator mode to form networks, manage device joins, and route commands/events.

## Technical Context

**Language/Version**: C11 (ESP-IDF v5.5+)  
**Primary Dependencies**: espressif/esp-zigbee-lib, espressif/esp-zboss-lib (already in idf_component.yml)  
**Storage**: Zigbee stack's internal NVS for network parameters (PAN ID, channel, keys)  
**Testing**: Unity-based unit tests (tests/unit/test_zb_adapter.c), contract tests against fake adapter  
**Target Platform**: ESP32-C6 (Zigbee 3.0 coordinator)  
**Project Type**: Single embedded firmware project  
**Performance Goals**: Network formation &lt;5s, device join detection &lt;2s, command round-trip &lt;500ms, attribute report latency &lt;100ms  
**Constraints**: &lt;200 lines per source file, no dynamic allocation in callbacks, callbacks return &lt;1ms  
**Scale/Scope**: Single PAN, 64 devices max (esp_zb_overall_network_size_set default)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Compliance | Notes |
|-----------|------------|-------|
| **I. Single-Owner Domain Graph** | PASS | Adapter only emits events to bus; no direct registry mutation |
| **II. Callbacks Enqueue Only** | PASS | All esp_zb callbacks will enqueue os_event_t and return &lt;1ms |
| **III. Deterministic Event Loop** | PASS | Events use typed payloads (zb_cmd_confirm_t, etc.) with correlation IDs |
| **IV. Bounded Resources** | PASS | Event queue is fixed-size; explicit drop policy with counter |
| **V. Idempotent Outputs** | N/A | Adapter is ingress-only; MQTT/HA handled elsewhere |
| **VI. Capability Abstraction** | N/A | Adapter emits raw events; capability layer is separate |
| **Code Size Budget** | PASS | Target &lt;200 lines for zb_real.c |
| **No Dynamic Allocation in Callbacks** | PASS | All payloads fit in os_event_t.payload[32] |

**Gate Status**: PASS - No violations. Proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/001-real-zigbee-adapter/
├── plan.md              # This file (/speckit.plan command output) ✓
├── research.md          # Phase 0 output (/speckit.plan command) ✓
├── data-model.md        # Phase 1 output (/speckit.plan command) ✓
├── quickstart.md        # Phase 1 output (/speckit.plan command) ✓
├── contracts/           # Phase 1 output (/speckit.plan command) ✓
│   └── zb_adapter_contract.md  # API contract
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
drivers/zigbee/
├── zb_adapter.h         # Contract (existing, unchanged)
├── zb_fake.c            # Host simulation (existing, unchanged)
└── zb_real.c            # NEW: Real ESP-Zigbee implementation

tests/unit/
├── test_zb_adapter.c    # Contract tests (existing, extend)
└── test_zb_adapter.h

main/
└── src/                 # App integration (calls zb_init, zb_start_coordinator)
```

**Structure Decision**: Embedded single-project structure. New implementation file `zb_real.c` alongside existing `zb_fake.c`. Build system selects one based on target (host vs ESP32-C6).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

*No violations identified. Table intentionally empty.*
