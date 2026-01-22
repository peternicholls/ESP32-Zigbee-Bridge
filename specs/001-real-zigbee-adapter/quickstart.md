# Quickstart: Real Zigbee Adapter

**Spec**: [spec.md](spec.md) | **Contract**: [contracts/zb_adapter_contract.md](contracts/zb_adapter_contract.md)

## Prerequisites

- ESP-IDF v5.5+ installed and activated
- ESP32-C6 devkit connected
- USB-C cable for flashing

## Build & Flash

```bash
# Clone (if not already)
cd ~/code/ESP32-C6-GatewayOS

# Set target (once)
idf.py set-target esp32c6

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/tty.usbmodem* flash monitor
```

## Expected Startup Sequence

```
I (xxx) zb_real: Initializing Zigbee stack
I (xxx) zb_real: Role: Coordinator, Channel: auto
I (xxx) zb_real: Starting network formation
I (xxx) zb_real: Network formed, PAN ID: 0xXXXX, Channel: XX
I (xxx) os_event: [ZB_STACK_UP]
```

## Testing Permit Join

From serial console (once shell is integrated):

```
> zb permit 60
OK: Permit join enabled for 60s
```

Or via code:
```c
zb_set_permit_join(60);  // 60 seconds
```

## Testing Device Join

1. Enable permit join (see above)
2. Put Zigbee device in pairing mode (typically hold button 5+ sec)
3. Watch for join event:

```
I (xxx) zb_real: Device joined: EUI64=00:11:22:33:44:55:66:77, NWK=0x1234
I (xxx) os_event: [ZB_DEVICE_JOINED] eui64=0011223344556677
```

## Testing Commands

Send On command to endpoint 1 of device:

```c
os_corr_id_t corr = os_corr_id_new();
zb_send_onoff(0x0011223344556677ULL, 1, true, corr);
// Wait for OS_EVENT_ZB_CMD_CONFIRM with matching corr_id
```

## File Structure

```
drivers/zigbee/
├── zb_adapter.h     # Contract (unchanged)
├── zb_fake.c        # Host simulation (existing)
├── zb_real.c        # NEW: ESP-Zigbee implementation
└── CMakeLists.txt   # Build config
```

## Implementation Checklist

- [ ] Create `zb_real.c` skeleton
- [ ] Implement `zb_init()` — create Zigbee task, register handlers
- [ ] Implement signal handler — emit stack/device events
- [ ] Implement address cache — track EUI64↔NWK
- [ ] Implement `zb_set_permit_join()` — call `esp_zb_bdb_open_network()`
- [ ] Implement `zb_send_onoff()` — send command, track TSN
- [ ] Implement `zb_send_level()` — level cluster command
- [ ] Implement send status callback — emit confirm/error
- [ ] Implement core action callback — emit attr reports
- [ ] Implement `zb_read_attrs()` — read request
- [ ] Implement `zb_configure_reporting()` — configure reports
- [ ] Implement `zb_bind()` — bind request

## Key Implementation Notes

1. **Callback Pattern**: All ESP-Zigbee callbacks must enqueue events and return immediately. No blocking, no business logic.

2. **Locking**: Call `esp_zb_lock_acquire()` before any `esp_zb_*` API call from non-Zigbee task.

3. **Address Resolution**: Commands use NWK address internally. Look up from cache by EUI64.

4. **Correlation**: Store `corr_id`→`tsn` mapping when sending; look up on status callback.

5. **Line Limit**: Keep `zb_real.c` under 200 lines. Split if needed:
   - `zb_real.c` — lifecycle, init, permit join
   - `zb_cmd.c` — command sending, correlation
   - `zb_cb.c` — callbacks, event emission

## Debugging

Enable Zigbee logging:
```
idf.py menuconfig
# → Component config → ESP-Zigbee → Zigbee Debug Log Level = Info
```

Monitor with verbose:
```bash
idf.py monitor -p /dev/tty.usbmodem*
```

## Test Hardware

Recommended test devices:
- Sonoff SNZB-02 (temp/humidity sensor)
- Sonoff SNZB-01 (button)
- IKEA TRADFRI bulb (dimmable)

These are well-documented Zigbee 3.0 devices that work reliably with ESP-Zigbee.
