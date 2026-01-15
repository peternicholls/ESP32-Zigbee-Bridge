# ESP32-C6 Zigbee Bridge

A clean, modular Zigbee coordinator and bridge system for the ESP32-C6, designed to bridge Zigbee devices to MQTT for integration with home automation systems like Home Assistant.

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](.)
[![Tests](https://img.shields.io/badge/tests-30%20passing-brightgreen)](.)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

## Features

- **Zigbee Coordinator**: Forms and manages a Zigbee 3.0 network
- **Device Management**: Automatic device discovery, interview, and provisioning
- **Capability Abstraction**: Maps Zigbee clusters to stable, typed capabilities
- **MQTT Integration**: Publishes device states and receives commands via MQTT
- **Persistence**: Device registry survives reboots without re-pairing
- **Interactive Shell**: UART-based shell for debugging and configuration
- **Event-Driven Architecture**: Clean, decoupled services via internal event bus

## Table of Contents

- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Building](#building)
- [Running Tests](#running-tests)
- [Architecture](#architecture)
- [Shell Commands](#shell-commands)
- [MQTT Topics](#mqtt-topics)
- [Configuration](#configuration)
- [Development](#development)
- [Project Status](#project-status)
- [License](#license)

## Requirements

### Hardware

- **ESP32-C6** development board (for target deployment)
- USB cable for programming and UART console

### Software (Host Development)

- GCC compiler (C11 support)
- GNU Make
- POSIX-compatible system (Linux, macOS, WSL)

### Software (ESP32 Target)

- ESP-IDF v5.0+ (with Zigbee support)

## Quick Start

### Clone the Repository

```bash
git clone https://github.com/peternicholls/ESP32-Zigbee-Bridge.git
cd ESP32-Zigbee-Bridge
```

### Build and Run Tests (Host)

```bash
make test
```

### Build the Main Application (Host Simulation)

```bash
make
./build/bridge
```

### ESP32-C6 Target Build

```bash
# Set up ESP-IDF environment first
. $IDF_PATH/export.sh

# Build for ESP32-C6
idf.py set-target esp32c6
idf.py build
idf.py flash monitor
```

## Building

### Host Build (Simulation)

The project includes a host build for testing and development without ESP32 hardware:

```bash
# Clean build artifacts
make clean

# Build main application
make

# Build and run tests
make test

# Run the bridge (simulated)
make run
```

### Build Targets

| Target | Description |
|--------|-------------|
| `make` or `make all` | Build the main bridge application |
| `make test` | Build and run unit tests |
| `make run` | Build and run the bridge |
| `make clean` | Remove all build artifacts |

### Compiler Flags

The build uses strict compiler flags for code quality:

- `-Wall -Wextra -Werror`: All warnings as errors
- `-std=c11`: C11 standard
- `-g -O0`: Debug symbols, no optimization (development)
- `-DOS_PLATFORM_HOST=1`: Host platform indicator

## Running Tests

The test suite validates all core OS and service components:

```bash
make test
```

Expected output:
```
=== ESP32-C6 Zigbee Bridge OS Unit Tests ===

Type tests:
  Testing types... PASS

Event bus tests:
  Testing event_init... PASS
  ...

=== Results ===
Passed: 30
Failed: 0
```

### Test Coverage

- **Type tests**: Size assumptions and macros
- **Event bus tests**: Publish, subscribe, filter, dispatch
- **Log tests**: Levels, formatting, queue
- **Persistence tests**: Put, get, flush, schema version
- **Registry tests**: Node, endpoint, cluster, attribute management
- **Interview tests**: Device discovery simulation
- **Capability tests**: Cluster-to-capability mapping

## Architecture

### Layer Diagram

```
┌──────────────────────────────────────────────────┐
│  Northbound Adapters                             │
│  - MQTT adapter                                  │
│  - HA Discovery (planned)                        │
├──────────────────────────────────────────────────┤
│  Domain Services                                 │
│  - Device registry + lifecycle                   │
│  - Interview/provisioner                         │
│  - Capability mapper                             │
├──────────────────────────────────────────────────┤
│  Tiny OS                                         │
│  - Fibre scheduler (cooperative multitasking)    │
│  - Event bus                                     │
│  - Logging/tracing                               │
│  - Persistence service                           │
│  - UART shell                                    │
├──────────────────────────────────────────────────┤
│  Substrate (ESP32 only)                          │
│  - ESP-IDF / FreeRTOS                            │
│  - Vendor Zigbee stack                           │
│  - Wi-Fi stack                                   │
│  - NVS                                           │
└──────────────────────────────────────────────────┘
```

### Directory Structure

```
├── docs/           # Documentation
├── main/           # Application entry point
│   └── src/
│       └── main.c
├── os/             # Tiny OS components
│   ├── include/    # Public headers
│   │   ├── os.h
│   │   ├── os_config.h
│   │   ├── os_event.h
│   │   ├── os_fibre.h
│   │   ├── os_log.h
│   │   ├── os_persist.h
│   │   └── os_shell.h
│   └── src/        # Implementation
├── services/       # Domain services
│   ├── include/
│   │   ├── registry.h
│   │   ├── interview.h
│   │   └── capability.h
│   └── src/
├── adapters/       # Northbound adapters
│   ├── include/
│   │   └── mqtt_adapter.h
│   └── src/
├── drivers/        # Hardware drivers (placeholder)
├── apps/           # Demo applications
│   └── src/
│       └── app_blink.c
└── tests/          # Test suite
    └── unit/
        └── test_os.c
```

### Data Model

The device model follows the Zigbee canonical structure:

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

Capabilities provide stable abstractions mapped to Zigbee clusters:

| Capability | Type | Description |
|------------|------|-------------|
| `switch.on` | bool | On/Off switch state |
| `light.on` | bool | Light on/off state |
| `light.level` | int (0-100) | Light brightness percentage |
| `light.color_temp` | int (mireds) | Color temperature |
| `sensor.temperature` | float (°C) | Temperature reading |
| `sensor.humidity` | float (%) | Humidity percentage |
| `sensor.contact` | bool | Contact sensor state |
| `sensor.motion` | bool | Motion detection |
| `power.watts` | float (W) | Power consumption |
| `energy.kwh` | float (kWh) | Energy usage |

## Shell Commands

The interactive shell is accessible via UART (115200 baud) or stdin when running on host.

### Built-in Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `ps` | Show running tasks/fibres |
| `uptime` | Show system uptime |
| `loglevel [level]` | Get/set log level (ERROR, WARN, INFO, DEBUG, TRACE) |
| `stats` | Show event bus statistics |

### Device Commands

| Command | Description |
|---------|-------------|
| `devices` | List all registered Zigbee devices |
| `device <addr>` | Show detailed device information |

### Example Session

```
=== ESP32-C6 Zigbee Bridge Shell ===
Type 'help' for available commands.

> help
Available commands:
  help         - Show available commands
  ps           - Show running tasks
  uptime       - Show system uptime
  loglevel     - Get/set log level [level]
  stats        - Show event bus statistics
  devices      - List all registered devices
  device       - Show device details <addr>

> ps
ID   NAME         STATE      STACK     USED       RUNS
---- ------------ ---------- -------- -------- ----------
0    idle         READY          512        0          5
1    shell        RUNNING       4096      128         10
2    blink        SLEEPING      2048       64         50
3    dispatch     SLEEPING      2048       32         25

> uptime
Uptime: 00:05:23.456 (323456 ticks)

> stats
Event Bus Statistics:
  Published:    156
  Dispatched:   156
  Dropped:      0
  Queue size:   0
  High water:   12
```

## MQTT Topics

The MQTT adapter uses a structured topic scheme:

| Purpose | Topic Pattern | Example |
|---------|---------------|---------|
| State | `bridge/<node_id>/<capability>/state` | `bridge/00112233AABBCCDD/light.on/state` |
| Command | `bridge/<node_id>/<capability>/set` | `bridge/00112233AABBCCDD/light.on/set` |
| Metadata | `bridge/<node_id>/meta` | `bridge/00112233AABBCCDD/meta` |
| Status | `bridge/status` | `bridge/status` |

### Payload Format

All payloads use JSON with a value and timestamp:

```json
{"v": true, "ts": 123456}
```

### Examples

**Light state update:**
```json
// Topic: bridge/00112233AABBCCDD/light.on/state
{"v": true, "ts": 123456}
```

**Temperature sensor:**
```json
// Topic: bridge/00112233AABBCCDD/sensor.temperature/state
{"v": 22.5, "ts": 123789}
```

**Device metadata:**
```json
// Topic: bridge/00112233AABBCCDD/meta
{"ieee": "00112233AABBCCDD", "manufacturer": "IKEA", "model": "TRADFRI bulb"}
```

## Configuration

Configuration is done via `os/include/os_config.h`:

```c
/* Scheduler configuration */
#define OS_MAX_FIBRES           16      /* Maximum concurrent tasks */
#define OS_DEFAULT_STACK_SIZE   2048    /* Default stack per task */

/* Event bus configuration */
#define OS_EVENT_QUEUE_SIZE     256     /* Event queue depth */
#define OS_MAX_SUBSCRIBERS      32      /* Max event subscribers */

/* Logging configuration */
#define OS_LOG_QUEUE_SIZE       64      /* Log buffer size */
#define OS_LOG_DEFAULT_LEVEL    OS_LOG_LEVEL_INFO

/* Persistence configuration */
#define OS_PERSIST_FLUSH_MS     5000    /* Auto-flush interval */
```

### Device Registry Limits

From `services/include/reg_types.h`:

```c
#define REG_MAX_NODES           32      /* Max Zigbee devices */
#define REG_MAX_ENDPOINTS       8       /* Endpoints per device */
#define REG_MAX_CLUSTERS        16      /* Clusters per endpoint */
#define REG_MAX_ATTRIBUTES      32      /* Attributes per cluster */
```

## Development

### Coding Standards

- **Language**: C11 with GCC extensions
- **Style**: Explicit sizes (`uint32_t`, etc.), no hidden globals
- **Error handling**: Return explicit error codes
- **Module prefixes**: `os_`, `reg_`, `cap_`, `mqtt_`, etc.

### Adding a New Shell Command

```c
#include "os.h"

static int cmd_mycommand(int argc, char *argv[]) {
    printf("My command executed!\n");
    return 0;
}

// Register during init
static const os_shell_cmd_t cmd = {
    "mycommand", "Description", cmd_mycommand
};
os_shell_register(&cmd);
```

### Adding a New Capability

1. Add the capability ID in `capability.h`:
   ```c
   typedef enum {
       // ...
       CAP_MY_NEW_CAP,
       CAP_MAX
   } cap_id_t;
   ```

2. Add the info entry in `capability.c`:
   ```c
   static const cap_info_t cap_info_table[] = {
       // ...
       {CAP_MY_NEW_CAP, "my.capability", CAP_VALUE_INT, "unit"},
   };
   ```

3. Add the cluster mapping if applicable:
   ```c
   static const cluster_cap_map_t cluster_map[] = {
       // ...
       {CLUSTER_ID, ATTR_ID, CAP_MY_NEW_CAP},
   };
   ```

### Log Levels

```c
LOG_E("MODULE", "Error: %s", msg);    // Errors only
LOG_W("MODULE", "Warning: %s", msg);  // Warnings
LOG_I("MODULE", "Info: %s", msg);     // Informational (default)
LOG_D("MODULE", "Debug: %s", msg);    // Debug details
LOG_T("MODULE", "Trace: %s", msg);    // Very verbose
```

## Project Status

### Completed Milestones

- **M0**: Project structure and documentation
- **M1**: Tiny OS core (fibres, event bus, logging, console, shell)
- **M2**: Persistence service (NVS wrapper with buffered writes)
- **M3**: Device registry with lifecycle state machine
- **M5**: Interview/provisioner service (simulated)
- **M6**: Capability mapping for lights (OnOff, Level clusters)
- **M7**: MQTT adapter (simulated for host testing)

### In Progress

- **M8**: Home Assistant discovery (optional)

### Requires ESP32 Hardware

- **M4**: Zigbee adapter integration with ESP-IDF Zigbee stack

### Planned

- **M9**: Expand device classes (sensors, switches, etc.)
- **M10**: Matter bridge spike (bonus)

### Verified Working (Host Simulation)

- [x] Boots reliably
- [x] Shell responsive
- [x] Fibres switching (≥3 tasks)
- [x] Event bus stable under load
- [x] Logging queue stable
- [x] Persistence stable
- [x] Interview/provisioner works (simulated)
- [x] Registry persists and restores
- [x] MQTT connects and publishes (simulated)

### Requires ESP32 Hardware

- [ ] Zigbee coordinator up
- [ ] Permit join works
- [ ] Light control end-to-end
- [ ] MQTT commands trigger Zigbee actions

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch
3. Ensure all tests pass (`make test`)
4. Follow the existing code style
5. Submit a pull request

## Support

For issues and feature requests, please use the GitHub Issues tracker.

---

**Note**: This project is under active development. The Zigbee integration requires ESP32-C6 hardware and the ESP-IDF Zigbee stack for full functionality. Host builds provide simulation for development and testing
