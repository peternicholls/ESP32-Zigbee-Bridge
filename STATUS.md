# STATUS

## Current state
- Date (UTC): 2026-01-15
- Commit: (see git log)
- Branch: copilot/update-project-documents
- Last completed milestone: M7
- Next milestone in progress: M8 (HA Discovery - optional)
- Active task IDs: T080
- Known blockers: M4 (Zigbee adapter) requires ESP32 hardware

## Verified working
- [x] Boots reliably
- [x] Shell responsive over UART
- [x] Fibres switching (>=3 tasks)
- [x] Event bus stable under load
- [x] Logging queue stable (no stalls)
- [x] Persistence stable across reboot
- [ ] Zigbee coordinator up (requires ESP32 hardware)
- [ ] Permit join works (requires ESP32 hardware)
- [x] Interview/provisioner works (simulated)
- [x] Registry persists and restores
- [ ] Light control end-to-end (requires ESP32 hardware)
- [x] MQTT connects and stays connected (simulated)
- [x] MQTT publishes state updates (simulated)
- [ ] MQTT commands trigger Zigbee actions (requires ESP32 hardware)

## Progress notes
**Since last update**
- M0: Created project structure and documentation
- M1: Implemented Tiny OS core (fibres, event bus, logging, console, shell)
- M2: Implemented persistence service (NVS wrapper with buffered writes)
- M3: Implemented device registry with lifecycle state machine
- M5: Implemented interview/provisioner service (simulated Zigbee)
- M6: Implemented capability mapping for lights (OnOff, Level clusters)
- M7: Implemented MQTT adapter (simulated for host testing)

**Today focus**
- Complete all implementable milestones without ESP32 hardware
- All 30 unit tests passing

**Next actions**
- M4: Zigbee adapter requires ESP32 hardware with ESP-IDF
- M8: Home Assistant discovery (optional)
- M9: Expand device classes
- M10: Matter bridge spike (bonus)

## Decisions locked in
- Substrate approach: ESP-IDF/FreeRTOS substrate; kernel-in-a-task for fibres/services
- Capability taxonomy version: caps-v0
- Command semantics: actuators optimistic_with_reconcile; sensors report_driven
- MQTT topic scheme: bridge/<node_id>/<capability>/{state|set|meta}
- Persistence schema version: 1
- Queue overflow policy: drop_oldest_noncritical; log and count drops

## Metrics
- Unit tests: 30 passing
- OS components: 7 (fibre, event, log, console, shell, persist, types)
- Services: 4 (registry, interview, capability, reg_shell)
- Adapters: 1 (mqtt)

## Open questions
- None at this time

## Blockers and risks
**Blockers**
- M4 (Zigbee adapter) requires ESP32-C6 hardware with ESP-IDF

**Risks**
- R1: Zigbee stack callback context constraints
- R2: Persistence schema evolution
- R3: Device quirks / non-standard attributes
- R4: Cooperative scheduling starvation

## How to resume
Paste: Context Capsule + this STATUS.md + the next task request (e.g., "Implement M4 with ESP32 hardware").
