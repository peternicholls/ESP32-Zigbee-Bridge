# Zigbee Adapter Contract

**Version**: 1.0  
**Spec**: [../spec.md](../spec.md)

## Overview

The Zigbee Adapter provides the interface between the OS and the ESP-Zigbee stack. It implements the contract defined in `drivers/zigbee/zb_adapter.h`.

## Functions

### Lifecycle

#### `zb_err_t zb_init(void)`

Initialize the Zigbee stack and start the coordinator.

**Preconditions**:
- Must be called once before any other `zb_*` function
- NVS flash must be initialized

**Postconditions**:
- Zigbee task created and running
- Signal handler registered
- Returns immediately (async formation)

**Returns**:
| Value | Meaning |
|-------|---------|
| `OS_OK` | Stack initialization started |
| `OS_ERR_INVALID_STATE` | Already initialized |

**Events Emitted** (async):
- `OS_EVENT_ZB_STACK_UP` — Network formed, ready for commands

---

#### `zb_err_t zb_start_coordinator(void)`

Request coordinator role and network formation (optional, init handles this).

**Preconditions**:
- `zb_init()` completed successfully

**Returns**:
| Value | Meaning |
|-------|---------|
| `OS_OK` | Request accepted |
| `OS_ERR_INVALID_STATE` | Not initialized |

---

### Network Management

#### `zb_err_t zb_set_permit_join(uint16_t duration_sec)`

Enable/disable device joining.

**Parameters**:
| Name | Type | Description |
|------|------|-------------|
| `duration_sec` | `uint16_t` | Join window in seconds (0=close, 1-254=open, >254 clamped to 254) |

**Preconditions**:
- Stack is ready (`OS_EVENT_ZB_STACK_UP` received)

**Postconditions**:
- Network accept/reject new joins accordingly

**Returns**:
| Value | Meaning |
|-------|---------|
| `OS_OK` | Permit join state updated |
| `OS_ERR_INVALID_STATE` | Stack not ready |

---

### Commands

All command functions are non-blocking. Results delivered via events.

#### `zb_err_t zb_send_onoff(zb_node_id_t node, uint8_t endpoint, bool on, os_corr_id_t corr_id)`

Send On/Off cluster command.

**Parameters**:
| Name | Type | Description |
|------|------|-------------|
| `node` | `zb_node_id_t` | Target device EUI64 |
| `endpoint` | `uint8_t` | Target endpoint (1-240) |
| `on` | `bool` | `true` = On, `false` = Off |
| `corr_id` | `os_corr_id_t` | Correlation ID for response (0 = no response needed) |

**Returns**:
| Value | Meaning |
|-------|---------|
| `OS_OK` | Command queued |
| `OS_ERR_NOT_FOUND` | Device not in address cache |
| `OS_ERR_NO_MEM` | No pending command slot available |
| `OS_ERR_INVALID_STATE` | Stack not ready |

**Events Emitted** (async):
- `OS_EVENT_ZB_CMD_CONFIRM` — Success, payload contains `corr_id`
- `OS_EVENT_ZB_CMD_ERROR` — Failure, payload contains `corr_id` and error code

---

#### `zb_err_t zb_send_level(zb_node_id_t node, uint8_t endpoint, uint8_t level, uint16_t transition_ds, os_corr_id_t corr_id)`

Send Level cluster Move To Level command.

**Parameters**:
| Name | Type | Description |
|------|------|-------------|
| `node` | `zb_node_id_t` | Target device EUI64 |
| `endpoint` | `uint8_t` | Target endpoint |
| `level` | `uint8_t` | Target level 0-254 |
| `transition_ds` | `uint16_t` | Transition time in deciseconds |
| `corr_id` | `os_corr_id_t` | Correlation ID |

**Returns**: Same as `zb_send_onoff`

---

#### `zb_err_t zb_read_attrs(zb_node_id_t node, uint8_t endpoint, uint16_t cluster, const uint16_t *attr_ids, uint8_t count, os_corr_id_t corr_id)`

Request attribute read from device.

**Parameters**:
| Name | Type | Description |
|------|------|-------------|
| `node` | `zb_node_id_t` | Target device EUI64 |
| `endpoint` | `uint8_t` | Target endpoint |
| `cluster` | `uint16_t` | Cluster ID |
| `attr_ids` | `const uint16_t*` | Array of attribute IDs |
| `count` | `uint8_t` | Number of attributes (max 8) |
| `corr_id` | `os_corr_id_t` | Correlation ID |

**Returns**: Same as `zb_send_onoff`

**Events Emitted** (async):
- `OS_EVENT_ZB_ATTR_REPORT` — For each attribute read (may be multiple)
- `OS_EVENT_ZB_CMD_CONFIRM` — All reads complete
- `OS_EVENT_ZB_CMD_ERROR` — Read failed

---

#### `zb_err_t zb_configure_reporting(zb_node_id_t node, uint8_t endpoint, uint16_t cluster, uint16_t attr_id, uint16_t min_interval, uint16_t max_interval, os_corr_id_t corr_id)`

Configure attribute reporting on device.

**Parameters**:
| Name | Type | Description |
|------|------|-------------|
| `node` | `zb_node_id_t` | Target device EUI64 |
| `endpoint` | `uint8_t` | Target endpoint |
| `cluster` | `uint16_t` | Cluster ID |
| `attr_id` | `uint16_t` | Attribute ID |
| `min_interval` | `uint16_t` | Min reporting interval (seconds) |
| `max_interval` | `uint16_t` | Max reporting interval (seconds) |
| `corr_id` | `os_corr_id_t` | Correlation ID |

**Returns**: Same as `zb_send_onoff`

---

#### `zb_err_t zb_bind(zb_node_id_t node, uint8_t endpoint, uint16_t cluster, os_corr_id_t corr_id)`

Create binding from device to coordinator.

**Parameters**:
| Name | Type | Description |
|------|------|-------------|
| `node` | `zb_node_id_t` | Target device EUI64 |
| `endpoint` | `uint8_t` | Source endpoint on device |
| `cluster` | `uint16_t` | Cluster to bind |
| `corr_id` | `os_corr_id_t` | Correlation ID |

**Returns**: Same as `zb_send_onoff`

---

## Events

### Stack Events

| Event | Trigger | Payload |
|-------|---------|---------|
| `OS_EVENT_ZB_STACK_UP` | Network formation complete | none |

### Device Events

| Event | Trigger | Payload |
|-------|---------|---------|
| `OS_EVENT_ZB_DEVICE_JOINED` | New device joined network | `{eui64, nwk_addr}` |
| `OS_EVENT_ZB_DEVICE_LEFT` | Device left network | `{eui64}` |
| `OS_EVENT_ZB_ANNOUNCE` | Device announced (re-join) | `{eui64, nwk_addr}` |

### Command Response Events

| Event | Trigger | Payload |
|-------|---------|---------|
| `OS_EVENT_ZB_CMD_CONFIRM` | Command delivered | `{corr_id, status}` |
| `OS_EVENT_ZB_CMD_ERROR` | Command failed | `{corr_id, error_code}` |
| `OS_EVENT_ZB_ATTR_REPORT` | Attribute value received | `{eui64, endpoint, cluster_id, attr_id, value}` |

---

## Error Codes

| Code | Value | Meaning |
|------|-------|---------|
| `OS_OK` | 0 | Success |
| `OS_ERR_INVALID_STATE` | 1 | Operation not valid in current state |
| `OS_ERR_NOT_FOUND` | 2 | Device not in cache |
| `OS_ERR_NO_MEM` | 3 | No slot available |
| `OS_ERR_TIMEOUT` | 4 | Command timed out |
| `OS_ERR_BUSY` | 5 | Resource busy |

---

## Thread Safety

- All `zb_*` functions are safe to call from any task
- Internal locking via `esp_zb_lock_acquire()`
- Callbacks run in Zigbee task context; they emit events and return immediately
