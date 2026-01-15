# ESP32-C6 Clean Zigbee Bridge OS

## Context Capsule

Project: ESP32-C6 Zigbee bridge with Wi-Fi + 802.15.4 (Zigbee). Approach: ESP-IDF/FreeRTOS
is substrate for boot + vendor Zigbee/Wi-Fi stacks. Our "tiny OS" runs as kernel-in-a-task
hosting fibres/green threads, event bus, timers, logging, persistence, shell. Canonical model:
Node→Endpoint→Cluster→Attribute. Capability layer provides stable abstractions (light.on, etc.)
and maps to Zigbee clusters. Northbound: MQTT first; design keeps runway for Matter/HomeKit
bridging later. Non-goals v0: no filesystem, no custom bootloader, no reimplementing Zigbee.

## Project Goals

- Coordinator can form Zigbee network, permit joins, manage devices
- Clean internal device graph model (Node/Endpoint/Cluster/Attribute)
- Capability abstraction layer (stable, typed) mapped to Zigbee clusters
- Event-driven services via an internal bus (no callback soup)
- Robust persistence to survive reboot without re-pairing
- Northbound MQTT adapter for state publish + command ingest
- Developer UX: UART shell, structured logs, ps/uptime/devices

## Non-Goals (v0)

- Reimplement Zigbee stack or Wi-Fi stack from scratch
- Custom bootloader / full bare-metal startup
- Full Zigbee2MQTT feature parity
- Matter bridge in v0 (design runway only)

## Target Hardware

- **Board:** ESP32-C6 dev board
- **CPU:** RISC-V
- **Radios:** Wi-Fi, 802.15.4 (Zigbee)

## Architecture Overview

### Layers

```
┌──────────────────────────────────────────────────┐
│  Northbound Adapters                             │
│  - MQTT adapter (v0)                             │
│  - HA Discovery (optional)                       │
│  - Matter bridge adapter (later)                 │
├──────────────────────────────────────────────────┤
│  Domain Services                                 │
│  - Device registry + lifecycle                   │
│  - Interview/provisioner                         │
│  - Capability mapper                             │
│  - Command router                                │
├──────────────────────────────────────────────────┤
│  Tiny OS (ours)                                  │
│  - Fibre scheduler                               │
│  - Timers/tick                                   │
│  - Event bus                                     │
│  - Logging/tracing                               │
│  - Persistence service wrapper                   │
│  - UART shell                                    │
├──────────────────────────────────────────────────┤
│  Substrate                                       │
│  - ROM + bootloader + ESP-IDF startup            │
│  - FreeRTOS runtime                              │
│  - Vendor Zigbee stack                           │
│  - Wi-Fi stack                                   │
│  - NVS                                           │
└──────────────────────────────────────────────────┘
```

### Approach: Kernel-in-a-Task

Run our tiny OS inside a single high-priority FreeRTOS task. Vendor Zigbee/Wi-Fi stacks
run in their required substrate tasks. Our OS schedules fibres/green threads for services
and app logic, connected via an event bus.

**Scheduling:** Cooperative fibres with sleep/yield
**Tick source:** ESP-IDF timer (periodic), ~1ms granularity

## Data Model

### Canonical Structure

```
Node (ieee_eui64, nwk_short_addr)
├── metadata (manufacturer, model, sw_build)
├── telemetry (lqi, rssi, power_source)
└── Endpoints[]
    ├── endpoint_id, profile_id, device_id
    └── Clusters[]
        ├── cluster_id, direction (server|client)
        └── Attributes[]
            └── attribute_id, type, value, last_updated_ts
```

### Capability Layer

Stable product/API abstraction, decoupled from Zigbee specifics.
Mapping: Clusters/attributes ↔ capabilities

**Actuators:**
- `switch.on` (bool)
- `light.on` (bool)
- `light.level` (int, 0-100%)

**Sensors:**
- `sensor.temperature` (float, °C)
- `sensor.humidity` (float, %)
- `sensor.contact` (bool)

**Power:**
- `power.watts` (float, W)
- `energy.kwh` (float, kWh)

## Event Bus

**Envelope fields:** type, ts, src, corr_id, payload (typed union)

**Guidance:**
- Zigbee callbacks must enqueue events quickly; never run business logic in callback context.
- Use fixed-size queues initially; define drop/backpressure policy.

**Minimum event types:**
- BOOT, LOG, NET_UP, NET_DOWN
- ZB_STACK_UP, ZB_DEVICE_JOINED, ZB_DEVICE_LEFT, ZB_ANNOUNCE
- ZB_DESC_ENDPOINTS, ZB_DESC_CLUSTERS, ZB_ATTR_REPORT
- CAP_STATE_CHANGED, CAP_COMMAND, PERSIST_FLUSH

## MQTT Topics (v0)

| Purpose | Topic Pattern |
|---------|---------------|
| State   | `bridge/<node_id>/<capability>/state` |
| Command | `bridge/<node_id>/<capability>/set` |
| Meta    | `bridge/<node_id>/meta` |
| Status  | `bridge/status` |

**Payload conventions:** Keep payloads typed and small, include timestamp when useful.

Example: `{"v": true, "ts": 123456}`

## Repository Structure

```
├── docs/       # Documentation
├── main/       # Application entry point
├── os/         # Tiny OS components (scheduler, bus, timers, logging, shell)
├── services/   # Domain services (registry, interview, capabilities)
├── adapters/   # Northbound adapters (MQTT, HA discovery)
├── drivers/    # Hardware drivers (UART console, Zigbee adapter)
└── apps/       # Demo apps (blink, etc.)
```

## Coding Standards

- **Language:** C (with minimal asm only if needed later)
- **Style:** Explicit sizes (uint32_t etc.), no hidden globals, no long critical sections
- **Error handling:** Return explicit error codes, never silently drop critical events
- **Module prefixes:** `os_`, `bus_`, `log_`, `persist_`, `reg_`, `zb_`, `mqtt_`, `cap_`

## Developer UX

### Shell Commands (v0)
- `help`, `ps`, `uptime`, `loglevel`

### Shell Commands (planned)
- `devices`, `device <id>`, `zb permit-join <sec>`, `mqtt status`, `mqtt pub <topic> <payload>`

### Logging
- Structured format with levels: ERROR, WARN, INFO, DEBUG, TRACE
- Safe to call from ISR/callback (enqueue); avoid blocking

## Acceptance Criteria

### System
- Boots and shell is responsive over UART
- Fibre scheduler runs >=3 tasks reliably (idle/shell/blink)
- Event bus stable under concurrent producers

### Zigbee
- Coordinator forms network and permit join works
- Device join creates registry entry
- Interview populates endpoints and basic metadata
- At least one device class controllable end-to-end

### Northbound
- MQTT publishes state changes
- MQTT command triggers Zigbee actuation
- Reboot restores registry without requiring re-pair (reasonable cases)
