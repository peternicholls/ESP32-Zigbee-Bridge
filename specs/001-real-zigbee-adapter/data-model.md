# Data Model: Real Zigbee Adapter

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Date**: 2026-01-20

## Entities

### 1. zb_nwk_entry_t (Address Cache Entry)

Maps device EUI64 (stable) to NWK address (dynamic).

| Field | Type | Description |
|-------|------|-------------|
| `eui64` | `os_eui64_t` | 64-bit IEEE address (stable identifier) |
| `nwk_addr` | `uint16_t` | 16-bit network short address |
| `valid` | `bool` | Entry in use |

**Constraints**:
- Max 64 entries (statically allocated)
- EUI64 is primary key (unique)
- NWK address updated on device re-announce

**State Transitions**:
```
Empty → Valid: device_joined/device_announce signal
Valid → Empty: device_left signal
Valid → Valid: device re-announce (NWK addr update)
```

---

### 2. zb_pending_cmd_t (Pending Command Slot)

Correlates outgoing command TSN with caller's correlation ID.

| Field | Type | Description |
|-------|------|-------------|
| `tsn` | `uint8_t` | ZCL transaction sequence number |
| `corr_id` | `os_corr_id_t` | Caller's correlation ID |
| `cluster_id` | `uint16_t` | Target cluster |
| `cmd_id` | `uint8_t` | Command identifier |
| `timestamp_ms` | `uint32_t` | Send time for timeout |
| `in_use` | `bool` | Slot in use |

**Constraints**:
- Max 16 slots (bounded concurrency)
- TSN provided by ZCL layer (0-255)
- Timeout: 10 seconds (stale entries purged)

**State Transitions**:
```
Empty → Pending: command sent, awaiting status callback
Pending → Empty: send status received (success or fail), emit confirm/error event
Pending → Empty: timeout exceeded, emit timeout error event
```

---

### 3. zb_state_t (Adapter State)

Internal state machine for adapter lifecycle.

| Value | Description |
|-------|-------------|
| `ZB_STATE_UNINITIALIZED` | Before `zb_init()` |
| `ZB_STATE_INITIALIZING` | Stack starting, waiting for formation |
| `ZB_STATE_READY` | Network formed, accepting commands |
| `ZB_STATE_ERROR` | Unrecoverable error |

**State Transitions**:
```
UNINITIALIZED → INITIALIZING: zb_init() called
INITIALIZING → READY: ESP_ZB_BDB_SIGNAL_FORMATION success
INITIALIZING → ERROR: formation failure
READY → ERROR: stack error signal
```

---

## Event Payloads

Events emitted to OS bus with 32-byte max payload.

### OS_EVENT_ZB_DEVICE_JOINED

| Field | Offset | Size | Type |
|-------|--------|------|------|
| `eui64` | 0 | 8 | `os_eui64_t` |
| `nwk_addr` | 8 | 2 | `uint16_t` |
| _reserved_ | 10 | 22 | - |

### OS_EVENT_ZB_ANNOUNCE

| Field | Offset | Size | Type |
|-------|--------|------|------|
| `eui64` | 0 | 8 | `os_eui64_t` |
| `nwk_addr` | 8 | 2 | `uint16_t` |
| _reserved_ | 10 | 22 | - |

### OS_EVENT_ZB_DEVICE_LEFT

| Field | Offset | Size | Type |
|-------|--------|------|------|
| `eui64` | 0 | 8 | `os_eui64_t` |
| _reserved_ | 8 | 24 | - |

### OS_EVENT_ZB_ATTR_REPORT

| Field | Offset | Size | Type |
|-------|--------|------|------|
| `eui64` | 0 | 8 | `os_eui64_t` |
| `endpoint` | 8 | 1 | `uint8_t` |
| `cluster_id` | 9 | 2 | `uint16_t` (LE) |
| `attr_id` | 11 | 2 | `uint16_t` (LE) |
| `attr_type` | 13 | 1 | `uint8_t` |
| `value` | 14 | 18 | raw bytes |

### OS_EVENT_ZB_CMD_CONFIRM

| Field | Offset | Size | Type |
|-------|--------|------|------|
| `corr_id` | 0 | 4 | `os_corr_id_t` |
| `status` | 4 | 1 | `uint8_t` (0=success) |
| _reserved_ | 5 | 27 | - |

### OS_EVENT_ZB_CMD_ERROR

| Field | Offset | Size | Type |
|-------|--------|------|------|
| `corr_id` | 0 | 4 | `os_corr_id_t` |
| `error_code` | 4 | 2 | `uint16_t` |
| _reserved_ | 6 | 26 | - |

---

## Memory Layout

Static allocation only (constitution compliance).

```c
// Address cache
static zb_nwk_entry_t s_nwk_cache[ZB_MAX_DEVICES];  // 64 * 12 = 768 bytes

// Pending command slots  
static zb_pending_cmd_t s_pending_cmds[ZB_MAX_PENDING];  // 16 * 12 = 192 bytes

// Adapter state
static zb_state_t s_state = ZB_STATE_UNINITIALIZED;

// Total static: ~1KB
```

---

## Validation Rules

1. **EUI64**: Must be non-zero (0x0000000000000000 is invalid)
2. **NWK Address**: 0x0000 reserved for coordinator, 0xFFFF = broadcast
3. **Endpoint**: Valid range 1-240 (0 = ZDO, 241-254 reserved)
4. **Correlation ID**: 0 = no correlation expected
5. **Permit Join Duration**: 0-254 seconds (255 = permanent, avoid)

---

## Relationships

```
Domain Registry (single owner)
       │
       │ events
       ▼
┌─────────────────────┐
│    OS Event Bus     │
└─────────────────────┘
       ▲
       │ emit
┌─────────────────────┐      ┌──────────────────┐
│  zb_real.c adapter  │──────│ ESP-Zigbee Stack │
│                     │ lock │                  │
│  - s_nwk_cache[]    │◄─────│ ZBOSS callbacks  │
│  - s_pending_cmds[] │      │                  │
│  - s_state          │      └──────────────────┘
└─────────────────────┘
```

The adapter is a *gateway* between ESP-Zigbee stack and OS event bus. It does NOT own device registry data — that belongs to the Domain (constitution: Single-Owner Domain Graph).
